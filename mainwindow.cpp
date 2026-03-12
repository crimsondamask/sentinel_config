#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QWidget>
#include <QtNetwork/QtNetwork>

SentinelTag::SentinelTag(qint16 id, QString tk) {
    this->id      = id;
    this->tk      = QString("%1:%2").arg(tk).arg(id);
    this->name    = QString("TAG_%1").arg(id);
    this->enabled = false;
    this->address = SentinelTagAddress{.type = 0, .modbus_register = 0};
    this->value   = SentinelTagValue{
          .type = 0, .real_value = 0.0, .int_value = 0, .bit_value = 0};
    this->status = QString("Initialized.");
}

QString SentinelTag::displayName() { return QString("%1").arg(this->name); }

QString SentinelTag::displayTk() { return QString("%1").arg(this->tk); }

QString SentinelTag::displayStatus() { return QString("%1").arg(this->status); }

bool SentinelTag::isEnabled() { return this->enabled; }

QString SentinelTag::displayValue() {
    switch (this->value.type) {
    case ST_INT_VALUE:
        return QString("%1").arg(this->value.int_value);
        break;
    case ST_REAL_VALUE:
        return QString("%1").arg(this->value.real_value);
        break;
    case ST_BIT_VALUE:
        return QString("%1").arg(this->value.bit_value);
        break;

    default:
        return QString("%1").arg(this->value.int_value);
        break;
    }
}

SentinelLink::SentinelLink(qint16 id, QString tk) {
    this->id       = id;
    this->tk       = tk;
    this->name     = QString("%1%2").arg(tk).arg(id);
    this->enable   = true;
    this->protocol = ST_MODBUS_TCP;
    this->tags.reserve(N_CHANNELS);
    for (size_t i = 0; i < N_CHANNELS; i++) {
        SentinelTag tag = SentinelTag(i, tk);
        this->tags.push_back(tag);
    }
    this->tag_count      = N_CHANNELS;
    this->status         = QString("Link is disconnected");
    this->last_poll_time = QString("N/A");
}

SentinelTableModel::SentinelTableModel(QObject *parent)
    : QAbstractTableModel(parent) {

    link_data = nullptr;
}

int SentinelTableModel::rowCount(const QModelIndex &parent) const {

    if (link_data == nullptr || link_data->tags.capacity() != N_CHANNELS) {
        return 5;
    }
    return N_CHANNELS;
}

int SentinelTableModel::columnCount(const QModelIndex &parent) const {
    return 6;
}

QVariant SentinelTableModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        if (link_data == nullptr || link_data->tags.capacity() != N_CHANNELS) {
            if (index.column() == 5) {
                return QString("No data available.");
            } else {
                return QString("");
            }
        }

        for (size_t i = 0; i < N_CHANNELS; i++) {
            if (index.row() == i) {
                if (index.column() == 0) {
                    return QString("%1").arg(link_data->tags[i].displayTk());
                }
                if (index.column() == 1) {
                    return QString("%1").arg(link_data->tags[i].displayName());
                }
                if (index.column() == 2) {
                    if (!link_data->tags[i].isEnabled()) {
                        return QString("DISABLED");
                    } else {
                        return QString("%1").arg(
                            link_data->tags[i].displayValue());
                    }
                }
            }
        }
    }
    return QString("");
}
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      statusLabel(new QLabel("Disconnected.")), downloadButton(new QPushButton),
      pollTimer(new QTimer(this)), centralWidget(new QWidget(this)) {

    ui->setupUi(this);

    url = QUrl::fromUserInput(
        QString("http://localhost:3000/api/get_links_config").trimmed());

    setWindowTitle("Sentinel Monitor V0.0.1");

    connect(pollTimer, &QTimer::timeout, this, &MainWindow::initRequest);
    pollTimer->start(1000);
    // Repeating timer
    pollTimer->setSingleShot(false);
    linksList = new QComboBox();

    linksList->setFixedWidth(300);
    linksList->addItem("Link_1");
    linksList->addItem("Link_2");

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addWidget(linksList);
    formLayout->addRow(statusLabel);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(formLayout);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
}

MainWindow::~MainWindow() { delete ui; }
bool    MainWindow::isError() { return this->error; }
QString MainWindow::errorString() { return this->serverError; }
void    MainWindow::initRequest() {
    reply.reset(qnam.get(QNetworkRequest(url)));
    connect(reply.get(), &QNetworkReply::finished, this,
               &MainWindow::parseServerData);
}
void MainWindow::parseServerData() {
    QByteArray      readData = reply->readAll();
    QJsonParseError jsonError;
    QJsonDocument   doc = QJsonDocument::fromJson(readData, &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        this->error       = true;
        this->serverError = jsonError.errorString();
        this->statusLabel->setText(this->serverError);
        return;
    }

    if (!doc.isArray()) {
        this->error       = true;
        this->serverError = QString("JSON is not an Array");
        this->statusLabel->setText(this->serverError);
        return;
    }
    qsizetype arrayLen = doc.array().count();
    this->numLinks     = arrayLen;

    for (size_t i = 0; i < arrayLen; i++) {

        QJsonValue value = doc.array()[i];

        if (!value.isObject()) {
            this->error       = true;
            this->serverError = QString("JSON Array doesn't contain objects.");
            this->statusLabel->setText(this->serverError);
            return;
        }

        if (value.toObject().value("Device").isObject()) {
            QJsonObject deviceLinkObject =
                value.toObject().value("Device").toObject();

            int idValue = deviceLinkObject.value("id").toInt();
            if (!deviceLinkObject.value("tk").isString()) {
                this->error       = true;
                this->serverError = QString("Could not parse link tk value.");
                this->statusLabel->setText(this->serverError);
                return;
            }

            QString tkValue = deviceLinkObject.value("tk").toString();

            if (!deviceLinkObject.value("name").isString()) {
                this->error       = true;
                this->serverError = QString("Could not parse link name value.");
                this->statusLabel->setText(this->serverError);
                return;
            }

            QString nameValue = deviceLinkObject.value("name").toString();
            linksList->setItemText(i, nameValue);
        }
    }
}
