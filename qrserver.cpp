#include "qrserver.h"
#include <unistd.h>
#include "sts.h"
#include <QNetworkInterface>
#include <iostream>
#include <string>

QRServer::QRServer(QObject *parent) : QObject(parent)
{
    qDebug()<<"QR版本:"<<SysVersion;
    writeSysVersion();

    QSettings *settings;
    settings = new QSettings("setting.ini",QSettings::IniFormat);
    QString upgradeRemoteUrl = settings->value("upgrade/url").toString();
    download = new Download(this->parent());
    handleZip = new HandleZipType(this->parent());
    cv = new CheckVersion(this->parent(),upgradeRemoteUrl);
    GlobalVal::programRootDir = QDir::currentPath();

    // 检查更新
    cv->requestRemoteVersion();
    // 每隔24小时检查一次
    QTimer *versionTimer = new QTimer();
    QObject::connect(versionTimer, &QTimer::timeout, [=](){
        cv->requestRemoteVersion();
    });
    versionTimer->start(1 * 24 * 60 * 60 * 1000);

    global_port.setDataBits(QSerialPort::Data8);
    global_port.setParity(QSerialPort::NoParity);
    global_port.setStopBits(QSerialPort::OneStop);
    global_port.setBaudRate(460800);//设置波特率460800
    global_port.setPortName("ttyACM0");//LINUX设置端口号

    StatusPath = currentPath+"/QR-randomStatus.csv";
    initializeFileStatus(StatusPath);

    setupLogDeletion(1, 7);
}

void QRServer::startServer(const QBluetoothAddress &localAdapter) {
    QBluetoothLocalDevice localDevice;
    QString localDeviceName;
    if (localDevice.isValid()) {
        localDevice.powerOn();
        localDeviceName = localDevice.name();
        qDebug() << "蓝牙名字:" << localDeviceName << "蓝牙MAC地址: " << localDevice.address().toString();
        localDevice.setHostMode(QBluetoothLocalDevice::HostDiscoverable);
        QList<QBluetoothAddress> remotes;
        remotes = localDevice.connectedDevices();
    }

    qDebug() << "启动蓝牙服务器...";
    m_rfcommServer = new QBluetoothServer(QBluetoothServiceInfo::RfcommProtocol, this);
    connect(m_rfcommServer, SIGNAL(newConnection()), this, SLOT(clientConnected()));
    bool result = m_rfcommServer->listen(localAdapter);
    if (!result) {
        qWarning() << "无法将蓝牙服务器绑定到" << localAdapter.toString();
        return;
    }

    //服务名称、描述和提供者
    m_serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceName, tr("QR Server"));
    m_serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceDescription,tr("bluetooth server"));
    m_serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceProvider, tr("Quakey"));

    //创建uuid
    m_serviceInfo.setServiceUuid(QBluetoothUuid(serviceUuid));

    //服务可发现
    qDebug() << "让蓝牙服务器可被发现...";
    QBluetoothServiceInfo::Sequence publicBrowse;
    publicBrowse << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));
    m_serviceInfo.setAttribute(QBluetoothServiceInfo::BrowseGroupList,publicBrowse);

    //协议描述符列表
    QBluetoothServiceInfo::Sequence protocolDescriptorList;
    QBluetoothServiceInfo::Sequence protocol;
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    protocol.clear();
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
             << QVariant::fromValue(quint8(m_rfcommServer->serverPort()));
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    m_serviceInfo.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList,protocolDescriptorList);

    //注册服务
    m_serviceInfo.registerService(localAdapter);

    QString wirelessInterfaceName = "wlan0";// 替换为你的无线接口名称
    QNetworkInterface interface = QNetworkInterface::interfaceFromName(wirelessInterfaceName);// 获取特定的网络接口
    if (!interface.isValid()) {
        qDebug() << "接口:" << wirelessInterfaceName << "没有找到";
    }// 检查接口是否存在
    macAddress = interface.hardwareAddress();// 获取接口的硬件地址（MAC地址）
    if (!macAddress.isEmpty()) {
        qDebug() << "无线MAC地址:" << macAddress;
    } else {
        qDebug() << "未找到MAC地址:" << interface.name();
    }// 检查MAC地址是否有效

    getWalletAddr();
}

QRServer::~QRServer() {
    stopServer();
}

void QRServer::stopServer() {
    // 注销服务
    m_serviceInfo.unregisterService();

    // 关闭套接字
    qDeleteAll(m_clientSockets);

    // 关闭服务器
    delete m_rfcommServer;
    m_rfcommServer = nullptr;

    m_TcpSocket->disconnectFromHost();

    if(randomTimer){
        randomTimer->stop();
        delete randomTimer;
        randomTimer = nullptr;
    }
    if(drbgTimer){
        drbgTimer->stop();
        delete drbgTimer;
        drbgTimer = nullptr;
    }
    if(connectTimer){
        connectTimer->stop();
        delete connectTimer;
        connectTimer = nullptr;
    }
    if(endTimer){
        endTimer->stop();
        delete endTimer;
        endTimer = nullptr;
    }
}

void QRServer::clientConnected() {
    qDebug() << "检测到新连接!";
    QBluetoothSocket *socket = m_rfcommServer->nextPendingConnection();
    if (!socket)
        return;

    connect(socket, &QBluetoothSocket::readyRead, this, &QRServer::readBTSocket);
    connect(socket, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
    m_clientSockets.append(socket);
    qDebug() << "客户端[" << socket->peerName() << "]已连接!";
    emit clientConnected(socket->peerName());
}

void QRServer::clientDisconnected() {
    QBluetoothSocket *socket = qobject_cast<QBluetoothSocket *>(sender());
    if (!socket)
        return;

    emit clientDisconnected(socket->peerName());

    m_clientSockets.removeOne(socket);

    socket->deleteLater();
}

void QRServer::readBTSocket() {
    QBluetoothSocket *socket = qobject_cast<QBluetoothSocket *>(sender());
    if (!socket)
        return;

    //都可以读取
    QByteArray receiveBTData ;
    receiveBTData.append(socket->readAll());
    qDebug() <<  "Bluetooth receving: " <<receiveBTData;

    jsonDocreceiveBTData = QJsonDocument::fromJson(receiveBTData);//收到的字节转为json文档
    jsonObjreceiveBTData = jsonDocreceiveBTData.object();//json文档转为json对象

    if(jsonObjreceiveBTData["header"].isObject() && jsonObjreceiveBTData["body"].isObject()){
        jsonObjreceiveBTDataheader = jsonObjreceiveBTData["header"].toObject();//收到的header对象
        jsonObjreceiveBTDatabody = jsonObjreceiveBTData["body"].toObject();//收到的body对象

        if(jsonObjreceiveBTDataheader["messageName"].toString() == "wirelessConf")
        {
            openWifi();
            Delay(500);

            jsonObjsendBTDatabody["status"] = "success";

            //计算body对象下的数据长度
            int bodylength = calculateBodySize(jsonObjsendBTDatabody);

            jsonObjsendBTDataheader["checksum"] = 11001;
            jsonObjsendBTDataheader["messageLength"] = bodylength;//消息长度
            jsonObjsendBTDataheader["messageName"] = "wirelessConf";
            jsonObjsendBTDataheader["messageType"] = "response";
            jsonObjsendBTDataheader["version"] = "1.0";

            jsonObjsendBTData["header"] = jsonObjsendBTDataheader;
            jsonObjsendBTData["body"] = jsonObjsendBTDatabody;

            QJsonDocument jsonDocsend(jsonObjsendBTData);
            QByteArray sendBTData  = jsonDocsend.toJson();
            foreach (QBluetoothSocket *socket, m_clientSockets)
                socket->write(sendBTData);

            qDebug() << "Bluetooth sending: " << sendBTData;
            sendBTData.clear();

            jsonObjsendBTDataheader = QJsonObject();
            jsonObjsendBTDatabody = QJsonObject();
        }
        else if(jsonObjreceiveBTDataheader["messageName"].toString() == "walletAddr")
        {
            int intkeyNo = jsonObjreceiveBTDatabody["keyNo"].toInt();
            QString strkeyNo = QString::number(intkeyNo);

            addKey(strkeyNo);
            Delay(500);

            jsonObjsendBTDatabody["status"] = "success";
            QString walletAddrPath = currentPath+"/QR-walletAddr" + strkeyNo + ".txt";
            QFile walletAddrfile(walletAddrPath);
            if (walletAddrfile.open(QFile::ReadOnly)){
                QString strwalletAddr = walletAddrfile.readAll();
                walletAddrfile.close();
                jsonObjsendBTDatabody["walletAddr"] = strwalletAddr;
            } else {
                qDebug() << "打开文件失败:" << walletAddrPath;
            }

            QString pubkeypath = currentPath+"/QR-pubKey" + strkeyNo + ".txt";
            QFile pubkeyfile(pubkeypath);
            if (pubkeyfile.open(QFile::ReadOnly)) {
                QString strpubKey = pubkeyfile.readAll();
                pubkeyfile.close();
                jsonObjsendBTDatabody["pubKey"] = strpubKey;
            } else {
                qDebug() << "打开文件失败:" << pubkeypath;
            }

            //计算body对象下的数据长度
            int bodylength = calculateBodySize(jsonObjsendBTDatabody);

            jsonObjsendBTDataheader["checksum"] = 11002;
            jsonObjsendBTDataheader["messageLength"] = bodylength;//消息长度
            jsonObjsendBTDataheader["messageName"] = "walletAddr";
            jsonObjsendBTDataheader["messageType"] = "response";
            jsonObjsendBTDataheader["version"] = "1.0";

            jsonObjsendBTData["header"] = jsonObjsendBTDataheader;
            jsonObjsendBTData["body"] = jsonObjsendBTDatabody;

            QJsonDocument jsonDocsend(jsonObjsendBTData);
            QByteArray sendBTData  = jsonDocsend.toJson();

            foreach (QBluetoothSocket *socket, m_clientSockets)
                socket->write(sendBTData);

            qDebug() << "Bluetooth sending: " << sendBTData;
            sendBTData.clear();

            jsonObjsendBTDataheader = QJsonObject();
            jsonObjsendBTDatabody = QJsonObject();
        }
        else if(jsonObjreceiveBTDataheader["messageName"].toString() == "vqrIPConf")
        {
            int intkeyNo = jsonObjreceiveBTDatabody["keyNo"].toInt();
            QString strkeyNo = QString::number(intkeyNo);

            vqrServerIP = jsonObjreceiveBTDatabody["ipAddr"].toString();
            vqrServerIPfile = currentPath+"/QR-vqrServerIP.txt" ;
            QFile ipfile(vqrServerIPfile);
            ipfile.open(QFile::WriteOnly);
            ipfile.resize(0);
            ipfile.write(vqrServerIP.toLocal8Bit().data());
            ipfile.close();

            startTcp();
            Delay(1000);

            //计算body对象下的数据长度
            int bodylength = calculateBodySize(jsonObjsendBTDatabody);

            jsonObjsendBTDataheader["checksum"] = 11003;
            jsonObjsendBTDataheader["messageLength"] = bodylength;//消息长度
            jsonObjsendBTDataheader["messageName"] = "vqrIPConf";
            jsonObjsendBTDataheader["messageType"] = "response";
            jsonObjsendBTDataheader["version"] = "1.0";

            jsonObjsendBTData["header"] = jsonObjsendBTDataheader;
            jsonObjsendBTData["body"] = jsonObjsendBTDatabody;

            QJsonDocument jsonDocsend(jsonObjsendBTData);
            QByteArray sendBTData  = jsonDocsend.toJson();
            foreach (QBluetoothSocket *socket, m_clientSockets)
                socket->write(sendBTData);

            qDebug() << "Bluetooth sending: " << sendBTData;
            sendBTData.clear();

            jsonObjsendBTDataheader = QJsonObject();
            jsonObjsendBTDatabody = QJsonObject();
        }
    }
}

void QRServer::openWifi() {
    QString jsonwifiName = jsonObjreceiveBTDatabodymessagedata["wifiName"].toString();
    QString jsonwifiPwd = jsonObjreceiveBTDatabodymessagedata["wifiPwd"].toString();

    QFile wififile("/etc/wpa_supplicant/wpa_supplicant.conf");
    if(wififile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream stream(&wififile);

        stream<<"ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\n";
        stream<<"update_config=1\n";
        stream<<"country=IN\n";
        stream<<"\n";
        stream<<"network={\n";
        stream<<"\tssid=\"";
        stream<<jsonwifiName+"\"\n";
        stream<<"\tpsk=\"";
        stream<<jsonwifiPwd+"\"\n";
        stream<<"\tkey_mgmt=WPA-PSK\n";
        stream<<"}";

        wififile.close();
        QProcess process1;
        process1.start("sh",QStringList()<<"-c"<<"sudo wpa_cli -i wlan0 reconfigure");
        process1.waitForFinished();
    }
}

void QRServer::getWalletAddr()
{
    int walletcount = 1;
    QTimer *walletAddrTimer = new QTimer;
    walletAddrTimer->setSingleShot(false);//设置为非单次触发
    walletAddrTimer->setInterval(100);//触发时间，单位：毫秒
    walletAddrTimer->start();//触发时间，单位：毫秒

    connect(walletAddrTimer,&QTimer::timeout,[=]()mutable{
        // 如果已经执行了足够次数的操作，停止定时器
        if (walletcount > walletAddrCount) {
            walletAddrTimer->stop();
            qDebug() << "钱包地址已生成完毕";

            getWalletAddrSig();
        }else{
            QString strCount = QString::number(walletcount);

            addKey(strCount);
            //            qDebug() << "钱包地址生成"<<walletcount;
            walletcount++;
        }
    });
}

void QRServer::addKey(QString strCount)
{
    if (!global_port.open(QIODevice::ReadWrite)){
        qDebug() << "无法打开串口，错误：" << global_port.errorString();
        blinkLed(0,1000,2,3);
        return;
    }

    //发送生成公钥指令
    QByteArray send_NK;
    send_NK.resize(3);
    send_NK[0] = 0x4E;//N
    send_NK[1] = 0x4B;//K
    send_NK[2] = strCount.toInt();
    global_port.write(send_NK);

    //接收公钥写入文件
    connect(&global_port,&QSerialPort::readyRead,this,[=](){
        QByteArray arrKey = global_port.readAll();
        QString strKey=arrKey.toHex();

        QString pubkeypath = currentPath+"/QR-pubKey" + strCount + ".txt" ;
        QFile pubkeyfile(pubkeypath);
        if (pubkeyfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
            QTextStream pubKey(&pubkeyfile);
            pubKey << strKey;
            pubkeyfile.close();
        }else {
            qDebug() << "打开文件失败:" << walletAddrPath;
        }
        disconnect(&global_port,&QSerialPort::readyRead,this,nullptr);

        decompressKeytowalletAddr(strCount);
    });
}

void QRServer::decompressKeytowalletAddr(QString strCount)
{
    QString pubkeypath = currentPath + "/QR-pubKey" + strCount + ".txt";
    QFile pubkeyfile(pubkeypath);
    pubkeyfile.open(QFile::ReadOnly);
    QString strpubKey = pubkeyfile.readAll();
    pubkeyfile.close();

    //发送生成公钥指令
    QByteArray send_DP;
    send_DP.resize(2);
    send_DP[0] = 0x44;//D
    send_DP[1] = 0x50;//P
    global_port.write(send_DP + QByteArray::fromHex(strpubKey.toUtf8()));

    //接收公钥写入文件
    connect(&global_port,&QSerialPort::readyRead,this,[=](){
        QByteArray arrDPpubKey = global_port.readAll();
        QString strDPpubKey = arrDPpubKey.toHex();

        QString dppubkeypath = currentPath + "/QR-dppubKey" + strCount + ".txt";
        QFile dppubkeyfile(dppubkeypath);
        if (dppubkeyfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
            QTextStream DPpubKey(&dppubkeyfile);
            DPpubKey << strDPpubKey;
            dppubkeyfile.close();
        }else {
            qDebug() << "打开文件失败:" << walletAddrPath;
        }
        
        QString ETHaddr = keccak_256(strDPpubKey).right(40);//以太坊哈希值的后20字节
        QString strwalletAddr = Erc55checksum(ETHaddr);

        walletAddrPath = currentPath+"/QR-walletAddr" + strCount + ".txt";
        QFile walletAddrfile(walletAddrPath);
        if (walletAddrfile.open(QFile::WriteOnly | QIODevice::Truncate)){
            QTextStream walletAddr(&walletAddrfile);
            walletAddr << strwalletAddr; //写入钱包地址
            walletAddr.flush(); // 确保钱包地址被写入文件
            walletAddrfile.close();
        }else {
            qDebug() << "打开文件失败:" << walletAddrPath;
        }

        disconnect(&global_port,&QSerialPort::readyRead,this,nullptr);
        global_port.close();
    });
}

void QRServer::startTcp()
{
    m_TcpSocket = new QTcpSocket(this);
    connect(m_TcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(handleTcpSocketError(QAbstractSocket::SocketError)));
    connect(m_TcpSocket, SIGNAL(readyRead()), this, SLOT(handleTcpSocketReadyRead()));
    connect(m_TcpSocket,SIGNAL(disconnected()),this,SLOT(handleTcpSocketDisconnect()));

    vqrServerPort = 8080;
    isConnect = false;

    connectTimer=new QTimer(this);
    connectTimer->setInterval(5000);
    connect(connectTimer,SIGNAL(timeout()),this,SLOT(vqrserverTimeout()));

    tcpConnected();
}

void QRServer::tcpConnected()
{
    if(isConnect){
        m_TcpSocket->close();
        isConnect = false;
        qDebug()<<"断开连接";
        return;
    }

    if (randomTimer) {
        randomTimer->stop();
    }

    if (drbgTimer) {
        drbgTimer->stop();
    }

    m_TcpSocket->connectToHost(QHostAddress(vqrServerIP),vqrServerPort);

    if (m_TcpSocket->waitForConnected(1000))
    {
        qDebug()<<QString("连接成功 %1 %2").arg(m_TcpSocket->peerAddress().toString()).arg(m_TcpSocket->peerPort());
        isConnect = true;
        connectTimer->stop();
        jsonObjsendBTDatabody["status"] = "success";

        openLed(0,0,2,0,3,1);

        Delay(500);

        loginVqr();

        if (randomTimer) {
            randomTimer->start();
        }
        else{
            getRandom();
        }
    }
    else {
        qDebug()<<QString("连接失败 %1").arg(m_TcpSocket->errorString());
        jsonObjsendBTDatabody["status"] = "fail";
        connectTimer->start();

        blinkLed(2,1000,0,3);
    }
}

void QRServer::handleTcpSocketError(QAbstractSocket::SocketError)
{
    qDebug()<<QString("Socket错误 %1").arg(m_TcpSocket->errorString());
}

void QRServer::handleTcpSocketDisconnect()
{
    qDebug()<<QString("Socket断开 %1").arg(m_TcpSocket->state());
    if(isConnect){
        isConnect = false;
        if (connectTimer) {
        connectTimer->start();
        }
        if (randomTimer) {
            randomTimer->stop();
        }
        if (drbgTimer) {
            drbgTimer->stop();
        }
    }
}

void QRServer::vqrserverTimeout()
{
    qDebug()<<"服务器超时,将重新尝试连接...";
    tcpConnected();
}

void QRServer::ipfileExists()
{
    vqrServerIPfile = currentPath + "/QR-vqrServerIP.txt";
    QFile vqripfile(vqrServerIPfile);
    if(vqripfile.exists()){
        if(!vqripfile.open(QIODevice::ReadOnly)){
            return;
        }
        QTextStream vqrip(&vqripfile);
        vqrServerIP = vqrip.readAll();
        vqripfile.close();

        startTcp();
    }
}

void QRServer::handleTcpSocketReadyRead()
{
    QByteArray receiveTCPData = m_TcpSocket->readAll();
    qDebug()<<  "TCP接收: " <<receiveTCPData;

    processReceivedData(receiveTCPData);
}

void QRServer::processReceivedData(const QByteArray &data) {
    jsonDocreceiveTCPData = QJsonDocument::fromJson(data);//收到的字节转为json文档
    jsonObjreceiveTCPData = jsonDocreceiveTCPData.object();//json文档转为json对象

    if(jsonObjreceiveTCPData["header"].isObject()&&jsonObjreceiveTCPData["body"].isObject())
    {
        jsonObjreceiveTCPDataheader = jsonObjreceiveTCPData["header"].toObject();
        jsonObjreceiveTCPDatabody = jsonObjreceiveTCPData["body"].toObject();

        if(jsonObjreceiveTCPDataheader["messageName"].toString()=="lotteryResult")
        {
            winnerWallet = jsonObjreceiveTCPDatabody["winnerWallet"].toString();
            int totalPackets = jsonObjreceiveTCPDatabody["totalPackets"].toInt();
            int currentPacket = jsonObjreceiveTCPDatabody["currentPacket"].toInt();

            // 如果已经有定时器在运行，先停止它
            if(waitForNextPacketTimer && waitForNextPacketTimer->isActive())
            {
                waitForNextPacketTimer->stop();
            }
            // 如果定时器还没有创建，创建一个新的定时器
            if(!waitForNextPacketTimer)
            {
                waitForNextPacketTimer = new QTimer;
                connect(waitForNextPacketTimer, &QTimer::timeout, this, [=]()mutable{
                    qDebug()<<"超过2秒没有请求下一个包，判断超时，取消随机数发送";
                    packetNumber = 0;
                    delete packetTimer;
                    packetTimer = nullptr;
                });
            }
            // 设置定时器2秒后触发
            waitForNextPacketTimer->setSingleShot(true);
            waitForNextPacketTimer->start(2000);

            if(packetTimer)
            {
                if(currentPacket < packetCount)
                {
                    if (currentPacket == packetNumber)
                    {
                        packetTimer->start();
                    }else
                    {
                        packetNumber = currentPacket;
                        packetTimer->start();
                    }
                }else
                {
                    qDebug()<<"所有随机数包发送完成，准备删除文件";
                    packetNumber = 0;
                    delete packetTimer;
                    packetTimer = nullptr;
                    // 如果是最后一个包，不需要等待，直接取消定时器
                    if(waitForNextPacketTimer)
                    {
                        waitForNextPacketTimer->stop();
                    }
                }
            }else
            {
                lotteryResult();
            }
            return;
        }else if(jsonObjreceiveTCPDataheader["messageName"].toString()=="luckyWallet")
        {
            if (randomTimer) {
                randomTimer->stop();
            }
            if (drbgTimer) {
                drbgTimer->stop();
            }
            if (endTimer) {
                endTimer->stop();
            }
            qDebug()<<"停止生成随机数和摇号定时器";
            //删除随机数和签名文件
            for (int fileNumber = 1; fileNumber <= walletAddrCount; ++fileNumber) {
                QString strkeyNo = QString::number(fileNumber);
                QString signedhashpath = currentPath + "/QR-drbgaesrandomhash" + strkeyNo + ".txt.sig";
                QFile::remove(signedhashpath);
                QString signedrandompath = currentPath + "/QR-drbgaesrandom" + strkeyNo + ".txt.sig";
                QFile::remove(signedrandompath);
            }
            QString keydrbgrandomhashsigpath = currentPath + "/QR-drbgaesrandomhashsig.txt";
            QFile::remove(keydrbgrandomhashsigpath);
            qDebug()<<"删除随机数和签名文件";

            QString luckylotteryTime = jsonObjreceiveTCPDatabody["lotteryTime"].toString();
            QString luckyqrId = jsonObjreceiveTCPDatabody["qrId"].toString();
            QString luckywalletAddr = jsonObjreceiveTCPDatabody["walletAddr"].toString();
            QString luckywalletPubKey = jsonObjreceiveTCPDatabody["walletPubKey"].toString();
            strlotteryTime = jsonObjreceiveTCPDatabody["nextLotteryStart"].toString();

            QDateTime endTime = QDateTime::fromString(strlotteryTime, "yyyyMMdd HH:mm:ss");
            QDateTime currentTime = QDateTime::currentDateTime();
            int secondDifference = currentTime.secsTo(endTime);//秒差

            endTimer->setInterval(secondDifference * 1000 - 35000);//将endTime转换为毫秒
            endTimer->start();
            qDebug()<<"距离摇号开始："<<secondDifference<<"秒，启动定时器，参与摇号";

            randomTimer->start();
            qDebug()<<"开始补充随机数";

            return;
        }else if(jsonObjreceiveTCPDataheader["messageName"].toString()=="loginVqr")
        {
            registerWallet();
            return;
        }else if(jsonObjreceiveTCPDataheader["messageName"].toString()=="registerWallet")
        {
            getLotteryTime();
            return;
        }else if(jsonObjreceiveTCPDataheader["messageName"].toString()=="getLotteryTime")
        {
            QString strlotteryPhase = jsonObjreceiveTCPDatabody["lotteryPhase"].toString();
            strlotteryTime = jsonObjreceiveTCPDatabody["lotteryTime"].toString();
            QDateTime endTime = QDateTime::fromString(strlotteryTime, "yyyyMMdd HH:mm:ss");
            QDateTime currentTime = QDateTime::currentDateTime();
            if(strlotteryPhase == "COLLECTING"){
                int secondDifference = currentTime.secsTo(endTime);//秒差
                if (getallrandom == true) {
                    hashSig();
                }else{
                    if(!endTimer){
                        endTimer = new QTimer;
                        endTimer->setSingleShot(true);//设置为单次触发
                        connect(endTimer, &QTimer::timeout, this, &QRServer::onEndTimeReached);
                    }else{
                        endTimer->stop();
                    }
                    if(secondDifference >= 30){
                        endTimer->setInterval(secondDifference * 1000 - 35000);//将endTime转换为毫秒
                        endTimer->start();
                        qDebug()<<"距离摇号开始："<<secondDifference<<"秒，启动定时器，参与摇号";
                    }
                    else{
                        endTimer->start(0);
                        qDebug()<<"距离摇号开始不到："<<secondDifference<<"秒，不参与摇号";
                    }
                }
            }
            return;
        }
    }
}

void QRServer::getRandom()
{
    randomTimer = new QTimer;
    randomTimer->setInterval(10);//触发时间，单位：毫秒
    randomTimer->start();

    connect(randomTimer,&QTimer::timeout,[=]()mutable{
        QVector<int> processedNumbers = readProcessedFileNumbers(StatusPath);
        auto intNo = processedNumbers.begin();
        // 检查迭代器是否到达了processedNumbers的末尾
        if (intNo != processedNumbers.end()) {
            getallrandom = false;
            randomTimer->stop();
            QString strkeyNo = QString::number(*intNo); // 使用当前编号
            n_drbgrandomPath = currentPath + "/QR-drbgaesrandom" + strkeyNo + ".txt";
            n_drbgrandomhashPath = currentPath + "/QR-drbgaesrandomhash" + strkeyNo + ".txt";

            getDrbgRandom();

            randomcount = *intNo;
            qDebug() << "随机数补充"<<randomcount;
            ++intNo;
        }else
        {
            getallrandom = true;
            qDebug() << "本次随机数补充完成";
            randomTimer->stop();
            if (endTimer && endTimer->isActive()) {
                endTimer->stop();
                hashSig();
            }
        }
    });
}

void QRServer::getDrbgRandom()
{
    QString drbgrandompath = currentPath + "/QR-drbgaesrandom.txt";//drbgaesrandom路径
    QFile drbgfile(drbgrandompath);
    if(drbgfile.exists())
    {
        drbgfile.resize(0);
    }

    int loopCount = 1024; // 循环次数
    int count = 0;
    drbgTimer = new QTimer;
    drbgTimer->start(0);//触发时间，单位：毫秒

    connect(drbgTimer,&QTimer::timeout,[=]()mutable{
        // 如果已经执行了足够次数的操作，停止定时器
        if (count >= loopCount) {
            drbgTimer->stop();
            delete drbgTimer;
            drbgTimer = nullptr;

            qDebug()<<"随机数输出完成"<<count;

            QFile drbgfile(drbgrandompath);
            QFile keydrbgfile(n_drbgrandomPath);
            // 检查目标文件是否存在
            if (keydrbgfile.exists()) {
                if (keydrbgfile.remove()) {
                    if (!drbgfile.rename(n_drbgrandomPath)) {
                        qDebug() << "重命名失败";
                        return;
                    }
                } else {
                    qDebug() << "无法删除已存在的文件";
                    return;
                }
            } else {
                if (!drbgfile.rename(n_drbgrandomPath)) {
                    qDebug() << "重命名失败";
                    return;
                }
            }

            qDebug()<<"等待随机数测试";
            testRandomFile();
        }else{
            QProcess DRBG;
            DRBG.setProgram(currentPath+"/libdrbg/drbg"); // 替换为你的可执行文件路径

            // 执行程序
            DRBG.start();
            DRBG.waitForFinished();
            QString output = DRBG.readAllStandardOutput();
            count++;
        }
    });
}

void QRServer::testRandomFile()
{
    QFile drbgfile(n_drbgrandomPath);
    drbgfile.open(QFile::ReadWrite);
    QByteArray arrdrbgRandom = drbgfile.readAll();
    drbgfile.close();
    unsigned char *rnd_data = reinterpret_cast<unsigned char *>(arrdrbgRandom.data());
    int i = nist_randomness_evaluate(rnd_data);
    if (i) {
        if (drbgfile.exists()) {
            if (drbgfile.remove()) {
                qDebug()<<"已删除失败的随机数文件";
            } else {
                qDebug()<<"删除随机数文件失败";
            }
        }
        qDebug()<<"随机数测试失败,等待重新生成随机数"<<randomcount;
        getDrbgRandom();
    }
    else {
        qDebug()<<"随机数测试通过";
        QByteArray randomHash = QCryptographicHash::hash(arrdrbgRandom, QCryptographicHash::Sha256);
        saveHashToFile(randomHash.toHex(),n_drbgrandomhashPath);
        updateFileStatus(StatusPath, randomcount, 1);//随机数哈希状态更新为1，表示已存在

        if (randomTimer) {
            randomTimer->start();
        }
    }
}

void QRServer::hashSig()
{
    QByteArray allHashes;
    hashfileExists = false;

    for (int fileNumber = 1; fileNumber <= walletAddrCount; ++fileNumber) {
        QString strkeyNo = QString::number(fileNumber);
        QString drbgaesrandomhashpath = currentPath + "/QR-drbgaesrandomhash" + strkeyNo + ".txt"; // drbghash路径
        QString sighashpath = drbgaesrandomhashpath + ".sig";
        QFile randomhashfile(drbgaesrandomhashpath);
        QFile sighashfile(sighashpath);
        if (randomhashfile.exists()) {  // 如果randomhash文件存在
            hashfileExists = true;// 至少有一个存在，设置为true
            if (sighashfile.exists()) {
                if (sighashfile.remove()) {
                    if (!randomhashfile.rename(sighashpath)) {
                        qDebug() << "重命名失败";
                        return;
                    }
                } else {
                    qDebug() << "无法删除已存在的文件";
                    return;
                }
            } else {
                if (!randomhashfile.rename(sighashpath)) {
                    qDebug() << "重命名失败";
                    return;
                }
            }

            if (!sighashfile.open(QIODevice::ReadOnly)) {
                continue;
            }

            allHashes.append(sighashfile.readAll());
            sighashfile.close();

            updateFileStatus(StatusPath, fileNumber, 2);//随机数哈希状态更新为2，表示已使用签名

            QString keydrbgrandompath = currentPath + "/QR-drbgaesrandom" + strkeyNo + ".txt";
            QFile randomfile(keydrbgrandompath);
            QString sigrandompath = keydrbgrandompath + ".sig";
            QFile sigrandomfile(sigrandompath);
            if (sigrandomfile.exists()) {
                if (sigrandomfile.remove()) {
                    if (!randomfile.rename(sigrandompath)) {
                        qDebug() << "重命名失败";
                        return;
                    }
                } else {
                    qDebug() << "无法删除已存在的文件";
                    return;
                }
            } else {
                if (!randomfile.rename(sigrandompath)) {
                    qDebug() << "重命名失败";
                    return;
                }
            }
        }
    }

    // 循环结束后检查hashfileExists的值
    if (!hashfileExists) {
        qDebug() << "没有hash文件存在，不参与摇号";
        lotteryStart();
    }
    else{
        if (!global_port.open(QIODevice::ReadWrite)){
            qDebug() << "无法打开串口，错误：" << global_port.errorString();
            if(ledTimer){
                ledTimer->stop();
                delete ledTimer;
                blinkLed(0,1000,2,3);
            }else{
                blinkLed(0,1000,2,3);
            }
            return;
        }
        QByteArray finalHash = QCryptographicHash::hash(allHashes, QCryptographicHash::Sha256);

        QByteArray send_SH;
        send_SH.resize(3);
        send_SH[0]= 0x53;//S
        send_SH[1]= 0x48;//H
        send_SH[2]= 1;
        global_port.write(send_SH + finalHash);

        connect(&global_port,&QSerialPort::readyRead,this,[=](){
            QString keydrbgrandomhashsigpath = currentPath + "/QR-drbgaesrandomhashsig.txt";//drbghash签名文件路径
            QByteArray arrdrbgRandomhashSig = global_port.readAll();
            QFile hashsigfile(keydrbgrandomhashsigpath);
            hashsigfile.open(QFile::WriteOnly);
            hashsigfile.write(arrdrbgRandomhashSig);
            hashsigfile.close();

            disconnect(&global_port,&QSerialPort::readyRead,this,nullptr);
            global_port.close();
            lotteryStart();
        });
    }
}
void QRServer::lotteryStart(){
    jsonObjsendTCPDatabody["lotteryStart"] = strlotteryTime;
    jsonObjsendTCPDatabody["qrId"] = macAddress;

    if(hashfileExists == true){
        //是否参加（yes/no）
        jsonObjsendTCPDatabody["isJoin"] = "yes";

        //drgb随机数哈希签名
        QString keydrbgrandomhashsigpath = currentPath + "/QR-drbgaesrandomhashsig.txt";
        QFile drbgrandomhashsigfile(keydrbgrandomhashsigpath);
        drbgrandomhashsigfile.open(QFile::ReadOnly);
        QByteArray arrdrbgrandomhashsig = drbgrandomhashsigfile.readAll();
        QString strdrbgrandomhashsig = arrdrbgrandomhashsig.toHex();
        drbgrandomhashsigfile.close();

        jsonObjsendTCPDatabody["signature"] = strdrbgrandomhashsig;
    }else{
        jsonObjsendTCPDatabody["isJoin"] = "no";
        jsonObjsendTCPDatabody["signature"] = "";
    }

    for (int fileNumber = 1; fileNumber <= walletAddrCount; ++fileNumber) {
        QString strkeyNo = QString::number(fileNumber);
        //钱包地址
        QString walletAddrpath = currentPath+"/QR-walletAddr" + strkeyNo + ".txt";
        QFile walletAddrfile(walletAddrpath);
        if (!walletAddrfile.exists()) continue;  // 如果文件不存在，则跳过
        walletAddrfile.open(QFile::ReadOnly | QIODevice::Text);
        QString strwalletAddr = walletAddrfile.readAll();
        walletAddrfile.close();

        lotteryItem["walletAddr"] = strwalletAddr;

        //drgb随机数哈希值
        QString signedhashpath = currentPath + "/QR-drbgaesrandomhash" + strkeyNo + ".txt.sig";
        QFile hashfile(signedhashpath);
        if (!hashfile.exists()) continue; // 如果文件不存在，则跳过
        hashfile.open(QFile::ReadOnly | QIODevice::Text);
        QString strdrbgrandomhash = hashfile.readAll();
        hashfile.close();

        lotteryItem["randomHash"] = strdrbgrandomhash;

        jsonArrsendTCPDatabodylist.append(lotteryItem);
    }
    jsonObjsendTCPDatabody["lotteryList"] = jsonArrsendTCPDatabodylist;

    int bodylength = bodylength = calculateBodySize(jsonObjsendTCPDatabody);

    jsonObjsendTCPDataheader["checksum"] = 21001;
    jsonObjsendTCPDataheader["messageLength"] = bodylength ;
    jsonObjsendTCPDataheader["messageName"] = "lotteryStart";
    jsonObjsendTCPDataheader["messageType"] = "request";
    jsonObjsendTCPDataheader["version"] = "1.0";

    jsonObjsendTCPData["header"] = jsonObjsendTCPDataheader;
    jsonObjsendTCPData["body"] = jsonObjsendTCPDatabody;

    QJsonDocument jsonDocsend(jsonObjsendTCPData);
    QByteArray sendTCPData  = jsonDocsend.toJson() + "#C";

    m_TcpSocket->write(sendTCPData);
    qDebug() << "TCP发送: " << sendTCPData;

    while (!jsonArrsendTCPDatabodylist.isEmpty()) {
        jsonArrsendTCPDatabodylist.removeAt(0);
    }

    jsonObjsendTCPDataheader = QJsonObject();
    jsonObjsendTCPDatabody = QJsonObject();
    lotteryItem  = QJsonObject();

    if (randomTimer) {
        randomTimer->start();
    }else{
        getRandom();
    }
}

void QRServer::onEndTimeReached()
{
    qDebug()<<"时间到，停止相关操作";
    if (endTimer) {
        endTimer->stop();
    }
    if (randomTimer) {
        randomTimer->stop();
    }
    if (drbgTimer) {
        drbgTimer->stop();
    }

    hashSig();
}

void QRServer::lotteryResult()
{
    QString strwinnerwalletAddr = winnerWallet;

    // 循环读取文件
    bool isMatch = false;
    for (int i = 1; i <= walletAddrCount && !isMatch; ++i) {
        QString strkeyNo = QString::number(i);

        QString walletAddrPath = currentPath+"/QR-walletAddr" + strkeyNo + ".txt";
        QFile walletAddrfile(walletAddrPath);
        if (walletAddrfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString strwalletAddr = walletAddrfile.readAll();
            if (strwalletAddr == strwinnerwalletAddr) {
                // 找到匹配的文件，停止循环
                isMatch = true;
                qDebug() << "找到匹配的编号: " << strkeyNo;
                // 读取对应编号的随机数文件内容
                QString sigrandompath = currentPath + "/QR-drbgaesrandom" + strkeyNo + ".txt.sig";
                QFile randomfile(sigrandompath);
                if (randomfile.open(QIODevice::ReadOnly)) {
                    QByteArray arrdrbgRandom = randomfile.readAll();
                    QString strdrbgRandom = arrdrbgRandom.toHex();
                    randomfile.close();

                    // 分割随机数并计算数据包数量
                    packetCount = (strdrbgRandom.length() + packetSize - 1) / packetSize;

                    // 发送数据包
                    packetTimer = new QTimer;
                    packetTimer->setSingleShot(true);//设置为单次触发
                    packetTimer->setInterval(0);//触发时间，单位：毫秒
                    packetTimer->start();
                    connect(packetTimer,&QTimer::timeout,[=]()mutable{
                        int startIndex = packetNumber * packetSize;
                        int endIndex = qMin((packetNumber + 1) * packetSize, strdrbgRandom.length());
                        QString packetData = strdrbgRandom.mid(startIndex, endIndex - startIndex);

                        jsonObjsendTCPDatabody["random"] = packetData;
                        jsonObjsendTCPDatabody["totalPackets"] = packetCount;
                        jsonObjsendTCPDatabody["currentPacket"] = packetNumber + 1;
                        jsonObjsendTCPDatabody["winnerWallet"] = winnerWallet;

                        int bodylength = calculateBodySize(jsonObjsendTCPDatabody);

                        jsonObjsendTCPDataheader["checksum"] = 21002;
                        jsonObjsendTCPDataheader["messageLength"] = bodylength;
                        jsonObjsendTCPDataheader["messageName"] = "lotteryResult";
                        jsonObjsendTCPDataheader["messageType"] = "response";
                        jsonObjsendTCPDataheader["version"] = "1.0";

                        jsonObjsendTCPData["header"] = jsonObjsendTCPDataheader;
                        jsonObjsendTCPData["body"] = jsonObjsendTCPDatabody;

                        QJsonDocument jsonDocsend(jsonObjsendTCPData);
                        QByteArray sendTCPData = jsonDocsend.toJson() + "#C";

                        m_TcpSocket->write(sendTCPData);
                        qDebug() << "TCP发送: lotteryResult packet" << packetNumber + 1 << "of" << packetCount << "OK";
                        qDebug() << sendTCPData.size();
                        ++packetNumber;

                        jsonObjsendTCPDataheader = QJsonObject();
                        jsonObjsendTCPDatabody = QJsonObject();
                    });

                } else {
                    qDebug() << "没有找到随机数文件: " << sigrandompath;
                }
            }
            walletAddrfile.close();
        } else {
            qDebug() << "打开文件失败: " << walletAddrPath;
        }
    }
}

void QRServer::loginVqr()
{
    jsonObjsendTCPDatabody["qrId"] = macAddress;

    //钱包地址
    QString walletAddrPath = currentPath+"/QR-walletAddr1.txt";
    QFile walletAddrfile(walletAddrPath);
    if (walletAddrfile.open(QFile::ReadOnly)){
        QString strwalletAddr = walletAddrfile.readAll();
        walletAddrfile.close();
        jsonObjsendTCPDatabody["walletAddr"] = strwalletAddr;
    } else {
        qDebug() << "打开文件失败:" << walletAddrPath;
    }
    //QR设备32位公钥
    QString pubkeypath = currentPath+"/QR-pubKey1.txt";
    QFile pubkeyfile(pubkeypath);
    if (pubkeyfile.open(QFile::ReadOnly)) {
        QString strpubKey = pubkeyfile.readAll();
        pubkeyfile.close();
        jsonObjsendTCPDatabody["pubKey"] = strpubKey;
    } else {
        qDebug() << "打开文件失败:" << pubkeypath;
    }

    //签名
    QString walletAddrsigpath = currentPath+"/QR-walletAddr1.txt.sig";//loginconfighash签名文件路径
    QFile walletAddrsigfile(walletAddrsigpath);
    if (walletAddrsigfile.open(QFile::ReadOnly)) {
        QByteArray arrwalletAddrsig = walletAddrsigfile.readAll();
        QString strwalletAddrsig = arrwalletAddrsig.toHex();
        walletAddrsigfile.close();
        jsonObjsendTCPDatabody["signature"] = strwalletAddrsig;
    } else {
        qDebug() << "打开文件失败:" << pubkeypath;
    }
    int bodylength = bodylength = calculateBodySize(jsonObjsendTCPDatabody);

    jsonObjsendTCPDataheader["checksum"] = 21004;
    jsonObjsendTCPDataheader["messageLength"] = bodylength ;
    jsonObjsendTCPDataheader["messageName"] = "loginVqr";
    jsonObjsendTCPDataheader["messageType"] = "request";
    jsonObjsendTCPDataheader["version"] = "1.0";

    jsonObjsendTCPData["header"] = jsonObjsendTCPDataheader;
    jsonObjsendTCPData["body"] = jsonObjsendTCPDatabody;

    QJsonDocument jsonDocsend(jsonObjsendTCPData);
    QByteArray sendTCPData  = jsonDocsend.toJson() + "#C";

    m_TcpSocket->write(sendTCPData);
    qDebug() << "TCP发送: " << sendTCPData;

    jsonObjsendTCPDataheader = QJsonObject();
    jsonObjsendTCPDatabody = QJsonObject();
}

void QRServer::getWalletAddrSig()
{
    int fileNumber = 1;
    QTimer *sigTimer = new QTimer;
    sigTimer->setSingleShot(false);//设置为非单次触发
    sigTimer->setInterval(100);//触发时间，单位：毫秒
    sigTimer->start();

    connect(sigTimer,&QTimer::timeout,[=]()mutable{
        if (fileNumber > walletAddrCount) {
            sigTimer->stop();
            qDebug() << "注册签名已生成完毕";

            ipfileExists();
        }else{
            QString strkeyNo = QString::number(fileNumber);
            walletAddrPath = currentPath+"/QR-walletAddr" + strkeyNo + ".txt";
            walletAddrsigPath = currentPath + "/QR-walletAddr" + strkeyNo + ".txt.sig";
            walletAddrSig();

            fileNumber++;
        }
    });
}

void QRServer::walletAddrSig()
{
    if (!global_port.open(QIODevice::ReadWrite)){
        qDebug() << "无法打开串口，错误：" << global_port.errorString();
        blinkLed(0,1000,2,3);
        return;
    }

    QFile walletAddrfile(walletAddrPath);
    walletAddrfile.open(QFile::ReadOnly);
    QByteArray arrwalletAddr = walletAddrfile.readAll();
    arrwalletAddr = arrwalletAddr + macAddress.toLatin1();
    QByteArray registerHash = QCryptographicHash::hash(arrwalletAddr, QCryptographicHash::Sha256);

    QByteArray send_SH;
    send_SH.resize(3);
    send_SH[0]= 0x53;//S
    send_SH[1]= 0x48;//H
    send_SH[2]= 1;
    global_port.write(send_SH + registerHash);

    connect(&global_port,&QSerialPort::readyRead,this,[=](){
        QByteArray arrwalletAddrsigdata = global_port.readAll();

        QFile walletAddrsigfile(walletAddrsigPath);
        walletAddrsigfile.open(QFile::WriteOnly);
        walletAddrsigfile.write(arrwalletAddrsigdata);
        walletAddrsigfile.close();

        disconnect(&global_port,&QSerialPort::readyRead,this,nullptr);
        global_port.close();
    });
}

void QRServer::registerWallet()
{
    jsonObjsendTCPDatabody["qrId"] = macAddress;

    for (int fileNumber = 2; fileNumber <= walletAddrCount; ++fileNumber) {
        QString strkeyNo = QString::number(fileNumber);
        //钱包地址
        QString walletAddrPath = currentPath+"/QR-walletAddr" + strkeyNo + ".txt";
        QFile walletAddrfile(walletAddrPath);
        if (!walletAddrfile.exists()) continue;  // 如果文件不存在，则跳过
        walletAddrfile.open(QFile::ReadOnly | QIODevice::Text);
        QString strwalletAddr = walletAddrfile.readAll();
        walletAddrfile.close();

        lotteryItem["walletAddr"] = strwalletAddr;

        //QR设备32位公钥
        QString pubkeypath = currentPath + "/QR-pubKey" + strkeyNo + ".txt";
        QFile pubkeyfile(pubkeypath);
        if (!pubkeyfile.exists()) continue; // 如果文件不存在，则跳过
        pubkeyfile.open(QFile::ReadOnly);
        QString strpubKey = pubkeyfile.readAll();
        pubkeyfile.close();

        lotteryItem["walletPubKey"] = strpubKey;

        //注册钱包地址签名
        QString walletAddrsigpath = currentPath + "/QR-walletAddr" + strkeyNo + ".txt.sig";
        QFile walletAddrsigfile(walletAddrsigpath);
        walletAddrsigfile.open(QFile::ReadOnly);
        QByteArray arrwalletAddrsig = walletAddrsigfile.readAll();
        QString strwalletAddrsig = arrwalletAddrsig.toHex();
        walletAddrsigfile.close();

        lotteryItem["signature"] = strwalletAddrsig;

        jsonArrsendTCPDatabodylist.append(lotteryItem);
    }
    jsonObjsendTCPDatabody["walletList"] = jsonArrsendTCPDatabodylist;

    int bodylength = bodylength = calculateBodySize(jsonObjsendTCPDatabody);

    jsonObjsendTCPDataheader["checksum"] = 21005;
    jsonObjsendTCPDataheader["messageLength"] = bodylength ;
    jsonObjsendTCPDataheader["messageName"] = "registerWallet";
    jsonObjsendTCPDataheader["messageType"] = "request";
    jsonObjsendTCPDataheader["version"] = "1.0";

    jsonObjsendTCPData["header"] = jsonObjsendTCPDataheader;
    jsonObjsendTCPData["body"] = jsonObjsendTCPDatabody;

    QJsonDocument jsonDocsend(jsonObjsendTCPData);
    QByteArray sendTCPData  = jsonDocsend.toJson() + "#C";

    m_TcpSocket->write(sendTCPData);
    qDebug() << "TCP发送: " << sendTCPData;

    while (!jsonArrsendTCPDatabodylist.isEmpty()) {
        jsonArrsendTCPDatabodylist.removeAt(0);
    }
    jsonObjsendTCPDataheader = QJsonObject();
    jsonObjsendTCPDatabody = QJsonObject();
    lotteryItem  = QJsonObject();
}

void QRServer::getLotteryTime()
{
    jsonObjsendTCPDatabody["lotteryPhase"] = "";
    jsonObjsendTCPDatabody["lotteryTime"] = "";

    int bodylength = bodylength = calculateBodySize(jsonObjsendTCPDatabody);

    jsonObjsendTCPDataheader["checksum"] = 21006;
    jsonObjsendTCPDataheader["messageLength"] = bodylength ;
    jsonObjsendTCPDataheader["messageName"] = "getLotteryTime";
    jsonObjsendTCPDataheader["messageType"] = "request";
    jsonObjsendTCPDataheader["version"] = "1.0";

    jsonObjsendTCPData["header"] = jsonObjsendTCPDataheader;
    jsonObjsendTCPData["body"] = jsonObjsendTCPDatabody;

    QJsonDocument jsonDocsend(jsonObjsendTCPData);
    QByteArray sendTCPData  = jsonDocsend.toJson() + "#C";

    m_TcpSocket->write(sendTCPData);
    qDebug() << "TCP发送: " << sendTCPData;

    jsonObjsendTCPDataheader = QJsonObject();
    jsonObjsendTCPDatabody = QJsonObject();
}

QString QRServer::keccak_256(const QString &input)
{
    QCryptographicHash hash(QCryptographicHash::Keccak_256);
    hash.addData(input.toUtf8());
    return hash.result().toHex();
}

QString QRServer::Erc55checksum(const QString &address)
{
    QString checksum = keccak_256(address).mid(0,8);
    QString erc55addr = "0x"+address;
    for(int i = 0;i<40;++i){
        int sum = i%2 == 0? address[i+2].toLatin1() -'0':address[i+2].toLatin1()-'a'+10;
        sum += i%2 == 0?(checksum[i/2].toLatin1()-'0')*16:(checksum[i/2].toLatin1()-'a'+10)*16;
        sum += 1%2 == 0?checksum[i/2+1].toLatin1()-'0':checksum[i/2+1].toLatin1()-'a'+10;
        if(sum%2 == 0){
            erc55addr[i+2] = erc55addr[i+2].toLower();
        }else{
            erc55addr[i+2] = erc55addr[i+2].toUpper();
        }
    }
    return erc55addr;
}
// 保存hash文件
void QRServer::saveHashToFile(const QString &hashvalue, const QString &hashfilepath)
{
    QFile hashFile(hashfilepath);
    if (hashFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&hashFile);
        out << hashvalue;
    }
}
// 计算body字节数
int QRServer::calculateBodySize(const QJsonObject& bodyObject)
{
    // 使用 QJsonDocument 来序列化 QJsonObject
    QJsonDocument doc(bodyObject);
    // 将 QJsonDocument 转换为 QByteArray
    QByteArray byteArray = doc.toJson(QJsonDocument::Compact);
    // 返回 QByteArray 的大小
    return byteArray.size();
}

// 初始化文件状态
void QRServer::initializeFileStatus(const QString &filePath) {
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        // 写入文件头
        out << "FileNumber,Status\n";
        // 初始化编号1-10的状态为0
        for (int i = 1; i <= 10; ++i) {
            out << i << ",0\n";
        }
        file.close();
    } else {
        qDebug() << "无法打开文件进行写：" << filePath;
    }
}

// 更新文件状态
void QRServer::updateFileStatus(const QString &filePath, int fileNumber, int status) {
    QFile file(filePath);
    if (file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QTextStream in(&file);
        QString line;
        bool found = false;
        // 创建一个临时文件来存储更新后的内容
        QFile tempFile(filePath + ".tmp");
        if (tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&tempFile);
            while (!in.atEnd()) {
                line = in.readLine();
                QStringList parts = line.split(",");
                if (parts.size() == 2 && parts[0].toInt() == fileNumber) {
                    // 更新指定编号的状态
                    out << fileNumber << "," << status << "\n";
                    found = true;
                } else {
                    out << line << "\n";
                }
            }
            if (!found) {
                qDebug() << "文件编号" << fileNumber << "在文件中找不到。";
            }
            tempFile.close();
            file.close();
            // 替换原文件
            file.remove();
            tempFile.rename(filePath);
        } else {
            qDebug() << "无法打开临时文件进行写入。";
        }
    } else {
        qDebug() << "无法打开文件进行读/写：" << filePath;
    }
}

void QRServer::Delay(unsigned int msec)
{
    QEventLoop loop;
    QTimer::singleShot(msec, &loop, SLOT(quit()));
    loop.exec();
}

// 读取状态为0或2的编号
QVector<int> QRServer::readProcessedFileNumbers(const QString &filePath) {
    QVector<int> processedNumbers;
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString line;
        while (!in.atEnd()) {
            line = in.readLine();
            QStringList parts = line.split(",");
            if (parts.size() >= 2) {
                bool ok;
                int number = parts[0].toInt(&ok);
                int status = parts[1].toInt(&ok);
                if (ok && (status == 0 || status == 2)) {
                    processedNumbers.append(number);
                }
            }
        }
        file.close();
    } else {
        qDebug() << "无法打开文件进行读取:" << filePath;
    }
    return processedNumbers;
}

void QRServer::writeSysVersion()
{
    QString versionFilePath = QDir::currentPath()+"/QR-version.dat";
    versionFilePath = QDir::toNativeSeparators(versionFilePath);
    QFile versionFile(versionFilePath);
    if(versionFile.exists()){
        return;
    }
    versionFile.open(QIODevice::WriteOnly);
    versionFile.write(SysVersion.toUtf8());
    versionFile.close();
}

void QRServer::onUpgrade()
{
    blinkLed(2,500,0,3);

    int updateTtype = GlobalVal::updateTtype;
    QString programRootDir = GlobalVal::programRootDir;
    bool updateOK = false;
    if(updateTtype==1){
        QJsonArray fileList = GlobalVal::fileList;
        for(int i =0;i<fileList.size();i++){
            QJsonObject item = fileList[i].toObject();
            QString path = item.value("path").toString();
            QString downloadRootDir = programRootDir;
            if(!path.isEmpty() && path!="/"){
                downloadRootDir = programRootDir + "/" + path;
            }
            QJsonArray sublist = item.value("sublist").toArray();
            for(int j =0;j<sublist.size();j++){
                QString fileUrl = sublist[j].toString();
                fileUrl = Download::urlEncode(fileUrl);
                QUrl url(fileUrl);
                download->resetStatus();
                download->downloadFile(url,downloadRootDir);
                this->syncVersion();
                updateOK = true;
            }
        }
    }else if(updateTtype==2){
        QString zipurl = GlobalVal::zipurl;
        if(!zipurl.isEmpty()){
            QUrl url(zipurl);
            handleZip->downloadZip(url);
            this->syncVersion();
            qDebug() << "更新结束^_^";
            updateOK = true;
        }
    }else{
        qDebug() << "参数校验失败[错误代码:500]";
    }
    //启动主程序
    if(updateOK){
        this->startMainApp();
    }
}

void QRServer::syncVersion()
{
    QString newVersion = GlobalVal::newVersion;
    QString versionFilePath = QDir::currentPath()+"/QR-version.dat";
    versionFilePath = QDir::toNativeSeparators(versionFilePath);
    QFile versionFile(versionFilePath);
    versionFile.open(QIODevice::WriteOnly);
    versionFile.write(newVersion.toUtf8());
    versionFile.close();
}

void QRServer::startMainApp(){
    QString mainAppName = GlobalVal::mainAppName;
    if(!mainAppName.isEmpty()){
        qDebug() << "等待重新运行" ;
        QProcess::startDetached(mainAppName,QStringList());
        _Exit(EXIT_FAILURE);
    }
}

void QRServer::openLed(int pin1,int value1,int pin2,int value2,int pin3,int value3){
    if(ledTimer){
        ledTimer->stop();
        delete ledTimer;
    }
    digitalWrite(pin1, value1);
    digitalWrite(pin2, value2);
    digitalWrite(pin3, value3);
}
void QRServer::blinkLed(int pin1,int delayTime,int pin2,int pin3){
    if(!ledTimer){
        ledTimer = new QTimer(this);
        connect(ledTimer, &QTimer::timeout, this, [=](){
            digitalWrite(pin1, !digitalRead(pin1));
        });
    }
    ledTimer->start(delayTime);
    digitalWrite(pin2, 0);
    digitalWrite(pin3, 0);
}
void QRServer::setupLogDeletion(int intervalDays, int daysToKeep) {
    // 设置定时器，每天检查一次
    QTimer *logtimer = new QTimer();
    connect(logtimer, &QTimer::timeout, [=]() {
        deleteOldLogFiles(daysToKeep);
    });
    // 启动定时器，单位为毫秒，这里设置为一天
    logtimer->start(intervalDays * 24 * 60 * 60 * 1000);
}
