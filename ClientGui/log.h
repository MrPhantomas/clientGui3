#ifndef LOG_H
#define LOG_H

#include <QObject>
#include <QTcpSocket>

#define maxLogSizeDef 1048576
#define maxFilesDef 5

class Log : public QObject
{
    Q_OBJECT
public:
    explicit Log(QString path, QString projectName, int maxLogSize = maxLogSizeDef,
                 int maxFiles = maxFilesDef, QObject *parent = nullptr);
    void createLog(QString message); //обычное логирование
    void createLog(QTcpSocket * socket, QString message);// логирование с указанием ip и порта для Tcp
    void createLog(QString ip, QString port, QString message);// логирование с указанием ip и порта
    friend Log* operator<< (Log* obj, QString msg);//Перегрузка для удобства
private:
    QString curTime();//Функция возвращает текущее время в нужном формате
    void checkLogFile();//Проверка существовния и создание папки для логирования
    QString path;//путь к папке для логирования
    int maxLogSize;//Максимальный размер файла с логами
    int maxFiles;//МАксимальное кол-во файлов с логами
    QString projectName;

    void updateLogs();
signals:

};



#endif // LOG_H
