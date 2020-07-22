#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QColor>
#include <QPalette>
#include <QThread>
#include <QDebug>
#include <QHostAddress>
#include <QVBoxLayout>

MainWindow::MainWindow(char *argv[], QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QStringList lst = QString(argv[0]).split("/");
    client =  new Client(lst[lst.size()-1]);

    connect(&curTimeTimer, SIGNAL(timeout()), this, SLOT(setCurTime()));
    curTimeTimer.start();
    ui->wHeader->setGeometry(10,0,ui->centralwidget->width()-20,headerHight);

    widgetInfoLst = new QVector<widgetInfo*>;
    xmlPath = defaultXMLPath;
    readXML();
}

MainWindow::~MainWindow()
{
    delete ui;
}

//Процедура обработки изменения цвета кнопок
void MainWindow::function()
{
    qDebug()<<"start function";
    QTimer* timer = static_cast<QTimer *>(QObject::sender());

    //Получения индекса кнопки массиве
    int i;
    for(i = 0; i < widgetInfoLst->count() ; i++)
    {
        if(widgetInfoLst->at(i)->tmr->timerId() == timer->timerId())
            break;
    }
    if(i==widgetInfoLst->count())return;

    QHostAddress ip(widgetInfoLst->at(i)->ip);

    QDateTime* lastDtm = client->lastMessageTime(ip);
    int interval;
    if(lastDtm!=nullptr) interval = QDateTime::currentDateTime().toMSecsSinceEpoch() - lastDtm->toMSecsSinceEpoch();
    TInfo* tInfo = client->lastTInfo(ip);
    int tInfoInterval;
    if(tInfo!=nullptr) tInfoInterval = QDateTime::currentDateTime().toMSecsSinceEpoch() - tInfo->lastMessTime.toMSecsSinceEpoch();
    DfInfo* dfInfo = client->lastDfInfo(ip);
    int dfInfoInterval;
    if(dfInfo!=nullptr) dfInfoInterval = QDateTime::currentDateTime().toMSecsSinceEpoch() - dfInfo->lastMessTime.toMSecsSinceEpoch();
    PoInfo* poInfo = client->lastPoInfo(ip);
    int poInfoInterval;
    if(poInfo!=nullptr) poInfoInterval = QDateTime::currentDateTime().toMSecsSinceEpoch() - poInfo->lastMessTime.toMSecsSinceEpoch();

    QLabel* label_3 = static_cast<QLabel *>(widgetInfoLst->at(i)->widget->children().at(2));
    if(tInfo==nullptr?true:tInfoInterval>20000)
        label_3->setStyleSheet("background-color:rgb(137,129,118); \n border-radius:5px;");
    else if(tInfo->t>90)
        label_3->setStyleSheet("background-color:rgb(250,0,0); \n border-radius:5px;");
    else
        label_3->setStyleSheet("background-color:rgb(0,250,0); \n border-radius:5px;");

    QLabel* label_4 = static_cast<QLabel *>(widgetInfoLst->at(i)->widget->children().at(3));
    if(dfInfo==nullptr?true:dfInfoInterval>20000)
        label_4->setStyleSheet("background-color:rgb(137,129,118); \n border-radius:5px;");
    else if(dfInfo->emptyDfSize<1000000)
        label_4->setStyleSheet("background-color:rgb(250,0,0); \n border-radius:5px;");
    else
        label_4->setStyleSheet("background-color:rgb(0,250,0); \n border-radius:5px;");

    QLabel* label_5 = static_cast<QLabel *>(widgetInfoLst->at(i)->widget->children().at(4));
    if(poInfo==nullptr?true:poInfoInterval>20000)
        label_5->setStyleSheet("background-color:rgb(137,129,118); \n border-radius:5px;");
    //!!!!
    //Поменять условие
    //!!!!
    else if(poInfo->poLst.count()<2)
        label_5->setStyleSheet("background-color:rgb(250,0,0); \n border-radius:5px;");
    else
        label_5->setStyleSheet("background-color:rgb(0,250,0); \n border-radius:5px;");


    //Изменение цвета кнопки
    if(lastDtm==nullptr?true:interval>60000)
        widgetInfoLst->at(i)->widget->setStyleSheet("background-color:rgb(120,120,120); \n border-radius:5px;");
    else if(interval>20000)
        widgetInfoLst->at(i)->widget->setStyleSheet("background-color:rgb(200,10,10); \n border-radius:5px;");
    else
        widgetInfoLst->at(i)->widget->setStyleSheet("background-color:rgb(10,200,10); \n border-radius:5px;");
    qDebug()<<"end function";
}

//Чтение xml
void MainWindow::readXML()
{
    QFile file(xmlPath);
    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QXmlStreamReader xmlReader;
        xmlReader.setDevice(&file);
        while(!xmlReader.atEnd())
        {
            if(xmlReader.name() == "catalog" && xmlReader.isStartElement())
            {
                foreach(const QXmlStreamAttribute &attr, xmlReader.attributes())
                {
                    if(attr.name().toString() == "width")
                        this->setFixedWidth(attr.value().toInt());
                    else if(attr.name().toString() == "height")
                        this->setFixedHeight(attr.value().toInt());
                }

                while(!(xmlReader.name() == "catalog" && xmlReader.isEndElement()))
                {
                    if(xmlReader.name() == "object")
                    {
                        int x,y,xsize,ysize;
                        QString name, ip;
                        while(!(xmlReader.name() == "object" && xmlReader.isEndElement()))
                        {
                            if(xmlReader.isStartElement())
                            {
                                if(xmlReader.name() == "x")
                                    x = xmlReader.readElementText().toInt();
                                else if(xmlReader.name() == "y")
                                    y = xmlReader.readElementText().toInt();
                                else if(xmlReader.name() == "xsize")
                                    xsize = xmlReader.readElementText().toInt();
                                else if(xmlReader.name() == "ysize")
                                    ysize = xmlReader.readElementText().toInt();
                                else if(xmlReader.name() == "name")
                                    name = xmlReader.readElementText();
                                else if(xmlReader.name() == "ip")
                                    ip = xmlReader.readElementText();
                            }
                            xmlReader.readNext();
                        }
                        widgetInfo* wInfo = new widgetInfo;
                        wInfo->widget = new QWidget(ui->centralwidget);
                        wInfo->widget->setGeometry(x,y+headerHight,xsize,ysize);
                        QLabel* label_1 = new QLabel(wInfo->widget);
                        label_1->setAlignment(Qt::AlignCenter);
                        label_1->setGeometry(10,10,130,20);
                        label_1->setText(name);
                        QLabel* label_2 = new QLabel(wInfo->widget);
                        label_2->setAlignment(Qt::AlignCenter);
                        label_2->setGeometry(10,50,130,20);
                        label_2->setText(ip);
                        QLabel* label_3 = new QLabel(wInfo->widget);
                        label_3->setAlignment(Qt::AlignCenter);
                        label_3->setGeometry(10,90,30,20);
                        label_3->setText("t");
                        QLabel* label_4 = new QLabel(wInfo->widget);
                        label_4->setAlignment(Qt::AlignCenter);
                        label_4->setGeometry(60,90,30,20);
                        label_4->setText("df");
                        QLabel* label_5 = new QLabel(wInfo->widget);
                        label_5->setAlignment(Qt::AlignCenter);
                        label_5->setGeometry(110,90,30,20);
                        label_5->setText("po");
                        wInfo->tmr = new QTimer;
                        wInfo->tmr->setInterval(2000);
                        connect(wInfo->tmr, SIGNAL(timeout()), this, SLOT(function()));
                        wInfo->tmr->start();
                        wInfo->ip = ip;
                        wInfo->name = name;
                        widgetInfoLst->push_back(wInfo);
                    }
                    xmlReader.readNext();
                }
                xmlReader.readNext();
            }
            xmlReader.readNext();
        }
        ui->wHeader->setGeometry(10,0,490,headerHight);
        //QMessageBox::warning(this,"",QString::number(ui->centralwidget->width()));
    }
    else
    {
        QMessageBox::warning(this,"","файл не открывается");
    }
}

void MainWindow::setCurTime()
{
    ui->curTimer->setText(QTime::currentTime().toString("hh:mm:ss"));
}
