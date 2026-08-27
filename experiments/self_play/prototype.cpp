#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,lzcnt,popcnt")
#pragma GCC optimize("Ofast")

// ── 自对弈快照 · prototype ─────────────────────────────────────────
// 改造原型(原名 t1.cpp):挑战 baseline 的第一刀——弃用查表曲线,
// 探索常数改为一条指数衰减公式(clamp [0.16, 0.35]);渐进扩展阈值放宽,
// 深层采样减半至 200;Super-Mobility 权重暂复用机动性权重(w_params[4])。
// 血缘与参数对照见同目录 README.md。

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <queue>
#include <random>
#include <sstream>
#include <cctype>
#include <chrono>
#include <thread>

using namespace std;

// 使用强制内联宏
#ifdef _MSC_VER
    #define FORCE_INLINE __forceinline
#else
    #define FORCE_INLINE inline __attribute__((always_inline))
#endif


// ==================== 全局配置 ====================
#define GRIDSIZE 8
#define MAX_TIME_MS 997
#define INF 1e9

// 棋子定义
const int EMPTY = 0;
const int BLACK = 1;         // 黑方 (先手)
const int WHITE = 2;         // 白方
const int OBSTACLE = 3;      // 箭/障碍
static std::mt19937 g_rng(std::chrono::steady_clock::now().time_since_epoch().count());

// 算法参数
const int SIMULATION_DEPTH = 7;

// 方向向量
const int dx[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
const int dy[] = { -1, 0, 1, -1, 1, -1, 0, 1 };

// ==================== 评估参数矩阵 (28阶段) ====================


const double PARAMS[6][28]={
    {0.0543272421, 0.0617351652, 0.0700140728, 0.0789426993, 0.0882997788, 0.0974635117, 0.1065111362, 0.1150821818, 0.1229343751, 0.1296996611, 0.1351567743, 0.1390844488, 0.1418751502, 0.1425026131, 0.1413346452, 0.1381395985, 0.1328469389, 0.1256353909, 0.1166836786, 0.1061705264, 0.0946746586, 0.0827292211, 0.0707325252, 0.0599551348, 0.0518332023, 0.0479505778, 0.0503022134, 0.0613580594},
    {0.0558123491, 0.0612456723, 0.0654321098, 0.0721893456, 0.0805432187, 0.0894321098, 0.0991234567, 0.1095678901, 0.1201234567, 0.1305432189, 0.1398765432, 0.1475432109, 0.1531234567, 0.1561234567, 0.1565432109, 0.1548765432, 0.1521234567, 0.1501234567, 0.1505432109, 0.1534321098, 0.1578765432, 0.1621234567, 0.1651234567, 0.1664321098, 0.1655432109, 0.1621234567, 0.1561234567, 0.1462774902},
    {0.6558123491, 0.6384567234, 0.6201234567, 0.6015432109, 0.5824321098, 0.5631234567, 0.5438765432, 0.5251234567, 0.5074321098, 0.4905432189, 0.4748765432, 0.4601234567, 0.4465432109, 0.4341234567, 0.4231234567, 0.4135432109, 0.4054321098, 0.3985432109, 0.3921234567, 0.3861234567, 0.3805432109, 0.3754321098, 0.3711234567, 0.3681234567, 0.3668765432, 0.3665806330, 0.3665806330, 0.3665806330},
    {0.6985432109, 0.6751234567, 0.6484321098, 0.6185432109, 0.5871234567, 0.5634321098, 0.5458765432, 0.5331234567, 0.5234321098, 0.5165432109, 0.5108765432, 0.5041234567, 0.4954321098, 0.4851234567, 0.4735432109, 0.4611234567, 0.4485432109, 0.4361234567, 0.4254321098, 0.4168765432, 0.4101234567, 0.4044321098, 0.3991234567, 0.3952004992, 0.3952004992, 0.3952004992, 0.3952004992, 0.3952004992},
    {0.0244123457, 0.0221456789, 0.0205432101, 0.0198765432, 0.0202345678, 0.0218765432, 0.0249123457, 0.0294567890, 0.0356789012, 0.0436789012, 0.0534567890, 0.0648765432, 0.0775432101, 0.0898765432, 0.0991234567, 0.1034567890, 0.1021234567, 0.0954567890, 0.0845678901, 0.0712345678, 0.0578765432, 0.0456789012, 0.0354567890, 0.0278765432, 0.0234567890, 0.0219426369, 0.0219426369, 0.0219426369},
    {0.0025123457, 0.0027543210, 0.0030123457, 0.0034567890, 0.0040123457, 0.0048765432, 0.0058123457, 0.0068456790, 0.0076123457, 0.0081234567, 0.0083456790, 0.0082123457, 0.0078456790, 0.0071234567, 0.0062345679, 0.0052123457, 0.0041234567, 0.0031234567, 0.0022345679, 0.0015432100, 0.0009123457, 0.0005123457, 0.0002345679, 0.0001123457, 0.0000567890, 0.0000123457, 0.0000123457, 0.0000123457}
};  

// ==================== 数据结构 ====================
struct Move {
    int x0, y0, x1, y1, x2, y2;  
    bool operator==(const Move& o) const {
        return x0 == o.x0 && y0 == o.y0 && x1 == o.x1 && y1 == o.y1 && x2 == o.x2 && y2 == o.y2;
    }
};

struct BoardState {
    int grid[GRIDSIZE][GRIDSIZE];
    int turnColor;
};

// 全局变量
int grid[GRIDSIZE][GRIDSIZE];
int currentTurnColor = BLACK;
int myColor = BLACK;  // 我方的颜色
int turnID;

// ==================== 辅助函数 ====================
FORCE_INLINE double fast_pow2_negative(int n) {
    if (n <= 0) return 1.0;
    if (n >= 1024) return 0.0;
    uint64_t bits = 0x3ff0000000000000ULL - (static_cast<uint64_t>(n) << 52);
    return *reinterpret_cast<double*>(&bits);
}

FORCE_INLINE bool inMap(int x, int y) {
    return x >= 0 && x < GRIDSIZE && y >= 0 && y < GRIDSIZE;
}

// ==================== 棋盘操作 ====================
FORCE_INLINE void initBoard() {
    memset(grid, 0, sizeof(grid));
    grid[0][2] = grid[2][0] = grid[5][0] = grid[7][2] = BLACK;
    grid[0][5] = grid[2][7] = grid[5][7] = grid[7][5] = WHITE;
    currentTurnColor = BLACK;
}

FORCE_INLINE void applyMove(const Move& m, int color) {
    grid[m.x0][m.y0] = EMPTY;      // 清空起点
    grid[m.x1][m.y1] = color;      // 终点放置棋子
    grid[m.x2][m.y2] = OBSTACLE;   // 箭靶放置障碍
}

FORCE_INLINE void undoMoveLogic(const Move& m, int color) {
    grid[m.x2][m.y2] = EMPTY;      // 移除障碍
    grid[m.x1][m.y1] = EMPTY;      // 移除棋子
    grid[m.x0][m.y0] = color;      // 恢复棋子
}

// ==================== 走法生成 ====================
FORCE_INLINE vector<Move>& generateMoves(int color) {
    static vector<Move> moves;
    moves.clear(); // 清除元素但保留已分配的内存空间
    if (moves.capacity() < 600) moves.reserve(600);
    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            if (grid[i][j] == color) {
                for (int d = 0; d < 8; d++) {
                    for (int k = 1; k < GRIDSIZE; k++) {
                        int nx = i + dx[d] * k;
                        int ny = j + dy[d] * k;
                        if (!inMap(nx, ny) || grid[nx][ny] != EMPTY) break;
                        grid[i][j] = EMPTY; grid[nx][ny] = color;
                        for (int ad = 0; ad < 8; ad++) {
                            for (int ak = 1; ak < GRIDSIZE; ak++) {
                                int ax = nx + dx[ad] * ak;
                                int ay = ny + dy[ad] * ak;
                                if (!inMap(ax, ay) || grid[ax][ay] != EMPTY) break;
                                moves.push_back({i, j, nx, ny, ax, ay});
                            }
                        }
                        grid[nx][ny] = EMPTY; grid[i][j] = color;
                    }
                }
            }
        }
    }
    return moves;
}

FORCE_INLINE bool getFastRandomMove(int color, Move& m) {
    int q_pos[4];
    int q_cnt = 0;
    // 严谨获取棋子位置
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            if (grid[i][j] == color) q_pos[q_cnt++] = (i << 3) | j;
        }
    }

    // 随机打乱棋子顺序
    for (int i = q_cnt - 1; i > 0; --i) {
        std::uniform_int_distribution<int> d(0, i);
        std::swap(q_pos[i], q_pos[d(g_rng)]);
    }

    for (int i = 0; i < q_cnt; ++i) {
        int x0 = q_pos[i] >> 3, y0 = q_pos[i] & 7;
        int dirs[8] = {0,1,2,3,4,5,6,7};
        for (int j = 7; j > 0; --j) std::swap(dirs[j], dirs[std::uniform_int_distribution<int>(0, j)(g_rng)]);

        for (int d = 0; d < 8; ++d) {
            int dir = dirs[d];
            int max_k = 0;
            for (int k = 1; k < 8; ++k) {
                int nx = x0 + dx[dir] * k, ny = y0 + dy[dir] * k;
                if (inMap(nx, ny) && grid[nx][ny] == EMPTY) max_k = k;
                else break;
            }

            if (max_k >= 1) { // 必须大于等于1
                std::uniform_int_distribution<int> dist_k(1, max_k);
                int k = dist_k(g_rng);
                int x1 = x0 + dx[dir] * k, y1 = y0 + dy[dir] * k;

                // 虚拟应用移动以寻找射箭点
                grid[x0][y0] = EMPTY;
                int old_target = grid[x1][y1]; // 理论上应为 EMPTY
                grid[x1][y1] = color;

                int adirs[8] = {0,1,2,3,4,5,6,7};
                for (int j = 7; j > 0; --j) std::swap(adirs[j], adirs[std::uniform_int_distribution<int>(0, j)(g_rng)]);

                bool found_arrow = false;
                for (int ad = 0; ad < 8; ++ad) {
                    int adir = adirs[ad];
                    int max_ak = 0;
                    for (int ak = 1; ak < 8; ++ak) {
                        int ax = x1 + dx[adir] * ak, ay = y1 + dy[adir] * ak;
                        if (inMap(ax, ay) && grid[ax][ay] == EMPTY) max_ak = ak;
                        else break;
                    }
                    if (max_ak >= 1) {
                        std::uniform_int_distribution<int> dist_ak(1, max_ak);
                        int ak = dist_ak(g_rng);
                        m = {x0, y0, x1, y1, x1 + dx[adir] * ak, y1 + dy[adir] * ak};
                        found_arrow = true;
                        break;
                    }
                }

                // 严谨还原状态
                grid[x1][y1] = old_target;
                grid[x0][y0] = color;
                
                if (found_arrow) return true;
            }
        }
    }
    return false;
}

// ==================== 评估系统 ====================
const double ADJUST1=1.6,ADJUST2=0.2-0.2*(2-myColor); // 小孩子才做选择题——我选择直接调整评估函数输出范围来影响搜索树选择性
const double AD1=0.99,AD2=0.01; //评估函数参数微调范围
bool ON=false; //是否启用超级修正

int compute_game_stage() {
    int occupied = 0;
    for(int i=0; i<GRIDSIZE; i++)
        for(int j=0; j<GRIDSIZE; j++)
            if(grid[i][j] != EMPTY) occupied++;
    int stage = (occupied - 8) / 2;
    return min(max(stage, 0), 27);
}


void calcDistances(int distMap[2][GRIDSIZE][GRIDSIZE], int type) {
    for (int p = 0; p < 2; p++) {
        for(int i=0; i<GRIDSIZE; i++) fill(distMap[p][i], distMap[p][i]+GRIDSIZE, INF);
        queue<pair<int, int>> q;
        int c = (p == 0) ? BLACK : WHITE;
        for(int i=0; i<GRIDSIZE; i++)
            for(int j=0; j<GRIDSIZE; j++)
                if(grid[i][j] == c) { q.push({i, j}); distMap[p][i][j] = 0; }
        while(!q.empty()) {
            auto curr = q.front(); q.pop();
            int d = distMap[p][curr.first][curr.second];
            for(int dir=0; dir<8; dir++) {
                if (type == 1) { // Queen
                    for (int step = 1; step < GRIDSIZE; step++) {
                        int nx = curr.first + dx[dir] * step;
                        int ny = curr.second + dy[dir] * step;
                        if (!inMap(nx, ny) || grid[nx][ny] != EMPTY) break;
                        if (distMap[p][nx][ny] == INF) {
                            distMap[p][nx][ny] = d + 1; q.push({nx, ny});
                        }
                    }
                } else { // King
                    int nx = curr.first + dx[dir];
                    int ny = curr.second + dy[dir];
                    if (inMap(nx, ny) && grid[nx][ny] == EMPTY && distMap[p][nx][ny] == INF) {
                        distMap[p][nx][ny] = d + 1; q.push({nx, ny});
                    }
                }
            }
        }
    }
}

double evaluatePosition(int targetPlayer) {
    int stage = compute_game_stage();
    double w_params[6];
    std::uniform_real_distribution<double> dist(AD1, AD1 + AD2);
    double jitter = dist(g_rng); // 使用全局 mt19937

    for(int k=0; k<6; k++) w_params[k] = PARAMS[k][stage];
    for(int k=0; k<6; k++) w_params[k] *= jitter;

    int D1[2][GRIDSIZE][GRIDSIZE], D2[2][GRIDSIZE][GRIDSIZE];
    calcDistances(D1, 1);
    calcDistances(D2, 2);

    double score_qt = 0, score_kt = 0, score_qp = 0, score_kp = 0, score_mob = 0, score_supermob = 0;

    for(int i=0; i<GRIDSIZE; i++) {
        for(int j=0; j<GRIDSIZE; j++) {
            if(grid[i][j] != EMPTY) continue;
            if (D1[0][i][j] < D1[1][i][j]) score_qt += 1.0;
            else if (D1[1][i][j] < D1[0][i][j]) score_qt -= 1.0;
            if (D2[0][i][j] < D2[1][i][j]) score_kt += 1.0;
            else if (D2[1][i][j] < D2[0][i][j]) score_kt -= 1.0;
            if (D1[0][i][j] != INF) score_qp += fast_pow2_negative(D1[0][i][j]);
            if (D1[1][i][j] != INF) score_qp -= fast_pow2_negative(D1[1][i][j]);
            double diffK = (double)D2[1][i][j] - D2[0][i][j];
            score_kp += min(1.0, max(-1.0, diffK / 6.0));
        }
    }

    double mobB = 0, mobW = 0;
    for(int i=0; i<GRIDSIZE; i++) {
        for(int j=0; j<GRIDSIZE; j++) {
            if (grid[i][j] == EMPTY) {
                int libs = 0;
                for(int k=0; k<8; k++) 
                    if(inMap(i+dx[k], j+dy[k]) && grid[i+dx[k]][j+dy[k]] == EMPTY) libs++;
                mobB+=1.0*libs/D2[0][i][j];
                mobW+=1.0*libs/D2[1][i][j];
            }
        }
    }
    score_mob = mobB - mobW;


    //超级修正——第六参数——单颗棋子非BFS计算的机动性
    double supermobB=0,supermobW=0;
    if(ON)
    for(int i=0;i<GRIDSIZE;i++){
        for(int j=0;j<GRIDSIZE;j++){
            if(grid[i][j]!=EMPTY){
                int color=grid[i][j];
                for(int d=0;d<8;d++){
                    for(int k=1;k<GRIDSIZE;k++){
                        int nx=i+dx[d]*k;
                        int ny=j+dy[d]*k;
                        if(!inMap(nx,ny) || grid[nx][ny]!=EMPTY) break; 
                        int libs=0;
                        for(int d2=0;d2<8;d2++){
                                int ax=nx+dx[d2];
                                int ay=ny+dy[d2];
                                if(!inMap(ax,ay) || grid[ax][ay]!=EMPTY) continue; 
                                libs++;
                        }
                        if(color==BLACK && D1[1][nx][ny] < INF && D1[1][nx][ny] > k) supermobB+=1.0*libs/fast_pow2_negative(k);
                        else if(color==WHITE && D1[0][nx][ny] < INF && D1[0][nx][ny] > k) supermobW+=1.0*libs/fast_pow2_negative(k);
                    }
                }
            }
        }
    };
    score_supermob=supermobB-supermobW;

    double value=w_params[0]*score_qt + w_params[1]*score_kt + w_params[2]*score_qp + w_params[3]*score_kp + w_params[4]*score_mob*ADJUST1+ w_params[4]*score_supermob*ADJUST2;

    double probBlack = 1.0 / (1.0 + exp(-value * 0.2));
    return (targetPlayer == BLACK) ? probBlack : (1.0 - probBlack);
}

// ==================== MCTS 核心改进 ====================

struct UCTNode {
    Move move;
    int visits;
    double total_value;
    int playerToMove;
    bool is_expanded;
    UCTNode* parent;
    vector<UCTNode*> children;

    UCTNode(Move m, int pColor, UCTNode* par) 
        : move(m), playerToMove(pColor), parent(par), visits(0), total_value(0.0), is_expanded(false) {}
    ~UCTNode() { for(auto c : children) delete c; }
};

double getDynamicC(int turn) {
    double C = 0.177 * exp(-0.008 * (turn - 1.41));
    return max(0.16, min(0.35, C));  //待定
}

double computeUCT(UCTNode* node, double C,bool isMaxLayer) {
    if (node->visits == 0) return isMaxLayer ? 1e18 : -1e18;
    double winRate = node->total_value / node->visits;
    double exploration = C * sqrt(log(node->parent->visits + 1) / (1.0 + node->visits));
    
    // 如果是己方层，我们要 max(winRate + exploration)
    // 如果是对手层，我们要 min(winRate - exploration)
    return isMaxLayer ? (winRate + exploration) : (winRate - exploration);
}
double verbose(UCTNode* node, double C){
    if (node->visits == 0) return 1e18;
    return (node->total_value / (1.0 + node->visits)) + C * sqrt(log(node->parent->visits+1) / (1.0 + node->visits));
}

int getPruningCount(long timeLeftMs, int totalMoves, int totalvisits=0) {
    // 基础剪枝：先砍掉绝对没戏的
    int limit = totalMoves;
    if(timeLeftMs >1350){
        limit=50;
    }
    if (timeLeftMs > 800) {
        limit = 25; // 刚开始，看宽一点
    } else if (timeLeftMs > 400) {
        limit = 12; // 中期，开始收缩
    } else {
        limit = 6;  // 后期，极其专注！只看最强的6个
    }

    // 关键改进！！！！！ 随着模拟次数增加，更进一步压缩，强迫算力向深层渗透
    if (totalvisits > 800) limit = min(limit, 8);
    if (totalvisits > 12000) limit = min(limit, 4);

    return max(1, min(limit, totalMoves));
}

int getExpansionThreshold(int visits,int turn) {
    return 18-(turn-14)*(turn-14)/20;
}

double rollout(int color) {
    Move path[SIMULATION_DEPTH + 2];
    int actual_steps = 0;
    int cur = color;
    int max_d = SIMULATION_DEPTH + (color != currentTurnColor ? 1 : 0);

    for (int d = 0; d < max_d; ++d) {
        Move m;
        if (!getFastRandomMove(cur, m)) {
            // 判定输赢：当前轮到 cur 走，但无棋可走，则 cur 输
            double result = (cur == color) ? 0.0 : 1.0;
            // 撤销已做的移动
            while (actual_steps > 0) {
                cur = (cur == BLACK ? WHITE : BLACK);
                undoMoveLogic(path[--actual_steps], cur);
            }
            return result;
        }
        applyMove(m, cur);
        path[actual_steps++] = m;
        cur = (cur == BLACK ? WHITE : BLACK);
    }

    double eval = evaluatePosition(color);
    // 最终还原
    while (actual_steps > 0) {
        cur = (cur == BLACK ? WHITE : BLACK);
        undoMoveLogic(path[--actual_steps], cur);
    }
    return eval;
}

Move selectBestMove(UCTNode* root) {
    double bestrate= -INF;
    double mostvismark = -INF;
    Move bestMove = {-1};
    int totalVisits = root->visits;

    for (auto child : root->children) {
        if (child->visits == 0) continue;
        
        // 胜率
        double winRate = child->total_value / child->visits;
        double vismark=sqrt(child->visits);

        if (vismark>2*mostvismark/3 && winRate>bestrate) {
            bestrate=winRate;
            bestMove = child->move;
        }
        if(vismark>mostvismark){
            mostvismark=vismark;
        }
    }
    
    // 如果还没选出来，退化为Robust Child
    if (bestMove.x0 == -1 && !root->children.empty()) {
        int maxV = -1;
        for(auto c : root->children) {
            if(c->visits > maxV) { maxV = c->visits; bestMove = c->move; }
        }
    }
    return bestMove;
}

Move getMCTSMove() {
    int turn = compute_game_stage() * 2;
    if(turn==0 && myColor==BLACK) return {2,0,2,5,1,4};
    int addtion=0;
    if(turn==0) addtion=1000;
    double C = getDynamicC(turn);
    int aiColor = currentTurnColor;
    UCTNode* root = new UCTNode({-1,-1,-1,-1,-1,-1}, aiColor, nullptr);
    
    auto startTime=chrono::high_resolution_clock::now();
    vector<pair<Move, int>> path; 

    while(true) {
        // [时间检查]
        auto currentTime=chrono::high_resolution_clock::now();
        long elapsed = chrono::duration_cast<chrono::milliseconds>(currentTime - startTime).count();
        if (elapsed >= MAX_TIME_MS+addtion) break;
        long timeLeft = MAX_TIME_MS - elapsed+addtion;

        UCTNode* node = root;
        int simColor = aiColor;
        path.clear();
        
        // 1. Selection
        while(node->is_expanded && !node->children.empty()) {
            UCTNode* best = nullptr;
            double maxUCT = (simColor == aiColor) ? -1e18 : 1e18;
            for(auto c : node->children) {
                double uct = computeUCT(c, C, simColor == aiColor);
                if(simColor == aiColor){
                if(uct > maxUCT) { maxUCT = uct; best = c; }
                } 
                else {
                if(uct < maxUCT) { maxUCT = uct; best = c;}
                }
            }
            node = best;
            applyMove(node->move, simColor);
            path.push_back({node->move, simColor});
            simColor = (simColor == BLACK ? WHITE : BLACK);
        }

        // 2. Expansion (Progressive Threshold)
        int threshold = getExpansionThreshold(node->visits,turn); //要选择哪个呢？？？root?,node?,parent?
        if(!node->is_expanded && (node->visits >= threshold || node == root)) {
            vector<Move> moves = generateMoves(simColor);
            if(!moves.empty()) {
                // 核心修改！！先随机打乱所有走法 ---
                std::shuffle(moves.begin(), moves.end(), g_rng);
        
                int sampleSize = moves.size();
                if (node != root) {
                    sampleSize = std::min((int)moves.size(), 200); // 深层节点采样200就够了
                    std::shuffle(moves.begin(), moves.end(), g_rng);
                }
                // 如果是根节点(node == root)，保持 sampleSize 为 moves.size()，评估所有走法
        
                vector<pair<double, Move>> ranked;
                for(int i = 0; i < sampleSize; i++) {
                    const auto& m = moves[i];
                    applyMove(m, simColor);
                    double score = evaluatePosition(simColor);
                    undoMoveLogic(m, simColor);
                    ranked.push_back({score, m});
                }
                sort(ranked.begin(), ranked.end(), [](auto& a, auto& b){ return a.first > b.first; });
                
                // [自适应剪枝]
                int limit = getPruningCount(timeLeft, ranked.size(),root->visits);
                for(int i=0; i<limit; i++) {
                    node->children.push_back(new UCTNode(ranked[i].second, (simColor==BLACK?WHITE:BLACK), node));
                }
                node->is_expanded = true;
                if (!node->children.empty()) {
                    node = node->children[0];
                    applyMove(node->move, simColor);
                    path.push_back({node->move, simColor});
                    simColor = (simColor == BLACK ? WHITE : BLACK);
                }
            } else {
                node->is_expanded = true;
            }
        }

        // 3. Simulation
        double result = rollout(simColor);
        if(simColor != aiColor) result = 1.0 - result;

        // 4. Backprop & Restore
        UCTNode* temp = node;
        while(temp != nullptr) {
            temp->visits++;
            temp->total_value += result;
            temp = temp->parent;
        }
        for (int i = path.size() - 1; i >= 0; i--) {
            undoMoveLogic(path[i].first, path[i].second);
        }
    }

    //  混合策略
    Move bestMove = selectBestMove(root);
    delete root;
    return bestMove;
}

// Botzone输入输出适配 

// 初始化棋盘和读取输入
void init() {
    initBoard();
    
    cin >> turnID;
    
    myColor = WHITE; // 默认
    
    int x0, y0, x1, y1, x2, y2;

    // 循环处理历史记录中的每一对走法（黑方一步，白方一步）
    for (int i = 0; i < turnID; i++) {
        // --- 1. 读取并应用黑方走法 (总是先手) ---
        cin >> x0 >> y0 >> x1 >> y1 >> x2 >> y2;
        
        if (x0 == -1) {
            myColor = BLACK;
            // 如果 turnID=1，到这里就该我们走了，直接跳出循环
            if (turnID == 1) break; 
            continue; // 否则跳过 -1 的处理
        }
        
        Move moveB = {x0, y0, x1, y1, x2, y2};
        applyMove(moveB, BLACK);

        // --- 2. 读取并应用白方走法 (总是后手) ---
        // 只有在不是当前回合 (i < turnID - 1) 且不是起始回合 (i >= 0) 时，才读取白方走法
        if (i < turnID + 1 + -myColor  ) { 
            cin >> x0 >> y0 >> x1 >> y1 >> x2 >> y2;
            if (x0 >= 0) {
                Move moveW = {x0, y0, x1, y1, x2, y2};
                applyMove(moveW, WHITE);
            }
        }
    }
    
    // 确定当前轮到谁
    currentTurnColor = myColor;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    // Botzone初始化
    init();
    
    // 检查是否有合法走法
    vector<Move> availableMoves = generateMoves(currentTurnColor);
    
    if (availableMoves.empty()) {
        // 无棋可走，输出认负
        cout << "-1 -1 -1 -1 -1 -1" << endl;
    } else {
        // 使用AI计算最佳走法
        Move bestMove = getMCTSMove();
        
        // 输出走法（Botzone格式：x0 y0 x1 y1 x2 y2）
        cout << bestMove.x0 << " " << bestMove.y0 << " "
             << bestMove.x1 << " " << bestMove.y1 << " "
             << bestMove.x2 << " " << bestMove.y2 << endl;

    }
    
    return 0;
}