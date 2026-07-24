# QSerialPort 串口模块使用文档

> 适用：Qt 5 / Qt 6。结合 BMS CAN 监控上位机场景给出实战示例。
> 模块属于 `Qt Serial Port`，需单独引入，不是 Qt 核心模块。

---

## 目录

1. [模块概述](#1-模块概述)
2. [项目配置](#2-项目配置)
3. [核心类](#3-核心类)
4. [串口参数详解](#4-串口参数详解)
5. [基本使用流程](#5-基本使用流程)
6. [扫描可用串口](#6-扫描可用串口)
7. [打开与关闭串口](#7-打开与关闭串口)
8. [配置串口参数](#8-配置串口参数)
9. [发送数据](#9-发送数据)
10. [接收数据](#10-接收数据)
11. [信号槽一览](#11-信号槽一览)
12. [错误处理](#12-错误处理)
13. [完整实战示例：BMS 串口通信类](#13-完整实战示例bms-串口通信类)
14. [常见问题与坑](#14-常见问题与坑)
15. [高级用法](#15-高级用法)

---

## 1. 模块概述

`QSerialPort` 是 Qt 提供的串口通信模块，封装了底层操作系统的串口 API（Windows 的 `Win32`、Linux 的 `termios`），提供统一的跨平台接口。

| 类 | 作用 |
|---|---|
| `QSerialPort` | 串口读写主体，负责打开、配置、收发数据 |
| `QSerialPortInfo` | 查询系统可用串口信息（端口名、描述、厂商、VID/PID） |

**典型应用场景**：
- 与嵌入式设备通信（单片机、BMS、传感器）
- 配合 USB-CAN 适配器收发 CAN 帧
- 与 RS232/RS485 设备通信
- 调试串口设备（替代串口助手）

---

## 2. 项目配置

### 2.1 在 `.pro` 文件中引入模块

```pro
QT += serialport
```

完整示例（参考你的 `BMS.pro`）：

```pro
QT += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

SOURCES += main.cpp mainwindow.cpp serialworker.cpp
HEADERS += mainwindow.h serialworker.h
```

### 2.2 CMake 项目（Qt6）

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core SerialPort)
target_link_libraries(BMS PRIVATE Qt6::Core Qt6::SerialPort)
```

### 2.3 C++ 中包含头文件

```cpp
#include <QSerialPort>
#include <QSerialPortInfo>
```

### 2.4 确认模块已安装

Qt 安装时勾选 `Qt Serial Port` 组件即可。命令行验证：

```bash
# Linux
ls /opt/Qt/*/gcc_64/lib/libQt5SerialPort*

# Windows
dir C:\Qt\<版本>\<编译器>\lib\Qt*SerialPort*.lib
```

---

## 3. 核心类

### 3.1 QSerialPort

```cpp
QSerialPort *serial = new QSerialPort(this);
serial->setPortName("COM5");
serial->setBaudRate(QSerialPort::Baud115200);
serial->open(QIODevice::ReadWrite);
```

继承自 `QIODevice`，所以拥有标准 IO 接口（`read()` / `write()` / `readyRead` 信号等）。

### 3.2 QSerialPortInfo

```cpp
foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
    qDebug() << info.portName() << info.description() << info.manufacturer();
}
```

常用方法：

| 方法 | 返回 | 说明 |
|---|---|---|
| `availablePorts()` | `QList<QSerialPortInfo>` | 系统所有可用串口 |
| `portName()` | `QString` | 端口名（COM3 / ttyUSB0） |
| `description()` | `QString` | 设备描述（"USB Serial Port"） |
| `manufacturer()` | `QString` | 厂商 |
| `serialNumber()` | `QString` | 序列号 |
| `vendorIdentifier()` | `quint16` | USB VID |
| `productIdentifier()` | `quint16` | USB PID |
| `hasVendorIdentifier()` | `bool` | 是否有 VID |
| `systemLocation()` | `QString` | 系统设备路径 |

---

## 4. 串口参数详解

| 参数 | 枚举/类型 | 常用值 |
|---|---|---|
| **端口名** | `QString` | `COM1`/`COM5`/`ttyUSB0`/`ttyS0` |
| **波特率** | `QSerialPort::BaudRate` | `Baud9600`/`Baud115200`/`Baud250000`/`Baud500000`/`Baud1000000` |
| **数据位** | `QSerialPort::DataBits` | `Data5`/`Data6`/`Data7`/`Data8`（默认 8） |
| **停止位** | `QSerialPort::StopBits` | `OneStop`/`OneAndHalfStop`/`TwoStop`（默认 1） |
| **校验位** | `QSerialPort::Parity` | `NoParity`/`EvenParity`/`OddParity` |
| **流控** | `QSerialPort::FlowControl` | `NoFlowControl`/`HardwareControl`/`SoftwareControl` |

**CAN 适配器常见配置**：波特率 `115200` 或 `500000`/`1000000`（高速 CAN），数据位 8，停止位 1，无校验，无流控。

---

## 5. 基本使用流程

```
1. 扫描可用串口  →  QSerialPortInfo::availablePorts()
2. 选择串口       →  setPortName()
3. 配置参数       →  setBaudRate() / setDataBits() / ...
4. 打开串口       →  open(ReadWrite)
5. 连接信号       →  readyRead / errorOccurred
6. 收发数据       →  write() / read() / readAll()
7. 关闭串口       →  close()
```

---

## 6. 扫描可用串口

### 6.1 填充下拉框（推荐）

```cpp
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
}
```

调用时取真实端口名：

```cpp
QString portName = ui->comboBox_port->currentData().toString();
```

### 6.2 按 VID/PID 精确识别指定设备

```cpp
for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
    if (info.vendorIdentifier() == 0x1A86 && info.productIdentifier() == 0x7523) {
        // CH340 芯片的 USB 转串口
        qDebug() << "找到 CH340 设备:" << info.portName();
    }
}
```

> 常见 USB 转串口芯片：CH340（VID=0x1A86）、CP210x（VID=0x10C4）、FT232（VID=0x0403）、PL2303（VID=0x067B）。

### 6.3 热插拔检测

USB 串口插拔时，可用 `QSerialPortInfo` 定时轮询，或监听系统事件。简单做法：

```cpp
QTimer *timer = new QTimer(this);
connect(timer, &QTimer::timeout, this, &MainWindow::refreshSerialPorts);
timer->start(2000);  // 每 2 秒刷新一次串口列表
```

---

## 7. 打开与关闭串口

### 7.1 打开

```cpp
QSerialPort *serial = new QSerialPort(this);
serial->setPortName("COM5");

if (serial->open(QIODevice::ReadWrite)) {
    qDebug() << "串口打开成功";
} else {
    qWarning() << "打开失败:" << serial->errorString();
}
```

打开模式（`QIODevice::OpenMode`）：

| 模式 | 说明 |
|---|---|
| `QIODevice::ReadOnly` | 只读 |
| `QIODevice::WriteOnly` | 只写 |
| `QIODevice::ReadWrite` | 读写（最常用） |

### 7.2 关闭

```cpp
serial->close();
```

关闭后会释放串口资源，其它程序才能占用。

### 7.3 检查状态

```cpp
if (serial->isOpen()) { ... }
if (serial->isReadable()) { ... }
if (serial->isWritable()) { ... }
```

---

## 8. 配置串口参数

**建议顺序**：先 `setPortName`，再 `open`，打开成功后再设其它参数（部分系统要求打开后才能配置）。

```cpp
serial->setBaudRate(QSerialPort::Baud115200);
serial->setDataBits(QSerialPort::Data8);
serial->setParity(QSerialPort::NoParity);
serial->setStopBits(QSerialPort::OneStop);
serial->setFlowControl(QSerialPort::NoFlowControl);
```

也可以在打开前用 `setPort(const QSerialPortInfo &)` 一次性设置：

```cpp
QSerialPortInfo info = ...;
serial->setPort(info);   // 自动填充端口名等
```

### 自定义波特率

部分系统支持非标准波特率（如 74880）：

```cpp
serial->setBaudRate(74880);   // 整数形式，自动转为自定义波特率
```

---

## 9. 发送数据

### 9.1 异步发送（推荐）

`write()` 立即返回，数据进入发送缓冲区，由底层异步发出。

```cpp
QByteArray data = QByteArray::fromHex("AA071002010000000000");
qint64 bytes = serial->write(data);
if (bytes == -1) {
    qWarning() << "发送失败";
}
```

### 9.2 等待发送完成

```cpp
serial->write(data);
if (serial->waitForBytesWritten(1000)) {   // 最多等 1 秒
    qDebug() << "已发送";
}
```

> 主线程中慎用 `waitForXxx`，会阻塞 UI。一般在子线程中使用。

### 9.3 发送字符串/HEX

```cpp
// 发送 ASCII 字符串
serial->write("AT\r\n");

// 发送 HEX（"AA0710" → 3 字节 0xAA 0x07 0x10）
serial->write(QByteArray::fromHex("AA0710"));

// 发送原始字节
char buf[] = {0xAA, 0x07, 0x10};
serial->write(buf, sizeof(buf));
```

### 9.4 刷新发送缓冲区

```cpp
serial->flush();   // 尽快把缓冲区数据发出（非必需）
```

---

## 10. 接收数据

### 10.1 异步接收（推荐）

连接 `readyRead` 信号，有数据到达时自动触发：

```cpp
connect(serial, &QSerialPort::readyRead, this, [this]() {
    QByteArray data = serial->readAll();   // 读取全部可用数据
    while (serial->waitForReadyRead(10)) { // 短暂等待，拼包
        data += serial->readAll();
    }
    processReceivedData(data);
});
```

> **重要**：`readyRead` 可能**多次触发**（一帧数据分多次到达）。需要在槽函数中**累积缓冲**，按协议头/尾或长度判断完整帧后再处理。

### 10.2 累积缓冲示例（处理分包）

```cpp
// 类成员
QByteArray m_rxBuffer;

void onReadyRead()
{
    m_rxBuffer.append(serial->readAll());

    while (m_rxBuffer.size() >= MIN_FRAME_LEN) {
        // 假设协议：帧头 0xAA + 长度字节 + 数据 + 帧尾 0x55
        int head = m_rxBuffer.indexOf(0xAA);
        if (head < 0) { m_rxBuffer.clear(); return; }
        if (head > 0) m_rxBuffer.remove(0, head);   // 丢弃帧头前垃圾

        if (m_rxBuffer.size() < 2) break;           // 长度字节还没到
        int frameLen = (quint8)m_rxBuffer[1] + 4;   // 协议总长
        if (m_rxBuffer.size() < frameLen) break;    // 数据未到齐

        QByteArray frame = m_rxBuffer.left(frameLen);
        m_rxBuffer.remove(0, frameLen);
        parseFrame(frame);
    }
}
```

### 10.3 同步接收（阻塞，慎用）

```cpp
serial->write(request);
serial->waitForBytesWritten(500);

QByteArray response;
while (serial->waitForReadyRead(100)) {
    response += serial->readAll();
}
```

> 同步方式会卡住 UI，仅适用于子线程或一次性命令交互。

### 10.4 读取单字节/行

```cpp
char c = serial->read(1).at(0);   // 读 1 字节
QByteArray line = serial->readLine();  // 读一行（到 '\n'）
```

---

## 11. 信号槽一览

| 信号 | 触发时机 | 用途 |
|---|---|---|
| `readyRead()` | 有新数据可读 | 接收数据 |
| `bytesWritten(qint64)` | 数据已写入底层发送 | 确认发送完成 |
| `errorOccurred(QSerialPort::SerialPortError)` | 发生错误 | 错误处理 |
| `baudRateChanged(...)` | 波特率改变 | - |
| `dataTerminalReadyChanged(bool)` | DTR 状态变化 | 硬件流控 |
| `requestToSendChanged(bool)` | RTS 状态变化 | 硬件流控 |

**最常用三个**：

```cpp
connect(serial, &QSerialPort::readyRead, this, &SerialWorker::onReadyRead);
connect(serial, &QSerialPort::bytesWritten, this, [](qint64 bytes){
    qDebug() << "已发送" << bytes << "字节";
});
connect(serial, &QSerialPort::errorOccurred, this, &SerialWorker::onError);
```

> **注意**：`errorOccurred` 在 Qt5 中是重载信号，推荐用 `QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred)` 或 Qt 5.7+ 的函数指针语法。

---

## 12. 错误处理

### 12.1 错误枚举

`QSerialPort::SerialPortError` 常见值：

| 值 | 含义 |
|---|---|
| `NoError` | 无错误 |
| `DeviceNotFoundError` | 设备不存在 |
| `PermissionError` | 权限不足/被占用 |
| `OpenError` | 打开失败 |
| `NotOpenError` | 操作未打开的串口 |
| `TimeoutError` | 超时 |
| `ReadError` / `WriteError` | 读写错误 |
| `ResourceError` | 资源意外失效（如 USB 拔出） |
| `UnsupportedOperationError` | 不支持的操作 |
| `UnknownError` | 未知错误 |

### 12.2 错误处理示例

```cpp
void SerialWorker::onError(QSerialPort::SerialPortError err)
{
    if (err == QSerialPort::NoError) return;

    qWarning() << "串口错误:" << err << serial->errorString();

    switch (err) {
    case QSerialPort::ResourceError:
        // USB 串口被拔出
        emit portLost();
        serial->close();
        break;
    case QSerialPort::PermissionError:
        emit errorOccurred("串口被占用或无权限");
        break;
    default:
        emit errorOccurred(serial->errorString());
    }

    serial->clearError();   // 清除错误状态，避免重复触发
}
```

### 12.3 Linux 权限问题

Linux 下访问 `/dev/ttyUSB0` 通常需要加入 `dialout` 组：

```bash
sudo usermod -aG dialout $USER
# 注销重新登录生效
```

否则会报 `PermissionError`。

---

## 13. 完整实战示例：BMS 串口通信类

将串口逻辑封装成独立类，主线程只负责 UI，避免阻塞。

### SerialWorker.h

```cpp
#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>

class SerialWorker : public QObject
{
    Q_OBJECT
public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker();

    // 扫描可用串口
    static QList<QSerialPortInfo> availablePorts();

public slots:
    bool openPort(const QString &portName, qint32 baudRate);
    void closePort();
    void sendData(const QByteArray &data);
    bool isOpen() const;

signals:
    void dataReceived(const QByteArray &data);
    void portOpened();
    void portClosed();
    void errorOccurred(const QString &msg);

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError err);

private:
    QSerialPort *m_serial;
    QByteArray   m_rxBuffer;
    void parseBuffer();   // 协议解析
};

#endif // SERIALWORKER_H
```

### SerialWorker.cpp

```cpp
#include "serialworker.h"
#include <QDebug>

SerialWorker::SerialWorker(QObject *parent)
    : QObject(parent)
    , m_serial(new QSerialPort(this))
{
    connect(m_serial, &QSerialPort::readyRead, this, &SerialWorker::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &SerialWorker::onError);
}

SerialWorker::~SerialWorker()
{
    closePort();
}

QList<QSerialPortInfo> SerialWorker::availablePorts()
{
    return QSerialPortInfo::availablePorts();
}

bool SerialWorker::openPort(const QString &portName, qint32 baudRate)
{
    if (m_serial->isOpen()) m_serial->close();

    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serial->open(QIODevice::ReadWrite)) {
        emit portOpened();
        return true;
    }
    emit errorOccurred(m_serial->errorString());
    return false;
}

void SerialWorker::closePort()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        emit portClosed();
    }
}

void SerialWorker::sendData(const QByteArray &data)
{
    if (m_serial->isOpen()) m_serial->write(data);
}

bool SerialWorker::isOpen() const
{
    return m_serial->isOpen();
}

void SerialWorker::onReadyRead()
{
    m_rxBuffer.append(m_serial->readAll());
    parseBuffer();
}

void SerialWorker::parseBuffer()
{
    // 示例协议：帧头 0xAA + 长度 + 数据 + 校验 + 帧尾 0x55
    while (m_rxBuffer.size() >= 4) {
        if ((quint8)m_rxBuffer[0] != 0xAA) {
            m_rxBuffer.remove(0, 1);   // 找帧头
            continue;
        }
        int frameLen = (quint8)m_rxBuffer[1] + 4;
        if (m_rxBuffer.size() < frameLen) break;   // 数据未到齐

        QByteArray frame = m_rxBuffer.left(frameLen);
        m_rxBuffer.remove(0, frameLen);

        // 简单校验：末字节必须是 0x55
        if ((quint8)frame[frame.size()-1] == 0x55) {
            emit dataReceived(frame);
        }
    }
}

void SerialWorker::onError(QSerialPort::SerialPortError err)
{
    if (err == QSerialPort::NoError) return;
    emit errorOccurred(m_serial->errorString());

    if (err == QSerialPort::ResourceError) {
        m_serial->close();
        emit portClosed();
    }
    m_serial->clearError();
}
```

### 在 MainWindow 中使用

```cpp
// mainwindow.h
#include "serialworker.h"
class MainWindow : public QMainWindow {
    // ...
private:
    SerialWorker *m_serial;
};

// mainwindow.cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
    , m_serial(new SerialWorker(this))
{
    ui->setupUi(this);
    refreshPorts();

    connect(m_serial, &SerialWorker::dataReceived, this, &MainWindow::onFrameReceived);
    connect(m_serial, &SerialWorker::portOpened,   this, &MainWindow::onPortOpened);
    connect(m_serial, &SerialWorker::portClosed,   this, &MainWindow::onPortClosed);
    connect(m_serial, &SerialWorker::errorOccurred,this, &MainWindow::onSerialError);
}

void MainWindow::refreshPorts()
{
    ui->comboBox_port->clear();
    for (const auto &info : SerialWorker::availablePorts())
        ui->comboBox_port->addItem(info.portName(), info.portName());
}

// 打开按钮
void MainWindow::on_btn_open_clicked()
{
    QString port = ui->comboBox_port->currentData().toString();
    qint32 baud  = ui->comboBox_baud->currentText().toInt();
    m_serial->openPort(port, baud);
}

// 关闭按钮
void MainWindow::on_btn_close_clicked()
{
    m_serial->closePort();
}

// 收到一帧
void MainWindow::onFrameReceived(const QByteArray &frame)
{
    // 转 HEX 显示到日志
    QString hex;
    for (char b : frame)
        hex += QString("%1 ").arg((quint8)b, 2, 16, QChar('0')).toUpper();
    ui->text_log->appendPlainText("[Rx] " + hex.trimmed());

    // 解析 BMS 数据，更新 UI...
    // parseBmsFrame(frame);
}
```

---

## 14. 常见问题与坑

### 14.1 收到数据不完整 / 粘包

**原因**：`readyRead` 可能一帧分多次触发，或多帧一次到达。

**解决**：在槽函数中**累积缓冲**，按协议头/长度/校验判断完整帧（见 10.2）。

### 14.2 中文乱码

串口收发的应是**原始字节**，不要用 `QString` 直接处理：

```cpp
// ❌ 错误：QString 走编码转换
QString s = serial->readAll();

// ✅ 正确：用 QByteArray 保存原始字节
QByteArray data = serial->readAll();
QString display = QString::fromUtf8(data);   // 仅在需要显示时按编码转
```

### 14.3 `waitForReadyRead` 卡死 UI

**原因**：在主线程调用阻塞等待函数。

**解决**：
- 主线程用异步 `readyRead` 信号
- 同步操作放到 `QThread` 子线程

### 14.4 关闭程序后串口未释放

**原因**：没有显式 `close()`，或对象未正确销毁。

**解决**：

```cpp
MainWindow::~MainWindow()
{
    if (m_serial) m_serial->closePort();
    delete ui;
}
```

### 14.5 USB 串口拔出后崩溃

**原因**：`ResourceError` 后继续操作已失效的串口。

**解决**：在 `errorOccurred` 中检测 `ResourceError`，关闭串口并更新 UI 状态（见 12.2）。

### 14.6 Linux 下 `PermissionError`

```bash
sudo usermod -aG dialout $USER   # 加入 dialout 组
# 注销重新登录
```

### 14.7 高波特率丢数据

- 用更高的波特率（如 1Mbps）时，确保 UI 不卡顿（数据处理放子线程）
- 适当增大串口缓冲：`serial->setReadBufferSize(1024 * 1024);`
- 数据量大时考虑用 `QSerialPort` + 独立线程，或改用 `QIODevice` 直接读取

### 14.8 波特率不支持

某些系统不支持任意波特率（如 74880）。Linux 可用 `setBaudRate(74880, QSerialPort::CustomBaudRate)`，Windows 一般只支持标准波特率。

### 14.9 中文端口名/描述乱码

`QSerialPortInfo::description()` 在某些 Windows 系统会乱码，可用系统编码转换：

```cpp
QString desc = QString::fromLocal8Bit(info.description().toUtf8());
```

---

## 15. 高级用法

### 15.1 子线程串口通信

把 `SerialWorker` 移到子线程，避免阻塞 UI：

```cpp
QThread *thread = new QThread(this);
SerialWorker *worker = new SerialWorker;
worker->moveToThread(thread);

connect(thread, &QThread::finished, worker, &QObject::deleteLater);
thread->start();

// 跨线程调用：通过信号槽（自动排队连接）
QMetaObject::invokeMethod(worker, "openPort",
    Q_ARG(QString, "COM5"), Q_ARG(qint32, 115200));
```

> 注意：`QSerialPort` 必须在它所属的线程中使用（创建、打开、读写都在同一线程）。

### 15.2 DTR/RTS 控制

某些设备（如 ESP32、STM32 BOOT）需要通过 DTR/RTS 控制复位：

```cpp
serial->setDataTerminalReady(false);   // DTR = LOW
serial->setRequestToSend(true);        // RTS = HIGH
QThread::msleep(100);
serial->setRequestToSend(false);       // 复位触发
serial->setDataTerminalReady(true);
```

### 15.3 超时读取（命令-响应模式）

```cpp
QByteArray sendCommand(QSerialPort *s, const QByteArray &cmd, int timeoutMs)
{
    s->clear();                  // 清空缓冲
    s->write(cmd);
    if (!s->waitForBytesWritten(timeoutMs)) return {};

    QByteArray resp;
    while (s->waitForReadyRead(timeoutMs)) {
        resp += s->readAll();
        if (resp.contains('\n')) break;   // 收到换行结束
    }
    return resp;
}
```

### 15.4 自定义协议解析状态机

复杂协议建议用状态机（`QStateMachine` 或手写 switch-case），比简单 `indexOf` 更鲁棒，能处理乱序、重传。

### 15.5 与 CAN 适配器配合

USB-CAN 适配器通常提供两种接口：
1. **厂商 SDK**（如周立功、PCAN、CANalyst-II）—— 直接调用 `VCI_Transmit` 等
2. **串口透传模式**—— CAN 帧封装成串口数据，用 `QSerialPort` 收发

后者需按适配器协议封装/解析，常见格式：

```
帧头(0xAA) | 命令 | CAN_ID(4B) | DLC | 数据(8B) | 校验 | 帧尾(0x55)
```

---

## 附录：常用速查

### 标准波特率

| 枚举 | 值 |
|---|---|
| `Baud1200` / `Baud2400` / `Baud4800` / `Baud9600` | 1200~9600 |
| `Baud19200` / `Baud38400` / `Baud57600` | 中速 |
| `Baud115200` | 常用 |
| `Baud230400` / `Baud460800` / `Baud921600` | 高速 |
| `Baud250000` / `Baud500000` / `Baud1000000` | CAN 常用 |

### 流控对比

| 类型 | 说明 | 适用 |
|---|---|---|
| `NoFlowControl` | 无流控 | 短距离、低速 |
| `HardwareControl` | RTS/CTS 硬件流控 | 长距离、高速、可靠 |
| `SoftwareControl` | XON/XOFF 软件流控 | 不能用硬件流控时 |

### 关键 API 速查

```cpp
// 配置
setPortName(name)
setBaudRate(rate)
setDataBits(QSerialPort::Data8)
setParity(QSerialPort::NoParity)
setStopBits(QSerialPort::OneStop)
setFlowControl(QSerialPort::NoFlowControl)

// IO
open(QIODevice::ReadWrite)
close()
isOpen()
readAll() / read(maxSize)
write(data) / write(data, size)
bytesAvailable()
waitForReadyRead(msec)
waitForBytesWritten(msec)
clear()                       // 清空缓冲

// 信号
readyRead()
bytesWritten(qint64)
errorOccurred(SerialPortError)

// 状态
flush()
clearError()
error() / errorString()
```

---

**文档完。** 配合 BMS CAN 监控项目使用时，建议按"扫描 → 打开 → 收发 → 解析 → UI 更新"的顺序实现，串口逻辑封装在 `SerialWorker`，UI 通过信号槽交互，避免阻塞主线程。
