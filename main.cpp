#include <QCoreApplication>
#include "qrserver.h"
#include <iostream>

QMutex mutex;
void outputLog(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    mutex.lock();

    QString text;
    switch(type)
    {
    case QtDebugMsg:
        text = QString("Debug:");
        break;

    case QtInfoMsg:
        text = QString("Info:");
        break;

    case QtWarningMsg:
        text = QString("Warning:");
        break;

    case QtCriticalMsg:
        text = QString("Critical:");
        break;

    case QtFatalMsg:
        text = QString("Fatal:");

    default:
        break;
    }

    QString context_info = QString("[%1 %2]:").arg(QString(context.function)).arg(context.line);//文件名,所在行数
    QString current_datetime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString current_date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    QString message = QString("%1 %2 %3").arg(current_datetime).arg(text).arg(msg);//.arg(context_info).arg(msg);

    QString logName = QString("QR-log_%1.txt").arg(current_date);
    QFile file(logName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream text_stream(&file);
        text_stream << message << "\r\n";
        file.flush();
        file.close();
    } else {
        // 如果无法打开日志文件，则将错误信息输出到标准错误
        QTextStream stderrStream(stderr);
        stderrStream << "致命：无法打开日志文件：" << logName << "\n";
    }

    // 将消息写入到应用程序输出中
    QStringList pathList = QString(context.file).split("\\");
    int pathi = pathList.count();
    QString curFileName;
    if(pathi >= 3){
        curFileName = pathList.at(2);
    }else{
        curFileName = pathList.at(0);
    }
    QTextStream console(stdout);
    console << QString("[%1:%2]: ").arg(curFileName).arg(context.line) << msg << "\n";

    mutex.unlock();
}
// 信号处理器函数
void signalHandler(int signal) {
    std::fprintf(stderr, "程序被强制结束，信号编号：%d\n", signal);
    digitalWrite(0, HIGH);
    digitalWrite(2, LOW);
    digitalWrite(3, LOW);
    mutex.lock();

    // 构造信号消息
    QString message = QString("程序被强制结束，信号编号：%1\n").arg(signal);
    QString current_datetime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString current_date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    QString logMessage = QString("%1 %2").arg(current_datetime).arg(message);

    QString logName = QString("QR-log_%1.txt").arg(current_date);
    QFile file(logName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream text_stream(&file);
        text_stream << logMessage;
        file.flush();
        file.close();
    } else {
        // 如果无法打开日志文件，则将错误信息输出到标准错误
        QTextStream stderrStream(stderr);
        stderrStream << "致命：无法打开日志文件：" << logName << "\n";
    }

    mutex.unlock();

    // 终止程序
    _Exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(outputLog); // 安装消息处理程序

    QCoreApplication a(argc, argv);

    // 安装信号处理器
    signal(SIGINT, signalHandler); // 处理Ctrl+C
    signal(SIGTERM, signalHandler); // 处理终止信号
    signal(SIGABRT, signalHandler);
    signal(SIGFPE, signalHandler);
    signal(SIGILL, signalHandler);
    signal(SIGSEGV, signalHandler);
    signal(SIGBUS, signalHandler);
    signal(SIGQUIT, signalHandler);

    wiringPiSetup();
    pinMode(0, OUTPUT);//红灯
    pinMode(2, OUTPUT);//黄灯
    pinMode(3, OUTPUT);//绿灯
    digitalWrite(0, LOW);
    digitalWrite(2, HIGH);
    digitalWrite(3, LOW);

    QRServer *server = new QRServer();
    server->startServer();

    return a.exec();
}
