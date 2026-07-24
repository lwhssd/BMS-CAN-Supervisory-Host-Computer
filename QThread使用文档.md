# QThread 多线程模块使用文档

> 适用：Qt 5 / Qt 6。结合 BMS CAN 监控上位机场景（串口收发、数据采集、协议解析）给出实战示例。
> 所有代码均含详细中文注释。

---

## 目录

1. [基础概念](#1-基础概念)
2. [核心用法一：继承 QThread 重写 run()](#2-核心用法一继承-qthread-重写-run)
3. [核心用法二：QObject + moveToThread（官方推荐）](#3-核心用法二qobject--movetothread官方推荐)
4. [两种方式对比与选择](#4-两种方式对比与选择)
5. [信号与槽通信](#5-信号与槽通信)
6. [线程安全与资源管理](#6-线程安全与资源管理)
7. [常见坑与最佳实践](#7-常见坑与最佳实践)
8. [完整实战示例：BMS 数据采集线程](#8-完整实战示例bms-数据采集线程)
9. [速查附录](#9-速查附录)

---

## 1. 基础概念

### 1.1 QThread 是什么

`QThread` 是 Qt 提供的跨平台线程管理类，封装了操作系统原生线程（Windows 线程 / pthread）。每个 `QThread` 对象管理一个独立的执行线程，拥有自己的事件循环（event loop）。

### 1.2 为什么需要多线程

Qt GUI 程序的**主线程（UI 线程）**负责处理界面绘制和用户交互。如果在主线程执行耗时操作（如串口阻塞读取、大量数据解析、网络请求），会导致界面卡死、无响应。

| 场景 | 是否需要多线程 |
|---|---|
| 串口同步阻塞读取（`waitForReadyRead`） | ✅ 需要 |
| 大批量 CAN 帧协议解析 | ✅ 需要 |
| 定时高频数据采集 | ✅ 需要 |
| 文件大体积读写 | ✅ 需要 |
| 网络请求（HTTP/TCP） | ❌ Qt 网络类自带异步，通常不需 |
| 简单的 UI 更新 | ❌ 不需要 |
| 定时器触发的轻量任务 | ❌ 用 QTimer 即可 |

> **核心原则**：只在主线程会因耗时操作卡顿时，才把任务挪到子线程。

### 1.3 QThread 的关键特性

- 每个 `QThread` 自带事件循环，可通过 `exec()` 启动
- 信号槽跨线程时自动变为**队列连接**（`Qt::QueuedConnection`）
- `QObject` 对象**依附于**创建它的线程（thread affinity），可通过 `moveToThread` 改变
- 子线程中**不能直接操作 UI 控件**（QWidget 及其子类只能在主线程访问）

---

## 2. 核心用法一：继承 QThread 重写 run()

### 2.1 基本思路

继承 `QThread`，重写 `run()` 方法，在 `run()` 中编写线程要执行的循环逻辑。调用 `start()` 启动线程，`run()` 返回时线程结束。

### 2.2 示例：周期性采集数据的线程

#### WorkerThread.h

```cpp
#ifndef WORKERTHREAD_H
#define WORKERTHREAD_H

#include <QThread>

// 继承 QThread 的自定义线程类
class WorkerThread : public QThread
{
    Q_OBJECT
public:
    explicit WorkerThread(QObject *parent = nullptr);
    ~WorkerThread();

    // 外部请求停止线程（线程安全）
    void requestStop();

signals:
    // 子线程向主线程发送采集到的数据
    void dataReady(const QByteArray &data);

protected:
    // 重写 run()：线程的入口函数，在这里写循环逻辑
    void run() override;

private:
    // 停止标志位，用 volatile 或 atomic 保证多线程可见性
    volatile bool m_stop;
};

#endif // WORKERTHREAD_H
```

#### WorkerThread.cpp

```cpp
#include "workerthread.h"
#include <QThread>

WorkerThread::WorkerThread(QObject *parent)
    : QThread(parent)
    , m_stop(false)
{
}

WorkerThread::~WorkerThread()
{
    // 析构前确保线程安全退出，避免资源泄漏或崩溃
    requestStop();
    wait();   // 阻塞等待 run() 返回
}

// 外部请求停止：把标志位置 true，run() 循环检测到后退出
void WorkerThread::requestStop()
{
    m_stop = true;
}

// 线程入口：在线程中执行的核心逻辑
void WorkerThread::run()
{
    // run() 内部的局部变量属于子线程，安全使用
    while (!m_stop) {
        // 模拟耗时采集（实际是串口读取、协议解析等）
        QThread::msleep(100);   // 休眠 100ms，避免 CPU 空转

        QByteArray data = "采集到的数据";

        // 通过信号把数据发给主线程（跨线程自动队列连接）
        emit dataReady(data);
    }
    // run() 返回，线程自动结束
}
```

#### 在 MainWindow 中使用

```cpp
// mainwindow.h
#include "workerthread.h"
class MainWindow : public QMainWindow {
private:
    WorkerThread *m_worker;
};

// mainwindow.cpp 构造函数
m_worker = new WorkerThread(this);

// 连接信号：子线程发数据 → 主线程更新 UI
connect(m_worker, &WorkerThread::dataReady, this, [this](const QByteArray &data){
    ui->text_log->appendPlainText(QString::fromUtf8(data));
});

m_worker->start();   // 启动线程，自动调用 run()
```

### 2.3 适用场景

- 线程内部是**一个持续运行的循环**（如数据采集、轮询）
- 不需要在线程中接收外部信号做响应
- 逻辑相对独立、自包含

### 2.4 局限

- `run()` 中**默认没有事件循环**，无法使用定时器、网络异步、信号槽接收
- 如果需要在 `run()` 中接收信号，必须手动调用 `exec()` 启动事件循环
- 一个 `QThread` 实例只能跑一个任务，灵活性较低

---

## 3. 核心用法二：QObject + moveToThread（官方推荐）

### 3.1 基本思路

创建一个继承 `QObject` 的"工作对象"（Worker），把它的耗时方法用槽函数实现，再通过 `moveToThread()` 把这个对象**移动到**一个 `QThread` 中。线程启动后会自动运行事件循环，工作对象的槽函数在线程中被调用。

这是 Qt 官方文档推荐的方式，更灵活、更安全。

### 3.2 示例：可响应信号的串口工作对象

#### SerialWorker.h

```cpp
#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QByteArray>

// 工作对象类：继承 QObject，不继承 QThread
class SerialWorker : public QObject
{
    Q_OBJECT
public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker();

signals:
    // 子线程 → 主线程：发送接收到的数据
    void dataReceived(const QByteArray &data);
    // 子线程 → 主线程：通知连接状态
    void connectionChanged(bool connected);
    // 子线程 → 主线程：发送错误信息
    void errorOccurred(const QString &msg);

public slots:
    // 主线程 → 子线程：打开串口（此槽函数在子线程执行）
    void openPort(const QString &portName, qint32 baudRate);
    // 主线程 → 子线程：关闭串口
    void closePort();
    // 主线程 → 子线程：发送数据
    void sendData(const QByteArray &data);

private slots:
    // 串口数据到达时的内部处理槽（在子线程执行）
    void onReadyRead();
    // 串口错误处理（在子线程执行）
    void onError(QSerialPort::SerialPortError err);

private:
    QSerialPort *m_serial;   // 串口对象，必须在子线程中创建和使用
};

#endif // SERIALWORKER_H
```

#### SerialWorker.cpp

```cpp
#include "serialworker.h"
#include <QDebug>

SerialWorker::SerialWorker(QObject *parent)
    : QObject(parent)
    , m_serial(nullptr)   // 先置空，等 openPort 时再创建
{
    // 注意：QSerialPort 必须在它要使用的线程中创建
    // 这里构造函数在 moveToThread 之前执行（主线程），所以先不创建
}

SerialWorker::~SerialWorker()
{
    // 析构时关闭串口（此时已在子线程，安全）
    if (m_serial) {
        m_serial->close();
        delete m_serial;
    }
}

// 打开串口槽函数——由主线程通过信号触发，实际在子线程执行
void SerialWorker::openPort(const QString &portName, qint32 baudRate)
{
    // 第一次调用时创建串口对象（此时在子线程，符合线程亲和性要求）
    if (!m_serial) {
        m_serial = new QSerialPort(this);   // this 作为父对象，自动管理内存
        // 连接串口信号到本对象的槽（都在子线程，自动队列连接）
        connect(m_serial, &QSerialPort::readyRead, this, &SerialWorker::onReadyRead);
        connect(m_serial, &QSerialPort::errorOccurred, this, &SerialWorker::onError);
    }

    // 如果已打开，先关闭
    if (m_serial->isOpen()) m_serial->close();

    // 配置串口参数
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serial->open(QIODevice::ReadWrite)) {
        emit connectionChanged(true);   // 通知主线程：已连接
    } else {
        emit errorOccurred(m_serial->errorString());   // 通知主线程：错误
    }
}

// 关闭串口
void SerialWorker::closePort()
{
    if (m_serial && m_serial->isOpen()) {
        m_serial->close();
        emit connectionChanged(false);   // 通知主线程：已断开
    }
}

// 发送数据
void SerialWorker::sendData(const QByteArray &data)
{
    if (m_serial && m_serial->isOpen()) {
        m_serial->write(data);
    }
}

// 串口数据到达——在子线程执行
void SerialWorker::onReadyRead()
{
    QByteArray data = m_serial->readAll();
    emit dataReceived(data);   // 转发给主线程处理
}

// 串口错误——在子线程执行
void SerialWorker::onError(QSerialPort::SerialPortError err)
{
    if (err == QSerialPort::NoError) return;
    emit errorOccurred(m_serial->errorString());
    m_serial->clearError();
}
```

#### 在 MainWindow 中创建并移动到线程

```cpp
// mainwindow.h
#include <QThread>
#include "serialworker.h"

class MainWindow : public QMainWindow {
private:
    QThread      *m_thread;    // 线程对象
    SerialWorker *m_worker;    // 工作对象
};

// mainwindow.cpp 构造函数
// 1. 创建线程和工作对象（都不要指定 parent，因为要 moveToThread）
m_thread = new QThread(this);
m_worker = new SerialWorker;   // 暂无 parent

// 2. 把工作对象移动到子线程
m_worker->moveToThread(m_thread);

// 3. 连接信号槽（跨线程自动队列连接）
//    子线程 → 主线程：更新 UI
connect(m_worker, &SerialWorker::dataReceived, this, [this](const QByteArray &data){
    ui->text_log->appendPlainText("[Rx] " + data.toHex(' '));
});
connect(m_worker, &SerialWorker::connectionChanged, this, [this](bool ok){
    ui->label_link_status->setText(ok ? "已连接" : "未连接");
    setDotStatus(ui->label_link_dot, ok);
});
connect(m_worker, &SerialWorker::errorOccurred, this, [this](const QString &msg){
    ui->text_log->appendPlainText("[错误] " + msg);
});

// 4. 线程结束时自动清理工作对象（关键：防止内存泄漏）
connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

// 5. 启动线程（启动后会自动运行事件循环，等待信号触发槽函数）
m_thread->start();

// 之后通过信号调用工作对象的槽（自动排队到子线程执行）：
// emit openPort(...) 或直接 QMetaObject::invokeMethod
```

#### 触发工作对象的槽（主线程 → 子线程）

```cpp
void MainWindow::on_btn_open_clicked()
{
    QString port = ui->comboBox_port->currentData().toString();
    qint32 baud  = ui->comboBox_baud->currentText().toInt();
    // 通过 invokeMethod 跨线程调用（自动队列连接）
    QMetaObject::invokeMethod(m_worker, "openPort",
        Qt::QueuedConnection,
        Q_ARG(QString, port),
        Q_ARG(qint32, baud));
}
```

> 也可以定义一个主线程的信号 `void requestOpen(QString, qint32)`，`connect` 到 `m_worker` 的 `openPort`，然后 `emit requestOpen(port, baud)`。效果一样。

---

## 4. 两种方式对比与选择

| 对比项 | 继承 QThread 重写 run() | QObject + moveToThread |
|---|---|---|
| **灵活性** | 低，一个线程一个任务 | 高，可挂多个工作对象 |
| **事件循环** | run() 内默认无，需手动 exec() | 自带事件循环 |
| **响应信号** | 需手动 exec() 才能收信号 | 天然支持信号槽 |
| **线程亲和性** | QThread 对象本身在主线程 | 工作对象在子线程 |
| **官方推荐** | 旧式用法 | ✅ 官方推荐 |
| **适用场景** | 单一持续循环任务 | 复杂业务、多任务、需响应请求 |
| **代码量** | 少 | 稍多 |

**选择建议**：
- 简单的"死循环采集"任务 → 继承 `QThread`
- 需要根据主线程指令动态开关、收发、解析 → `moveToThread`
- 不确定时 → 优先 `moveToThread`（更通用、更安全）

---

## 5. 信号与槽通信

### 5.1 跨线程自动队列连接

当信号发送者和接收槽函数**位于不同线程**时，Qt 自动使用 `Qt::QueuedConnection`：
- 信号发出后，参数被拷贝到接收线程的事件队列
- 接收线程的事件循环取出事件，调用槽函数
- 槽函数在**接收者所在的线程**执行

```cpp
// m_worker 在子线程，this（MainWindow）在主线程
// 自动队列连接：dataReady 在子线程发出 → 槽在主线程执行
connect(m_worker, &SerialWorker::dataReceived,
        this, &MainWindow::onDataReceived);
```

### 5.2 连接类型（Qt::ConnectionType）

| 类型 | 行为 |
|---|---|
| `Qt::DirectConnection` | 槽函数在信号发送者线程同步执行（不跨线程） |
| `Qt::QueuedConnection` | 槽函数在接收者线程异步执行（跨线程） |
| `Qt::AutoConnection` | **默认**，自动判断：同线程→Direct，跨线程→Queued |
| `Qt::BlockingQueuedConnection` | 队列连接但发送者阻塞等待槽执行完（必须跨线程，否则死锁） |
| `Qt::UniqueConnection` | 防重复连接标志，可与上述组合 |

> 日常开发用默认 `AutoConnection` 即可，Qt 会自动处理跨线程。

### 5.3 主线程 → 子线程

```cpp
// 方式一：信号槽（推荐，自动队列）
connect(this, &MainWindow::requestOpen, m_worker, &SerialWorker::openPort);
emit requestOpen("COM5", 115200);   // 主线程发出 → 子线程执行 openPort

// 方式二：QMetaObject::invokeMethod
QMetaObject::invokeMethod(m_worker, "openPort",
    Qt::QueuedConnection,
    Q_ARG(QString, "COM5"),
    Q_ARG(qint32, 115200));
```

### 5.4 子线程 → 主线程

```cpp
// 子线程发出信号 → 主线程槽函数更新 UI
connect(m_worker, &SerialWorker::dataReceived, this, [this](const QByteArray &data){
    ui->text_log->appendPlainText(QString::fromUtf8(data));   // 在主线程操作 UI，安全
});
```

### 5.5 传递数据的注意事项

- 跨线程信号槽的参数会**拷贝**，所以用 `QString`、`QByteArray` 等隐式共享类型开销很小
- 传递自定义类型需用 `qRegisterMetaType` 注册：

```cpp
// 注册自定义类型，使其能跨线程传递
qRegisterMetaType<MyStruct>("MyStruct");

// 之后即可在信号槽中传递
connect(m_worker, &SerialWorker::frameParsed, this, [](const MyStruct &frame){
    // ...
});
```

---

## 6. 线程安全与资源管理

### 6.1 线程生命周期控制

| 方法 | 作用 |
|---|---|
| `start()` | 启动线程，调用 `run()` |
| `run()` | 线程入口，返回则线程结束 |
| `exec()` | 在 `run()` 中启动事件循环（阻塞直到 `quit()`） |
| `quit()` | 请求事件循环退出（安全，等当前事件处理完） |
| `exit(int)` | 同 quit，可带返回码 |
| `terminate()` | **强制终止线程**（危险，可能资源泄漏，避免使用） |
| `wait()` | 阻塞等待线程结束（常用于析构前） |
| `wait(unsigned long ms)` | 最多等待 ms 毫秒 |
| `isRunning()` | 线程是否在运行 |
| `isFinished()` | 线程是否已结束 |
| `requestInterruption()` | 请求中断（配合 `isInterruptionRequested()` 检查） |

### 6.2 安全退出线程（moveToThread 方式）

```cpp
MainWindow::~MainWindow()
{
    // 1. 请求事件循环退出（quit 是线程安全的）
    m_thread->quit();

    // 2. 阻塞等待线程真正结束（最多等 5 秒）
    if (!m_thread->wait(5000)) {
        qWarning() << "线程未在 5 秒内退出";
        // 极端情况才考虑 terminate()
    }

    // 3. m_worker 已通过 deleteLater 在线程结束时自动销毁
    //    m_thread 是 MainWindow 的子对象，MainWindow 析构时自动删除
}
```

### 6.3 安全退出线程（继承 QThread 方式）

```cpp
// 通过标志位让 run() 循环自然退出
m_worker->requestStop();   // 设置 m_stop = true
m_worker->wait();          // 等待 run() 返回
```

### 6.4 互斥锁保护共享数据

当多个线程访问同一份数据时，必须加锁：

```cpp
#include <QMutex>
#include <QMutexLocker>

class SerialWorker : public QObject {
private:
    QMutex m_mutex;          // 互斥锁
    QByteArray m_buffer;     // 共享数据
};

void SerialWorker::appendData(const QByteArray &data)
{
    QMutexLocker locker(&m_mutex);   // 构造时加锁，作用域结束自动解锁
    m_buffer.append(data);
}

QByteArray SerialWorker::takeData()
{
    QMutexLocker locker(&m_mutex);
    QByteArray out = m_buffer;
    m_buffer.clear();
    return out;
}
```

> `QMutexLocker` 是 RAII 封装，推荐使用，避免忘记解锁。

### 6.5 资源释放顺序

```
1. quit()              → 通知事件循环退出
2. wait(timeout)       → 等待线程结束
3. deleteLater()       → 让工作对象在线程结束时自动删除（通过 finished 信号连接）
```

错误示范：

```cpp
// ❌ 错误：直接 delete 工作对象，可能线程还在使用它
delete m_worker;

// ❌ 错误：线程还在跑就 delete 线程对象
delete m_thread;   // 可能崩溃
```

---

## 7. 常见坑与最佳实践

### 7.1 子线程不能操作 UI

```cpp
// ❌ 错误：在子线程直接操作 UI 控件，可能崩溃或异常
void SerialWorker::onReadyRead()
{
    ui->text_log->appendPlainText(...);   // 禁止！
}

// ✅ 正确：通过信号把数据发给主线程，主线程更新 UI
emit dataReceived(data);
```

### 7.2 QSerialPort 等对象的线程亲和性

`QSerialPort`、`QTimer`、`QTcpSocket` 等对象**必须在使用它的线程中创建**：

```cpp
// ❌ 错误：在主线程创建 serial，却想在子线程用
m_serial = new QSerialPort(this);   // 主线程创建
m_worker->moveToThread(m_thread);   // serial 还在主线程

// ✅ 正确：在工作对象的槽函数中创建（此时已在子线程）
void SerialWorker::openPort(...)
{
    if (!m_serial) m_serial = new QSerialPort(this);   // 子线程创建
}
```

### 7.3 moveToThread 必须在对象创建后、使用前

```cpp
m_worker = new SerialWorker;   // 创建（在主线程）
m_worker->moveToThread(m_thread);   // 移动（必须在 start() 之前）
m_thread->start();              // 启动
```

> 已设置 parent 的对象不能 moveToThread。所以工作对象创建时**不要给 parent**。

### 7.4 不要重写 QThread::run() 后又用 moveToThread

这两种方式**不要混用**：
- 继承 QThread 重写 run() → 任务在 run() 里
- moveToThread → 任务在工作对象的槽里，run() 用默认的 exec()

混用会导致逻辑混乱、事件循环不工作。

### 7.5 wait() 不能在自身线程调用

```cpp
// ❌ 错误：在线程内部调用自己的 wait()，死锁
void WorkerThread::run()
{
    this->wait();   // 死锁！
}
```

### 7.6 信号槽参数的自定义类型必须注册

```cpp
// 跨线程传递自定义类型，必须在 connect 之前注册
qRegisterMetaType<MyStruct>("MyStruct");
```

### 7.7 析构顺序

```cpp
MainWindow::~MainWindow()
{
    // 正确顺序：先停线程，再释放 UI
    m_thread->quit();
    m_thread->wait();
    delete ui;   // ui 在线程停止后释放，安全
}
```

### 7.8 避免频繁创建销毁线程

线程创建开销大，长时间运行的任务应复用线程，而非反复 new/delete。

---

## 8. 完整实战示例：BMS 数据采集线程

结合 BMS CAN 监控场景，用 `moveToThread` 方式实现一个完整的串口数据采集线程。

### BmsWorker.h

```cpp
#ifndef BMSWORKER_H
#define BMSWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QByteArray>
#include <QMutex>

// BMS 数据采集工作对象
class BmsWorker : public QObject
{
    Q_OBJECT
public:
    explicit BmsWorker(QObject *parent = nullptr);
    ~BmsWorker();

signals:
    // 解析后的 BMS 数据（电压、电流、SOC 等）
    void bmsDataUpdated(int totalVoltage, int current, int soc);
    // 原始 CAN 帧记录（供表格显示）
    void canFrameReceived(int seq, quint32 canId, const QByteArray &data);
    // 连接状态
    void connectionChanged(bool connected);
    // 日志
    void logMessage(const QString &msg);

public slots:
    void openPort(const QString &portName, qint32 baudRate);
    void closePort();
    void sendData(const QByteArray &data);

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError err);

private:
    QSerialPort *m_serial;
    QByteArray   m_rxBuffer;   // 接收缓冲（处理粘包分包）
    int          m_frameSeq;   // 帧序号
    QMutex       m_mutex;      // 保护共享数据

    void parseBuffer();        // 协议解析
};

#endif // BMSWORKER_H
```

### BmsWorker.cpp

```cpp
#include "bmsworker.h"
#include <QDebug>

BmsWorker::BmsWorker(QObject *parent)
    : QObject(parent)
    , m_serial(nullptr)
    , m_frameSeq(0)
{
}

BmsWorker::~BmsWorker()
{
    // 析构时关闭串口（此时在子线程，安全）
    if (m_serial) {
        m_serial->close();
        delete m_serial;
        m_serial = nullptr;
    }
}

// 打开串口（在子线程执行）
void BmsWorker::openPort(const QString &portName, qint32 baudRate)
{
    // 首次调用时创建串口对象（子线程中创建，符合亲和性）
    if (!m_serial) {
        m_serial = new QSerialPort(this);
        connect(m_serial, &QSerialPort::readyRead, this, &BmsWorker::onReadyRead);
        connect(m_serial, &QSerialPort::errorOccurred, this, &BmsWorker::onError);
    }

    if (m_serial->isOpen()) m_serial->close();

    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serial->open(QIODevice::ReadWrite)) {
        emit connectionChanged(true);
        emit logMessage(QString("串口 %1 打开成功").arg(portName));
    } else {
        emit logMessage(QString("打开失败: %1").arg(m_serial->errorString()));
    }
}

// 关闭串口（在子线程执行）
void BmsWorker::closePort()
{
    if (m_serial && m_serial->isOpen()) {
        m_serial->close();
        emit connectionChanged(false);
        emit logMessage("串口已关闭");
    }
}

// 发送数据（在子线程执行）
void BmsWorker::sendData(const QByteArray &data)
{
    if (m_serial && m_serial->isOpen()) {
        m_serial->write(data);
    }
}

// 串口数据到达（在子线程执行）
void BmsWorker::onReadyRead()
{
    // 累积到接收缓冲
    {
        QMutexLocker locker(&m_mutex);
        m_rxBuffer.append(m_serial->readAll());
    }
    // 解析完整帧
    parseBuffer();
}

// 协议解析（在子线程执行）
void BmsWorker::parseBuffer()
{
    QMutexLocker locker(&m_mutex);

    // 示例协议：帧头 0xAA + 长度 + 数据 + 帧尾 0x55
    while (m_rxBuffer.size() >= 4) {
        if ((quint8)m_rxBuffer[0] != 0xAA) {
            m_rxBuffer.remove(0, 1);   // 寻找帧头
            continue;
        }
        int frameLen = (quint8)m_rxBuffer[1] + 4;
        if (m_rxBuffer.size() < frameLen) break;   // 数据未到齐

        QByteArray frame = m_rxBuffer.left(frameLen);
        m_rxBuffer.remove(0, frameLen);

        // 校验帧尾
        if ((quint8)frame[frame.size() - 1] != 0x55) continue;

        // 提取 CAN ID（示例：第 2-5 字节）
        quint32 canId = 0;
        for (int i = 0; i < 4; ++i)
            canId = (canId << 8) | (quint8)frame[2 + i];

        // 提取数据段
        QByteArray payload = frame.mid(6, frame[1]);

        // 发送解析结果到主线程
        emit canFrameReceived(++m_frameSeq, canId, payload);

        // 简单模拟：根据 CAN ID 解析 BMS 数据
        if (canId == 0x1801) {
            int totalV = (quint8)payload[0] << 8 | (quint8)payload[1];
            int current = (qint16)((quint8)payload[2] << 8 | (quint8)payload[3]);
            int soc = (quint8)payload[4];
            emit bmsDataUpdated(totalV, current, soc);
        }
    }
}

// 错误处理（在子线程执行）
void BmsWorker::onError(QSerialPort::SerialPortError err)
{
    if (err == QSerialPort::NoError) return;
    emit logMessage(QString("串口错误: %1").arg(m_serial->errorString()));

    // USB 拔出等致命错误
    if (err == QSerialPort::ResourceError) {
        m_serial->close();
        emit connectionChanged(false);
    }
    m_serial->clearError();
}
```

### MainWindow 集成

```cpp
// mainwindow.h
#include <QThread>
#include "bmsworker.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
private:
    QThread   *m_thread;
    BmsWorker *m_worker;

private slots:
    void onBmsDataUpdated(int totalV, int current, int soc);
    void onCanFrameReceived(int seq, quint32 canId, const QByteArray &data);
};

// mainwindow.cpp 构造函数
m_thread = new QThread(this);
m_worker = new BmsWorker;          // 无 parent
m_worker->moveToThread(m_thread);

// 子线程 → 主线程：信号连接
connect(m_worker, &BmsWorker::bmsDataUpdated, this, &MainWindow::onBmsDataUpdated);
connect(m_worker, &BmsWorker::canFrameReceived, this, &MainWindow::onCanFrameReceived);
connect(m_worker, &BmsWorker::connectionChanged, this, [this](bool ok){
    ui->label_link_status->setText(ok ? "已连接" : "未连接");
    setDotStatus(ui->label_link_dot, ok);
});
connect(m_worker, &BmsWorker::logMessage, this, [this](const QString &msg){
    ui->text_log->appendPlainText(msg);
});

// 线程结束时自动清理工作对象
connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

m_thread->start();   // 启动线程

// 主线程 → 子线程：触发打开串口
void MainWindow::on_btn_open_clicked()
{
    QString port = ui->comboBox_port->currentData().toString();
    qint32 baud  = ui->comboBox_baud->currentText().toInt();
    QMetaObject::invokeMethod(m_worker, "openPort",
        Qt::QueuedConnection,
        Q_ARG(QString, port),
        Q_ARG(qint32, baud));
}

// 主线程接收解析结果，更新 UI（在主线程执行，安全）
void MainWindow::onBmsDataUpdated(int totalV, int current, int soc)
{
    ui->val_total_v->setText(QString::number(totalV) + " mV");
    ui->val_total_i->setText(QString::number(current) + " A");
    ui->val_soc->setText(QString::number(soc) + " %");
    ui->progress_soc->setValue(soc);
}

// 主线程接收 CAN 帧，添加到表格
void MainWindow::onCanFrameReceived(int seq, quint32 canId, const QByteArray &data)
{
    int row = ui->table_can->rowCount();
    ui->table_can->insertRow(row);
    ui->table_can->setItem(row, 0, new QTableWidgetItem(QString::number(seq)));
    ui->table_can->setItem(row, 2, new QTableWidgetItem(
        QString("0x%1").arg(canId, 8, 16, QChar('0')).toUpper()));

    QString hex;
    for (char b : data)
        hex += QString("%1 ").arg((quint8)b, 2, 16, QChar('0')).toUpper();
    ui->table_can->setItem(row, 5, new QTableWidgetItem(hex.trimmed()));

    if (ui->chk_continue->isChecked())
        ui->table_can->scrollToBottom();
}

// 析构：安全退出线程
MainWindow::~MainWindow()
{
    m_thread->quit();
    m_thread->wait(5000);
    delete ui;
}
```

---

## 9. 速查附录

### 9.1 关键 API 速查

```cpp
// QThread 生命周期
thread->start();                  // 启动
thread->quit();                   // 退出事件循环（安全）
thread->wait(timeout);            // 等待结束
thread->terminate();              // 强制终止（危险，避免）
thread->isRunning();              // 是否运行中
thread->requestInterruption();    // 请求中断
thread->isInterruptionRequested();// 检查中断请求

// moveToThread
worker->moveToThread(thread);     // 移动到目标线程
thread->start();                  // 启动后 worker 的槽在子线程执行

// 跨线程调用
QMetaObject::invokeMethod(obj, "slotName",
    Qt::QueuedConnection,
    Q_ARG(Type, value));

// 线程安全
QMutex mutex;
QMutexLocker locker(&mutex);      // RAII 加锁
```

### 9.2 信号连接类型速查

```cpp
// 默认（推荐，自动判断）
connect(sender, &Sender::signal, receiver, &Receiver::slot);

// 显式指定队列连接（跨线程）
connect(sender, &Sender::signal, receiver, &Receiver::slot,
        Qt::QueuedConnection);

// 阻塞队列连接（发送者等待槽执行完，慎用）
connect(sender, &Sender::signal, receiver, &Receiver::slot,
        Qt::BlockingQueuedConnection);
```

### 9.3 决策流程图

```
是否需要多线程？
├── 否 → 主线程 + QTimer / 异步 API
└── 是 → 任务类型？
         ├── 单一持续循环 → 继承 QThread 重写 run()
         └── 需响应请求/多任务 → QObject + moveToThread
```

### 9.4 检查清单

上线前确认：
- [ ] 子线程中没有任何 UI 控件操作
- [ ] `QSerialPort` / `QTimer` 等对象在使用线程中创建
- [ ] 析构时先 `quit()` 再 `wait()`
- [ ] 工作对象通过 `finished` → `deleteLater` 自动释放
- [ ] 跨线程自定义类型已 `qRegisterMetaType`
- [ ] 共享数据有 `QMutex` 保护
- [ ] 没有在子线程调用自身的 `wait()`

---

**文档完。** BMS 项目中建议采用第 8 章的 `BmsWorker + moveToThread` 架构：串口收发、协议解析都在子线程，主线程只负责 UI 更新，保证界面流畅不卡顿。
