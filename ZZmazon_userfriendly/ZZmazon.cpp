//The Bot of Game of the Amazons--vTerminator
//User-friendly 
//UI-Terminal Functions:
// 1.人机对战（支持A.悔棋 B.实时输出局势 C.退出） 2.双人对战——支持切换到人机对战 3.难度切换（多档Bot思考时间可调） 4.可开关调试模式-可输出搜索过程 
// 5.可读取前次存档记录——默认存放于临时文件SAVE_PATH——每次提取需要单独操作，否则再次进行对战将覆盖前次记录


//不做成QT界面是因为既然已经有botzone平台的更加聚焦博弈和纯粹的对局GUI界面，这里侧重调试和终端命令行UI用户体验也即CUI


#include <iostream>
#include <cstring> //棋盘清空复盘操作
#include <cctype> //字符isdigit和isspace判断
#include <iomanip>
#include <vector> //此处有可优化之处：位棋盘优化
#include <queue> //BFS
#include <algorithm>
#include <cmath>
#include <ctime>  //用于clock()--Bot思考时间卡时
#include <chrono> //获取高精度时间--观察用户时间
#include <thread> //配合高精度时间让线程暂停触发提示词（超时幽默提示用户）
#include <fstream> //文件读写存档
#include <cstdlib> //C风格快但略差的随机数——03优化情况下
#include <random> //也可以随机数但似乎更慢一点
#include <cstdint> //借鉴的“快速二次幂函数”使用
#include <sstream> //未来可以解析复杂的字符串流提高体验感或者其他操作

using namespace std;


//～～～～～～～～～～～～～～～～～～～～全局配置～～～～～～～～～～～～～～～～～～～～～～～～～～～～
#define GRIDSIZE 8
#define BREAK_TIME_MS 3000 //默认最大思考时间
#define INF 1e9 //无穷大——距离计算


const int EMPTY=0;
const int BLACK=1;
const int WHITE=2;
const int OBSTACLE=3; //箭（障碍）
const string SAVE_PATH="CppCode/Projects_practice/AmazonBot_design/ZZmazon_userfriendly/save.txt"; //存档路径
static std::mt19937 g_rng(std::chrono::steady_clock::now().time_since_epoch().count());

const int dx[]={-1,-1,-1,0,0,1,1,1};
const int dy[]={-1,0,1,-1,1,-1,0,1};

struct Move{                            //记录移动
    int x1,y1,x2,y2,x3,y3;              //分别代表起点，终点，箭点——顺序一定要对啊！
    bool operator==(const Move& o)const{
        return x1==o.x1 &&x2==o.x2 && x3==o.x3 && y1==o.y1 && y2==o.y2 && y3==o.y3;
    }
};

//记录时刻状态
struct BoardState{
    int grid[GRIDSIZE][GRIDSIZE];
    int turnColor;
};

int grid[GRIDSIZE][GRIDSIZE]; 
int currentTurnColor=BLACK;
int MAX_TIME_MS=BREAK_TIME_MS; //可调整思考时间
int HUMAN_PLAYER_COLOR=BLACK; //玩家执棋颜色
bool DEBUG_MODE=false; //调试模式开关
bool BOT_FIRST=false; //先手标记——用于切换先后手
bool shoot=false;
vector<BoardState> undoStack; //悔棋栈——用于实现悔棋功能
vector<Move> history; //记录历史走法——用于存档和读取存档功能



//～～～～～～～～～～～～～～～～～～～～～辅助函数～～～～～～～～～～～～～～～～～～～～～～～
//存档辅助函数
void overwriteSaveWithHistory(const vector<Move>& history){
    ofstream saveFile(SAVE_PATH);
    if(!saveFile.is_open()) return;
    for(const Move& m:history){
        saveFile<<m.x1<<" "<<m.y1<<" "<<m.x2<<" "<<m.y2<<" "<<m.x3<<" "<<m.y3<<endl;
    }
    saveFile.close();
}
//用历史记录覆盖存档（默认情况下，ofstream 会以“覆盖”模式打开文件。如果文件已存在，它会清空里面的所有内容，从头开始写。）
//用于悔棋无法删除末尾行，只得全部重刷一遍（全量覆盖）

void saveMoveToFile(const Move& m){
    ofstream saveFile(SAVE_PATH,ios::app);
    if(!saveFile.is_open()) return;
    saveFile<<m.x1<<" "<<m.y1<<" "<<m.x2<<" "<<m.y2<<" "<<m.x3<<" "<<m.y3<<endl;
    saveFile.close();
}
//正常下棋时用来“实时记录”（增量追加）


double fast_pow_2_negative(int n){
    if(n<=0) return 1.0;    //防御性编程
    if(n>=62) return 0.0; //防止溢出
    return 1.0 / (1ULL << n); //使用64位无符号整数位运算避免溢出

    //return 1.0 / (1 << n); //直接使用较为低级但比pow好一点的位运算

    /*//想法是通过位运算直接构造IEEE 754格式的double（别人妙码）
    if (n <= 0) return 1.0;
    if (n >= 1024) return 0.0;
    uint64_t bits = 0x3ff0000000000000ULL - (static_cast<uint64_t>(n) << 52);
    return *reinterpret_cast<double*>(&bits);*/ //放在未来高性能备用——目前可能差距并不大
}

bool inMap(int x,int y){
    return x>=0 && x<GRIDSIZE && y>=0 && y<GRIDSIZE;
}

//～～～～～～～～～～～～～～～～～～棋盘操作～～～～～～～～～～～～～～～～～～～～～～～～～
//初始化
void initBoard(){
    undoStack.clear(); //清空悔棋栈
    history.clear(); //清空历史记录
    //相当于是重启了
    //不在这里而是在gameloop里刷新文档，是为了当且仅当你真正点击菜单开始一盘新对局时，旧的物理存档才会被“物理抹除”。（或者是悔棋或正常结束时）
    memset(grid,0,sizeof(grid));
    grid[0][2]=grid[2][0]=grid[0][5]=grid[2][7]=BLACK;
    grid[5][0]=grid[7][2]=grid[5][7]=grid[7][5]=WHITE;
    currentTurnColor=BLACK;
    BOT_FIRST=false;
}

//执行走法
void applyMove(const Move& m,int color){
    grid[m.x1][m.y1]=EMPTY;
    grid[m.x2][m.y2]=color;
    grid[m.x3][m.y3]=OBSTACLE;
}

//保存当前盘用于悔棋
void saveStateForUndo(){
    BoardState state;
    memcpy(state.grid,grid,sizeof(grid));
    state.turnColor=currentTurnColor;
    undoStack.push_back(state);
}  //原本想法：从SAVE_PATH反向读取并删除两步存档实现悔棋，但似乎操作更加麻烦，不如直接内存悔棋（似乎文件读取似乎更耗能耗时

bool performUndo(){
    if(undoStack.empty()) return false;
    BoardState state=undoStack.back();
    undoStack.pop_back();
    memcpy(grid,state.grid,sizeof(grid));
    currentTurnColor=state.turnColor;
    // 同步物理存档
    if(!history.empty()) history.pop_back();
    overwriteSaveWithHistory(history);
    return true;
}

void undoMoveLogic(const Move& m,int color){
    grid[m.x3][m.y3]=EMPTY;
    grid[m.x2][m.y2]=EMPTY;
    grid[m.x1][m.y1]=color;
}

//～～～～～～～～～～～～～～～～～～生成走法+走法检查～～～～～～～～～～～～～～～～～～～～～～～

//本来可以使用一个通用版本检测Queen Move的合法性，但为了提高用户体验，分开写了；我将在botzone适配版本中（为简化逻辑进行合并(x)）——根本不要使用--默认合法

bool isValidMove(const Move& m,int color,bool flag=false){ //增加flag参数用于控制是否输出错误提示——也让你可以选择shutup
    //1.基础坐标范围检测
    if(!inMap(m.x1,m.y1) || !inMap(m.x2,m.y2) || !inMap(m.x3,m.y3)){
        if(flag) cout<<endl<<"蠢货！坐标超出棋盘范围(0-7)啦！你再乱输入就不跟你玩了！"<<endl;
        return false;
    }
    //2.起点归属检测
    if(grid[m.x1][m.y1]!=color){
        if(flag) cout<<endl<<"犯傻啦你！起点("<<m.x1<<","<<m.y1<<")不是你的棋子！精神点！"<<endl;
        return false;
    }
    //3.移动路径检测
    int dx1=m.x2-m.x1;
    int dy1=m.y2-m.y1;
    //1）不能原地不动
    if(dx1==0 && dy1==0){
        if(flag) cout<<endl<<"小伙子，别想耍花招！你想不走偷偷赢我，没门！也没得窗户！必须移动棋子！"<<endl;
        return false;
    }
    //2）不能走曲线
    if(dx1!=0 && dy1!=0 && abs(dx1)!=abs(dy1)){
        if(flag) cout<<endl<<"都快被你整无语了！你的棋子移动还能神龙摆尾也是服了（必须是直线或对角线）！"<<endl;
        return false;
    }
    //3）不能穿墙
    int steps=max(abs(dx1),abs(dy1));
    int sx=dx1/steps;
    int sy=dy1/steps;
    for(int k=1;k<steps;k++){
        if(grid[m.x1+k*sx][m.y1+k*sy]!=EMPTY){
            if(flag) cout<<endl<<"撞墙了哥！棋子移动路径上有阻挡！"<<endl;
            return false;  
        }
    }
    //4）目标点必须为空
    if(grid[m.x2][m.y2]!=EMPTY){
        if(flag) cout<<endl<<"这不是围棋！你吃不了子的！你棋子移动的终点有东西了"<<endl;
        return false;
    }

    //4.射箭路径检测
    //暂时模拟移动然后模拟射箭检测是否合法，随后恢复原来的棋盘（执行让执行函数统一进行）

    //暂时备份起点终点
    int backup_source=grid[m.x1][m.y1];
    int backup_destination=grid[m.x2][m.y2];

    //执行模拟
    grid[m.x1][m.y1]=EMPTY;
    grid[m.x2][m.y2]=color; 

    bool validArrow=true;
    int dx2=m.x3-m.x2;
    int dy2=m.y3-m.y2;  
    //1）箭必须发射
    if(dx2==0 && dy2==0){
        if(flag) cout<<endl<<"别射箭自杀呀！箭落点不能与当前棋子位置重合！"<<endl;
        validArrow=false;
    }
    //2）箭不能走曲线
    else if(dx2!=0 && dy2!=0 && abs(dx2)!=abs(dy2)){
        if(flag) cout<<endl<<"笑死你也是打破物理规律了！箭不走直线你也是神人（必须是直线或对角线）！"<<endl;
        validArrow=false;
    }
    else{
        //3）箭路径阻挡检测
        int a_steps=max(abs(dx2),abs(dy2));
        int asx=dx2/a_steps;
        int asy=dy2/a_steps;
        for(int k=1;k<a_steps;k++){
            if(grid[m.x2+k*asx][m.y2+k*asy]!=EMPTY){
                if(flag) cout<<endl<<"浪费箭！射箭路径上被阻挡了！"<<endl;
                validArrow=false;
                break;
            }
        }
        //4）箭目标点必须为空
        if(validArrow && grid[m.x3][m.y3]!=EMPTY){
            if(flag) cout<<endl<<"不杀自己也别杀别人吧！箭落点位置非空！"<<endl;
            validArrow=false;
        }
    }

    //恢复原棋盘
    grid[m.x1][m.y1]=backup_source;
    grid[m.x2][m.y2]=backup_destination;

    return validArrow;
}

//有想法：可以不用遍历棋盘，而是直接记录每方棋子位置以加速走法生成——但是MCTS中随机模拟太多每次都要存储更新写起来比较麻烦
//于是就先遍历吧——
vector<Move> generateMoves(int color){
    vector<Move> moves; //用来存储所有合法走法
    moves.reserve(300); //预分配空间，多分一点大约200步（平均可能100步）

    //遍历棋盘所有位置
    for(int i=0;i<GRIDSIZE;i++){
        for(int j=0;j<GRIDSIZE;j++){
            if(grid[i][j]==color){
                for(int d=0;d<8;d++){
                    for(int k=1;k<GRIDSIZE;k++){
                        int nx=i+dx[d]*k;
                        int ny=j+dy[d]*k;
                        if(!inMap(nx,ny) || grid[nx][ny]!=EMPTY) break; 
                        grid[i][j]=EMPTY;
                        grid[nx][ny]=color; //要走啊！
                        //棋子移动到(nx,ny)后尝试射箭
                        for(int d2=0;d2<8;d2++){
                            for(int k2=1;k2<GRIDSIZE;k2++){
                                int ax=nx+dx[d2]*k2;
                                int ay=ny+dy[d2]*k2;
                                if(!inMap(ax,ay) || grid[ax][ay]!=EMPTY) break; 
                                //合法走法加入列表
                                moves.push_back(Move{i,j,nx,ny,ax,ay});
                            }
                        }
                        grid[nx][ny]=EMPTY;
                        grid[i][j]=color; //回溯！
                    }
                }
            }
        }
    }
    return moves;
}

//～～～～～～～～～～～～～～～～～～评估系统～～～～～～～～～～～～～～～～～～～～～～～

// 28阶段参数（这是别人经过机器训练学习所得的数据——我能做的就是在后期进行随机数震荡起伏微调并尽量配合他的评估函数结构使用）

const double STAGE_PARAMS[28][6] = {
		{ 0.07747249543793637 ,0.05755603330699520 ,0.64627749023334498 ,0.70431267004292740 ,0.02438131097879579 , 0.001 },
		{ 0.05093047840251742 ,0.06276538622537013 ,0.69898059004821581 ,0.66192728970497727 ,0.02362598306372760 , 0.001 },
		{ 0.06036622274224539 ,0.06253298199478051 ,0.60094570235521628 ,0.67719126081076242 ,0.01873142786640421 , 0.001 },
		{ 0.07597341130849308 ,0.06952095866594065 ,0.69061184234845333 ,0.67989394578528273 ,0.02098781856298665 , 0.001 },
		{ 0.08083391263897154 ,0.08815144960484271 ,0.58981849824874917 ,0.54664183543259470 ,0.02318479501373763 , 0.001 },
		{ 0.09155731347030857 ,0.08397548702353251 ,0.56392480085083986 ,0.54319242129550227 ,0.02317401477849946 , 0.001 },
		{ 0.10653095458609237 ,0.10479793630859575 ,0.54840938009286515 ,0.53023658889860381 ,0.02084758939889652 , 0.001 },
		{ 0.11534143744086589 ,0.11515706838023705 ,0.53325566869906469 ,0.52423368303553451 ,0.02237127451593010 , 0.001 },
		{ 0.12943854523554690 ,0.12673742164114844 ,0.50841519367287034 ,0.52208373964502879 ,0.02490545306630711 , 0.001 },
		{ 0.12882484162931859 ,0.13946973532382280 ,0.49621839819987758 ,0.51776460089353364 ,0.03045473763611049 , 0.001 },
		{ 0.13701233819832731 ,0.15338865590616042 ,0.47601466399954588 ,0.51500429509193190 ,0.03249896738636078 , 0.001 },
		{ 0.14530543898518938 ,0.15565237403332051 ,0.45365475320199057 ,0.50934623406618500 ,0.03830491784046246 , 0.001 },
		{ 0.14521045986025419 ,0.16388365022083374 ,0.44531995327608060 ,0.50517597255948953 ,0.04864124027084386 , 0.001 },
		{ 0.13750613208150655 ,0.16326621164859418 ,0.43619350878439399 ,0.50328876650721398 ,0.05912794240603884 , 0.001 },
		{ 0.13565263325548560 ,0.15529175902376631 ,0.42382223063419649 ,0.50288212924827379 ,0.07437679521343679 , 0.001 },
		{ 0.12382760525087406 ,0.10361944098637088 ,0.50487335391408680 ,0.55808747967333505 ,0.02791980213792046 , 0.001 },
		{ 0.11809487853625075 ,0.14632850080535232 ,0.40738388113193924 ,0.41782129616811122 ,0.10308050317730764 , 0.001 },
		{ 0.10805473551960752 ,0.15043981450391137 ,0.40520488356004784 ,0.43073574707030956 ,0.10967613304465569 , 0.001 },
		{ 0.09668240983912251 ,0.15666221434557865 ,0.40215634987047013 ,0.44165716517577754 ,0.10906426061069142 , 0.001 },
		{ 0.10585263971502025 ,0.16319090506614549 ,0.38220029690800922 ,0.45465487463858675 ,0.10062997439277618 , 0.001 },
		{ 0.11123671989551248 ,0.15516074827095279 ,0.36904588744714037 ,0.46534418781939937 ,0.09118229977179015 , 0.001 },
		{ 0.12535649823409767 ,0.10492555251930048 ,0.35567115915540981 ,0.48043579160677637 ,0.08337580273275977 , 0.001 },
		{ 0.28657326967317970 ,0.16655279311197080 ,0.38060545469477008 ,0.42472577515072628 ,0.10316994796202342 , 0.001 },
		{ 0.07143084940040888 ,0.16655279311197080 ,0.36658063304313299 ,0.39520049916162908 ,0.02194263694320541 , 0.001 },
		{ 0.07143084940040888 ,0.16655279311197080 ,0.36658063304313299 ,0.39520049916162908 ,0.02194263694320541 , 0.001 },
		{ 0.07143084940040888 ,0.16655279311197080 ,0.36658063304313299 ,0.39520049916162908 ,0.02194263694320541 , 0.001 },
		{ 0.07143084940040888 ,0.16655279311197080 ,0.36658063304313299 ,0.39520049916162908 ,0.02194263694320541 , 0.001 },
		{ 0.07143084940040888 ,0.14627749023334498 ,0.36658063304313299 ,0.39520049916162908 ,0.02194263694320541 , 0.001 }
};
const double KP_PARAM=6.0; //用于调国王距离潜力的归一化的参数，这里默认是6.0
const double PUNISHMENT=5.0; //用于惩罚无路可走的棋子——自由度为0时的惩罚分数
const double MODIFIED=0.2; //用于调整评估函数输出的陡峭度——影响搜索树的选择性
const double C1=0.177,C2=-0.008,C3=-1.41,C4=0.12,C5=0.35;//用于计算动态C的参数——此处均为匹配他人参数的参数
const double EE1=0.02,EE2=1.0,EE3=0.0,ADJUST1=1.6,ADJUST2=1; //小孩子才做选择题——我选择直接调整评估函数输出范围来影响搜索树选择性
const double AD1=0.97,AD2=0.06; //用于评估函数参数微调的随机数范围——在0.97到1.03之间波动

//鉴于如果只是遍历棋盘的话并不会因此而大开销时间，因此我选择每次都遍历棋盘来计算当前阶段（其实也是为了模拟不需要额外开数组）
//当然，极致优化的话是可以考虑存储每局的棋子位置以及当前局面也许会更快一点——但为了不出错这里也就采取这种保险手段了
//这里为了补偿遍历，我们可以设计让他思考更久一点

//计算局数
int compute_game_stage(){
    int occupied=0;
    for(int i=0;i<GRIDSIZE;i++){
        for(int j=0;j<GRIDSIZE;j++){
            if(grid[i][j]!=EMPTY) occupied++;
        }
    }
    return min(max((occupied-8)/2, 0),27);
}

//计算距离——论文上所谓的皇后距离和国王距离
void calculateDistances(int distMap[2][GRIDSIZE][GRIDSIZE],int type){ //type=1 指皇后距离，反之指国王距离（2） //其中0是指黑方，1是指白方
    for(int p=0;p<2;p++){
        for(int i=0;i<GRIDSIZE;i++) fill(distMap[p][i],distMap[p][i]+GRIDSIZE,INF);
        queue<pair<int,int>> q;
        int curcolor=(p==0)?BLACK:WHITE; //其中0是指黑方，1是指白方

        for(int i=0;i<GRIDSIZE;i++){
            for(int j=0;j<GRIDSIZE;j++){
                if(grid[i][j]==curcolor){
                    q.push({i,j});
                    distMap[p][i][j]=0;
                }
            }
        }

        while(!q.empty()){
            auto cur=q.front();q.pop();
            int d=distMap[p][cur.first][cur.second];
            for(int dir=0;dir<8;dir++){
                if(type==1){
                    //Queen
                    for(int step=1;step<GRIDSIZE;step++){
                        int nx=cur.first+dx[dir]*step;
                        int ny=cur.second+dy[dir]*step;
                        if(!inMap(nx,ny) || grid[nx][ny]!=EMPTY) break;
                        if(distMap[p][nx][ny]==INF){
                            distMap[p][nx][ny]=d+1;q.push({nx,ny});
                        }
                    }
                }

                else{
                    //King
                    int nx=cur.first+dx[dir];
                    int ny=cur.second+dy[dir];
                    if(inMap(nx,ny) && grid[nx][ny]==EMPTY && distMap[p][nx][ny]==INF){
                        distMap[p][nx][ny]=d+1;q.push({nx,ny});
                    }
                }
            }
        }
    }
}

//核心评估函数
//计算五种特征（皇后距离优势、国王距离优势、皇后势能、国王势能、自由度），加权求和后sigmoid,修改：多了一个超级机动性特征
double evaluate(int targetPlayer){
    int stage=compute_game_stage();
    double wparams[6];
    std::uniform_real_distribution<double> dist(AD1, AD1 + AD2);
    double jitter = dist(g_rng); // 使用全局 mt19937
    //double jitter = AD1 + ((double)rand() / RAND_MAX) * AD2; //在0.97到1.03之间波动，用于参数微调

    for(int k=0;k<6;k++) wparams[k]=STAGE_PARAMS[stage][k];
    for(int k=0;k<6;k++) wparams[k]*=jitter;

    int D1[2][GRIDSIZE][GRIDSIZE],D2[2][GRIDSIZE][GRIDSIZE];
    calculateDistances(D1,1);  //1是指皇后距离，根据上面的函数
    calculateDistances(D2,2);  //2就是国王距离了

    double qt=0,kt=0,qp=0,kp=0,mob=0,supermob=0; //territory领土优势——先到+1分 //proximity潜力优势——越近越有优势，q利用指数衰减，k除一下即可
    for(int i=0;i<GRIDSIZE;i++){
        for(int j=0;j<GRIDSIZE;j++){
            if(grid[i][j]!=EMPTY) continue;
            if(D1[0][i][j]<D1[1][i][j]) qt+=1; //我（默认黑方0）的皇后距离更小表明我更加有优势，得分！
            else if(D1[0][i][j]>D1[1][i][j]) qt-=1;
            
            if(D2[0][i][j]<D2[1][i][j]) kt+=1;
            else if(D2[0][i][j]>D2[1][i][j]) kt-=1;

            //根据论文的说法，不能只去看远处的领土，也要平衡近处（领土越近优势越大）
            //皇后潜力距离
            if(D1[0][i][j]!=INF) qp+=fast_pow_2_negative(D1[0][i][j]);
            if(D1[1][i][j]!=INF) qp-=fast_pow_2_negative(D1[1][i][j]);

            //国王潜力距离
            double diffk=(double)D2[1][i][j]-D2[0][i][j];
            kp+=min(1.0,max(-1.0,diffk/KP_PARAM));
            //大概意思就是限制一个归一化的区间，一旦相差大于6步就直接认为满分而不再移动，6也许可以训练调试一下
        }
    }

    //机动性评估——最神乎其神的的一个指标
    //怀疑还要修改——目前为了配合参数设计一个简化的机动性参数，就是该处自由度/国王距离，如果国王距离越小表明离自由度高的格子们更近
    //但显然因为全部都采用了全局的结果，因此似乎并不能变得更聪明——应该也是我所有bot所卡住的地方
    //目前考虑牺牲一点时间来写一个巨大的8分支BFS评估函数
    //但以下仍然是原版本
    double mobB=0,mobW=0;
    for(int i=0;i<GRIDSIZE;i++){
        for(int j=0;j<GRIDSIZE;j++){
            int libs=0;
            for(int k=0;k<8;k++){
                int nx=i+dx[k],ny=j+dy[k];
                if(inMap(nx,ny) && grid[nx][ny]==EMPTY) libs++;
            }
            if(grid[i][j]==EMPTY){
                if(D2[0][i][j]!=INF) mobB+=1.0*libs/D2[0][i][j];
                if(D2[1][i][j]!=INF) mobW+=1.0*libs/D2[1][i][j]; //考虑使用指数级递减
            }
            else if(grid[i][j]==BLACK) mobB+=(libs==0?-PUNISHMENT:libs);
            else mobW+=(libs==0?-PUNISHMENT:libs);
        }
    }
    mob=mobB-mobW;


    //超级修正——第六参数——单颗棋子非BFS计算的机动性
    double supermobB=0,supermobW=0;
    for(int i=0;i<GRIDSIZE;i++){
        for(int j=0;j<GRIDSIZE;j++){
            if(grid[i][j]!=EMPTY){
                int color=grid[i][j];
                for(int d=0;d<8;d++){
                    for(int k=1;k<GRIDSIZE;k++){
                        int nx=i+dx[d]*k;
                        int ny=j+dy[d]*k;
                        if(!inMap(nx,ny) || grid[nx][ny]!=EMPTY) break; 
                        //grid[i][j]=EMPTY;grid[nx][ny]=color;//棋子移动到(nx,ny)后尝试射箭 //论文中似乎不需要回溯
                        int libs=0;
                        for(int d2=0;d2<8;d2++){
                                int ax=nx+dx[d2];
                                int ay=ny+dy[d2];
                                if(!inMap(ax,ay) || grid[ax][ay]!=EMPTY) break; 
                                libs++;
                        }
                        if(color==BLACK) supermobB+=1.0*libs/(EE1*k+EE2*fast_pow_2_negative(k));
                        else supermobW+=1.0*libs/(EE1*k+EE3*k*k+EE2*fast_pow_2_negative(k));
                        //grid[nx][ny]=EMPTY;grid[i][j]=color; //回溯！
                    }
                }
            }
        }
    }
    supermob=supermobB-supermobW;

    double value=wparams[0]*qt + wparams[1]*kt + wparams[2]*qp + wparams[3]*kp + wparams[4]*mob*ADJUST1+ wparams[5]*supermob*ADJUST2;
    //sigmoid归一化
    double eval=1.0/(1.0+exp(-value*MODIFIED));
    return (targetPlayer==BLACK)?eval:1.0-eval;
}

//～～～～～～～～～～～～～～～～～～MCTS引擎～～～～～～～～～～～～～～～～～～～～～～～～
//一些参数
const int T1=3*MAX_TIME_MS/4,T2=MAX_TIME_MS/2,NUM1=40,NUM2=25,NUM3=12; //用于动态剪枝getPruning的时间阈值和对应剪枝数
const int V1=200,V2=1000,E1=4,E2=8,E3=20; //用于渐进性扩展getExpansionThreshold的访问阈值和对应扩展数
const int SIMULATION_DEPTH=7; //快速模拟的最大深度
const bool OFF_PARITY_ADJUST=true; //关闭奇偶性调整开关——感觉加了奇偶分析变菜了
const double S1=0.5,S2=0.3,S3=0; //用于混合选择策略selectBestMove的权重参数——目前就简单访问次数为主吧

struct UCTNode{
    Move move;
    int visits;
    double total_value;
    int playertomove;
    bool is_expanded;
    UCTNode *parent;
    vector<UCTNode*> children;
    UCTNode(Move m,int pColor,UCTNode* par):move(m),playertomove(pColor),parent(par),visits(0),total_value(0.0),is_expanded(false){} //构造函数初始化
    ~UCTNode() {for(auto c:children)delete c;} //析构函数递归删除子节点（去根则删除整棵树）
};

/*
按照visits？
//动态C值调整
double getDynamicCC(int visits){
    return 1.4*exp(-0.0005*visits)+0.1; //经验值
}
//
*/

//按照turn?
double getDynamicC(int turn){
    double C=C1*exp(-C2*turn)+C3;
    return max(C4,min(C5,C));
}

double computeUCT(UCTNode* node,double C,bool isMax){
    if(node->visits==0) return isMax?INF:-INF; //未访问过的节点优先选择
    double winRate = node->total_value / node->visits;
    double exploration = C * sqrt(log(node->parent->visits + 1) / ( node->visits)); //+1防止log(0)，分母不会为0因为未访问节点已经直接返回了
    // 如果是己方层，我们要 max(winRate + exploration)
    // 如果是对手层，我们要 min(winRate - exploration)
    return isMax ? (winRate + exploration) : (winRate - exploration);
}

//前向剪枝（大大剪枝）
int getPruning(int timeLeftMs,int totalMoves){
    if(timeLeftMs>T1) return min(NUM1,totalMoves);
    else if(timeLeftMs>T2) return min(NUM2,totalMoves);
    else return min(NUM3,totalMoves);
}

//渐进性扩展——早中晚期分阶段（集大成者！）
int getExpansionThreshold(int visits){
    if(visits<V1) return E1;
    else if(visits<V2) return E2;
    else return E3;
}

//快速模拟——最后节点修改了关于奇偶分析和当前颜色而非当前整轮颜色的错误
double rollout(int color){
    int backupGrid[GRIDSIZE][GRIDSIZE];
    memcpy(backupGrid,grid,sizeof(grid)); //备份当前棋盘

    int d=0,cur=color;
    bool pariti_adjust=color!=currentTurnColor; //如果模拟方和当前方不同则需要调整奇偶性
    if(OFF_PARITY_ADJUST) pariti_adjust=false; //关闭奇偶性调整开关
    while(d<SIMULATION_DEPTH+pariti_adjust){
        vector<Move> ms=generateMoves(cur);
        if(ms.empty()){
            memcpy(grid,backupGrid,sizeof(grid)); //恢复棋盘
            return (cur==color?0.0:1.0); //当前方输 //重大改进把CurrentTurnColor改成color避免调用全局
        }
        std::uniform_int_distribution<int> dist(0, (int)ms.size() - 1);
        Move m = ms[dist(g_rng)]; // 使用你高质量的全局随机引擎

        applyMove(m, cur);
        //applyMove(ms[rand()%ms.size()], cur); //原来只用rand（）
        cur=(cur==BLACK)?WHITE:BLACK;
        d++;
    }

    double eval=evaluate(color);//评估最终局面 //重大改进把CurrentTurnColor改成color避免调用全局
    memcpy(grid,backupGrid,sizeof(grid)); //恢复棋盘
    return eval;
}


//混合选择策略——传统上是选择高访问量的节点，但此处也赋予胜率一定的权重吧
//一般来说访问量高的胜率也不低
Move selectBestMove(UCTNode* root){
    double bestScore=-INF;
    Move bestMove={-1}; //无效走法标记——为了看有没有选出好棋（没有返回-1
    int totalVisits=root->visits;

    //保险
    if(totalVisits==0 && !root->children.empty()){
        //万一没有访问过就随机选一个（防止死机）
        bestMove=root->children[0]->move;
        return bestMove;
    }


    for(auto child:root->children){
        double winRate=child->visits==0?0:child->total_value/child->visits;
        
        //double score=S1*winRate + S2*sqrt(child->visits/totalVisits)+S3*winRate*sqrt(child->visits/totalVisits); //归一化版本但感觉没法让它发挥访问量为主的绝对优势
        //哎感觉有点难以抉择选哪种策略，就丢给超参数吧
        double score=S1*winRate + S2*sqrt(child->visits)+S3*winRate*sqrt(child->visits);

        //但是实验结果是似乎换成超大访问量就很不戳了
        if(score>bestScore){
            bestScore=score;
            bestMove=child->move;
        }
    }
    return bestMove;
}


//MCTS主函数
Move getMCTSMove(){
    int turn=compute_game_stage()*2;
    if(turn==0 && BOT_FIRST){
        //我喜欢的走法
        return {0,2,5,2,4,1}; //开局先手固定走法
    }
    double C=getDynamicC(turn);//动态C值
    UCTNode* root=new UCTNode(Move{-1, -1, -1, -1, -1, -1},currentTurnColor,nullptr); //根节点
    
    auto startTime=chrono::high_resolution_clock::now();
    //clock_t start = clock(); 似乎这个是CPU时间，不太精确

    vector<pair<Move,int>> path; //存储路径用于回溯

    //大头戏
    while(true){
        //时间检查——更加精确的计时退出（来源于网上的建议）
        auto currentTime=chrono::high_resolution_clock::now();
        auto elapsedMs=chrono::duration_cast<chrono::milliseconds>(currentTime - startTime).count();
        if(elapsedMs>=MAX_TIME_MS) break; //跳出循环——实践上感觉要预留一点时间
        int timeleft=MAX_TIME_MS - elapsedMs;

        //从根节点开始选择
        UCTNode* node=root;
        int simcolor=currentTurnColor;
        path.clear();

        //选择
        while(node->is_expanded && !node->children.empty()){
            UCTNode* bestChild=nullptr;
            double bestUCT= (simcolor == currentTurnColor) ? -1e18 : 1e18;
            for(auto child:node->children){
                double uct=computeUCT(child,C,simcolor==currentTurnColor);
                if(simcolor==currentTurnColor){ //跟我一样我要最大化，反之最小化
                    if(uct>bestUCT){
                        bestUCT=uct;
                        bestChild=child;
                    }
                }
                else{
                    if(uct<bestUCT){
                        bestUCT=uct;
                        bestChild=child;
                    }
                }
            }//关键的修正——让他极大极小化选择（原本一个劲以为对方给他好处）
            if(bestChild==nullptr) break; //保险,实际上似乎不会出现
            node=bestChild;
            applyMove(bestChild->move, simcolor);
            path.push_back({node->move,simcolor}); //先交换颜色再来存盘是错的！！！！
            simcolor=(simcolor==BLACK)?WHITE:BLACK; //切换颜色
        }

        //扩展
        int threshold=getExpansionThreshold(root->visits); ////要选择哪个呢？？？root?,node?,parent?
        if(!node->is_expanded && (node->visits>=threshold || node==root)){ //根结点或者是访问次数达到阈值才扩展
            vector<Move> moves=generateMoves(simcolor);
            //初步评估和排序
            if(!moves.empty()) {
                // 核心修改！！先随机打乱所有走法 ---
                std::shuffle(moves.begin(), moves.end(), g_rng);
        
                int sampleSize = moves.size();
                if (node != root) {
                    sampleSize = std::min((int)moves.size(), 250); // 深层节点采样250就够了
                    std::shuffle(moves.begin(), moves.end(), g_rng);
                }

                // 如果是根节点(node == root)，保持 sampleSize 为 moves.size()，评估所有走法
                vector<pair<double,Move>> ranked;
                //这里是深层排序，总在想想能不能浅层先排一下——调用评估函数真的很费时间

                for(const auto& m:moves){
                    applyMove(m,simcolor);
                    double val= evaluate(simcolor);
                    ranked.push_back({val,m});
                    //恢复
                    undoMoveLogic(m,simcolor);
                }
            
            
                 //降序排列
                sort(ranked.begin(),ranked.end(),[](const pair<double,Move>& a,const pair<double,Move>& b){return a.first>b.first; });
                //前向剪枝
                int toExpand=getPruning(timeleft,ranked.size());
                for(int i=0;i<toExpand;i++){
                    UCTNode* child=new UCTNode(ranked[i].second,(simcolor==BLACK)?WHITE:BLACK,node);
                    node->children.push_back(child);
                }

                node->is_expanded=true;
                if(!node->children.empty()){
                    node=node->children[0];
                    applyMove(node->move, simcolor);
                    path.push_back({node->move,simcolor});
                    simcolor=(simcolor==BLACK)?WHITE:BLACK; //切换颜色
                }//直接开始搞第一步棋反正其他的后来会因为UCT重新去走
            }
            else{
                node->is_expanded=true; //没有扩展也标记为扩展过，防止重复生成走法遍历(无合法走法不是浪费时间吗！)
            }
        }


        //模拟
        double reward=0.0;
        if(!node->children.empty()){
            reward=rollout(simcolor); 
        }
        else{
            reward=0.0; //没有子节点说明已经输掉了
        }
        if(simcolor!=currentTurnColor){
            reward=1.0-reward; //如果模拟方不是当前方则要反转结果
        }

        //回溯
        UCTNode* temp=node;
        while(temp!=nullptr){
            temp->visits+=1;
            temp->total_value+=reward;
            temp=temp->parent;      
        }
        //恢复棋盘
        for(int i=path.size()-1;i>=0;i--){
            undoMoveLogic(path[i].first, path[i].second);
        }
    }

    Move bestMove=selectBestMove(root);

    if (DEBUG_MODE) { //还要完善！
        cout << "[BOT] MCTS visits=" << root->visits << " bestMove=" 
             << bestMove.x1 << bestMove.y1 << "-" << bestMove.x2 << bestMove.y2 << "-" << bestMove.x3 << bestMove.y3 << endl;
    }
    delete root; //释放内存
    return bestMove;
}

//～～～～～～～～～～～～～～～～～～输入与交互～～～～～～～～～～～～～～～～～～～～～～～
int NOTSHUTUP=true; //静音模式开关

bool parseMoveInput(string input,Move& m){
    //解析输入字符串
    string clean;
    for(char c:input){
        if(!isspace(c)) clean+=c;
    }
    if(clean.length()!=6) return false;
    for(char c:clean){
        if(!isdigit(c)) return false;
    }
    m.x1=clean[0]-'0';
    m.y1=clean[1]-'0';
    m.x2=clean[2]-'0';
    m.y2=clean[3]-'0';  
    m.x3=clean[4]-'0';
    m.y3=clean[5]-'0';
    if(!inMap(m.x1,m.y1) || !inMap(m.x2,m.y2) || !inMap(m.x3,m.y3)) return false;
    return true;
}

//小功能：显示当前局势（皇后/国王优势）
void displayAdvantageBoard(int advantageType) {
    // advantageType: 1=皇后优势, 2=国王优势
    int D1[2][GRIDSIZE][GRIDSIZE], D2[2][GRIDSIZE][GRIDSIZE];
    if (advantageType == 1) {
        calculateDistances(D1, 1); // 皇后距离
    } else {
        calculateDistances(D2, 2); // 国王距离
    }

    cout << "\n\t" << (advantageType == 1 ? "皇后优势 (Queen)" : "国王优势 (King)") << " 步数评估\n\n";
    cout << "   "; for (int i = 0; i < GRIDSIZE; i++) cout << "  " << i << " "; cout << "\n";
    cout << "   ┌"; for (int i = 0; i < GRIDSIZE-1; i++) cout << "────┬"; cout << "────┐\n";
    for (int i = 0; i < GRIDSIZE; i++) {
        cout << " " << i << " │";
        for (int j = 0; j < GRIDSIZE; j++) {
            if (grid[i][j] != EMPTY && grid[i][j] != OBSTACLE) {
                cout << (grid[i][j] == BLACK ? " ●  " : " ○  ") << "│";
            } else if (grid[i][j] == OBSTACLE) {
                cout << " ×  │";
            } else {
                if (advantageType == 1) {
                    int bDist = D1[0][i][j];
                    int wDist = D1[1][i][j];
                    if (bDist == INF && wDist == INF) cout << "    │";
                    else {
                        string s = (bDist < wDist ? "B" : "W") + to_string(min(bDist, wDist));
                        cout << setw(4) << s << "│";
                    }
                } else {
                    int bDist = D2[0][i][j];
                    int wDist = D2[1][i][j];
                    if (bDist == INF && wDist == INF) cout << "    │";
                    else {
                        string s = (bDist < wDist ? "B" : "W") + to_string(min(bDist, wDist));
                        cout << setw(4) << s << "│";
                    }
                }
            }
        }
        cout << "\n";
        if (i < GRIDSIZE - 1) { cout << "   ├"; for (int j = 0; j < GRIDSIZE-1; j++) cout << "────┼"; cout << "────┤\n"; }
    }
    cout << "   └"; for (int i = 0; i < GRIDSIZE-1; i++) cout << "────┴"; cout << "────┘\n";
    cout << "rmk：Bx 表示黑方优势(步数x)，Wx 表示白方优势(步数x)\n";
}


void displayBoard(bool vsAI) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
    cout << "\n\tGame of the Amazons 终极版本\n\n";
    cout << "   "; for (int i = 0; i < GRIDSIZE; i++) cout << "  " << i << " "; cout << "\n";
    cout << "   ┌"; for (int i = 0; i < GRIDSIZE-1; i++) cout << "───┬"; cout << "───┐\n";
    for (int i = 0; i < GRIDSIZE; i++) {
        cout << " " << i << " │";
        for (int j = 0; j < GRIDSIZE; j++) {
            if (grid[i][j] == EMPTY) cout << "   │";
            else if (grid[i][j] == BLACK) cout << " ● │";
            else if (grid[i][j] == WHITE) cout << " ○ │";
            else if (grid[i][j] == OBSTACLE) cout << " × │";
        }
        cout << "\n";
        if (i < GRIDSIZE - 1) { cout << "   ├"; for (int j = 0; j < GRIDSIZE-1; j++) cout << "───┼"; cout << "───┤\n"; }
    }
    cout << "   └"; for (int i = 0; i < GRIDSIZE-1; i++) cout << "───┴"; cout << "───┘\n";
    cout << "\n当前行动: " << (currentTurnColor == BLACK ? "黑方 (●)" : "白方 (○)") << "\n";
    if(vsAI) cout << "AI难度: " << MAX_TIME_MS << "ms | AI调试: " << (DEBUG_MODE ? "ON" : "OFF") << endl;
    cout << "您执: " << (HUMAN_PLAYER_COLOR == BLACK ? "黑子" : "白子") << endl;
    cout << "对局中您可以试试如下命令: a=显示皇后优势, b=显示国王优势, -1=悔棋";
    if(NOTSHUTUP && vsAI) cout<<"（您随便悔吧，别悔得肠子都青了就行！）, ";
    cout<<"-2=退出\n";
    if(!vsAI) cout<<"pk=BOT PK ME!\n";
    if(NOTSHUTUP && vsAI) cout << "要是您太慢我随时可能吓你一跳！\n";
    if(NOTSHUTUP && vsAI) cout << "我也许会让你烦，如果你忍心就输入“shutup!”让我闭嘴吧\n";

}

// 小小功能：超时幽默提示
bool checkInputTimeout(int timeout_ms) {
    auto start = chrono::steady_clock::now();
    while (shoot) {
        
        if(!NOTSHUTUP) return false; //静音模式下不提示
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - start).count();
        if (elapsed >= timeout_ms) {
            // 随机输出幽默提示
            if (rand() % 2 == 0) 
                cout << "\n\n哥们，太慢了，我等得花都谢了！！！\n";
            else 
                cout << "\n\nHey,you are too too slow that waiting you makes me almost fall asleep!!!\n";
            return false;
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

void playGame(bool vssAI) {
    bool vsAI=vssAI;
    string inputLine;
    while (true) {
        displayBoard(vsAI);
        vector<Move> moves = generateMoves(currentTurnColor);
        if (moves.empty()) {
            cout << "\nGAME OVER！" << (currentTurnColor == BLACK ? "白方" : "黑方") << " WIN！\n按回车键返回菜单...";
            getline(cin, inputLine);  //强制暂停直到你按下回车键
            return;
        }

        if (vsAI && currentTurnColor!= HUMAN_PLAYER_COLOR) {
            cout << "BOT THINKING..." << endl;
            Move m = getMCTSMove();
            if (m.x1 == -1) { cout << "BOT LOSE！\n"; return; }
            saveStateForUndo();
            applyMove(m, currentTurnColor);
            saveMoveToFile(m); // [新增] AI落子存盘
            history.push_back(m); //同步记录，否则txt也清空了
            currentTurnColor = (currentTurnColor == BLACK ? WHITE : BLACK);
        } else {
            cout << "请输入走法 (支持多种格式任你选择！如 121312 或 1 2 1 3 1 2) \n 输入走法（如 121312 或 1 2 1 3 1 2）或命令其中一者(a,b,-1,-2,";
            if(!vsAI) cout<<"pk（双人模式切换到bot)";
            cout<<"）: "<<flush;
            
            if (NOTSHUTUP && vsAI && !checkInputTimeout(3000)) {
                cout << "请继续输入（刚才输入已经读取）: ";
            }
            
            getline(cin, inputLine);
            
            if(vsAI && inputLine == "shutup!" && NOTSHUTUP) {
                if(NOTSHUTUP)cout << "呜呜呜你真的太坏了！ 我的嘴……" << " 按回车继续...";
                NOTSHUTUP = false;
                getline(cin, inputLine);
                continue;
            }
            if(!vsAI && inputLine == "pk") {
                cout << "切换到与BOT对战模式！按回车继续...";
                getline(cin, inputLine);
                vsAI = true;
                continue;
            }
            if (inputLine == "-2") return;
            if (inputLine == "-1") {
                if (performUndo()) { 
                    if (vsAI) performUndo(); 
                    continue; 
                }
                else { 
                    cout << "无法悔棋！按回车继续..."; 
                    getline(cin, inputLine); 
                    continue; 
                }
            }
            if (inputLine == "a") {
                displayAdvantageBoard(1);
                cout << "按回车继续...";
                getline(cin, inputLine);
                continue;
            }
            if (inputLine == "b") {
                displayAdvantageBoard(2);
                cout << "按回车继续...";
                getline(cin, inputLine);
                continue;
            }
            
            Move m;
            if (!parseMoveInput(inputLine, m) || !isValidMove(m, currentTurnColor, NOTSHUTUP)) {
                cout << "输入错误或非法走法！按回车重试..."; 
                getline(cin, inputLine); 
                continue;
            }
            saveStateForUndo();
            applyMove(m, currentTurnColor);
            saveMoveToFile(m); // 人类落子存盘
            history.push_back(m); //同步记录，否则txt也清空了
            currentTurnColor = (currentTurnColor == BLACK ? WHITE : BLACK);
        }
    }
}


void gameLoop(bool vsAI) {
    initBoard();
    history.clear(); // 确保历史记录清空
    currentTurnColor = BLACK; // 默认黑棋先手

    // [关键] 每次开新局前，清空旧的存档文件
    ofstream fout(SAVE_PATH, ios::trunc); 
    fout.close();

    // [关键] 初始化完成后，直接调用 playGame 进入主循环
    // 这样 replayAndContinue 也能复用 playGame 的逻辑
    playGame(vsAI);
}

// 设置AI难度
void setAIDifficulty() {
    cout << "\n请选择AI思考时长 (ms):\n";
    cout << "1. 500ms 初出茅庐 \n2. 800ms 渐有所进 \n3. 1000ms 竞争之手 \n";
    cout << "4. 2000ms 干练之手 \n5. 3000ms 深思之主 \n6. 5000ms 冥想之剑 \n7. 8000ms 恶魔困境 \n";
    cout << "当前设置: " << MAX_TIME_MS << "ms\n选择: ";
    string choice;
    getline(cin, choice);
    if (choice == "1") MAX_TIME_MS = 500;
    else if (choice == "2") MAX_TIME_MS = 800;
    else if (choice == "3") MAX_TIME_MS = 1000;
    else if (choice == "4") MAX_TIME_MS = 2000;
    else if (choice == "5") MAX_TIME_MS = 3000;
    else if (choice == "6") MAX_TIME_MS = 5000;
    else if (choice == "7") MAX_TIME_MS = 8000;
    else cout << "无效选择，保持原设置 " << MAX_TIME_MS << "ms\n";
}

// 选择执棋颜色
void choosePlayerColor() {
    cout << "\n请选择您执的棋子:\n";
    cout << "1. 执黑 (默认，先手)\n";
    cout << "2. 执白 \n";
    cout << "选择: ";
    string choice;
    getline(cin, choice);
    if (choice == "2") {
        HUMAN_PLAYER_COLOR = WHITE;
        cout << "您选择执白，AI将执黑先手\n";
    } else {
        HUMAN_PLAYER_COLOR = BLACK;
        cout << "您选择执黑，您先手\n";
    }
}



void replayAndContinue() {
    initBoard();
    vector<Move> fullHistory;
    ifstream fin(SAVE_PATH);
    if (!fin) {
        cout << "错误：未发现存档文件！" << endl;
        this_thread::sleep_for(chrono::seconds(1));
        return;
    }

    // 1. 预读所有历史走法
    Move m;
    while (fin >> m.x1 >> m.y1 >> m.x2 >> m.y2 >> m.x3 >> m.y3) {
        fullHistory.push_back(m);
    }
    fin.close();

    if (fullHistory.empty()) {
        cout << "存档为空！" << endl;
        return;
    }

    int currentStep = 0;
    int totalSteps = fullHistory.size();
    
    while (true) {
        displayBoard(false); // 显示当前步后的棋盘
        cout << "\n--- 复盘模式 [步数: " << currentStep << " / " << totalSteps << "] ---" << endl;
        if (currentStep < totalSteps) {
            Move nextM = fullHistory[currentStep];
            cout << "\n下一步预告: (" << nextM.x1 << nextM.y1 << ")->" << nextM.x2 << nextM.y2 << " 箭" << nextM.x3 << nextM.y3 << endl;
        }

        cout << "操作说明: [n]下一步 [p]上一步 [k]跳转到步数 [c]从此处续玩 [q]退出\n选择: ";
        string choice;
        getline(cin, choice);

        if (choice == "n") { // 下一步
            if (currentStep < totalSteps) {
                saveStateForUndo(); // 记录状态以便悔棋或回退
                applyMove(fullHistory[currentStep], currentTurnColor);
                history.push_back(fullHistory[currentStep]);
                currentTurnColor = (currentTurnColor == BLACK ? WHITE : BLACK);
                currentStep++;
            } else {
                cout << "已经是最后一步了！按回车继续...";
                getline(cin, choice);
            }
        } 
        else if (choice == "p") { // 上一步 (回退)
            if (currentStep > 0) {
                performUndo(); // 利用你现有的悔棋逻辑
                currentStep--;
            }
        }
        else if (choice == "k") { // 跳转到第 k 步
            cout << "请输入目标步数 (0-" << totalSteps << "): ";
            string kStr;
            getline(cin, kStr);
            
            // 简单的字符串转数字判断
            if(!kStr.empty() && isdigit(kStr[0])) {
                int k = stoi(kStr);
                if (k >= 0 && k <= totalSteps) {
                    // --- 核心：重建现场 ---
                    initBoard(); // 这会清空 undoStack
                    currentStep = 0;
                    currentTurnColor = BLACK;
                    
                    for (int i = 0; i < k; i++) {
                        saveStateForUndo(); // 关键：重建 undoStack，让你跳转后还能 p
                        applyMove(fullHistory[i], currentTurnColor);
                        history.push_back(fullHistory[i]);
                        currentTurnColor = (currentTurnColor == BLACK ? WHITE : BLACK);
                        currentStep++;
                    }
                }
            }
        }
        else if (choice == "c") { // 核心功能：续玩
            cout << "正在从此状态进入对局...\n";
            // 重新刷新物理存档，确保之后的操作是连续的
            overwriteSaveWithHistory(history);
            cout << "[1] CONTINUE [2] BOT PK ME! [q] 返回主菜单\n选择: ";

            string choice;
            getline(cin, choice);
            if (choice == "1") {
                // 注意：这里不要调用 gameLoop，直接进入 while(true) 逻辑
                playGame(false); 
            } else if (choice == "2") {
                playGame(true);
            }
            else if( choice == "q") {
                return; // 退出复盘
        }
        }
        else if (choice == "q") { // 退出复盘
            break;
        }
    }
}

int main() {
    srand(time(0));
    string line;
    while (true) {
        #ifdef _WIN32
        system("cls");
        #else
        system("clear");
        #endif
        cout << "===============Welcome to my Amazons World==================\n";
        cout<<"温馨提示：如果在Mac终端，颜色可能会相反哦！\n";
        cout << "1. 人机对战\n";
        cout << "2. 双人对战\n";
        cout << "3. 设置AI难度 (当前: " << MAX_TIME_MS << "ms)\n";
        cout << "4. AI调试模式 (当前: " << (DEBUG_MODE ? "ON" : "OFF") << ")\n";
        cout << "5. 选择执棋颜色 (当前: " << (HUMAN_PLAYER_COLOR == BLACK ? "执黑" : "执白") << ")\n";
        cout << "6. 退出\n";
        cout << "7.复盘功能\n";
        cout << "=====================================================\n";
        cout << "请选择: ";
        getline(cin, line);
        
        if (line == "1") {
            shoot=true;
            gameLoop(true);
        } else if (line == "2") {
            gameLoop(false);
        } else if (line == "3") {
            setAIDifficulty();
        } else if (line == "4") {
            DEBUG_MODE = !DEBUG_MODE;
            cout << "AI调试模式 " << (DEBUG_MODE ? "已开启" : "已关7闭") << endl;
            cout << "按回车继续...";
            getline(cin, line);
        } else if (line == "5") {
            choosePlayerColor();
        } else if (line == "6") {
            break;
        } else if (line == "7") {
            replayAndContinue();
        } else {
            cout << "无效选择" << endl;
            cout << "按回车继续...";
            getline(cin, line);
        }
    }
    return 0;
}