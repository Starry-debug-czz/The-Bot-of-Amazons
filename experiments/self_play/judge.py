#!/usr/bin/env python3
"""Run parallel self-play matches between two Botzone-compatible binaries."""

import argparse
import os
import subprocess
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path


LOSS_MOVE = "-1 -1 -1 -1 -1 -1"


def run_bot(executable: str, turn_id: int, moves: list[str], timeout: float):
    input_text = f"{turn_id}\n" + "\n".join(moves) + "\n"
    try:
        result = subprocess.run(
            [executable],
            input=input_text,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            timeout=timeout,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None

    if result.returncode != 0:
        return None
    return result.stdout.strip()


def play_one_game(
    game_id: int,
    bot_a: str,
    bot_b: str,
    name_a: str,
    name_b: str,
    timeout: float,
):
    """Play one game and alternate which candidate moves first."""
    if game_id % 2:
        black, white = bot_a, bot_b
        black_name, white_name = name_a, name_b
    else:
        black, white = bot_b, bot_a
        black_name, white_name = name_b, name_a

    black_history = [LOSS_MOVE]
    white_history: list[str] = []

    for turn in range(1, 200):
        black_move = run_bot(black, turn, black_history, timeout)
        if not black_move or black_move == LOSS_MOVE:
            return white_name

        white_history.append(black_move)
        black_history.append(black_move)

        white_move = run_bot(white, turn, white_history, timeout)
        if not white_move or white_move == LOSS_MOVE:
            return black_name

        black_history.append(white_move)
        white_history.append(white_move)

    return "Draw"


def parse_args():
    parser = argparse.ArgumentParser(description="并行运行两个 Botzone Bot 的自对弈")
    parser.add_argument("--bot-a", required=True, type=Path, help="候选 A 的可执行文件")
    parser.add_argument("--bot-b", required=True, type=Path, help="候选 B 的可执行文件")
    parser.add_argument("--rounds", type=int, default=20, help="总对局数（默认：20）")
    parser.add_argument(
        "--workers",
        type=int,
        default=min(4, os.cpu_count() or 1),
        help="并行进程数（默认：最多 4）",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="单次 Bot 调用超时秒数（默认：10）",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    bot_a = args.bot_a.expanduser().resolve()
    bot_b = args.bot_b.expanduser().resolve()

    for executable in (bot_a, bot_b):
        if not executable.is_file():
            raise SystemExit(f"找不到 Bot 可执行文件：{executable}")

    if args.rounds < 1 or args.workers < 1 or args.timeout <= 0:
        raise SystemExit("rounds、workers 和 timeout 必须为正数")

    name_a = bot_a.stem
    name_b = bot_b.stem
    print(
        f"--- {name_a} vs {name_b}："
        f"{args.rounds} 局，并行数 {args.workers} ---"
    )

    results = []
    with ProcessPoolExecutor(max_workers=args.workers) as executor:
        futures = [
            executor.submit(
                play_one_game,
                game_id,
                str(bot_a),
                str(bot_b),
                name_a,
                name_b,
                args.timeout,
            )
            for game_id in range(1, args.rounds + 1)
        ]
        for index, future in enumerate(futures, start=1):
            winner = future.result()
            results.append(winner)
            print(f"[局次 {index:02d}] 胜者：{winner}")

    wins_a = results.count(name_a)
    wins_b = results.count(name_b)
    draws = results.count("Draw")
    print("\n" + "=" * 50)
    print(f"{name_a}: {wins_a} 胜 | {wins_a / args.rounds * 100:.1f}%")
    print(f"{name_b}: {wins_b} 胜 | {wins_b / args.rounds * 100:.1f}%")
    print(f"Draw: {draws}")
    print("=" * 50)


if __name__ == "__main__":
    main()
