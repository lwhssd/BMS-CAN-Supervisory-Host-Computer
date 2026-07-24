# BMS-CAN-Supervisory-Host-Computer

基于 Qt 开发的 **BMS（电池管理系统）串口监控上位机**，通过串口与下位机通信，实时显示电池组单体电压、总电压、电流、SOC、温度及 MOS 开关状态，并支持远程控制充放电 MOS 的开/关。

## 功能特性

- **串口通信**：自动扫描可用串口（识别 CH340 设备），可配置波特率，一键打开/关闭
- **实时数据监控**：
  - 15 节单体电压（支持未连接节显示 `--`）
  - 总电压、电流、SOC（进度条）、温度
  - 放电 MOS / 充电 MOS 实际状态同步显示
- **原始帧表格**：收发的每一帧数据实时写入表格，方向列红绿区分（Rx 绿 / Tx 红），支持连续显示、自动滚动、行数上限保护
- **MOS 远程控制**：点击界面开关即可发送指令控制充放电 MOS，下位机回传状态自动同步
- **连接计时**：连接成功后开始计时，断开后停止，主界面不卡顿
- **状态指示**：连接状态圆点（绿=已连接 / 红=未连接）
- **日志框**：深色等宽字体日志，自动滚动，限制最大行数
- **QSS 全局美化**：通过 `style.qss` 统一样式，深色监控风格

## 技术栈

| 项目 | 说明 |
|---|---|
| 语言 | C++11 |
| 框架 | Qt (Widgets) |
| 串口模块 | Qt SerialPort (`QSerialPort` / `QSerialPortInfo`) |
| 构建系统 | qmake |
| 样式 | QSS（类 CSS 语法） |
| 资源系统 | `.qrc` 嵌入式资源 |

## 项目结构

```
BMS/
├── BMS.pro                  # qmake 工程文件
├── main.cpp                 # 程序入口，加载全局 QSS 样式
├── mainwindow.h             # 主窗口类声明
├── mainwindow.cpp           # 主窗口逻辑（串口收发、协议解析、UI 更新）
├── mainwindow.ui            # Qt Designer 界面文件
├── style.qss                # 全局样式表
├── res.qrc                  # 资源文件（QSS + 图标）
├── icons/
│   ├── dianchi.png          # 电池图标
│   └── shuaxin.png          # 刷新图标
├── LICENSE                  # 开源许可证
├── README.md                # 本文档
├── UI设计文档.md             # 界面设计与控件布局说明
├── QSerialPort使用文档.md    # Qt 串口模块使用教程
└── QThread使用文档.md        # Qt 多线程使用教程
```

## 编译与运行

### 环境要求

- Qt 5.x 或 Qt 6.x（需包含 `serialport` 模块）
- C++11 及以上编译器（MSVC / MinGW / GCC 均可）

### 步骤

1. **获取源码**
   ```bash
   git clone <仓库地址>
   cd BMS
   ```

2. **用 Qt Creator 打开**
   - 打开 Qt Creator → 文件 → 打开文件或项目 → 选择 `BMS.pro`

3. **配置 Kit**
   - 选择已安装的 Qt 版本和编译器套件（Kit）

4. **构建并运行**
   - 点击左下角 ▶ 运行按钮，或按 `Ctrl+R`

### 命令行编译

```bash
qmake BMS.pro
make          # Linux/MinGW
nmake         # MSVC
```

## 界面布局

![](D:\Qt_projects\BMS\BMS\1784888587811.png)

## BMS 通信协议

### 帧格式

每帧固定 **8 字节**：

```
┌──────┬──────┬────────────────────┐
│ 帧头 │ 序号 │ 数据 (6 字节)       │
│ 0xAA │ seq  │ d0 d1 d2 d3 d4 d5  │
└──────┴──────┴────────────────────┘
```

### 下位机 → 上位机（接收帧）

| 序号 | 数据内容 | 字段说明 |
|---|---|---|
| `0x01` ~ `0x05` | 单体电压 | 每帧 3 节，每节 2 字节大端，单位 mV |
| | | seq=1 → 第1~3节，seq=2 → 第4~6节，... seq=5 → 第13~15节 |
| | | raw=0 表示该节未连接 |
| `0x06` | 系统状态 | 字段1(2B)：总电压 mV |
| | | 字段2(2B)：SOC（SOC_H << 8 \| SOC_L），单位 % |
| | | 字段3(2B)：电流，单位 mA |
| `0x07` | 温度与MOS | 字节1(1B)：温度 ℃ |
| | | 字节2(1B)：放电MOS状态（01=开，00=关） |
| | | 字节3(1B)：充电MOS状态（01=开，00=关） |
| | | 字节4~6：备用 |

**示例数据：**
```
AA 01 0F C9 0F C1 0F 8E    → 第1~3节: 4.041V / 4.033V / 3.982V
AA 02 0F 84 0F C8 0F AE    → 第4~6节: 3.972V / 4.040V / 4.014V
AA 03 0F B7 0F CF 0F C4    → 第7~9节: 4.023V / 4.047V / 4.036V
AA 04 00 00 00 00 00 00    → 第10~12节: 未连接
AA 05 00 00 00 00 00 00    → 第13~15节: 未连接
AA 06 8D 5C 00 54 00 02    → 总压36.188V / SOC 84% / 电流2mA
AA 07 1E 01 01 00 00 00    → 温度30℃ / 放电MOS开 / 充电MOS开
```

### 上位机 → 下位机（发送帧）

MOS 控制指令帧格式（4 字节）：

```
┌──────┬──────┬──────────┬──────┐
│ 帧头 │ 命令 │ 数据     │ 帧尾 │
│ 0xFF │ cmd  │ 01开/00关│ 0xEE │
└──────┴──────┴──────────┴──────┘
```

| 命令字 | 含义 |
|---|---|
| `0x01` | 放电 MOS 控制 |
| `0x02` | 充电 MOS 控制 |

**示例：** `FF 01 01 EE` = 打开放电 MOS

> ⚠️ 发送帧格式请按你的下位机实际协议调整 `sendMosCommand()` 中的字节构造。

## 代码架构

### 核心类

```
MainWindow (QMainWindow)
├── 串口管理
│   ├── serial (QSerialPort*)        串口对象
│   ├── refreshSerialPorts()         扫描可用串口
│   ├── on_btn_open_clicked()        打开串口
│   └── on_btn_close_clicked()       关闭串口
├── 数据接收与解析
│   ├── onSerialDataReady()          readyRead 信号槽，累计缓冲+拆帧
│   ├── parseBmsFrame()              按 0xAA 帧头解析协议
│   └── setCellVoltage()             更新单体电压 label
├── 数据发送
│   └── sendMosCommand()             构造并发送 MOS 控制帧
├── 表格管理
│   ├── addFrameToTable()            原始帧写入 table_can
│   └── on_btn_clear_can_clicked()   清空表格
├── 连接计时
│   ├── m_timer (QTimer*)            每秒触发刷新
│   ├── m_elapsed (QElapsedTimer)    记录连接起始时刻
│   └── onLinkTimeout()              计算并显示连接时长
└── 状态显示
    └── setDotStatus()               动态属性切换圆点颜色
```

### 数据流

```
┌─ 接收 ─────────────────────────────────────────────────┐
│ 下位机 → 串口 → readyRead → onSerialDataReady()        │
│   → 累计到 m_recvBuf → 按 0xAA 拆帧 → parseBmsFrame()  │
│   → 更新 cell_1~15 / label_31 / label_22 / label_24    │
│   → 更新 progress_soc / radioButton(DSG) / radioButton_2(CHG) │
│   → addFrameToTable(f, "Rx") 写入表格                   │
└─────────────────────────────────────────────────────────┘

┌─ 发送 ─────────────────────────────────────────────────┐
│ 用户点击 MOS 开关 → on_radioButton_clicked()           │
│   → sendMosCommand(cmd, on) → serial->write()          │
│   → addFrameToTable(frame, "Tx") 写入表格               │
│   → 下位机执行后回传帧07 → 界面同步真实状态              │
└─────────────────────────────────────────────────────────┘
```

### 关键设计点

1. **分包/粘包处理**：`onSerialDataReady` 把字节累计到 `m_recvBuf`，循环 `indexOf(0xAA)` 找帧头，凑齐 8 字节才解析一帧，残余半帧留待下次。
2. **MOS 状态同步无循环**：接收到的 MOS 状态用 `setChecked()` 同步到界面，`setChecked` 只触发 `toggled` 不触发 `clicked`，而发送槽连的是 `clicked`，因此不会回头重发指令。
3. **计时用 QTimer 不卡 UI**：每秒触发一次，槽函数只做字符串格式化 + `setText`，耗时微秒级。耗时操作（串口阻塞读）才需子线程。
4. **表格行数保护**：超过 500 行自动删最旧行，防止内存爆炸。
5. **QSS 动态属性切换**：`setDotStatus` 通过 `setProperty` + `unpolish/polish` 触发样式重算，实现圆点红绿切换。

## 配置说明

### 串口参数

在 `on_btn_open_clicked()` 中配置：

```cpp
serial->setDataBits(QSerialPort::Data8);      // 8 位数据位
serial->setParity(QSerialPort::NoParity);     // 无校验
serial->setStopBits(QSerialPort::OneStop);    // 1 位停止位
serial->setFlowControl(QSerialPort::NoFlowControl); // 无流控
```

波特率通过界面下拉框选择（默认 115200）。

### 样式表

全局样式在 `style.qss` 中定义，通过 `main.cpp` 在程序启动时加载：

```cpp
QFile file(":/style.qss");
file.open(QFile::ReadOnly);
qApp->setStyleSheet(QLatin1String(file.readAll()));
```

修改 `style.qss` 后需**重新编译**才生效（资源文件嵌入二进制）。

## 相关文档

| 文档 | 内容 |
|---|---|
| [UI设计文档.md](UI设计文档.md) | 界面区域划分、控件清单、Designer 搭建步骤、QSS 样式 |
| [QSerialPort使用文档.md](QSerialPort使用文档.md) | Qt 串口模块完整教程（15 章 + 速查表） |
| [QThread使用文档.md](QThread使用文档.md) | Qt 多线程使用教程（继承 run / moveToThread 两种方式） |

## 后续优化方向

- [ ] 将串口收发迁移至子线程（`moveToThread`），避免高频数据时卡 UI
- [ ] 增加电压/电流/温度历史曲线图（`QCustomPlot` 或 `QtCharts`）
- [ ] 增加数据导出功能（CSV / Excel）
- [ ] 完善保护告警（过压/欠压/过流/过温）的协议字段解析
- [ ] 增加发送帧的自定义编辑面板（手动输入 HEX 发送）
- [ ] 增加配置文件持久化（记住上次串口/波特率设置）

## 许可证

详见 [LICENSE](LICENSE) 文件。
