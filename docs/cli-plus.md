# ZZmazon Studio（`zzmazon_cli_plus`）

`zzmazon_cli_plus` 是独立于原终端版的 Qt 6 图形客户端。它保留亚马逊棋三段式落子规则，同时用鼠标交互、两阶段整格高亮和分段动画替代坐标文本输入。

![ZZmazon Studio 主界面](assets/cli-plus/studio.png)

## 主要体验

- **严格的两阶段落子**：第一阶段选择并移动棋子，棋子落稳后才进入第二阶段选择箭点。
- **整格高亮提示**：合法位置以整格色块标出，第一阶段是青绿色的移动终点，第二阶段是朱砂色的箭点落位。
- **悬停浮起反馈**：悬停的格子会整格浮起并罩上更强的光环；选中的棋子在原位轻轻悬浮待发。
- **自然落子动画**：点击终点后，棋子经历抬升、弧线移动和落下；点击箭点后，箭再独立沿路径飞行并落成障碍。
- **轻量音效**：棋子落定、放箭与中靶各有对应的音效，由程序即时合成，不依赖音频文件。
- **人机与双人模式**：支持执黑/执白、本地双人和三档 AI 思考时间。
- **异步 AI**：搜索在后台线程执行，不会卡住窗口动画和重绘。
- **对局工具**：新对局、悔棋、路线提示、走子记录和终局结算卡片。
- **高 DPI 矢量绘制**：棋盘、棋子、皇冠、障碍和高亮均由 `QPainter` 绘制，窗口缩放时保持清晰。

## 第一阶段：移动棋子

![青绿色整格高亮的移动阶段](assets/cli-plus/selection-paths.png)

点击己方棋子后，所有合法终点都以青绿色整格高亮铺开；悬停其中一格，它会浮起并罩上光环。确认后棋子先在原地轻轻悬浮蓄势，随后滑行至终点自然落下。

## 第二阶段：发射障碍物

![朱砂色整格高亮的射箭阶段](assets/cli-plus/arrow-paths.png)

棋子落稳后才进入第二阶段，所有合法箭点改以朱砂色整格高亮标出，与第一阶段的青绿色泾渭分明。确认后箭破空飞行，在落点凝成障碍印；它也可以射向棋子刚刚腾出的那格。

## 构建要求

- CMake 3.16 或更高版本；
- 支持 C++17 的编译器；
- Qt 6 的 `Widgets` 和 `Concurrent` 模块；
- 运行 GUI 交互测试时可选安装 Qt 6 `Test` 模块。

Qt GUI 默认开启。如果 CMake 未找到 Qt 6，会显示警告并继续构建原终端版和 Botzone 版。也可以显式关闭 GUI：

```bash
cmake -S . -B build -DZZMAZON_BUILD_GUI=OFF
```

## 构建与运行

在仓库根目录执行：

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DZZMAZON_BUILD_GUI=ON
cmake --build build --parallel --target zzmazon_cli_plus

./build/bin/zzmazon_cli_plus
```

快捷键：

- `Ctrl+N`：开始新对局；
- `Ctrl+Z`：悔棋；
- `Esc` 或鼠标右键：取消当前棋子/终点选择。

## 代码结构

| 文件 | 职责 |
| --- | --- |
| `src/zzmazon_cli_plus.cpp` | 应用入口、主题加载和窗口初始化。 |
| `src/cli_plus/amazons_game.*` | 棋盘状态、规则验证、历史快照和异步 AI 选步算法。 |
| `src/cli_plus/board_widget.*` | 棋盘绘制、两阶段鼠标状态机、整格高亮、落子动画与音效。 |
| `src/cli_plus/main_window.*` | 对局模式、后台 AI、侧栏、走子记录和胜负流程。 |
| `src/cli_plus/theme.qss` | 浅色宣纸水墨主题。 |
| `tests/cli_plus_*_test.cpp` | 规则/AI 与鼠标交互/动画回归测试。 |

增强版内置 AI 使用皇后距离场、领地、机动性与对手回应采样进行选步。其实现与 Botzone 单文件版彼此独立，后续可以在保持 GUI 不变的情况下替换为统一的 MCTS 引擎。

## 测试与截图

```bash
ctest --test-dir build --output-on-failure -R cli_plus
```

应用还提供离屏截图参数，便于检查不同平台上的 UI：

```bash
QT_QPA_PLATFORM=offscreen \
  ./build/bin/zzmazon_cli_plus --screenshot /tmp/zzmazon.png
```
