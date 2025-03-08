#include "globalval.h"
#include <QDir>


QString GlobalVal::newVersion;
QString  GlobalVal::programRootDir = QDir::currentPath();
QString GlobalVal::zipurl = "";
QJsonArray GlobalVal::fileList;
int GlobalVal::updateTtype;
QString GlobalVal::mainAppName = "";

