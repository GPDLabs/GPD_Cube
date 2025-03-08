#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <QtNetwork>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QFile>
#include <QObject>
#include <QEventLoop>

#include <memory>

QT_BEGIN_NAMESPACE
class QFile;

class QNetworkReply;

QT_END_NAMESPACE


class Download : public QObject //QDialog
{
    Q_OBJECT

public:
    Download(QObject *parent = nullptr);
    ~Download();

    void downloadFile(QUrl url,QString downloadDirectory);
    static QString urlEncode(QString url);
    QString downloadProgress;
    void resetStatus();

private:
     QUrl url;
     QNetworkAccessManager *manager;
     QNetworkReply *reply;
     std::unique_ptr<QFile> file;
     bool httpRequestAborted;
     std::unique_ptr<QFile> openFileForWrite(const QString &fileName);
     QString originFileName;

private slots:
    void startRequest(const QUrl &requestedUrl);
    void cancelDownload();
    void httpFinished();
    void httpReadyRead();
    void httpError(QNetworkReply::NetworkError error);
    void networkReplyProgress(qint64 bytesRead, qint64 totalBytes);

signals:


};

#endif // DOWNLOAD_H
