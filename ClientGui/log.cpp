#include "log.h"
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <QHostAddress>
Log::Log(QString path, QString projectName, int maxLogSize, int maxFiles, QObject *parent) :
    QObject(parent), path(path), projectName(projectName), maxLogSize(maxLogSize), maxFiles(maxFiles)
{
    checkLogFile();
    qDebug() << "ProjectName = "<<projectName;
}

void Log::createLog(QString message) //обычное логирование
{
    QString time = curTime();
    QFile file(path + "/" + projectName + ".log");

    if(file.open(QIODevice::Append | QIODevice::Text))
    {
        QTextStream fout(&file);
        QString log = "[ " + time + " ] " + message + "\n";
        fout << log;
    }
    file.close();
   if(QFileInfo(path + "/" + projectName + ".log").size() >= maxLogSize)
       updateLogs();

}

void Log::createLog(QTcpSocket * socket, QString message) // логирование с указанием ip и порта для Tcp
{
    createLog(" " + socket->peerAddress().toString() + " port " + QString::number(socket->peerPort()) + " " + message);
}

void Log::createLog(QString ip, QString port, QString message) // логирование с указанием ip и порта
{
    QString time = curTime();
    QFile file(path + "/" + projectName + ".log");

    if(file.open(QIODevice::Append | QIODevice::Text))
    {
        QTextStream fout(&file);
        QString log = "[ " + time + " ] " + ip + " port " + port + " " + message + "\n";
        fout << log;
    }
    file.close();
    if(QFileInfo(path + "/" + projectName + ".log").size() >= maxLogSize)
        updateLogs();
}

void Log::checkLogFile() //Проверка существовния и создание папки для логирования
{
    if(!QDir(path).exists())
    {
        QDir().mkdir(path);
    }
    if(!QFile(path + "/" + projectName + ".log").exists())
    {
        QFile file(path + "/" + projectName + ".log");
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        file.close();
    }
}

QString Log::curTime() //Функция возвращает текущее время в нужном формате
{
    return QDateTime::currentDateTime().toString("dd:MM:yyyy hh:mm:ss.zzz");
}


void Log::updateLogs()
{
    QDirIterator itr(path, QStringList()<<(projectName + ".log.*"), QDir::Files);
    while(itr.hasNext())
    {
        QFileInfo fileInf(itr.next());
        bool ok;
        int num = fileInf.suffix().toInt(&ok);
        if(!ok)
            QFile(itr.filePath()).remove();
        else if(num>maxFiles-1 || num<=0)
            QFile(itr.filePath()).remove();
    }

    for(int i = maxFiles-1;i;--i)
        QFile(path + "/" + projectName + ".log." + QString::number(i)).rename(path + "/" + projectName + ".log." + QString::number(i+1));

    QFile(path + "/" + projectName + ".log").rename(path + "/" + projectName + ".log.1");
    QFile file(path + "/" + projectName + ".log");
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.close();
}

Log* operator<< (Log* obj, QString msg) //Перегрузка для удобства
{
    obj->createLog(msg);
    return obj;
}
