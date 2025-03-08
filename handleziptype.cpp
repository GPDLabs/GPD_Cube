#include "handleziptype.h"

#pragma execution_character_set("utf-8")

HandleZipType::HandleZipType(QObject *parent)
    : QObject(parent)
    , reply(nullptr)
    , file(nullptr)
    , httpRequestAborted(false)
{
    manager = new QNetworkAccessManager(this);
}
HandleZipType::~HandleZipType(){

}

void HandleZipType::downloadZip(QUrl newUrl)
{
    if (!newUrl.isValid()) {
        qDebug()<<(tr("下载地址[%1]校验失败!").arg(newUrl.toString()));
        return;
    }

    QString fileName = newUrl.fileName();
    this->originFileName = fileName;
    if (fileName.isEmpty()){
        qDebug()<<(tr("压缩包地址有误:%1").arg(fileName));
        return;
    }
    qDebug()<<(tr("开始下载压缩包:%1").arg(fileName));

    QString downloadDirectory = QDir::currentPath();

    QString currDate =  QDate::currentDate().toString("yyyyMMdd");
    downloadDirectory = downloadDirectory + "/QRUpdate-"+currDate;
    QDir tempDownDir(downloadDirectory);
    if(!tempDownDir.exists()){
        tempDownDir.mkpath(downloadDirectory);
    }else{
        tempDownDir.removeRecursively();
        tempDownDir.mkpath(downloadDirectory);
    }
    this->tempDir = downloadDirectory;
    bool useDirectory = !downloadDirectory.isEmpty() && QFileInfo(downloadDirectory).isDir();
    if (useDirectory){
        fileName.prepend(downloadDirectory + '/');
    }
    this->zipFilePath = fileName;
    if (QFile::exists(fileName)) {
        QFile::remove(fileName);
    }
    file = openFileForWrite(fileName);
    if (!file)
        return;
    startRequest(newUrl);
}

void HandleZipType::startRequest(const QUrl &requestedUrl)
{
    url = requestedUrl;
    httpRequestAborted = false;

    QNetworkRequest request(url);
    reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, &HandleZipType::httpFinished);
    connect(reply, &QIODevice::readyRead, this, &HandleZipType::httpReadyRead);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(httpError(QNetworkReply::NetworkError)));

    connect(reply, &QNetworkReply::downloadProgress, this, &HandleZipType::networkReplyProgress);
    QEventLoop loop;
    connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
    loop.exec();
}

void HandleZipType::cancelDownload()
{
    httpRequestAborted = true;
    reply->abort();;
}

void HandleZipType::httpFinished(){
    QFileInfo fi;
    if (file) {
        fi.setFile(file->fileName());
        file->close();
        file.reset();
    }

    if (httpRequestAborted) {
        reply->deleteLater();
        reply = nullptr;
        return;
    }

    if (reply->error()) {
        QFile::remove(fi.absoluteFilePath());
        reply->deleteLater();
        reply = nullptr;
        return;
    }

    const QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    reply->deleteLater();
    reply = nullptr;

    if (!redirectionTarget.isNull()) {
        const QUrl redirectedUrl = url.resolved(redirectionTarget.toUrl());
        startRequest(redirectedUrl);
        return;
    }
    //下载完成，解压并拷贝到程序目录
    qDebug()<<(tr("正在解压文件[%1]").arg(this->originFileName));
    QFileInfo zipFile(this->zipFilePath);
    bool unzipStatus = this->unzip(this->zipFilePath);
    if(unzipStatus){
        qDebug()<<(tr("解压文件[%1]完成^_^").arg(this->originFileName));
        QFileInfo delfile(this->zipFilePath);
        delfile.dir().remove(delfile.fileName());
        QString suffix = zipFile.suffix();
        QString zipDir = this->originFileName.replace("."+suffix,"");
        QString originDir = this->tempDir;
        QDir originZipDir(QDir::toNativeSeparators(originDir+"/"+zipDir));
        if(originZipDir.exists()){
            originDir = originDir +"/"+zipDir;
        }
        qDebug()<<(tr("开始复制文件......"));
        bool rs = this->cpdir(QDir::toNativeSeparators(originDir),QDir::toNativeSeparators(GlobalVal::programRootDir),true);
        if(rs){
            qDebug()<<(tr("复制文件完成^_^"));
            QDir delTmpDir(this->tempDir);
            delTmpDir.removeRecursively();
        }
    }
}

void HandleZipType::httpReadyRead()
{
    if (file){
        file->write(reply->readAll());
    }
}

void HandleZipType::httpError(QNetworkReply::NetworkError error){
     qDebug()<<(tr("文件[%1]下载失败").arg(this->originFileName));
}

std::unique_ptr<QFile> HandleZipType::openFileForWrite(const QString &fileName)
{
    std::unique_ptr<QFile> file(new QFile(fileName));
    if (!file->open(QIODevice::WriteOnly)) {
        return nullptr;
    }
    return file;
}

void HandleZipType::networkReplyProgress(qint64 bytesRead, qint64 totalBytes)
{
    QString totalstr;
    QString readstr;
    QRegExp reg;
    reg.setPattern("(\\.){0,1}0+$");
    if(totalBytes>=1048576){
        double totalMb =  totalBytes/1024/1024;
        totalstr = QString("%1").arg(totalMb,0,'f',3).replace(reg,"") + "MB";
    }else{
        double totalKb =  totalBytes/1024;
        totalstr = QString("%1").arg(totalKb,0,'f',3).replace(reg,"") + "KB";
    }
    if(bytesRead>=1048576){
        double readMb =  bytesRead/1024/1024;
        readstr = QString("%1").arg(readMb,0,'f',3).replace(reg,"") + "MB";
    }else{
        double readKb =  bytesRead/1024;
        readstr = QString("%1").arg(readKb,0,'f',3).replace(reg,"") + "KB";
    }
    QString thisProgres = readstr + "/" + totalstr;
    qDebug()<<(tr("文件[%1]下载进度：%2").arg(this->originFileName,thisProgres));
    if(bytesRead==totalBytes){
       qDebug()<<(tr("文件[%1]下载完成。").arg(this->originFileName));
    }
}
/**
 * zip解压
 */
bool HandleZipType::unzip(QString zipurl){
    if(zipurl.isEmpty()){
        return false;
    }
    QString unzipUDir = this->tempDir;
    QDir tmpDir(unzipUDir);
    if(!tmpDir.exists()){
        tmpDir.mkpath(unzipUDir);
    }
    QZipReader read(zipurl);
    bool rs = read.extractAll(unzipUDir);
    return rs;
}
/**
 * 复制文件夹里面的文件
 * HandleZipType::cpdir
 * fromDirPath 源目录
 * toDirPath 目标目录
 * f 是否覆盖
 */
bool HandleZipType::cpdir(QString fromDirPath,QString toDirPath,bool f){
    QDir fromDir(fromDirPath);
    QDir toDir(toDirPath);
    if(!toDir.exists()){
        toDir.mkpath(toDirPath);
    }
    if(!fromDir.exists()){
        return false;
    }
    QFileInfoList fileInfoList = fromDir.entryInfoList();
    foreach(QFileInfo fileInfo,fileInfoList){
        if(fileInfo.fileName() == "." || fileInfo.fileName() == ".."){
            continue;
        }
        //复制目录
        if(fileInfo.isDir()){
            QString toDirPath =  toDir.filePath(fileInfo.fileName());
            QDir toDirPathDel(toDirPath);
            if(toDirPathDel.exists()){
                toDirPathDel.removeRecursively();
            }
            if(!cpdir(fileInfo.filePath(),toDirPath,true)){
                return false;
            }
        }else{
            //复制子文件
            if(f && toDir.exists(fileInfo.fileName())){
                toDir.remove(fileInfo.fileName());
            }
            if(!QFile::copy(fileInfo.filePath(),toDir.filePath(fileInfo.fileName()))){
                return false;
            }
            qDebug()<<(tr("复制文件：%1").arg(fileInfo.fileName()));
        }

    }
    return true;
}
