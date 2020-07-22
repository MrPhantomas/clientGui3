#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QDebug>
#include <QByteArray>
#include <QDataStream>
#include <QString>
#include <QUdpSocket>
#include "log.h"
#include <QSettings>
#include <QTimer>
#include <QTime>
#include <QDateTime>

#include "clientStruct.h"

//Порт udp сервера по умолчанию
#define defUdpServerPort 50002
//Порт tcp сервера по умолчанию
#define defTcpServerPort 50003
//Порт udp клиента по умолчанию
#define defUdpCLientPort 50001
//Порт tcp клиента по умолчанию
#define defTcpCLientPort 50004

//Папка с картинками по умолчанию
#define imagePathDef "Images"
//Папка для логирования по умолчанию
#define logPathDef "log"
//Флаг работы через udp порт
#define udpDef 0
//Хост по умолчанию ( не используется )
#define iniPath ".ini"
#define ENDDATA "EOC"

//Структура комманд для tcp сервера
struct tcpStruct
{
    tcpStruct(tcpStruct& strct)
    {
        command = strct.command;
        dataSize = strct.dataSize;
        data = strct.data;
        end = strct.end;
    }
    tcpStruct(){};
    //номер комнады
    qint8 command;
    //размер блока данных
    quint64 dataSize;
    //Блок данных
    QByteArray data;
    //Окончание блока данных
    QString end = ENDDATA;
};
//Структура комманд для udp сервера
struct udpStruct
{
    udpStruct(udpStruct& strct)
    {
        command = strct.command;
        shortInfo = strct.shortInfo;
        numPacket = strct.numPacket;
        packets = strct.packets;
        dataSize = strct.dataSize;
        data = strct.data;
        end = strct.end;
    }
    udpStruct(){};
    //Номер команды
    qint8 command;
    //Название файла
    QString shortInfo;
    //Номер пакета
    quint32 numPacket;
    //Количество пакетов
    quint32 packets;
    //Размер блока данных
    quint64 dataSize;
    //БЛок данных
    QByteArray data;
    //Окончание блока данных
    QString end = ENDDATA;
};

//Перегрузки для удобства
void operator>> (QDataStream& obj, tcpStruct& strct);
void operator>> (QDataStream& obj, udpStruct& strct);
void operator<< (QDataStream& obj, udpStruct& strct);
void operator>> (QDataStream& obj, udpStruct* strct);
void operator<< (QDataStream& obj, udpStruct* strct);

//Структура хранения данных о серверах
struct connections
{
    //Адресс хоста
    QHostAddress adress;
    //Порт хоста
    quint16 port = 0;
    //Tcp сокет (используется если сервер работает черс tcp)
    QTcpSocket * server = 0;
    //Таймер для запроса подключения в случае падения сервера
    QTimer *timer = 0;
    //Список хранения недособранных пакетов
    QList<udpStruct*>* datagramms = 0;
    //Недособранный TCP пакет
    tcpStruct* tcpstrct = 0;
    //Дата и время последнего сообщения
    QDateTime lastMessTime;

    TInfo* tInfo = nullptr;
    DfInfo* dfInfo = nullptr;
    PoInfo* poInfo = nullptr;
};



class Client : public QObject
{
    Q_OBJECT
public:
    explicit Client(QString projectName, QObject *parent = nullptr);
    //Функция получения времени последнего сообщения сервера по ip и порту
    QDateTime* lastMessageTime(QHostAddress ip, quint16 port = defTcpServerPort);
    TInfo* lastTInfo(QHostAddress ip, quint16 port = defTcpServerPort);
    DfInfo* lastDfInfo(QHostAddress ip, quint16 port = defTcpServerPort);
    PoInfo* lastPoInfo(QHostAddress ip, quint16 port = defTcpServerPort);

signals:

public slots:
    //Обработчик tcp сообщений
    void onReadyReadTcp();
    //Обработчик udp сообщений
    void onReadyReadUdp();
    //Слот для попыток подключения при падении tcp сервера
    void callServer();
    //Слот смены состоянийй tcp сокета
    void stateChanged(QAbstractSocket::SocketState stat);
    //Слот широковещательного udp запроса для нахождения серверов
    void broadcastUdpResponse();

private:
    //TCP cокет
    //QTcpSocket *_socket;

    //UDP сокет
    QUdpSocket *_udpSocket;
    //Таймер отвечающий за широковещательные udp рассылки
    QTimer *broadcastTimer;
    //Название ini файла
    QString initPath;
    //Используемый порт
    quint16 port;
    //Адрес папки с картинками
    QString imagePath;
    //Адрес папки логирования
    QString logPath;
    //ip адресс хоста
    QString tergetTcpHost;
    //Флаг использования udp
    bool udp;
    //Указатель на класс логирования
    Log *lg;
    //Название проекта
    QString projectName;
    //Класс настроек
    QSettings* settings;
    //Список информации об используемых сокетах
    QList<connections*>* lst;

    //инициализация
    void initialization();
    //инициализация в случае отсутствия или повреждения ini файла
    void initAsDefault();
    //Создание ini файла в случае его отсутствия или повреждения
    void createIni();
    //Процедура обработки bmp данных
    void parseDataNew(QByteArray bmpArray);
    //Процедура получение индекса в списке с данными этого соединения
    int  connectionsIndex(QTcpSocket* socket);
    //Процедура получение индекса в списке с данными этого соединения
    int  connectionsIndex(QHostAddress adress, quint16 port);

signals:

};





#endif // CLIENT_H
