import os
import subprocess
from concurrent.futures import ProcessPoolExecutor

# --- 路径配置 ---
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
BOT_5_PATH = os.path.join(BASE_DIR, "original.exe")
BOT_6_PATH = os.path.join(BASE_DIR, "t1.exe")

def run_bot(exe_path, turn_id, moves):
    input_str = f"{turn_id}\n" + "\n".join(moves) + "\n"
    process = subprocess.Popen(
        exe_path, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, text=True, encoding='utf-8'
    )
    try:
        # 设置稍微短一点的 timeout 以加快压测速度
        stdout, _ = process.communicate(input=input_str, timeout=10)
        return stdout.strip()
    except:
        process.kill()
        return None

def play_one_game(game_id):
    """单局对弈：负责处理角色互换和胜负判定"""
    # 奇数局 5黑6白，偶数局 6黑5白，确保公平
    if game_id % 2 != 0:
        p_black, p_white = BOT_5_PATH, BOT_6_PATH
        b_name, w_name = "original", "t1"
    else:
        p_black, p_white = BOT_6_PATH, BOT_5_PATH
        b_name, w_name = "t1", "original"

    black_history = ["-1 -1 -1 -1 -1 -1"]
    white_history = []
    
    for turn in range(1, 200):
        # 黑方动作
        move_b = run_bot(p_black, turn, black_history)
        if not move_b or move_b == "-1 -1 -1 -1 -1 -1":
            return w_name  # 白方胜

        white_history.append(move_b)
        black_history.append(move_b)

        # 白方动作
        move_w = run_bot(p_white, turn, white_history)
        if not move_w or move_w == "-1 -1 -1 -1 -1 -1":
            return b_name  # 黑方胜
            
        black_history.append(move_w)
        white_history.append(move_w)
    return "Draw"

def main():
    total_rounds = 140  # 压测建议至少 20 局
    max_workers = 13   # 同时开 10 局对弈，充分利用 4090 CPU 性能
    
    print(f"--- 开启并行压测模式：总局数 {total_rounds}，并行数 {max_workers} ---")
    
    results = []
    # 使用进程池加速
    with ProcessPoolExecutor(max_workers=max_workers) as executor:
        # 提交所有对弈任务
        futures = [executor.submit(play_one_game, i) for i in range(1, total_rounds + 1)]
        
        # 实时打印每局结束信息
        for i, future in enumerate(futures):
            winner = future.result()
            results.append(winner)
            print(f"[局次 {i+1:02d}] 胜者: {winner}")

    # 统计最终胜率
    win_5 = results.count("original")
    win_6 = results.count("t1")
    
    print("\n" + "="*50)
    print(" 严谨胜率分析报告 ".center(46, '#'))
    print(f" original 胜场: {win_5} | 胜率: {(win_5/total_rounds)*100:.1f}%")
    print(f" t1 胜场: {win_6} | 胜率: {(win_6/total_rounds)*100:.1f}%")
    print("="*50)

if __name__ == "__main__":
    main()