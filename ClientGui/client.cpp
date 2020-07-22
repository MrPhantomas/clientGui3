#include "client.h"

#include <QByteArray>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QHostAddress>
#include <QThread>
#include <QTimer>
#include <QDataStream>

#define NEWDATA 1
#define RESENDDATA 2
#define CONNECTIONRESPONSE 3
#define CONNECTIONASK 5

#define GETTINFO 6
#define GETDFINFO 7
#define GETPOINFO 8

Client::Client(QString projectName,QObject *parent) :
    QObject(parent), projectName(projectName)
{
    //Получение название ini файла
    initPath = projectName + iniPath;
    //Создания класса для считывания настроек
    settings = new QSettings(initPath, QSettings::IniFormat);
    //Иницифализция
    initialization();
    lg->createLog("daemon client start");
    //Создания листа данных соединений
    lst = new QList<connections*>;
    _udpSocket = new QUdpSocket(this);
    _udpSocket->bind(QHostAddress::Any, defUdpCLientPort);
    broadcastTimer = new QTimer();
    broadcastTimer->setInterval(2000);
    connect(broadcastTimer, SIGNAL(timeout()), this, SLOT(broadcastUdpResponse()));
    broadcastTimer->start();
    connect(_udpSocket, SIGNAL(readyRead()), this, SLOT(onReadyReadUdp()));
}

//Перегрузки для удобства
void operator>> (QDataStream& obj, tcpStruct& strct)
{
    obj>>strct.command>>strct.dataSize>>strct.data>>strct.end;
}

void operator>> (QDataStream& obj, udpStruct& strct)
{
    obj>>strct.command>>strct.shortInfo>>strct.numPacket>>strct.packets>>strct.dataSize>>strct.data>>strct.end;
}

void operator<< (QDataStream& obj, udpStruct& strct)
{
    obj<<strct.command<<strct.shortInfo<<strct.numPacket<<strct.packets<<strct.dataSize<<strct.data<<strct.end;
}

void operator>> (QDataStream& obj, udpStruct* strct)
{
    obj>>strct->command>>strct->shortInfo>>strct->numPacket>>strct->packets>>strct->dataSize>>strct->data>>strct->end;
}

void operator<< (QDataStream& obj, udpStruct* strct)
{
    obj<<strct->command<<strct->shortInfo<<strct->numPacket<<strct->packets<<strct->dataSize<<strct->data<<strct->end;
}

void Client::initialization() // получение настроек
{
    //Создание папки etc
    if(!QDir("etc").exists())
    {
        QDir().mkdir("etc");
    }
    //Проверка наличия .ini файла
    if(!QFile(initPath).exists())
    {
        createIni();
        initAsDefault();
        return;
    }
    //Считывание данных из файла .ini
    bool ok_2, ok_3, ok_4;
    QString imagePath_ = settings->value("imagePath", "").toString();// папка с картинками
    QString logPath_ = settings->value("logPath", "").toString();// папка для логирования
    int upd_ = settings->value("udp", -1).toInt(&ok_2);// флаг использования UDP
    int maxLogSize_ = settings->value("maxLogSize", -1).toInt(&ok_3);
    int maxFiles_ = settings->value("maxFiles", -1).toInt(&ok_4);

    if( !ok_2 || !ok_3 || !ok_3 || imagePath_ == ""
            || logPath_ == "" || maxLogSize_ == -1 || maxFiles_ == -1)
    {
        //если какая то переменная в файле не верна
        qDebug() << ".init file corrupted... initializate as default";
        createIni();
        initAsDefault();
    }
    else
    {
        imagePath = imagePath_;
        logPath = logPath_;
        udp = upd_;
        lg = new Log(logPath, projectName, maxLogSize_, maxFiles_);
        qDebug() << "initialization succes";
        qDebug() << "maxLogSize = "<<maxLogSize_;
        qDebug() << "maxFiles = "<<maxFiles_;
        qDebug() << "image path = "<<imagePath;
        qDebug() << "log path = "<<logPath;
        qDebug() << "use "<<(udp?"UPD":"TCP")<<" socket";
    }
}

void Client::createIni() // создание файла настроек
{
    settings->setValue("imagePath", QString(imagePathDef));
    settings->setValue("logPath", QString(logPathDef));
    settings->setValue("udp", QString::number(udpDef));
    settings->setValue("maxLogSize", QString::number(maxLogSizeDef));
    settings->setValue("maxFiles", QString::number(maxFilesDef));
}

void Client::initAsDefault() // инициализация по умолчанию
{
    qDebug() << "initialization with default configuration";
    imagePath = imagePathDef;
    if(!QDir(imagePath).exists())
        QDir().mkdir(imagePath);
    logPath = logPathDef;
    udp = udpDef;
    lg = new Log(logPath, projectName);
    qDebug() << "path = "<<imagePath;
    qDebug() << "log path = "<<logPath;
    qDebug() << "use "<<(udp?"UPD":"TCP")<<" socket";
}

void Client::onReadyReadTcp() // обработчик TCP
{
    //Сокет отправивший сигнал
    QTcpSocket* socket = static_cast<QTcpSocket *>(QObject::sender());
    //Получение индекса в списке с данными этого соединения
    int i = connectionsIndex(socket);
    //если индекс не найден (-1) то выходим из обработчика
    if(!(i+1)) return;
    //Записываем дату и ремя последнего сообщения
    lst->at(i)->lastMessTime = QDateTime::currentDateTime();
    //считываем данные
    QByteArray tmpMessage;
    while(socket->bytesAvailable() || socket->waitForReadyRead(10000))
    {
        tmpMessage.append(socket->readAll());
        QThread::msleep(5);
    }
    //Получение комманды сообщения
    quint8 command;
    QDataStream obj1(&tmpMessage, QIODevice::ReadOnly);
    obj1>>command;

    switch(command)
    {
        case NEWDATA:
        {
            //Если имеются остатки недособранного сообщения то удаляем их
            if(!lst->at(i)->tcpstrct) delete lst->at(i)->tcpstrct;
            //Парсим команду
            tcpStruct strct;
            QDataStream obj(&tmpMessage, QIODevice::ReadOnly);
            obj>>strct;

            if(strct.dataSize && quint64(strct.data.size())<strct.dataSize)
            {
                //Если размер данных не соотвествует
                lst->at(i)->tcpstrct = new tcpStruct(strct);
                //Отправляем запрос на переотправку данных с точки разрыва
                QByteArray message;
                QDataStream out(&message, QIODevice::WriteOnly);
                out<<quint8(RESENDDATA)<<strct.dataSize<<quint64(strct.data.size())<<QString("EOC");
                socket->write(message);
                socket->waitForBytesWritten();
                qDebug() << "Получен неполный пакет ждём ответа от сервера";
            }
            else if(strct.dataSize && quint64(strct.data.size())==strct.dataSize && strct.dataSize>0)
            {
                //Если размер данных соотвествует
                parseDataNew(strct.data);
                qDebug() << "Данные успешно считанны";
            }
            break;
        }

        case RESENDDATA:
        {
            //Если предыдущая часть не сохранена то выходим
            if(!lst->at(i)->tcpstrct) break;
            //Парсим команду
            tcpStruct strct;
            QDataStream obj(&tmpMessage, QIODevice::ReadOnly);
            obj>>strct;

            if(strct.dataSize && quint64(strct.data.size())<strct.dataSize)
            {
                //Если размер данных не соотвествует
                lst->at(i)->tcpstrct->data.append(strct.data);
                //Отправляем запрос на переотправку данных с точки разрыва
                QByteArray message;
                QDataStream out(&message, QIODevice::WriteOnly);
                out<<quint8(RESENDDATA)<<lst->at(i)->tcpstrct->dataSize<<quint64(strct.data.size() + lst->at(i)->tcpstrct->data.size())<<QString("EOC");
                socket->write(message);
                socket->waitForBytesWritten();
                qDebug() << "Получен неполный пакет ждём ответа от сервера";
            }
            else if(strct.dataSize && quint64(strct.data.size())==strct.dataSize && strct.dataSize>0)
            {
                //Если размер данных соотвествует
                lst->at(i)->tcpstrct->data.append(strct.data);
                parseDataNew(lst->at(i)->tcpstrct->data);
                delete lst->at(i)->tcpstrct;
                qDebug() << "Данные успешно считанны";
            }
            break;
        }

        default:
        {
            break;
        }
    }
}

void Client::onReadyReadUdp()//Обработчки UDP
{
    qDebug()<<"onReadyReadUdp start";
    //Хост сервера
    QHostAddress senderHost;
    //Порт сервера
    quint16 senderPort;
    QByteArray tmpMessage;
    tmpMessage.resize(_udpSocket->pendingDatagramSize());
    //Получаем сообщение
    _udpSocket->readDatagram(tmpMessage.data(), tmpMessage.size(), &senderHost, &senderPort);

    //Получение комманды сообщения
    quint8 command;
    QDataStream obj1(&tmpMessage, QIODevice::ReadOnly);
    obj1>>command;


    switch(command)
    {
        case NEWDATA:
        {
        //НЕДОДЕЛАНО
//            udpStruct *strct = new udpStruct;
//            QDataStream obj(&tmpMessage, QIODevice::ReadOnly);
//            obj>>strct;

        //            if(strct->dataSize == quint64(strct->data.size()))
        //            {
        //                QByteArray tmpAsk;
        //                udpStruct strctAsk;
        //                QDataStream out(&tmpAsk, QIODevice::WriteOnly);
        //                strctAsk.command = 1;
        //                strctAsk.packets = 1;
        //                strctAsk.filename = "ask";
        //                strctAsk.numPacket = 1;
        //                out<<strctAsk;
        //                _udpSocket->writeDatagram(tmpAsk, tmpAsk.size(), senderHost, senderPort);

        //                int i;
        //                for(i = 0;i<lst->size();i++)
        //                {
        //                    if(lst->at(i)->adress == senderHost && lst->at(i)->port == senderPort)
        //                        break;
        //                }

        //                if(lst->empty()?false:lst->at(i)->datagramms->empty()?
        //                        true:lst->at(i)->datagramms->back()->numPacket+1==strct->numPacket)
        //                {
        //                    lst->at(i)->datagramms->push_back(strct);
        //                    if(strct->numPacket == strct->packets)
        //                    {
        //                        QByteArray itog;
        //                        while(!lst->at(i)->datagramms->empty())
        //                        {
        //                            itog.append(lst->at(i)->datagramms->front()->data);
        //                            lst->at(i)->datagramms->pop_front();
        //                        }
        //                        parseDataNew(itog);
        //                    }
        //                }
        //                else delete strct;
        //            }
        //            else delete strct;
        //            break;
        }

        case RESENDDATA:
        {
            break;
        }

        case GETTINFO:
        {
            int i = connectionsIndex(senderHost, udp?defUdpServerPort:defTcpServerPort);
            if(!(i+1)) return;
            udpStruct *strct = new udpStruct;
            QDataStream obj(&tmpMessage, QIODevice::ReadOnly);
            obj>>strct;
            if(strct->dataSize == quint64(strct->data.size()) && strct->end == ENDDATA)
            {
                QDataStream objData(&strct->data, QIODevice::ReadOnly);
                objData>>lst->at(i)->tInfo->t;
                lst->at(i)->tInfo->lastMessTime = QDateTime::currentDateTime();
            }
            lst->at(i)->lastMessTime = QDateTime::currentDateTime();
            break;
        }

        case GETDFINFO:
        {
            int i = connectionsIndex(senderHost, udp?defUdpServerPort:defTcpServerPort);
            if(!(i+1)) return;
            udpStruct *strct = new udpStruct;
            QDataStream obj(&tmpMessage, QIODevice::ReadOnly);
            obj>>strct;
            if(strct->dataSize == quint64(strct->data.size()) && strct->end == ENDDATA)
            {
                QDataStream objData(&strct->data, QIODevice::ReadOnly);
                objData>>lst->at(i)->dfInfo->emptyDfSize;
                lst->at(i)->dfInfo->lastMessTime = QDateTime::currentDateTime();
            }
            lst->at(i)->lastMessTime = QDateTime::currentDateTime();
            break;
        }

        case GETPOINFO:
        {
            int i = connectionsIndex(senderHost, udp?defUdpServerPort:defTcpServerPort);
            if(!(i+1)) return;
            udpStruct *strct = new udpStruct;
            QDataStream obj(&tmpMessage, QIODevice::ReadOnly);
            obj>>strct;
            if(strct->dataSize == quint64(strct->data.size()) && strct->end == ENDDATA)
            {
                QDataStream objData(&strct->data, QIODevice::ReadOnly);
                objData>>lst->at(i)->poInfo->poLst;
                lst->at(i)->poInfo->lastMessTime = QDateTime::currentDateTime();
            }
            lst->at(i)->lastMessTime = QDateTime::currentDateTime();
            break;
        }

        case CONNECTIONASK:
        {
            //Если сервер уже есть в списках то вызодим
            qDebug()<<"host = "<<senderHost<<" port = "<<senderPort;
            int i = connectionsIndex(senderHost, udp?defUdpServerPort:defTcpServerPort);
            if(i+1)
            {
                lst->at(i)->lastMessTime = QDateTime::currentDateTime();
                return;
            }
            //Парсим команду
            udpStruct strct;
            QDataStream obj(&tmpMessage, QIODevice::ReadOnly);
            obj>>strct;
            //Создаём новую структуру для нового чоединения
            connections *conStrct = new connections;
            conStrct->port = udp?defUdpServerPort:defTcpServerPort;
            conStrct->adress = senderHost;
            conStrct->lastMessTime = QDateTime::currentDateTime();
            conStrct->tInfo = new TInfo;
            conStrct->dfInfo = new DfInfo;
            conStrct->poInfo = new PoInfo;
            if(!udp)
            {
                //Выделяем сокет сервера
                conStrct->server = new QTcpSocket();
                //Создаём таймер отвечающий за подключение
                conStrct->timer = new QTimer();
                conStrct->timer->setInterval(1000);
                connect(conStrct->timer, SIGNAL(timeout()), this, SLOT(callServer()));
                connect(conStrct->server, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this,
                        SLOT(stateChanged(QAbstractSocket::SocketState)));
                connect(conStrct->server, SIGNAL(readyRead()), this, SLOT(onReadyReadTcp()));
                conStrct->server->connectToHost(conStrct->adress, conStrct->port);
            }
            lst->push_back(conStrct);
            //qDebug() <<"Нужный хост = "<<senderHost.toString()<<" нужный порт = "<<QString::number(senderPort);
            break;
        }

        default:
        {
            break;
        }
    }
    qDebug()<<"onReadyReadUdp end";
}

//Процедура обработки bmp данных
void Client::parseDataNew(QByteArray bmpArray)
{
    QDataStream in(&bmpArray, QIODevice::ReadOnly);
    while(!in.atEnd())
    {
        QString fileName;
        quint64 fileSize;
        QByteArray tmpArray;

        in>>fileName>>fileSize>>tmpArray;
        qDebug() << "img - "<<fileName<<" size("<<QString::number(fileSize)<<") totalsize("<<tmpArray.size()<<")";
        lg->createLog(" find " + fileName + " size " + QString::number(fileSize) + " totalsize "  + QString::number(tmpArray.size())
                      +(tmpArray.size() == fileSize ?" Success" : " Failed" ) );

        if(tmpArray.size() == fileSize)
        {
            QFile file(imagePath+"/"+fileName);
            if(file.open(QIODevice::WriteOnly))
                file.write(tmpArray);
            file.close();
        }
        QThread::msleep(5);
        if(!fileSize) break;
    }
}

//Процедура прозвона сервера в случае его падения
void Client::callServer()
{
    qDebug()<<"callServer start";
    //Сокет отправивший сигнал
    QTcpSocket* socket = static_cast<QTcpSocket *>(QObject::sender());
    //Получение индекса в списке с данными этого соединения
    //int i = connectionsIndex(socket->peerAddress(), socket->peerPort());
    int i = connectionsIndex(socket);
    //если индекс не найден (-1) то выходим из обработчика
    if(!(i+1)) return;
    //Попытка подсоединения к серверу
    socket->connectToHost(lst->at(i)->adress, lst->at(i)->port);
    qDebug()<<"callServer end";
}

//Процедура обработки изменения состояния TCP сокета
void Client::stateChanged(QAbstractSocket::SocketState stat)
{
    //Сокет отправивший сигнал
    QTcpSocket* socket = static_cast<QTcpSocket *>(QObject::sender());
    //Получение индекса в списке с данными этого соединения
    int i = connectionsIndex(socket);
    //если индекс не найден (-1) то выходим из обработчика
    if(!(i+1)) return;

    if (stat == QAbstractSocket::UnconnectedState)
    {
        //Если сокет отключился
        lst->at(i)->timer->start();
    }
    else if(stat == QAbstractSocket::ConnectedState)
    {
        //Если сокет подключился
        lst->at(i)->timer->stop();
        qDebug() << "Succes connect to server";
    }

}

//Запрос для получения данных сервера
void Client::broadcastUdpResponse()
{
    QByteArray tmpMessage;
    QDataStream obj(&tmpMessage, QIODevice::WriteOnly);
    obj<<quint8(CONNECTIONRESPONSE)<<QString(ENDDATA);
    _udpSocket->writeDatagram(tmpMessage, tmpMessage.size(), QHostAddress::Broadcast, defUdpServerPort);
    _udpSocket->waitForBytesWritten();
}

TInfo* Client::lastTInfo(QHostAddress ip, quint16 port)
{
    for(int i=0;i<lst->count();i++)
        if(QString::number(lst->at(i)->adress.toIPv4Address()) == QString::number(ip.toIPv4Address()) && lst->at(i)->port == port)
        {
            if((lst->at(i)->tInfo->t))
                return (lst->at(i)->tInfo);
            else
                return nullptr;
        }
    return nullptr;
}

DfInfo* Client::lastDfInfo(QHostAddress ip, quint16 port)
{
    for(int i=0;i<lst->count();i++)
        if(QString::number(lst->at(i)->adress.toIPv4Address()) == QString::number(ip.toIPv4Address()) && lst->at(i)->port == port)
        {
            //!!!!
            //НЕ совсем верное условие (может реально 0 байт будет...)
            //!!!!
            if((lst->at(i)->dfInfo->emptyDfSize))
                return (lst->at(i)->dfInfo);
            else
                return nullptr;
        }
    return nullptr;
}

PoInfo* Client::lastPoInfo(QHostAddress ip, quint16 port)
{
    for(int i=0;i<lst->count();i++)
        if(QString::number(lst->at(i)->adress.toIPv4Address()) == QString::number(ip.toIPv4Address()) && lst->at(i)->port == port)
        {
            if((lst->at(i)->poInfo->poLst.count()))
                return (lst->at(i)->poInfo);
            else
                return nullptr;
        }
    return nullptr;
}

//Функция получения времени последнего сообщения
QDateTime* Client::lastMessageTime(QHostAddress ip, quint16 port)
{
    for(int i=0;i<lst->count();i++)
        if(QString::number(lst->at(i)->adress.toIPv4Address()) == QString::number(ip.toIPv4Address()) && lst->at(i)->port == port)
            return &(lst->at(i)->lastMessTime);
    return nullptr;
}

//Процедура получение индекса в списке с данными этого соединения
int  Client::connectionsIndex(QTcpSocket* socket)
{
    if(lst->count())
    {
        for(int i = 0;i<lst->count();i++)
        {
//            if(lst->at(i)->adress.toIPv4Address() == socket->peerAddress().toIPv4Address()
//                    && lst->at(i)->port == socket->peerPort())
            if(lst->at(i)->server == socket)
                return i;
        }
    }
    return -1;
}
//Процедура получение индекса в списке с данными этого соединения
int  Client::connectionsIndex(QHostAddress adress, quint16 port)
{
    if(lst->count())
    {
        for(int i = 0;i<lst->count();i++)
        {
            if(lst->at(i)->adress.toIPv4Address() == adress.toIPv4Address()
                    && lst->at(i)->port == port)
                return i;
        }
    }
    return -1;
}
