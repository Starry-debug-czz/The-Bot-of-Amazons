# 自对弈实验

这里保存了 Botzone 版本在参数调试过程中的历史候选实现：

- `original.cpp`：基准版本。
- `t1.cpp`、`t2.cpp`、`test.cpp`：不同参数或搜索策略的候选版本。
- `judge.py`：让两个候选程序交换先后手并行对弈，统计胜场。

这些文件存在较多重复代码，目的是保留当时可独立编译、可复现实验的快照；正式入口位于仓库的 `src/` 目录。

## 构建候选程序

在仓库根目录执行：

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DZZMAZON_BUILD_EXPERIMENTS=ON
cmake --build build --parallel
```

候选程序会生成到 `build/bin/`。

## 运行对弈

```bash
python3 experiments/self_play/judge.py \
  --bot-a build/bin/original \
  --bot-b build/bin/t1 \
  --rounds 20 \
  --workers 4
```

使用 `python3 experiments/self_play/judge.py --help` 查看全部参数。每次调用 Bot 的默认超时时间为 10 秒，可用 `--timeout` 调整。
