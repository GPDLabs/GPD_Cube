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

static const QLatin1String serviceUuid("e8e10f95-1a70-4b27-9ccf-02010264e9c8");
extern "C" {
    int nist_randomness_evaluate(unsigned char* rnd);
}
class QRServer : public QObject
{
    Q_OBJECT

public:
    explicit QRServer(QObject *parent = nullptr);

    ~QRServer();
//        QRServer(): ETHaddr(""),ETHaddrchecksum(""){}
    void startServer(const QBluetoothAddress &localAdapter = QBluetoothAddress());
    void stopServer();
    void Openwifi();//打开wifi
    void OpenPort();//打开串口
    void AddKey();//生成密钥
    void DecompressKey();//解压密钥
    void OutputRandom();//输出随机数
    void ReadRandomFile();//读取随机数文件
//    QString ETHaddr;
//    QString ETHaddrchecksum;
//    QString keccak_256(const QString &input);
//    QString Erc55checksum(const QString &address);
//    QByteArray DPKey;
//    QString strDPKey;
//    QString pubkey;
//    QByteArray pubKey;
//    QString strKey;

public slots:
    void sendMessage(const QString &message);


signals:
    void messageReceived(const QString &sender, const QString &message);
    void clientConnected(const QString &name);
    void clientDisconnected(const QString &name);

private slots:
    void clientConnected();
    void clientDisconnected();
    void readSocket();

private:
    QBluetoothServer *m_rfcommServer;
    QBluetoothServiceInfo m_serviceInfo;
    QList<QBluetoothSocket *> m_clientSockets;
    QString currentPath = QDir::currentPath();//当前文件夹路径

    QSerialPort global_port;//端口函数

    QString pubkeyfile; //未解压公钥文件保存路径
    QByteArray arrKey;
    QString strKey;

    QString DPpubkeyfile; //解压公钥文件保存路径
    QByteArray arrDPKey;
    QString strDPKey;

    QString randomfile;//随机数文件保存路径
    QByteArray Random;
    QByteArray arrRandom;//读取的随机数
    qintptr RandomSize;//读取的随机数字节数
    QString strRandom;

    QJsonDocument jsonDocreceive;//收到的json格式文档
    QJsonObject jsonObjreceive;//收到的json格式对象
    QJsonObject jsonObjreceivebody;//收到的body对象
    QJsonObject jsonObjreceiveheader;//收到的header对象
    QJsonObject jsonObjreceivemessagedata;//收到的body里messagedata对象

    QJsonDocument jsonDocsend;//发送的json格式文档
    QJsonObject jsonObjsend;//发送的json格式对象
    QJsonObject jsonObjsendbody;//发送的body对象
    QJsonObject jsonObjsendheader;//发送的header对象
    QJsonObject jsonObjsendmessagedata;//发送的messagedata对象
    QString jsonObjsendchecksum;//发送的checksum字符串
    QTimer *yanshiTimer;
    QString keccak_256(const QString &input);
    QString Erc55checksum(const QString &address);
    QString ETHaddr;
    QString ETHaddrchecksum;
    int i;
};

#endif // QRSERVER_H
