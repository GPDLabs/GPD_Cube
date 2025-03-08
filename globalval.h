#ifndef GLOBALVAL_H
#define GLOBALVAL_H

#include <QString>
#include <QJsonArray>


class GlobalVal
{
public:
    static QString newVersion;//新版本号
    static QString programRootDir;//程序根目录
    static QString zipurl;//压缩包地址,updateTtype为2时
    static QJsonArray fileList;//待更新文件列表信息,updateTtype为1时
    static int updateTtype;//更新类型：1更新指定文件，2以压缩包形式更新
    static QString mainAppName;//主程序名称，更新完毕后将启动主程序
};

#endif // GLOBALVAL_H
