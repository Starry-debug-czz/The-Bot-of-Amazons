# 自对弈实验

这里保存了 ZZmazon 引擎在 Botzone 版上做参数搜索时的"现场":四个可独立编译的单文件快照,以及一个让它们互相对弈、统计胜负的裁判脚本。正式入口位于仓库的 `src/` 目录,与本目录无关。

## 演化链

```text
baseline            守擂基准:探索常数用 DYNAMIC_C_TABLE 多项式拟合查表,
                    渐进扩展阈值最细(600/800/1200 → 12/10/8),深层采样 400
 └─ prototype       改造原型:弃用查表,探索常数换成一条指数衰减公式;
                    扩展阈值放宽(800→8 · 12000→4),深层采样减半至 200,
                    Super-Mobility 权重暂复用机动性权重(w_params[4])
     └─ prototype_tuned     原型扫参:思考时间 997→965、抖动范围 0.01→0.02、
                            C 下界 0.16→0.20,检验公式版的参数容错(仅差三行)
         └─ prototype_nofree    内存实验:注释掉 UCTNode 析构与 delete root。
                                Botzone 每回合新起进程,不释放内存并无危害,
                                反而省去释放开销(本地长局则不宜此配置)
             └─ 定稿 src/zzmazon_botzone.cpp:探索常数 C 固定 0.2、深层采样 350
```

**线上版本是 `prototype_nofree` 的直系后裔**,在此之上仅改了三处定稿参数。

## 参数对照

| 参数 | baseline | prototype | prototype_tuned | prototype_nofree | 线上版 |
| --- | --- | --- | --- | --- | --- |
| 思考时间 `MAX_TIME_MS` | 965 | 997 | 965 | 997 | 992 |
| 探索常数 C | 多项式拟合查表 | 指数公式,clamp [0.16, 0.35] | 同左,clamp [0.20, 0.35] | 同 prototype_tuned | 固定 0.2 |
| 渐进扩展阈值 | 600→12 · 800→10 · 1200→8 | 800→8 · 12000→4 | 同 prototype | 同 prototype | 同 prototype |
| 深层节点采样上限 | 400 | 200 | 200 | 200 | 350 |
| Super-Mobility 权重 | `w_params[5]` | 复用 `w_params[4]` | 同 prototype | 同 prototype | 同 prototype |
| 评估抖动 `AD2` | 0.02 | 0.01 | 0.02 | 0.02 | 0.02 |
| 释放 MCTS 节点内存 | 是 | 是 | 是 | **否** | 否 |

与 `src/zzmazon_botzone.cpp` 的行数距离(衡量血缘远近):`baseline` 52 行、`prototype` 13 行、`prototype_tuned` 11 行、`prototype_nofree` 7 行。

> 注意:`prototype_nofree` 的"不释放节点内存"只在每回合新起进程的平台上有意义;若把该配置搬到本地长期对局,内存会持续增长。另有一个先天怪癖:快照对"零走法历史"的输入不健壮(会崩溃),而裁判脚本的黑方首轮永远携带占位走法 `-1 -1 -1 -1 -1 -1`,故正常对弈不会触发;手工测试时请照此格式喂入。这些快照存在大量重复代码、彼此只差数行,正是当时逐项试错留下的痕迹——请勿在本目录继续开发;如需新实验,另立文件并在本 README 登记其定位。

## 构建候选程序

默认构建不会编译这些实验版,需要显式开启:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DZZMAZON_BUILD_EXPERIMENTS=ON
cmake --build build --parallel
```

四个候选会生成到 `build/bin/`:`baseline`、`prototype`、`prototype_tuned`、`prototype_nofree`。

## 运行对弈

```bash
python3 experiments/self_play/judge.py \
  --bot-a build/bin/baseline \
  --bot-b build/bin/prototype_nofree \
  --rounds 20 \
  --workers 4
```

裁判会把两个候选按 Botzone 协议对弈并轮流交换先后手,以抵消先手优势。使用 `python3 experiments/self_play/judge.py --help` 查看全部参数;每次调用 Bot 的默认超时为 10 秒,可用 `--timeout` 调整。
