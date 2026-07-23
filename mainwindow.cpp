#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("BMS CAN监控上位机 v1.0");
    this->resize(1100, 1000);
    // CAN 报文列表：精确设置每列宽度
    ui->table_can->setColumnWidth(0, 50);    // 序号    窄
    ui->table_can->setColumnWidth(1, 140);   // 时间戳  宽
    ui->table_can->setColumnWidth(2, 90);    // CAN ID
    ui->table_can->setColumnWidth(3, 60);    // 方向
    ui->table_can->setColumnWidth(4, 50);    // DLC
    // 第 6 列（数据 HEX）设为 Stretch，自动填满剩余空间
    ui->table_can->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
}

MainWindow::~MainWindow()
{
    delete ui;
}

