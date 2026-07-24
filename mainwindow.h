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

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    void refreshSerialPorts();
    ~MainWindow();

private slots:
    void on_btn_refrsh_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
