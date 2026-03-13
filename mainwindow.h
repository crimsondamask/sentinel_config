#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#define N_CHANNELS 1000

#include <QAbstractTableModel>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

QT_BEGIN_NAMESPACE

namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

enum SentinelProtocol {
    ST_MODBUS_TCP,
    ST_MODBUS_SERIAL,
};

enum TagAddressType {
    ST_MODBUS_ADDRESS,
};

enum TagValueType {
    ST_INT_VALUE,
    ST_REAL_VALUE,
    ST_BIT_VALUE,
};
struct SentinelTagAddress {
    int type;
    int modbus_register;
};

struct SentinelTagValue {
    int   type;
    float real_value;
    int   int_value;
    int   bit_value;
};

class SentinelTag {
  public:
    // TODO---
    SentinelTag(qint16 id, QString tk);
    //~SentinelTag();
    QString displayValue() const;
    QString displayName() const;
    QString displayTk() const;
    QString displayStatus() const;
    QString displayDetails() const;
    bool    isEnabled() const;
    // -------

    qint16             id;
    QString            tk;
    QString            name;
    QString            tagDetails;
    bool               enabled;
    SentinelTagAddress address;
    SentinelTagValue   value;
    QString            status;
};

class SentinelLink {
  public:
    SentinelLink(qint16 id, QString tk);
    //~SentinelLink();

    qint16                   id;
    QString                  tk;
    QString                  name;
    bool                     enable;
    int                      protocol;
    QString                  protocolDetails;
    std::vector<SentinelTag> tags;
    size_t                   tag_count;
    QString                  last_poll_time;
    QString                  status;

  private:
};

class SentinelTableModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    explicit SentinelTableModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int                role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void setTableData(SentinelLink *link);

  private:
    SentinelLink link_data;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void    initRequest();
    void    parseServerData();
    void    selectedLinkChanged();
    void    updateView();
    void    httpErrorOccurred();
    bool    isError();
    QString errorString();

  private:
    Ui::MainWindow *ui;
    QLabel         *statusLabel;
    QLineEdit      *linkDetails;
    QLineEdit      *linkStatus;
    QPushButton    *downloadButton;
    QComboBox      *linksList;
    QTimer         *pollTimer;
    QTableView     *tableView;
    QWidget        *centralWidget;

    int                                                      selectedLinkIndex;
    bool                                                     error;
    QString                                                  serverError;
    std::vector<SentinelLink>                                linksBuffer;
    qsizetype                                                numLinks;
    SentinelTableModel                                       model;
    QUrl                                                     url;
    QNetworkAccessManager                                    qnam;
    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply;
};
#endif // MAINWINDOW_H
