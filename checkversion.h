#ifndef CHECKVERSION_H
#define CHECKVERSION_H

#include <QObject>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include "globalval.h"


class GlobalVal;
class CheckVersion : public QObject
{
    Q_OBJECT
public:
    explicit CheckVersion(QObject *parent = nullptr,QString remoteUrl=nullptr);
    void requestRemoteVersion();

private:
    QString remoteInfoUrl;
    QString readSysVersion();


private slots:
    void requestRemoteVersionFinished(QNetworkReply *reply);

signals:


};

#endif // CHECKVERSION_H
