#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("BMS CAN监控上位机 v1.0");
    this->resize(1300, 1000);
    // CAN 报文列表：精确设置每列宽度
    ui->table_can->setColumnWidth(0, 50);    // 序号    窄
    ui->table_can->setColumnWidth(1, 140);   // 时间戳  宽
    ui->table_can->setColumnWidth(2, 90);    // CAN ID
    ui->table_can->setColumnWidth(3, 60);    // 方向
    ui->table_can->setColumnWidth(4, 50);    // DLC
    // 第 6 列（数据 HEX）设为 Stretch，自动填满剩余空间
    ui->table_can->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    refreshSerialPorts();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 扫描可用串口
void MainWindow::refreshSerialPorts()
{
    ui->comboBox_port->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        // 显示 "COM5 - USB Serial Port"，数据存 portName
        ui->comboBox_port->addItem(
            QString("%1 - %2").arg(info.portName(), info.description()),
            info.portName()   // userData 存真实端口名
        );
    }

    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        if (info.vendorIdentifier() == 0x1A86 && info.productIdentifier() == 0x7523) {
            // CH340 芯片的 USB 转串口
            qDebug() << "找到 CH340 设备:" << info.portName();
            ui->text_log->appendPlainText(QString("找到 CH340 设备: %1").arg(info.portName()));
            break;
        }else{
            qDebug() << "未找到可用设备";
            ui->text_log->appendPlainText("未找到可用设备");
        }
    }
}


void MainWindow::on_btn_refrsh_clicked()
{
    refreshSerialPorts();
}
