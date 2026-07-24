#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHBoxLayout>
#include <QDateTime>
#include <QTableWidgetItem>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , serial(new QSerialPort())
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("BMS CAN监控上位机 v1.0");
    setDotStatus(ui->label_link_dot, false);
    this->resize(1300, 1000);
    ui->btn_close->setEnabled(false);
    // CAN 报文列表：精确设置每列宽度
    ui->table_can->setColumnWidth(0, 50);    // 序号    窄
    ui->table_can->setColumnWidth(1, 140);   // 时间戳  宽
    ui->table_can->setColumnWidth(2, 90);    // CAN ID
    ui->table_can->setColumnWidth(3, 60);    // 方向
    ui->table_can->setColumnWidth(4, 50);    // DLC
    // 第 6 列（数据 HEX）设为 Stretch，自动填满剩余空间
    ui->table_can->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    // 清空 UI 预设的空行, 修正表头 (把 "CND ID" 改为 "帧序号")
    ui->table_can->setRowCount(0);
    ui->table_can->setHorizontalHeaderLabels(
        {"序号", "时间戳", "帧序号", "方向", "DLC", "数据(HEX)"});
    ui->table_can->verticalHeader()->setDefaultSectionSize(22);  // 行高 22px
    refreshSerialPorts();

    // 创建连接计时器（每秒触发，仅刷新一个 label，开销极小，不会卡顿）
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::onLinkTimeout);

    // 建立串口接收连接：底层收到字节时发出 readyRead 信号 → 触发接收槽
    // 放在构造函数只连一次，避免每次打开都重复连接导致收到重复数据
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::onSerialDataReady);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 状态显示切换
void MainWindow::setDotStatus(QLabel* dot, bool online)
{
    dot->setProperty("status", online ? "on" : "off");
    dot->style()->unpolish(dot);
    dot->style()->polish(dot);
    dot->update();
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

// 刷新串口
void MainWindow::on_btn_refrsh_clicked()
{
    refreshSerialPorts();
}

void MainWindow::on_btn_open_clicked()
{
    QString port = ui->comboBox_port->currentData().toString();
    qint32 baud = ui->comboBox_baud->currentText().toInt();
    serial->setPortName(port);
    serial->setBaudRate(baud);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if(serial->open(QIODevice::ReadWrite)){
        ui->label_link_status->setText("已连接");
        setDotStatus(ui->label_link_dot, true);   // 圆点变绿
        // 启动连接计时
        m_elapsed.start();          // 记录连接起始时刻
        m_timer->start();           // 启动每秒定时器
        onLinkTimeout();            // 立即刷新一次显示
        ui->text_log->appendPlainText(QString("%1打开成功").arg(port));
        ui->btn_open->setEnabled(false);
        ui->btn_close->setEnabled(true);
        ui->comboBox_port->setEnabled(false);
        ui->comboBox_baud->setEnabled(false);
    }else{
        QString err = serial->errorString();
        ui->text_log->appendPlainText(QString("打开失败%1").arg(err));
    }
}

void MainWindow::on_btn_close_clicked()
{
    serial->close();

    ui->btn_open->setEnabled(true);
    ui->btn_close->setEnabled(false);
    ui->comboBox_port->setEnabled(true);
    ui->comboBox_baud->setEnabled(true);
    ui->text_log->appendPlainText("已断开连接");
    ui->label_link_status->setText("未连接");
    setDotStatus(ui->label_link_dot, false);   // 圆点变红
    m_timer->stop();   // 停止连接计时（保留当前显示时长）
}

// 计时器超时槽：计算已连接时长并刷新 label_s 显示
void MainWindow::onLinkTimeout()
{
    qint64 ms = m_elapsed.elapsed();   // 距连接开始的毫秒数
    int totalSec = ms / 1000;
    int h   = totalSec / 3600;
    int m   = (totalSec % 3600) / 60;
    int sec = totalSec % 60;
    ui->label_s->setText(QString("连接：%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(sec, 2, 10, QChar('0')));
}

// ============================================================
// 串口接收：底层有数据到达时（readyRead 信号）自动调用
// ============================================================
void MainWindow::onSerialDataReady()
{
    // 把本次到达的字节累计到缓冲区（一次 readyRead 可能只包含一帧的一部分）
    m_recvBuf.append(serial->readAll());

    // —— 按 BMS 协议拆帧 —— 帧格式: AA + seq(1) + data(6) = 8 字节/帧
    while (true) {
        int idx = m_recvBuf.indexOf((char)0xAA);   // 找帧头 0xAA
        if (idx < 0) {
            m_recvBuf.clear();   // 完全没有帧头,全部视为垃圾数据丢弃
            break;
        }
        if (idx > 0)
            m_recvBuf.remove(0, idx);   // 丢掉帧头前的残余字节

        if (m_recvBuf.size() < 8) break; // 数据不足一帧,等下次 readyRead 再来解析

        // 取出完整一帧
        QByteArray frame = m_recvBuf.left(8);
        m_recvBuf.remove(0, 8);

        // 解析并更新 UI
        parseBmsFrame(frame);
    }

    // 缓冲区上限保护 (长时间没收到完整帧则清空)
    if (m_recvBuf.size() > 4096)
        m_recvBuf.clear();
}

// ============================================================
// 解析一帧 BMS 协议, 并把数据更新到 UI
// ============================================================
void MainWindow::parseBmsFrame(const QByteArray &f)
{
    if (f.size() < 8 || (quint8)f[0] != 0xAA) return;
    quint8 seq = (quint8)f[1];

    // 大端读取 2 字节无符号数 (BMS 协议常用大端)
    auto be16 = [](quint8 hi, quint8 lo) -> quint16 {
        return ((quint16)hi << 8) | (quint16)lo;
    };

    switch (seq) {
    case 1: case 2: case 3: case 4: case 5: {
        // —— 单体电压: 每帧 3 节, 每节 2 字节大端, 单位 mV ——
        //   seq=1 -> cell1,2,3   seq=2 -> cell4,5,6   seq=3 -> cell7,8,9
        //   seq=4 -> cell10,11,12  seq=5 -> cell13,14,15
        int baseCell = (seq - 1) * 3 + 1;   // 1, 4, 7, 10, 13
        for (int i = 0; i < 3; ++i) {
            quint16 raw = be16((quint8)f[2 + i*2], (quint8)f[3 + i*2]);
            setCellVoltage(baseCell + i, raw);  // raw=0 表示该节未连接
        }
        break;
    }
    case 6: {
        // —— 系统状态: 总压(2B mV) + SOC(2B) + 电流(2B mA) ——
        // 示例: 8D 5C 00 54 00 02
        //   总压=0x8D5C=36188mV=36.188V  SOC=0x0054=84%  电流=0x0002=2mA
        quint16 totalV = be16((quint8)f[2], (quint8)f[3]);   // 总电压 mV
        quint16 soc    = be16((quint8)f[4], (quint8)f[5]);   // SOC (SOC_H<<8 | SOC_L)
        quint16 curr   = be16((quint8)f[6], (quint8)f[7]);   // 电流 mA
        ui->label_31->setText(QString::number(totalV / 1000.0, 'f', 3) + " V");
        ui->label_22->setText(QString::number(curr   / 1000.0, 'f', 3) + " A");
        ui->progress_soc->setValue(soc > 100 ? 100 : soc);
        break;
    }
    case 7: {
        // —— 温度(1B ℃) + 放电MOS状态(1B) + 充电MOS状态(1B) + 3B备用 ——
        // 示例: 1E 01 01 00 00 00
        //   温度=0x1E=30℃  DSG=0x01(开)  CHG=0x01(开)  后3字节备用
        quint8 temp = (quint8)f[2];
        quint8 dsg  = (quint8)f[3];   // 放电MOS: 01=开 00=关
        quint8 chg  = (quint8)f[4];   // 充电MOS: 01=开 00=关

        ui->label_24->setText(QString::number(temp) + " ℃");

        // 同步 MOS 实际状态到界面 (setChecked 不触发 clicked, 不会重复发送指令)
        ui->radioButton->setChecked(dsg != 0);
        ui->radioButton->setText(dsg != 0 ? "开启" : "关闭");
        ui->radioButton_2->setChecked(chg != 0);
        ui->radioButton_2->setText(chg != 0 ? "开启" : "关闭");
        break;
    }
    }

    // 原始帧写入表格 (方向=Rx)
    addFrameToTable(f, "Rx");
    ui->label_frame_count->setText(QString("总帧数:%1").arg(m_frameSeq));
}

// ============================================================
// 把第 N 节单体电压写到对应 label; raw=0 表示该节未连接
// ============================================================
void MainWindow::setCellVoltage(int cellNum, quint16 raw)
{
    QString txt = (raw == 0)
        ? "--"                                          // 未连接 (你只有 9 节, 10~15 显示 --)
        : (QString::number(raw / 1000.0, 'f', 3) + " V");
    // 按 objectName 取对应 label
    QLabel *labels[15] = {
        ui->cell_1,  ui->cell_2,  ui->cell_3,  ui->cell_4,  ui->cell_5,
        ui->cell_6,  ui->cell_7,  ui->cell_8,  ui->cell_9,  ui->cell_10,
        ui->cell_11, ui->cell_12, ui->cell_13, ui->cell_14, ui->cell_15,
    };
    if (cellNum >= 1 && cellNum <= 15 && labels[cellNum - 1])
        labels[cellNum - 1]->setText(txt);
}

// 字节数组 → "01 0A FF" 形式的 HEX 字符串
QString MainWindow::bytesToHex(const QByteArray &data)
{
    QString hex;
    for (unsigned char c : data)
        hex += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();
    return hex.trimmed();
}

void MainWindow::on_radioButton_clicked(bool checked)
{
    ui->radioButton->setText(checked ? "开启" : "关闭");
    sendMosCommand(0x01, checked);   // 0x01 = 放电MOS 命令字
}

void MainWindow::on_radioButton_2_clicked(bool checked)
{
    ui->radioButton_2->setText(checked ? "开启" : "关闭");
    sendMosCommand(0x02, checked);   // 0x02 = 充电MOS 命令字
}

// 发送 MOS 开关指令（帧格式可按真实协议修改）
void MainWindow::sendMosCommand(quint8 cmd, bool on)
{
    if (!serial->isOpen()) {
        ui->text_log->appendPlainText("串口未打开，无法发送指令");
        return;
    }

    // 帧模板：帧头 0xFF + 命令字 + 数据(1开/0关) + 帧尾 0xFE
    // —— 按你的真实 BMS 协议替换以下字节即可 ——
    QByteArray frame;
    frame.append((char)0xFF);                  // 帧头
    frame.append((char)cmd);                   // 命令字：0x01放电 / 0x02充电
    frame.append((char)(on ? 0x01 : 0x00));    // 数据位：1=开启, 0=关闭
    frame.append((char)0xFE);                  // 帧尾

    qint64 n = serial->write(frame);
    if (n == frame.size()) {
        ui->text_log->appendPlainText(QString("[Tx] %1").arg(bytesToHex(frame)));
        addFrameToTable(frame, "Tx");   // 发送的帧也写入表格 (方向=Tx)
    }
    else
        ui->text_log->appendPlainText("发送失败");
}

// ============================================================
// 把一帧原始数据写入 table_can 表格
//   dir = "Rx" (接收) 或 "Tx" (发送)
// ============================================================
void MainWindow::addFrameToTable(const QByteArray &frame, const QString &dir)
{
    int row = ui->table_can->rowCount();
    ui->table_can->insertRow(row);

    quint8 seq = (frame.size() >= 2) ? (quint8)frame[1] : 0;

    // 各列内容: 序号 | 时间戳 | 帧序号 | 方向 | DLC | 数据(HEX)
    ui->table_can->setItem(row, 0, new QTableWidgetItem(QString::number(++m_frameSeq)));
    ui->table_can->setItem(row, 1, new QTableWidgetItem(
        QDateTime::currentDateTime().toString("HH:mm:ss.zzz")));
    ui->table_can->setItem(row, 2, new QTableWidgetItem(
        QString("0x%1").arg(seq, 2, 16, QChar('0')).toUpper()));
    ui->table_can->setItem(row, 3, new QTableWidgetItem(dir));
    ui->table_can->setItem(row, 4, new QTableWidgetItem(QString::number(frame.size())));
    ui->table_can->setItem(row, 5, new QTableWidgetItem(bytesToHex(frame)));

    // 方向列上色 + 居中: Tx 红色, Rx 绿色
    QTableWidgetItem *dirItem = ui->table_can->item(row, 3);
    dirItem->setTextAlignment(Qt::AlignCenter);
    dirItem->setForeground(QBrush((dir == "Tx") ? QColor("#ed3f14") : QColor("#37ff4e")));

    // 勾选"连续显示"时自动滚到最新行
    if (ui->chk_continue->isChecked())
        ui->table_can->scrollToBottom();

    // 限制最大行数, 防止内存爆炸 (超出则删最旧的一行)
    const int MAX_ROWS = 500;
    if (ui->table_can->rowCount() > MAX_ROWS)
        ui->table_can->removeRow(0);
}

// 清空 CAN 表格
void MainWindow::on_btn_clear_can_clicked()
{
    ui->table_can->setRowCount(0);
    m_frameSeq = 0;                       // 行号归零
    ui->label_frame_count->setText("总帧数:0");
}
