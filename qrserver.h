#ifndef QRSERVER_H
#define QRSERVER_H

#include <QObject>
#include <QBluetoothLocalDevice>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothServer>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QtSerialPort/QtSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QCryptographicHash>
#include <QtNetwork/qtcpserver.h>
#include <QtNetwork/qtcpsocket.h>
#include <QTimer>
#include <QTime>
#include <wiringPi.h>
#include <signal.h>
#include "download.h"
#include "checkversion.h"
#include "globalval.h"
#include "handleziptype.h"

static const QLatin1String serviceUuid("e8e10f95-1a70-4b27-9ccf-02010264e9c8");
extern "C" {
int nist_randomness_evaluate(unsigned char* rnd);
}
class GlobalVal;
class CheckVersion;
class Download;
class QRServer : public QObject
{
    Q_OBJECT

public:
    explicit QRServer(QObject *parent = nullptr);

    ~QRServer();
    void startServer(const QBluetoothAddress &localAdapter = QBluetoothAddress());
    void onUpgrade();
    void stopServer();

signals:
    void messageReceived(const QString &sender, const QString &message);
    void clientConnected(const QString &name);
    void clientDisconnected(const QString &name);

private slots:
    void clientConnected();
    void clientDisconnected();
    void readBTSocket();

    void handleTcpSocketError(QAbstractSocket::SocketError);
    void handleTcpSocketReadyRead();
    void handleTcpSocketDisconnect();
    void vqrserverTimeout();
    void tcpConnected();

    void onEndTimeReached();

private:
    void openWifi();
    void getWalletAddr();
    void walletAddrSig();
    void getWalletAddrSig();
    void addKey(QString strCount);
    void decompressKeytowalletAddr(QString strCount);
    void getRandom();
    void getDrbgRandom();
    void testRandomFile();
    void hashSig();
    void saveHashToFile(const QString &hashvalue, const QString &hashfilepath);
    void startTcp();
    void ipfileExists();
    void processReceivedData(const QByteArray &data);
    void loginVqr();
    void registerWallet();
    void getLotteryTime();
    void lotteryStart();
    void lotteryResult();
    void initializeFileStatus(const QString &filePath);
    void updateFileStatus(const QString &filePath, int fileNumber, int status);
    QVector<int> readProcessedFileNumbers(const QString &filePath);
    void writeSysVersion();
    void syncVersion();
    void startMainApp();
    void Delay(unsigned int msec);
    void openLed(int pin1,int value1,int pin2,int value2,int pin3,int value3);
    void blinkLed(int pin1,int delayTime,int pin2,int pin3);
    void setupLogDeletion(int intervalDays, int daysToKeep);

    QRServer *server;
    Download *download;
    HandleZipType *handleZip;
    CheckVersion *cv;

    QBluetoothServer *m_rfcommServer;
    QBluetoothServiceInfo m_serviceInfo;
    QList<QBluetoothSocket *> m_clientSockets;

    QTcpSocket *m_TcpSocket;
    QString macAddress;
    QString vqrServerIP;
    QString vqrServerIPfile;
    quint16 vqrServerPort;

    QSerialPort global_port;

    QString currentPath = QDir::currentPath();
    QString walletAddrPath;
    QString walletAddrsigPath;
    QString n_drbgrandomPath;
    QString n_drbgrandomhashPath;
    QString StatusPath;

    QString strlotteryTime;
    QString winnerWallet;
    QString keccak_256(const QString &input);
    QString Erc55checksum(const QString &address);

    QString SysVersion = "1.0.0";

    QJsonDocument jsonDocreceiveBTData;//收到的json格式文档
    QJsonObject jsonObjreceiveBTData;//收到的json格式对象
    QJsonObject jsonObjreceiveBTDatabody;//收到的body对象
    QJsonObject jsonObjreceiveBTDataheader;//收到的header对象
    QJsonObject jsonObjreceiveBTDatabodymessagedata;//收到的body里messagedata对象
    QJsonDocument jsonDocsendBTData;//发送的json格式文档
    QJsonObject jsonObjsendBTData;//发送的json格式对象
    QJsonObject jsonObjsendBTDatabody;//发送的body对象
    QJsonObject jsonObjsendBTDataheader;//发送的header对象
    QJsonObject jsonObjsendBTDatabodymessagedata;//发送的messagedata对象

    QByteArray m_bufferSend;
    QByteArray m_bufferReceive;

    bool isConnect;
    bool hashfileExists;
    bool getallrandom;

    QJsonDocument jsonDocreceiveTCPData;//收到的json格式文档
    QJsonObject jsonObjreceiveTCPData;//收到的json格式对象
    QJsonObject jsonObjreceiveTCPDatabody;//收到的body对象
    QJsonObject jsonObjreceiveTCPDataheader;//收到的header对象
    QJsonDocument jsonDocsendTCPData;//发送的json格式文档
    QJsonObject jsonObjsendTCPData;//发送的json格式对象
    QJsonObject jsonObjsendTCPDatabody;//发送的body对象
    QJsonObject jsonObjsendTCPDataheader;//发送的header对象
    QJsonArray jsonArrsendTCPDatabodylist;//发送的list数组
    QJsonObject lotteryItem;

    QTimer *drbgTimer;
    QTimer *endTimer;
    QTimer *connectTimer;
    QTimer *randomTimer;
    QTimer *packetTimer;
    QTimer *waitForNextPacketTimer;
    QTimer *ledTimer;

    int randomcount = 1;
    int packetCount;
    int packetNumber = 0;
    int calculateBodySize(const QJsonObject& bodyObject);//计算body字节
    const static int walletAddrCount = 10;//钱包地址数量
    const int packetSize = 8192 * 2; // 假设每个数据包随机数大小为8k字节
};
void outputLog(QtMsgType type, const QMessageLogContext &context, const QString &msg);//输出日志
void signalHandler(int signal);
void deleteOldLogFiles(int daysToKeep);
#endif // QRSERVER_H
