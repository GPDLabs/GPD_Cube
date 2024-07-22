#include "qrserver.h"
#include <unistd.h>
#include "sts.h"

QRServer::QRServer(QObject *parent) : QObject(parent)
{
    QBluetoothLocalDevice localDevice;
    QString localDeviceName;

    // Check if Bluetooth is available on this device
    if (localDevice.isValid()) {
        qDebug() << "Bluetooth is available !";

        // Turn Bluetooth on
        localDevice.powerOn();

        // Read local device name
        localDeviceName = localDevice.name();
        qDebug() << "My name is :" << localDeviceName << "with address : " << localDevice.address().toString();

        // Make it visible to others
        localDevice.setHostMode(QBluetoothLocalDevice::HostDiscoverable);

        // Get connected devices
        QList<QBluetoothAddress> remotes;
        remotes = localDevice.connectedDevices();
    }
}

void QRServer::startServer(const QBluetoothAddress &localAdapter) {
    qDebug() << "Starting bluetooth server...";
    m_rfcommServer = new QBluetoothServer(QBluetoothServiceInfo::RfcommProtocol, this);
    connect(m_rfcommServer, SIGNAL(newConnection()), this, SLOT(clientConnected()));
    bool result = m_rfcommServer->listen(localAdapter);
    if (!result) {
        qWarning() << "Cannot bind bluetooth server to" << localAdapter.toString();
        return;
    }

    // Set server attributes
    qDebug() << "Setting bluetooth server attributes...";
    m_serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceName, tr("Bt Rpi 3 Server"));
    m_serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceDescription,
                               tr("Example bluetooth rpi3 server"));
    m_serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceProvider, tr("qt-project.org"));

    // Create unique bt uuid
    m_serviceInfo.setServiceUuid(QBluetoothUuid(serviceUuid));

    // Make it discoverable
    qDebug() << "Make bluetooth server discoverable...";
    QBluetoothServiceInfo::Sequence publicBrowse;
    publicBrowse << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));
    m_serviceInfo.setAttribute(QBluetoothServiceInfo::BrowseGroupList,
                               publicBrowse);

    // Used protocol
    QBluetoothServiceInfo::Sequence protocolDescriptorList;
    QBluetoothServiceInfo::Sequence protocol;
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    protocol.clear();
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
             << QVariant::fromValue(quint8(m_rfcommServer->serverPort()));
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    m_serviceInfo.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList,
                               protocolDescriptorList);

    m_serviceInfo.registerService(localAdapter);
}

QRServer::~QRServer() {
    stopServer();
}

void QRServer::stopServer() {
    // Unregister service
    m_serviceInfo.unregisterService();

    // Close sockets
    qDeleteAll(m_clientSockets);

    // Close server
    delete m_rfcommServer;
    m_rfcommServer = nullptr;
}

void QRServer::sendMessage(const QString &message) {
    qDebug() << "Bluetooth sending: " << message;
    QByteArray text = message.toUtf8() + '\n';

    foreach (QBluetoothSocket *socket, m_clientSockets)
        socket->write(text);
}

void QRServer::clientConnected() {
    qDebug() << "New connection detected !";
    QBluetoothSocket *socket = m_rfcommServer->nextPendingConnection();
    if (!socket)
        return;

    connect(socket, &QBluetoothSocket::readyRead, this, &QRServer::readSocket);
    connect(socket, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
    m_clientSockets.append(socket);
    qDebug() << "Client [" << socket->peerName() << "] connected !";
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

void QRServer::readSocket() {
    QBluetoothSocket *socket = qobject_cast<QBluetoothSocket *>(sender());
    if (!socket)
        return;
    //接收到的字符串必须以"/n",二进制为0A结尾才能读到
    //    while (socket->canReadLine()) {
    //        QByteArray line = socket->readLine().trimmed();
    //        emit messageReceived(socket->peerName(),
    //                             QString::fromUtf8(line.constData(), line.length()));
    //    }
    //都可以读取
    QByteArray receiveData ;
    receiveData.append(socket->readAll());
    qDebug() <<  "Bluetooth receving: " <<receiveData;

    jsonDocreceive = QJsonDocument::fromJson(receiveData);//收到的字节转为json文档
    jsonObjreceive = jsonDocreceive.object();//json文档转为json对象
    if(jsonObjreceive["body"].isObject()&&jsonObjreceive["header"].isObject()){
        jsonObjreceivebody =jsonObjreceive["body"].toObject();
        jsonObjreceiveheader =jsonObjreceive["header"].toObject();
        jsonObjsendchecksum = jsonObjreceiveheader["checksum"].toString();
        jsonObjreceivemessagedata = jsonObjreceivebody["messageData"].toObject();
        if(jsonObjreceivebody["messageType"].toString()=="wirelessConf")
        {
            Openwifi();
            jsonObjsendmessagedata["status"]="success";
            jsonObjsendbody["messageType"]="wirelessConfResult";
            jsonObjsendbody["messageData"]=jsonObjsendmessagedata;
            //计算body对象下的数据长度，但不包括子对象内的
            int bodylength =0;
            QJsonValue bodyvalue = jsonObjsendbody;
            if(bodyvalue.isObject()){
                QJsonObject bodyobj =bodyvalue.toObject();
                for(const QString &key : bodyobj.keys()){
                    QJsonValue value = bodyobj[key];
                    if(value.isObject()){
                        QJsonObject obj = value.toObject();
                        bodylength += key.length() + obj.size() * 2;
                    }else if (value.isArray()) {
                        QJsonArray arr = value.toArray();
                        bodylength += key.length() + arr.size() * 2;
                    }else{
                        bodylength += key.length() + value.toString().length();
                    }
                }
            }
            //计算responsedata对象下的数据长度
            int datalength =0;
            QJsonValue datavalue = jsonObjsendmessagedata;
            if(datavalue.isObject()){
                QJsonObject dataobj =datavalue.toObject();
                for(const QString &key : dataobj.keys()){
                    QJsonValue value = dataobj[key];
                    if(value.isObject()){
                        QJsonObject obj = value.toObject();
                        datalength += key.length() + obj.size() * 2;
                    }else if (value.isArray()) {
                        QJsonArray arr = value.toArray();
                        datalength += key.length() + arr.size() * 2;
                    }else{
                        datalength += key.length() + value.toString().length();
                    }
                }
            }
            jsonObjsendheader["checksum"]=jsonObjsendchecksum;
            jsonObjsendheader["messageLength"]=bodylength + datalength - jsonObjsendmessagedata.size() * 2;//消息长度为body+data-data内的对象数*2
            jsonObjsendheader["messageType"]="response";
            jsonObjsendheader["version"]="1.0";

            jsonObjsend["header"]=jsonObjsendheader;
            jsonObjsend["body"]=jsonObjsendbody;

            QJsonDocument jsonDocsend(jsonObjsend);
            QByteArray sendData  = jsonDocsend.toJson();
            foreach (QBluetoothSocket *socket, m_clientSockets)
                socket->write(sendData);

            qDebug() << "Bluetooth sending: " << sendData;
            sendData.clear();
            jsonObjsend.remove("header");
            jsonObjsend.remove("body");
            return;
        }else if(jsonObjreceivebody["messageType"].toString()=="walletAddr")
        {
            OpenPort();
            AddKey();
            //延时100毫米保证文件写入完毕
            QEventLoop loop;
            QTimer::singleShot(100,&loop,SLOT(quit()));
            loop.exec();
            DecompressKey();

            //延时100毫米保证文件写入完毕
            QTimer::singleShot(100,&loop,SLOT(quit()));
            loop.exec();

            DPpubkeyfile = currentPath+"/DPpubkey.txt";
            QFile file(DPpubkeyfile);
            file.open(QFile::ReadOnly);
            arrDPKey=file.readAll();
            qDebug()<<arrDPKey;
            file.close();

            strDPKey=arrDPKey.toHex();
            ETHaddr = keccak_256(strDPKey).right(40);//以太坊哈希值的后20字节
            ETHaddrchecksum = Erc55checksum(ETHaddr);
            //            qDebug()<<strDPKey<<ETHaddr<<ETHaddr.size()<<ETHaddrchecksum;

            jsonObjsendmessagedata["status"]="success";
            jsonObjsendmessagedata["walletAddr"]=ETHaddrchecksum;
            jsonObjsendmessagedata["pubKey"]=strDPKey;

            jsonObjsendbody["messageType"]="walletAddrResult";
            jsonObjsendbody["messageData"]=jsonObjsendmessagedata;
            //计算body对象下的数据长度，但不包括子对象内的
            int bodylength =0;
            QJsonValue bodyvalue = jsonObjsendbody;
            if(bodyvalue.isObject()){
                QJsonObject bodyobj =bodyvalue.toObject();
                for(const QString &key : bodyobj.keys()){
                    QJsonValue value = bodyobj[key];
                    if(value.isObject()){
                        QJsonObject obj = value.toObject();
                        bodylength += key.length() + obj.size() * 2;
                    }else if (value.isArray()) {
                        QJsonArray arr = value.toArray();
                        bodylength += key.length() + arr.size() * 2;
                    }else{
                        bodylength += key.length() + value.toString().length();
                    }
                }
            }
            //计算messagedata对象下的数据长度
            int datalength =0;
            QJsonValue datavalue = jsonObjsendmessagedata;
            if(datavalue.isObject()){
                QJsonObject dataobj =datavalue.toObject();
                for(const QString &key : dataobj.keys()){
                    QJsonValue value = dataobj[key];
                    if(value.isObject()){
                        QJsonObject obj = value.toObject();
                        datalength += key.length() + obj.size() * 2;
                    }else if (value.isArray()) {
                        QJsonArray arr = value.toArray();
                        datalength += key.length() + arr.size() * 2;
                    }else{
                        datalength += key.length() + value.toString().length();
                    }
                }
            }
            jsonObjsendheader["checksum"]=jsonObjsendchecksum;
            jsonObjsendheader["messageLength"]=bodylength + datalength - jsonObjsendmessagedata.size() * 2;//消息长度为body+data-data内的对象数*2
            jsonObjsendheader["messageType"]="response";
            jsonObjsendheader["version"]="1.0";

            jsonObjsend["header"]=jsonObjsendheader;
            jsonObjsend["body"]=jsonObjsendbody;
            QJsonDocument jsonDocsend(jsonObjsend);
            QByteArray sendData  = jsonDocsend.toJson();

            foreach (QBluetoothSocket *socket, m_clientSockets)
                socket->write(sendData);
            qDebug() << "Bluetooth sending: " << sendData;
            sendData.clear();
            jsonObjsend.remove("header");
            jsonObjsend.remove("body");
            jsonObjsendmessagedata.remove("walletAddr");
            jsonObjsendmessagedata.remove("pubKey");
            global_port.close();
            return;

        }else if(jsonObjreceivebody["messageType"].toString()=="getRandom")
        {
            OpenPort();
            OutputRandom();
            //            int loopCount = 1024; // 循环次数
            //            yanshiTimer = new QTimer;
            //            yanshiTimer->start(100);//触发时间，单位：毫秒
            //            connect(yanshiTimer,&QTimer::timeout,[=](){
            //                static int count = 0;
            //                count++;
            //                // 如果已经执行了足够次数的操作，停止定时器
            //                if (count <= loopCount) {
            //                    OutputRandom();
            //                }else{
            //                    yanshiTimer->stop();
            //                    qDebug()<<"随机数输出完成";
            //                    ReadRandomFile();
            //                }
            //            });
            //延时100毫米保证文件写入完毕
            QEventLoop loop;
            QTimer::singleShot(100,&loop,SLOT(quit()));
            loop.exec();

            //            QProcess DRBG;
            //            DRBG.setProgram(currentPath+"/libdrbg/drbg"); // 替换为你的可执行文件路径
            //            // 执行程序
            //            DRBG.start();
            //            // 等待程序执行完成
            //            DRBG.waitForFinished();
            //            // 获取输出和错误
            //            QString output = DRBG.readAllStandardOutput();
            //            QString error = DRBG.readAllStandardError();
            //            // 输出程序的输出和错误
            //            qDebug() << "Output:" << output;
            //            qDebug() << "Error:" << error;

            //            randomfile = currentPath+"/drbgaesrandom.txt";//linux路径
            randomfile = currentPath+"/random.txt";//linux路径
            QFile file(randomfile);
            file.open(QFile::ReadOnly);
            arrRandom = file.readAll();
            RandomSize = arrRandom.size();
            strRandom = arrRandom.toHex();
            file.close();

            jsonObjsendmessagedata["status"]="success";
            jsonObjsendmessagedata["randomCounat"]=RandomSize/1024;
            jsonObjsendmessagedata["random"]=strRandom;

            jsonObjsendbody["messageType"]="getRandomResult";
            jsonObjsendbody["messageData"]=jsonObjsendmessagedata;

            //计算body对象下的数据长度，但不包括子对象内的
            int bodylength =0;
            QJsonValue bodyvalue = jsonObjsendbody;
            if(bodyvalue.isObject()){
                QJsonObject bodyobj =bodyvalue.toObject();
                for(const QString &key : bodyobj.keys()){
                    QJsonValue value = bodyobj[key];
                    if(value.isObject()){
                        QJsonObject obj = value.toObject();
                        bodylength += key.length() + obj.size() * 2;
                    }else if (value.isArray()) {
                        QJsonArray arr = value.toArray();
                        bodylength += key.length() + arr.size() * 2;
                    }else{
                        bodylength += key.length() + value.toString().length();
                    }
                }
            }
            //计算messagedata对象下的数据长度
            int datalength =0;
            QJsonValue datavalue = jsonObjsendmessagedata;
            if(datavalue.isObject()){
                QJsonObject dataobj =datavalue.toObject();
                for(const QString &key : dataobj.keys()){
                    QJsonValue value = dataobj[key];
                    if(value.isObject()){
                        QJsonObject obj = value.toObject();
                        datalength += key.length() + obj.size() * 2;
                    }else if (value.isArray()) {
                        QJsonArray arr = value.toArray();
                        datalength += key.length() + arr.size() * 2;
                    }else{
                        datalength += key.length() + value.toString().length();
                    }
                }
            }
            jsonObjsendheader["checksum"]=jsonObjsendchecksum;
            jsonObjsendheader["messageLength"]=bodylength + datalength - jsonObjsendmessagedata.size() * 2;//消息长度为body+data-data内的对象数*2
            jsonObjsendheader["messageType"]="response";
            jsonObjsendheader["version"]="1.0";

            jsonObjsend["header"]=jsonObjsendheader;
            jsonObjsend["body"]=jsonObjsendbody;
            QJsonDocument jsonDocsend(jsonObjsend);
            QByteArray sendData  = jsonDocsend.toJson();
            foreach (QBluetoothSocket *socket, m_clientSockets)
                socket->write(sendData);
            qDebug() << "Bluetooth sending: " << sendData;
            sendData.clear();
            jsonObjsend.remove("header");
            jsonObjsend.remove("body");
            jsonObjsendmessagedata.remove("random");
            jsonObjsendmessagedata.remove("randomCounat");
            global_port.close();
            return;
        }
        else{
            return;
        }
    }
}

void QRServer::Openwifi() {
    QString jsonwifiName = jsonObjreceivemessagedata["wifiName"].toString();
    QString jsonwifiPwd = jsonObjreceivemessagedata["wifiPwd"].toString();

    QFile file("/etc/wpa_supplicant/wpa_supplicant.conf");
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream stream(&file);

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

        file.close();
        QProcess process1;
        process1.start("sh",QStringList()<<"-c"<<"sudo wpa_cli -i wlan0 reconfigure");
        process1.waitForFinished();
    }
}
void QRServer::OpenPort()
{
    global_port.setDataBits(QSerialPort::Data8);
    global_port.setParity(QSerialPort::NoParity);
    global_port.setStopBits(QSerialPort::OneStop);
    global_port.setBaudRate(460800);//设置波特率460800
    global_port.setPortName("ttyACM0");//LINUX设置端口号
    if(global_port.open(QIODevice::ReadWrite))
    {
        qDebug()<<"串口打开成功";
        return;
    }else
    {
        qDebug()<<"串口打开失败";
        return;
    }
}
void QRServer::AddKey()
{
    int intkeyNo = jsonObjreceivemessagedata["keyNo"].toInt();
    QString strkeyNo = QString::number(intkeyNo);
    QByteArray arrkeyNo=strkeyNo.toUtf8();
    //发送生成公钥指令
    QByteArray send_NK;
    send_NK.resize(2);
    send_NK[0]= 0x4E;//N
    send_NK[1]= 0x4B;//K
    global_port.write(send_NK + arrkeyNo);

    //接收公钥写入文件
    connect(&global_port,&QSerialPort::readyRead,this,[=](){
        arrKey.clear();
        arrKey.append(global_port.readAll());
        //        qDebug()<<arrKey;
        strKey=arrKey.toHex();
        //        qDebug()<<strKey;
        pubkeyfile = currentPath+"/pubkey.txt" ;
        QFile file(pubkeyfile);
        file.open(QFile::ReadWrite|QFile::Truncate);
        file.resize(0);
        file.write(arrKey);
        file.close();
        disconnect(&global_port,0,0,0);
    });
}
void QRServer::DecompressKey()
{
    pubkeyfile = currentPath+"/pubkey.txt";
    QFile file(pubkeyfile);
    file.open(QFile::ReadOnly);
    arrKey=file.readAll();

    file.close();
    //发送生成公钥指令
    QByteArray send_DP;
    send_DP.resize(2);
    send_DP[0]= 0x44;//D
    send_DP[1]= 0x50;//P
    global_port.write(send_DP + arrKey);
    //接收公钥写入文件
    connect(&global_port,&QSerialPort::readyRead,this,[=](){
        arrDPKey.clear();
        arrDPKey.append(global_port.readAll());
        strDPKey=arrDPKey.toHex();

        DPpubkeyfile = currentPath+"/DPpubkey.txt";
        QFile file(DPpubkeyfile);
        file.open(QFile::ReadWrite|QFile::Truncate);
        file.resize(0);
        file.write(arrDPKey);
        file.close();
        disconnect(&global_port,0,0,0);
    });
}

void QRServer::OutputRandom()
{
    QByteArray OR;
    OR.resize(2);
    OR[0]= 0x4F;//O
    OR[1]= 0x52;//R
    global_port.write(OR);
    connect(&global_port,&QSerialPort::readyRead,this,[=](){
        Random.clear();
        Random.append(global_port.readAll());
        randomfile = currentPath+"/random.txt";
        QFile file(randomfile);
        file.open(QFile::ReadWrite|QFile::Append);
        //        if(file.size()==1048576){
        file.resize(0);
        file.write(Random);
        //            file.flush();
        file.close();
        //        }
        //        else{
        //            file.write(Random);
        //            file.flush();
        //            file.close();
        //        }
        disconnect(&global_port,0,0,0);
    });
}
void QRServer::ReadRandomFile()
{
    randomfile = currentPath+"/random.txt";//linux路径
    QFileInfo fileinfo(randomfile);
    if (randomfile.isEmpty() || fileinfo.size()== 0)
    {
        return;
    }
    QFile file(randomfile);
    file.open(QFile::ReadWrite);
    arrRandom= file.readAll();
    RandomSize = arrRandom.size();
    strRandom=arrRandom.toHex();
    file.close();
    unsigned char *rnd_data = reinterpret_cast<unsigned char *>(arrRandom.data());
    i = nist_randomness_evaluate(rnd_data);
    qDebug()<<i;
    if (i) {
        //            printf("\nSorry, Some Randomness Test Failed.\n");
        qDebug()<<("\nSorry, Some Randomness Test Failed.\n");
    }
    else {
        //            printf("\nCongratulations, All Randomness Test Passed.\n");
        qDebug()<<("\nCongratulations, All Randomness Test Passed.\n");
    }
}
QString QRServer::keccak_256(const QString &input) {
    QCryptographicHash hash(QCryptographicHash::Keccak_256);
    hash.addData(input.toUtf8());
    return hash.result().toHex();
}

QString QRServer::Erc55checksum(const QString &address){
    QString checksum = keccak_256(address).mid(0,8);
    QString erc55addr="0x"+address;
    for(int i=0;i<40;++i){
        int sum =i%2==0? address[i+2].toLatin1() -'0':address[i+2].toLatin1()-'a'+10;
        sum+= i%2==0?(checksum[i/2].toLatin1()-'0')*16:(checksum[i/2].toLatin1()-'a'+10)*16;
        sum+=1%2==0?checksum[i/2+1].toLatin1()-'0':checksum[i/2+1].toLatin1()-'a'+10;
        if(sum%2==0){
            erc55addr[i+2]=erc55addr[i+2].toLower();
        }else{
            erc55addr[i+2]=erc55addr[i+2].toUpper();
        }
    }
    return erc55addr;
}
