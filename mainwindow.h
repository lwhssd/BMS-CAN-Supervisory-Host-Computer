#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QHBoxLayout>
#include <QDebug>
#include <QLabel>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QPlainTextEdit>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    QSerialPort * serial;

    MainWindow(QWidget *parent = nullptr);
    void refreshSerialPorts(void);
    void setDotStatus(QLabel* dot, bool online);
    ~MainWindow();

private slots:
    void on_btn_refrsh_clicked();

    void on_btn_open_clicked();

    void on_btn_close_clicked();

    void onSerialDataReady();   // 串口收到数据时触发
    void onLinkTimeout();   // 计时器超时槽：刷新连接时长

    void on_radioButton_clicked(bool checked);

    void on_radioButton_2_clicked(bool checked);

    void on_btn_clear_can_clicked();  // 清空 CAN 表格

private:
    Ui::MainWindow *ui;
    QTimer       *m_timer;    // 计时定时器（每秒触发）
    QElapsedTimer m_elapsed;  // 记录连接起始时刻
    QByteArray    m_recvBuf;  // 接收累计缓冲（处理分包/粘包用）
    int           m_frameSeq = 0;  // 表格行号计数器

    QString bytesToHex(const QByteArray &data);  // 字节数组转 HEX 字符串
    void sendMosCommand(quint8 cmd, bool on);    // 发送放电/充电 MOS 开关指令
    void parseBmsFrame(const QByteArray &frame); // 解析一帧 BMS 协议 (AA + seq + 6B data)
    void setCellVoltage(int cellNum, quint16 raw);// 更新单体电压 label
    void addFrameToTable(const QByteArray &frame, const QString &dir); // 原始帧写入表格
};

#endif // MAINWINDOW_H
