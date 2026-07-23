# BMS CAN 监控上位机 - UI 界面设计方案

> 阶段目标：**先通过 `mainwindow.ui` 把界面摆好，配合 QSS 样式表**，不写底层逻辑。
> 下一步再处理串口通信、CAN 解析、数据刷新等业务代码。

---

## 一、界面结构分析

### 1.1 区域划分（参照截图）

| 编号 | 区域 | 位置 | 主要作用 |
|---|---|---|---|
| A | 标题栏 | 顶部 Row0 | 电池图标 + 系统标题 + 连接状态指示 |
| B | 工具栏 | Row1 | 串口/波特率选择 + 打开/关闭 + 帧类型 |
| C | 单体电压卡片 | Row2 左半 | 显示 12 节电池的单体电压（mV） |
| D | 系统状态卡片 | Row2 右半 | 总电压、电流、SOC、温度、充放电等 |
| E | CAN 报文列表 | Row3 | 接收/发送的 CAN 帧记录（表格） |
| F | 通信日志 | Row4 | 黑色文本框，滚动显示运行日志 |

### 1.2 整体布局

延续现有 `mainwindow.ui` 的 **`QGridLayout` 5 行结构**（已有 `rowstretch="1,1,5,4,2"`），在每个单元格内用**嵌套布局**（`QHBoxLayout` / `QVBoxLayout` / `QGridLayout`）继续细分。

整体骨架：

```
centralwidget
└── QGridLayout (gridLayout)
    ├── Row0 Col0 → QWidget (widget_7)  ← 标题栏（A）
    ├── Row1 Col0 → QWidget (widget_6)  ← 工具栏（B）
    ├── Row2 Col0 → QWidget (widget_2)  ← 左侧单体电压（C）
    ├── Row2 Col1 → QWidget (widget_3)  ← 右侧系统状态（D）
    ├── Row3 Col0 → QWidget (widget)   ← CAN 报文列表（E，跨整行）
    └── Row4 Col0 → QWidget (widget_4)  ← 通信日志（F，跨整行）
```

> 现有 ui 中部分 widget 已存在，可改名/复用，不必全部新建。

---

## 二、控件清单

| 区域 | 控件类型 | objectName | 数量 | 说明 |
|---|---|---|---|---|
| 标题栏 A | QLabel | `label_dianchi` | 1 | 已有，电池图标（`image: url(:/icons/dianchi.png)`） |
| 标题栏 A | QLabel | `label_title` | 1 | 系统标题文字（"BMS 电池管理系统 - CAN 监控"） |
| 标题栏 A | QLabel | `label_link_status` | 1 | "已连接"文字 |
| 标题栏 A | QLabel | `label_link_dot` | 1 | 状态点（绿色圆点） |
| 标题栏 A | QLabel | `label_ms` | 1 | "连接: 579 ms" |
| 标题栏 A | QLabel | `label_ms_dot` | 1 | 状态点 |
| 工具栏 B | QLabel | `label_port` | 1 | "串口:" 文字 |
| 工具栏 B | QComboBox | `comboBox_port` | 1 | COM 口号下拉 |
| 工具栏 B | QLabel | `label_baud` | 1 | "波特率:" 文字 |
| 工具栏 B | QComboBox | `comboBox_baud` | 1 | 波特率下拉 |
| 工具栏 B | QPushButton | `btn_open` | 1 | "打开串口" 绿色 |
| 工具栏 B | QPushButton | `btn_close` | 1 | "关闭串口" 红色 |
| 工具栏 B | QLabel | `label_frame` | 1 | "帧类型:" |
| 工具栏 B | QComboBox | `comboBox_frame` | 1 | 标准/扩展帧 |
| 单体电压 C | QGroupBox | `group_voltage` | 1 | "单体电压 (mV)" 容器 |
| 单体电压 C | QLabel | `cell_1` ~ `cell_12` | 12 | 各节电池电压值（每节一个 label） |
| 系统状态 D | QGroupBox | `group_status` | 1 | "系统状态" 容器 |
| 系统状态 D | QLabel | `val_total_v` | 1 | 总电压数值 |
| 系统状态 D | QLabel | `val_total_i` | 1 | 电流数值 |
| 系统状态 D | QLabel | `val_soc` | 1 | SOC 百分比 |
| 系统状态 D | QProgressBar | `progress_soc` | 1 | SOC 进度条（与 val_soc 配合） |
| 系统状态 D | QLabel | `val_charge` | 1 | 充放电状态 |
| 系统状态 D | QLabel | `val_temp` | 1 | 温度 |
| 系统状态 D | QLabel | `val_insulation` | 1 | 绝缘状态 |
| CAN 报文 E | QGroupBox | `group_can` | 1 | "CAN 报文列表" 容器 |
| CAN 报文 E | QTableWidget | `table_can` | 1 | 6 列表格 |
| CAN 报文 E | QCheckBox | `chk_continue` | 1 | "连续显示" |
| CAN 报文 E | QCheckBox | `chk_show_frame` | 1 | "显示收发帧" |
| CAN 报文 E | QLabel | `label_frame_count` | 1 | "总帧数: 511" |
| CAN 报文 E | QPushButton | `btn_clear_can` | 1 | "清空列表" |
| 通信日志 F | QGroupBox | `group_log` | 1 | "通信日志" 容器 |
| 通信日志 F | QPlainTextEdit | `text_log` | 1 | 黑底日志框 |
| 通信日志 F | QCheckBox | `chk_auto_scroll` | 1 | "自动滚动" |
| 通信日志 F | QPushButton | `btn_clear_log` | 1 | "清空日志" |

> **命名规范建议**：使用 `功能_具体` 形式（如 `btn_open`、`label_total_v`），便于 QSS 精确选择。

---

## 三、Designer 实施步骤

> 所有操作均在 `mainwindow.ui` 中以可视化方式完成，无需手写 XML。

### Step 1：调整 GridLayout 行数

现有 `gridLayout` 已经是 5 行结构，但各行 `rowstretch` 不完全匹配目标界面。修改 `rowstretch` 建议为 `"1,1,5,4,2"`（标题/工具栏矮，数据区高），列保持 1 列即可，Row2 在内部再用嵌套布局分左右两栏。

### Step 2：标题栏 Row0（widget_7）

`widget_7` 已存在，内部已是 `QHBoxLayout`，结构：

```
horizontalLayout_3
├── horizontalSpacer (左)
├── horizontalLayout_2 (stretch="0,0,0,0")
│   ├── label_dianchi    ← 已有电池图标 (minimumSize=64x64, QSS: image)
│   ├── label_3  "BMS"   ← 已有
│   ├── label_2  "电池管理系统"   ← 已有
│   └── label    "- CAN 监控"    ← 已有
└── horizontalSpacer_2 (右)
```

**追加**：在 `horizontalLayout_2` 之后再加一个 `QHBoxLayout` 用于右侧状态：
- `label_ms`  "连接: 0 ms"  +  `label_ms_dot`(绿色圆点)
- `label_link_status` "已连接" + `label_link_dot`(绿色圆点)

并把 `label_ms_dot` / `label_link_dot` 的 `minimumSize` 设为 `12x12`，QSS 中画成圆形。

### Step 3：工具栏 Row1（widget_6）

清空 `widget_6` 内残留的 label_4，新建水平布局：

```
QHBoxLayout
├── label_port "串口:" + comboBox_port
├── label_baud "波特率:" + comboBox_baud
├── btn_open "打开串口"
├── btn_close "关闭串口"
├── horizontalSpacer (弹簧)
├── label_frame "帧类型:" + comboBox_frame
```

`comboBox_port` 默认项：`COM1, COM2, COM3, COM4, COM5`（也可留空由代码填充）。
`comboBox_baud` 默认项：`9600, 19200, 38400, 57600, 115200, 250000, 500000, 1000000`，默认选 `115200`。
`comboBox_frame` 默认项：`标准帧(11位), 扩展帧(29位)`。

按钮宽度统一 `minimumWidth=80`，`maximumHeight=28`。

### Step 4：单体电压卡片 Row2 Col0（widget_2）

清空 `widget_2`，添加：

```
QGroupBox "单体电压 (mV)" (group_voltage)
└── QGridLayout (4列 × 3行)
    ├── 第1节: cell_1=3780    第2节: cell_2=3828    第3节: cell_3=--     第4节: cell_4=--
    ├── 第5节: cell_5=--      第6节: cell_6=3734    第7节: cell_7=3729   第8节: cell_8=--
    ├── 第9节: cell_9=3869    第10节: cell_10=3733  第11节: cell_11=3777 第12节: cell_12=3798
```

每个 cell label 的 `minimumWidth=80`，`alignment=AlignCenter`。
QSS 中给 `.cell` 类（或每个 `cell_x` 单独指定）加绿色字体 + 边框：

```css
QLabel[objectName^="cell_"]{
    color: #37ff4e;
    font-size: 16px;
    font-weight: bold;
    border: 1px solid #ccc;
    padding: 4px;
    qproperty-alignment: AlignCenter;
}
```
> 使用属性选择器 `[objectName^="cell_"]` 一次匹配所有以 `cell_` 开头的 label，无需重复写。

### Step 5：系统状态卡片 Row2 Col1（widget_3）

清空 `widget_3`，添加：

```
QGroupBox "系统状态" (group_status)
└── QGridLayout
    ├── row0: "总电压"  | val_total_v "34002 mV"   |  progress_soc (合并显示用)
    ├── row1: "电流"    | val_total_i "82 A"
    ├── row2: "SOC"     | val_soc "21.5 %"  +  progress_soc
    ├── row3: "充放电"  | val_charge "充电"  + 充电点
    ├── row4: "温度"    | val_temp "35 °C"
    ├── row5: "绝缘"    | val_insulation "正常"
    ├── row6: "单体状态" | val_cells "已连接: 12 / 12"
    ├── row7: "..."     | ...
```

`progress_soc` 的 `maximum=100`，`value=21`，`textVisible=false`，高度 8px。QSS 中设为绿色填充。

### Step 6：CAN 报文列表 Row3（widget）

清空 `widget`，添加：

```
QGroupBox "CAN 报文列表" (group_can)
└── QVBoxLayout
    ├── QTableWidget (table_can) — 占满上方
    │   - 列：["序号", "时间戳", "CAN ID", "方向", "DLC", "数据 (HEX)"]
    │   - 列宽策略：交互式可调
    │   - alternatingRowColors: true
    │   - selectionBehavior: SelectRows
    │   - editTriggers: NoEditTriggers
    │   - verticalHeaderVisible: false
    │
    └── QHBoxLayout (底部工具条)
        ├── chk_continue  "连续显示"
        ├── chk_show_frame "显示收发帧"
        ├── horizontalSpacer
        ├── label_frame_count "总帧数: 0"
        └── btn_clear_can "清空列表"
```

### Step 7：通信日志 Row4（widget_4）

清空 `widget_4`，添加：

```
QGroupBox "通信日志" (group_log)
└── QVBoxLayout
    ├── QPlainTextEdit (text_log)
    │   - readOnly: true
    │   - font: 10pt "Consolas"
    │   - 黑色背景由 QSS 设置
    │
    └── QHBoxLayout
        ├── chk_auto_scroll "自动滚动"  (默认 checked)
        ├── horizontalSpacer
        └── btn_clear_log "清空日志"
```

---

## 四、QSS 样式表（追加到 `style.qss`）

```css
/* ===== 全局标题样式 ===== */
QLabel#label_title, QLabel#label_3, QLabel#label_2, QLabel#label{
    color: #37ff4e;
    font-size: 25px;
    font-weight: bold;
}

/* ===== 标题栏电池图标 ===== */
QLabel#label_dianchi{
    min-width: 64px;
    max-width: 64px;
    min-height: 64px;
    max-height: 64px;
    image: url(:/icons/dianchi.png);
}

/* ===== 状态点（绿/红圆点） ===== */
QLabel#label_link_dot, QLabel#label_ms_dot{
    min-width: 12px;
    max-width: 12px;
    min-height: 12px;
    max-height: 12px;
    border-radius: 6px;
    background-color: #37ff4e;
}
QLabel[status="off"]{
    background-color: #ff4e4e;
}

/* ===== 按钮颜色 ===== */
QPushButton#btn_open{
    background-color: #2d8cf0;   /* 也可改为绿色 #19be6b */
    color: white;
    border-radius: 4px;
    padding: 4px 16px;
    min-height: 24px;
}
QPushButton#btn_open:hover{ background-color: #57a8f8; }

QPushButton#btn_close{
    background-color: #ed3f14;
    color: white;
    border-radius: 4px;
    padding: 4px 16px;
    min-height: 24px;
}
QPushButton#btn_close:hover{ background-color: #ff6347; }

/* ===== 工具栏标签 ===== */
QLabel#label_port, QLabel#label_baud, QLabel#label_frame{
    color: #555;
    font-size: 13px;
}

/* ===== 单体电压（用属性选择器一次性匹配所有 cell_*） ===== */
QLabel[objectName^="cell_"]{
    color: #37ff4e;
    font-size: 16px;
    font-weight: bold;
    border: 1px solid #e0e0e0;
    border-radius: 3px;
    padding: 6px 8px;
    background: #fafafa;
}

/* ===== 系统状态数值 ===== */
QLabel#val_total_v, QLabel#val_total_i, QLabel#val_soc,
QLabel#val_charge, QLabel#val_temp, QLabel#val_insulation{
    color: #37ff4e;
    font-size: 16px;
    font-weight: bold;
}

/* ===== SOC 进度条 ===== */
QProgressBar#progress_soc{
    border: 1px solid #ccc;
    border-radius: 4px;
    background: #f0f0f0;
    height: 10px;
    text-align: center;
}
QProgressBar#progress_soc::chunk{
    background-color: #37ff4e;
    border-radius: 3px;
}

/* ===== GroupBox 标题样式 ===== */
QGroupBox{
    font-size: 14px;
    font-weight: bold;
    color: #333;
    border: 1px solid #d0d0d0;
    border-radius: 4px;
    margin-top: 10px;
    padding-top: 10px;
}
QGroupBox::title{
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6px;
    left: 10px;
}

/* ===== CAN 报文表格 ===== */
QTableWidget#table_can{
    gridline-color: #e0e0e0;
    background: white;
    alternate-background-color: #f7f9fc;
    selection-background-color: #2d8cf0;
    selection-color: white;
}
QHeaderView::section{
    background-color: #2d8cf0;
    color: white;
    padding: 6px;
    border: none;
    font-weight: bold;
}
QTableWidget#table_can QTableCornerButton::section{
    background-color: #2d8cf0;
    border: none;
}

/* ===== 通信日志 ===== */
QPlainTextEdit#text_log{
    background-color: #1e1e1e;
    color: #dcdcdc;
    border: 1px solid #333;
    font-family: "Consolas";
    font-size: 10pt;
    selection-background-color: #264f78;
}
```

> 颜色风格延续已有 `#37ff4e` 绿；按钮/表头用蓝色 `#2d8cf0`；日志框深色背景。

---

## 五、Stage 1 验收清单（只摆 UI，不写逻辑）

按下面 6 步验证 UI：

- [ ] 标题栏：左侧电池图标完整显示 + 右侧两个绿点 + 状态文字
- [ ] 工具栏：串口、波特率下拉可点击；打开按钮绿、关闭按钮红
- [ ] 单体电压：12 个 cell 标签按 4×3 网格排布，绿色加粗
- [ ] 系统状态：标签 + 数值 + 进度条布局合理
- [ ] CAN 报文列表：6 列表格渲染正确，蓝底白字表头，奇偶行交替背景
- [ ] 通信日志：黑底白字，字体为等宽 Consolas

---

## 六、下一步：底层逻辑（Stage 2 预览）

UI 摆好之后再做的关键工作：

| 模块 | 涉及控件 | 关键 API |
|---|---|---|
| 串口扫描 | `comboBox_port` | `QSerialPortInfo::availablePorts()` |
| 串口打开/关闭 | `btn_open` / `btn_close` | `QSerialPort::open()` / `close()` |
| CAN 协议解析 | 串口数据接收 | 自定义解析逻辑（标准/扩展帧） |
| 周期刷新 | `val_*` 各 label、`cell_1..12` | `QTimer` 定时触发 |
| 报文入库 | `table_can` | `QTableWidget::insertRow()` |
| 日志追加 | `text_log` | `QPlainTextEdit::appendPlainText()` |
| 状态点切换 | `label_link_dot` 等 | `qApp->setStyleSheet()` 或 `setProperty("status", "off")` + 动态 QSS |

> **本阶段（Stage 1）目标：把所有控件摆好、QSS 配好、运行后静态界面与截图一致即可，不需要任何数据交互。**
