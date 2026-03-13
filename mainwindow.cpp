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

QString SentinelTag::displayName() const {
    return QString("%1").arg(this->name);
}

QString SentinelTag::displayTk() const { return QString("%1").arg(this->tk); }

QString SentinelTag::displayStatus() const {
    return QString("%1").arg(this->status);
}
QString SentinelTag::displayDetails() const {
    return QString("%1").arg(this->tagDetails);
}

bool SentinelTag::isEnabled() const { return this->enabled; }

QString SentinelTag::displayValue() const {
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
    : QAbstractTableModel(parent),
      link_data(SentinelLink(0, QString("LINK1"))) {}

int SentinelTableModel::rowCount(const QModelIndex &parent) const {
    return N_CHANNELS;
}

int SentinelTableModel::columnCount(const QModelIndex &parent) const {
    return 5;
}

void SentinelTableModel::setTableData(SentinelLink *link) {
    this->link_data         = *link;
    QModelIndex topLeft     = createIndex(0, 0);
    QModelIndex bottomRight = createIndex(5, N_CHANNELS - 1);
    emit dataChanged(topLeft, bottomRight, {Qt::DisplayRole});
}

Qt::ItemFlags SentinelTableModel::flags(const QModelIndex &index) const {
    return QAbstractTableModel::flags(index) & ~Qt::ItemIsUserCheckable;
}
QVariant SentinelTableModel::data(const QModelIndex &index, int role) const {
    for (size_t i = 0; i < N_CHANNELS; i++) {
        if (index.row() == i) {
            if (role == Qt::DisplayRole && index.column() == 0) {
                return QString("%1").arg(link_data.tags[i].displayTk());
            }
            if (role == Qt::DisplayRole && index.column() == 1) {
                return QString("%1").arg(link_data.tags[i].displayName());
            }
            if (role == Qt::DisplayRole && index.column() == 2) {
                if (!link_data.tags[i].isEnabled()) {
                    return QString("DISABLED");
                } else {
                    return QString("%1").arg(link_data.tags[i].displayValue());
                }
            }
            if (role == Qt::DisplayRole && index.column() == 3) {
                return QString("%1").arg(link_data.tags[i].displayDetails());
            }
            if (role == Qt::DisplayRole && index.column() == 4) {
                return QString("%1").arg(link_data.tags[i].displayStatus());
            }
        }
    }
    return QVariant();
}
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      statusLabel(new QLabel("Disconnected.")), downloadButton(new QPushButton),
      pollTimer(new QTimer(this)), centralWidget(new QWidget(this)),
      linkDetails(new QLineEdit("N/A")), linkStatus(new QLineEdit("N/A")),
      tableView(new QTableView) {

    ui->setupUi(this);

    linkStatus->setReadOnly(true);
    linkStatus->setFixedWidth(300);

    linkDetails->setReadOnly(true);
    linkDetails->setFixedWidth(300);
    url = QUrl::fromUserInput(
        QString("http://localhost:3000/api/get_links_config").trimmed());

    setWindowTitle("Sentinel Monitor V0.0.1");

    connect(pollTimer, &QTimer::timeout, this, &MainWindow::initRequest);

    pollTimer->start(1000);
    // Repeating timer
    pollTimer->setSingleShot(false);
    linksList = new QComboBox();

    linksList->setFixedWidth(300);
    linksList->setDisabled(true);

    connect(linksList, &QComboBox::currentIndexChanged, this,
            &MainWindow::selectedLinkChanged);
    QFormLayout *formLayout = new QFormLayout;
    formLayout->setFormAlignment(Qt::AlignLeft);
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->addWidget(linksList);

    formLayout->addRow(QString("Status:"), statusLabel);
    formLayout->addRow(QString("Link Details:"), linkDetails);
    formLayout->addRow(QString("Link Status:"), linkStatus);

    tableView->setModel(&model);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(tableView, 1);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
}

MainWindow::~MainWindow() { delete ui; }
bool    MainWindow::isError() { return this->error; }
QString MainWindow::errorString() { return this->serverError; }
void    MainWindow::selectedLinkChanged() {
    size_t arrayLen = linksBuffer.size();

    if (this->linksList->currentIndex() > arrayLen - 1) {
        this->selectedLinkIndex = 0;
        this->statusLabel->setText(
            QString("Selected link index exceeds links array bounds."));
    } else {
        this->selectedLinkIndex = this->linksList->currentIndex();
        this->updateView();
    }
}
void    MainWindow::initRequest() {
    reply.reset(qnam.get(QNetworkRequest(url)));
    connect(reply.get(), &QNetworkReply::finished, this,
               &MainWindow::parseServerData);
}

void MainWindow::parseServerData() {

    if (reply->error() != QNetworkReply::NoError) {
        this->error = true;
        this->serverError =
            QString("Error connecting to server. %1").arg(reply->errorString());
        this->statusLabel->setText(this->serverError);
        this->tableView->setDisabled(true);
        return;
    }

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

    this->linksBuffer.clear();

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

            SentinelLink deviceLink = SentinelLink(idValue, tkValue);

            if (!deviceLinkObject.value("name").isString()) {
                this->error       = true;
                this->serverError = QString("Could not parse link name value.");
                this->statusLabel->setText(this->serverError);
                return;
            }

            QString nameValue = deviceLinkObject.value("name").toString();
            deviceLink.name   = nameValue;

            linksList->setItemText(i,
                                   QString("%1  %2").arg(tkValue, nameValue));

            if (!deviceLinkObject.value("enabled").isBool()) {
                this->error = true;
                this->serverError =
                    QString("Could not parse link enabled value.");
                this->statusLabel->setText(this->serverError);
                return;
            }

            bool enabledValue = deviceLinkObject.value("enabled").toBool();
            deviceLink.enable = enabledValue;

            if (!deviceLinkObject.value("tags").isArray()) {
                this->error       = true;
                this->serverError = QString("Could not parse tags array.");
                this->statusLabel->setText(this->serverError);
                return;
            }

            QJsonArray tagsArray = deviceLinkObject.value("tags").toArray();

            if (tagsArray.count() != N_CHANNELS) {
                this->error       = true;
                this->serverError = QString(
                    "Server tags count doesn't equal monitor tags count.");
                this->statusLabel->setText(this->serverError);
                return;
            }

            for (size_t tag_index = 0; tag_index < N_CHANNELS; tag_index++) {
                // TODO
                // Parse the tags.
                QJsonObject tagObject = tagsArray[tag_index].toObject();

                if (!tagObject.value("tk").isString()) {
                    this->error = true;
                    this->serverError =
                        QString("tag %1 tk parse failed.").arg(tag_index);
                    this->statusLabel->setText(this->serverError);
                    return;
                }

                QString tagTk = tagObject.value("tk").toString();
                deviceLink.tags[tag_index].tk = tagTk;

                if (!tagObject.value("name").isString()) {
                    this->error = true;
                    this->serverError =
                        QString("tag %1 name parse failed.").arg(tag_index);
                    this->statusLabel->setText(this->serverError);
                    return;
                }

                QString tagName = tagObject.value("name").toString();
                deviceLink.tags[tag_index].name = tagName;

                if (!tagObject.value("enabled").isBool()) {
                    this->error = true;
                    this->serverError =
                        QString("tag %1 enabled parse failed.").arg(tag_index);
                    this->statusLabel->setText(this->serverError);
                    return;
                }

                bool tagEnabled = tagObject.value("enabled").toBool();
                deviceLink.tags[tag_index].enabled = tagEnabled;

                if (!tagObject.value("address").isObject()) {
                    this->error = true;
                    this->serverError =
                        QString("tag %1 address parse failed.").arg(tag_index);
                    this->statusLabel->setText(this->serverError);
                    return;
                }

                QJsonObject tagAddressObject =
                    tagObject.value("address").toObject();

                if (tagAddressObject.value("ModbusAddr").isObject()) {
                    QJsonObject modbusAddrObject =
                        tagAddressObject.value("ModbusAddr").toObject();
                    if (!modbusAddrObject.value("Holding").isDouble()) {
                        this->error = true;
                        this->serverError =
                            QString("tag %1 modbus address parse failed.")
                                .arg(tag_index);
                        this->statusLabel->setText(this->serverError);
                        return;
                    }
                    int holdingRegister =
                        modbusAddrObject.value("Holding").toInt();

                    deviceLink.tags[tag_index].address.type = ST_MODBUS_ADDRESS;
                    deviceLink.tags[tag_index].address.modbus_register =
                        holdingRegister;
                    deviceLink.tags[tag_index].tagDetails =
                        QString("%1,%2,Modbus,Holding=%3")
                            .arg(tagTk, tagName)
                            .arg(holdingRegister);

                } else {
                    this->error = true;
                    this->serverError =
                        QString("tag %1 address object is not implemented.")
                            .arg(tag_index);
                    this->statusLabel->setText(this->serverError);
                    return;
                }

                if (!tagObject.value("value").isObject()) {
                    this->error = true;
                    this->serverError =
                        QString("tag %1 value object parse failed.")
                            .arg(tag_index);
                    this->statusLabel->setText(this->serverError);
                    return;
                }

                QJsonObject tagValueObject =
                    tagObject.value("value").toObject();

                if (tagValueObject.value("Real").isDouble()) {

                    float tagRealValue =
                        tagValueObject.value("Real").toDouble();
                    deviceLink.tags[tag_index].value.type       = ST_REAL_VALUE;
                    deviceLink.tags[tag_index].value.real_value = tagRealValue;

                } else if (tagValueObject.value("Int").isDouble()) {

                    int tagIntValue = tagValueObject.value("Int").toInt();
                    deviceLink.tags[tag_index].value.type      = ST_INT_VALUE;
                    deviceLink.tags[tag_index].value.int_value = tagIntValue;

                } else if (tagValueObject.value("Bit").isDouble()) {

                    int tagBitValue = tagValueObject.value("Bit").toInt();
                    deviceLink.tags[tag_index].value.type      = ST_BIT_VALUE;
                    deviceLink.tags[tag_index].value.bit_value = tagBitValue;

                } else {
                    this->error = true;
                    this->serverError =
                        QString("tag %1 value type parse failed.")
                            .arg(tag_index);
                    this->statusLabel->setText(this->serverError);
                    return;
                }

                if (tagObject.value("status").isObject()) {
                    QJsonObject tagStatus =
                        tagObject.value("status").toObject();

                    if (!tagStatus.value("Error").isString()) {
                        this->error = true;
                        this->serverError =
                            QString("tag %1 status error value parse failed.")
                                .arg(tag_index);
                        this->statusLabel->setText(this->serverError);
                        return;
                    }

                    QString tagErrorString =
                        tagStatus.value("Error").toString();
                    deviceLink.tags[tag_index].status = tagErrorString;

                } else if (tagObject.value("status").isString()) {
                    QString tagStatus = tagObject.value("status").toString();
                    deviceLink.tags[tag_index].status = tagStatus;
                } else {
                    this->error = true;
                    this->serverError =
                        QString("tag %1 status parse failed.").arg(tag_index);
                    this->statusLabel->setText(this->serverError);
                    return;
                }
            }

            if (deviceLinkObject.value("status").isObject()) {
                QJsonObject statusObject =
                    deviceLinkObject.value("status").toObject();

                if (statusObject.value("Error").isString()) {
                    QString statusValue =
                        statusObject.value("Error").toString();
                    deviceLink.status = statusValue;
                } else {
                    this->error       = true;
                    this->serverError = QString("Could not parse error state.");
                    this->statusLabel->setText(this->serverError);
                    return;
                }
            } else if (deviceLinkObject.value("status").isString()) {
                QString statusString =
                    deviceLinkObject.value("status").toString();
                deviceLink.status = statusString;
            } else {
                this->error       = true;
                this->serverError = QString("Could not parse status value.");
                this->statusLabel->setText(this->serverError);
                return;
            }

            if (!deviceLinkObject.value("last_poll_time").isString()) {
                this->error = true;
                this->serverError =
                    QString("Could not parse last poll time value.");
                this->statusLabel->setText(this->serverError);
                return;
            }

            QString lastPollString =
                deviceLinkObject.value("last_poll_time").toString();
            deviceLink.last_poll_time = lastPollString;

            if (!deviceLinkObject.value("protocol").isObject()) {
                this->error       = true;
                this->serverError = QString("Could not parse protocol value.");
                this->statusLabel->setText(this->serverError);
                return;
            }

            QJsonObject protocolObject =
                deviceLinkObject.value("protocol").toObject();

            if (protocolObject.value("ModbusTcp").isObject()) {
                QJsonObject modbusTcpObject =
                    protocolObject.value("ModbusTcp").toObject();

                deviceLink.protocol = ST_MODBUS_TCP;

                if (!modbusTcpObject.value("ip").isString()) {
                    this->error = true;
                    this->serverError =
                        QString("Could not parse ModbusTcp ip value.");
                    this->statusLabel->setText(this->serverError);
                    return;
                }

                QString ipValue = modbusTcpObject.value("ip").toString();

                if (!modbusTcpObject.value("port").isDouble()) {
                    this->error = true;
                    this->serverError =
                        QString("Could not parse ModbusTcp port value.");
                    this->statusLabel->setText(this->serverError);
                    return;
                }

                int portValue = modbusTcpObject.value("port").toInt();
                deviceLink.protocolDetails =
                    QString("ModbusTcp:%1:%2").arg(ipValue).arg(portValue);
            } else {
                this->error       = true;
                this->serverError = QString("Could not parse ModbusTcp value.");
                this->statusLabel->setText(this->serverError);
                return;
            }

            this->linksBuffer.push_back(deviceLink);
            this->error       = false;
            this->serverError = QString("Parse successful.");
            this->statusLabel->setText(this->serverError);
        } else {
            this->error = true;
            this->serverError =
                QString("Other links parsing is not implementd.");
            this->statusLabel->setText(this->serverError);
            return;
        }

        // Parse successfull. We update the GUI.
        this->updateView();
    }
}

void MainWindow::updateView() {
    size_t arrayLen = this->numLinks;

    if (this->selectedLinkIndex > arrayLen - 1) {
        this->error       = true;
        this->serverError = QString(
            "Selected link index is outside the bounds of the links list.");
        this->statusLabel->setText(this->serverError);
        return;
    }
    // Edge case where vector is empty (size 0) and index is 0
    if (this->linksBuffer.empty()) {
        this->error       = true;
        this->serverError = QString("Links list is empty.");
        this->statusLabel->setText(this->serverError);
        return;
    }

    SentinelLink selectedLink = this->linksBuffer[this->selectedLinkIndex];
    this->linkDetails->setText(QString("%1:%2:%3")
                                   .arg(selectedLink.tk, selectedLink.name,
                                        selectedLink.protocolDetails));
    this->linkStatus->setText(selectedLink.status);
    this->model.setTableData(&selectedLink);
    this->tableView->setDisabled(false);
    for (size_t i = 0; i < N_CHANNELS; i++) {
        this->tableView->setRowHeight(i, 7);
    }

    if (this->linksList->count() == 0) {
        for (size_t i = 0; i < arrayLen; i++) {
            this->linksList->addItem(QString("%1    %2")
                                         .arg(this->linksBuffer[i].tk)
                                         .arg(this->linksBuffer[i].name));
        }
    }
    for (size_t i = 0; i < arrayLen; i++) {
        this->linksList->setItemText(i, QString("%1    %2")
                                            .arg(this->linksBuffer[i].tk)
                                            .arg(this->linksBuffer[i].name));
    }

    this->linksList->setDisabled(false);
}
