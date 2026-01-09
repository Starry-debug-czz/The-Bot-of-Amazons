# ZZmazon - The Bot of Amazons

[English](#english) | [中文](#中文)

## English

### About

ZZmazon is a computer game program for the Game of the Amazons, open-sourced in January 2026. This implementation provides:

- Complete rule implementation of the Game of the Amazons
- Command-line interface for interactive play
- Basic AI implementation
- Clean, readable Python code suitable for learning and extension

### Game Rules

The Game of the Amazons is a two-player strategy game played on a 10×10 chessboard:

1. Each player controls 4 amazons (pieces that move like chess queens)
2. On each turn, a player must:
   - Move one of their amazons (like a chess queen - any number of squares in any straight or diagonal direction)
   - Shoot an arrow from the new position (also like a chess queen)
   - The arrow permanently blocks the square it lands on
3. The player who cannot make a legal move loses

### Installation

```bash
# Clone the repository
git clone https://github.com/Starry-debug-czz/The-Bot-of-Amazons.git
cd The-Bot-of-Amazons

# Install the package
pip install -e .
```

### Usage

**Interactive mode** (human vs human):
```bash
python zzmazon.py
```

**AI Demo mode** (AI vs AI):
```bash
python zzmazon.py --ai-demo
```

**After installation as package**:
```bash
zzmazon
```

### How to Play

1. The board is displayed with coordinates (0-9 for both rows and columns)
2. White (W) always moves first
3. Enter your move in the format: `from_row from_col to_row to_col arrow_row arrow_col`
   - Example: `3 0 5 2 5 4` moves amazon from (3,0) to (5,2) and shoots arrow to (5,4)
4. Type `h` to see available moves
5. Type `q` to quit

### Board Legend

- `W` = White Amazon
- `B` = Black Amazon  
- `X` = Burned square (arrow impact)
- `.` = Empty square

### Development

This is a basic implementation suitable for:
- Learning the Game of the Amazons rules
- Implementing advanced AI algorithms (minimax, alpha-beta pruning, MCTS)
- Botzone integration
- Tournament play

### License

MIT License - see [LICENSE](LICENSE) file for details.

---

## 中文

### 关于

ZZmazon 是一个亚马逊棋对弈程序，于2026年1月开源。本实现提供：

- 完整的亚马逊棋规则实现
- 命令行交互界面
- 基础AI实现
- 清晰易读的Python代码，适合学习和扩展

### 游戏规则

亚马逊棋是一个双人策略游戏，在10×10的棋盘上进行：

1. 每位玩家控制4个亚马逊棋子（移动方式类似国际象棋的皇后）
2. 每回合，玩家必须：
   - 移动一个己方亚马逊棋子（如国际象棋皇后 - 沿直线或对角线移动任意格数）
   - 从新位置发射一支箭（也如国际象棋皇后）
   - 箭会永久阻挡其落点的格子
3. 无法进行合法移动的玩家输掉游戏

### 安装

```bash
# 克隆仓库
git clone https://github.com/Starry-debug-czz/The-Bot-of-Amazons.git
cd The-Bot-of-Amazons

# 安装包
pip install -e .
```

### 使用方法

**交互模式**（人类对人类）：
```bash
python zzmazon.py
```

**AI演示模式**（AI对AI）：
```bash
python zzmazon.py --ai-demo
```

**安装为包后**：
```bash
zzmazon
```

### 游戏说明

1. 棋盘显示坐标（行列均为0-9）
2. 白方（W）总是先手
3. 输入移动格式：`起始行 起始列 目标行 目标列 箭行 箭列`
   - 例如：`3 0 5 2 5 4` 将棋子从(3,0)移动到(5,2)，箭射向(5,4)
4. 输入 `h` 查看可用移动
5. 输入 `q` 退出游戏

### 棋盘图例

- `W` = 白方亚马逊
- `B` = 黑方亚马逊
- `X` = 燃烧格（箭的落点）
- `.` = 空格

### 开发

这是一个基础实现，适合：
- 学习亚马逊棋规则
- 实现高级AI算法（极小极大、alpha-beta剪枝、蒙特卡洛树搜索）
- Botzone平台集成
- 锦标赛对弈

### 许可证

MIT许可证 - 详见 [LICENSE](LICENSE) 文件。
