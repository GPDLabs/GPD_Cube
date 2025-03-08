#include "checkversion.h"
#include "qrserver.h"

extern QRServer *server;

CheckVersion::CheckVersion(QObject *parent,QString remoteUrl)
    : QObject{parent}
{
    this->remoteInfoUrl = remoteUrl;
}
/**
 * 发起http请求，请求后端版本更新信息
 */
void CheckVersion::requestRemoteVersion(){
    QUrl url(this->remoteInfoUrl);
    qDebug()<<"请求中...";
    QNetworkRequest req;
    QNetworkAccessManager nam;
    connect(&nam,&QNetworkAccessManager::finished,this,&CheckVersion::requestRemoteVersionFinished);
    req.setUrl(url);
    QNetworkReply *reply = nam.get(req);
    QEventLoop loop;
    connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
    loop.exec();
}
/**
 * 接收http请求响应数据
 */
void CheckVersion::requestRemoteVersionFinished(QNetworkReply *reply){
    QString rsdata = reply->readAll();
    QString sysVersion = this->readSysVersion();
    QJsonParseError jsonParseError;
    QJsonDocument jsonDoc(QJsonDocument::fromJson(rsdata.toUtf8(),&jsonParseError));
    if(QJsonParseError::NoError !=jsonParseError.error){
        qDebug()<<(tr("解析请求响应数据失败:%1").arg(jsonParseError.errorString()));
        return;
    }
    QJsonObject jsonObj = jsonDoc.object();
    QString newVer = jsonObj.value("version").toString();
    if(newVer<=sysVersion){
        //不更新
        qDebug()<<(tr("当前版本已经是最新版本"));
    }else{
        GlobalVal::newVersion = newVer;
        qDebug()<<(tr("发现新版本:%1，将立即更新并重启软件").arg(newVer));
        int updateTtype = jsonObj.value("type").toInt();
        GlobalVal::updateTtype = updateTtype;
        QString zipurl = jsonObj.value("zipurl").toString();
        QJsonArray fileList = jsonObj.value("filelist").toArray();
        QString mainAppName = jsonObj.value("mainAppName").toString();
        GlobalVal::zipurl = zipurl;
        GlobalVal::fileList = fileList;
        GlobalVal::mainAppName = mainAppName;

        server->stopServer();
        server->onUpgrade();
    }
}
/**
 * 获取本地程序版本号
 */
QString CheckVersion::readSysVersion()
{
    QString versionFilePath = QDir::currentPath()+"/QR-version.dat";
    versionFilePath = QDir::toNativeSeparators(versionFilePath);
    QFile versionFile(versionFilePath);
    versionFile.open(QIODevice::ReadOnly);
    QString sysVersion = versionFile.readAll();
    versionFile.close();
    return sysVersion;
}
