#include "main_window.h"

#include "board_widget.h"

#include <QtConcurrent/QtConcurrentRun>

#include <QComboBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace zzmazon {
namespace {

QString playerName(int player) {
    return player == Black ? QStringLiteral("黑方") : QStringLiteral("白方");
}

QString playerEnglishName(int player) {
    return player == Black ? QStringLiteral("BLACK") : QStringLiteral("WHITE");
}

void refreshStyle(QWidget* widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QLabel* makeSectionLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionLabel"));
    return label;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    buildInterface();
    connectInterface();
    startNewGame();
}

void MainWindow::buildInterface() {
    setWindowTitle(QStringLiteral("ZZmazon · Amazons Studio"));
    resize(1320, 850);
    setMinimumSize(1060, 700);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("appRoot"));
    setCentralWidget(root);

    auto* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(28, 24, 28, 24);
    rootLayout->setSpacing(24);

    auto* playArea = new QFrame(root);
    playArea->setObjectName(QStringLiteral("playArea"));
    auto* playLayout = new QVBoxLayout(playArea);
    playLayout->setContentsMargins(24, 20, 24, 22);
    playLayout->setSpacing(10);

    auto* topBar = new QWidget(playArea);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(6, 0, 6, 0);
    topLayout->setSpacing(10);

    auto* brandBlock = new QWidget(topBar);
    auto* brandLayout = new QVBoxLayout(brandBlock);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(1);
    auto* eyebrow = new QLabel(QStringLiteral("GAME OF THE AMAZONS"), brandBlock);
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    auto* brand = new QLabel(QStringLiteral("ZZmazon Studio"), brandBlock);
    brand->setObjectName(QStringLiteral("brandTitle"));
    brandLayout->addWidget(eyebrow);
    brandLayout->addWidget(brand);

    turnBadge_ = new QLabel(topBar);
    turnBadge_->setObjectName(QStringLiteral("turnBadge"));
    turnBadge_->setAlignment(Qt::AlignCenter);
    turnBadge_->setMinimumWidth(184);

    topLayout->addWidget(brandBlock);
    topLayout->addStretch();
    topLayout->addWidget(turnBadge_);

    boardWidget_ = new BoardWidget(playArea);
    boardWidget_->setGame(&game_);
    playLayout->addWidget(topBar);
    playLayout->addWidget(boardWidget_, 1);

    auto* playShadow = new QGraphicsDropShadowEffect(playArea);
    playShadow->setBlurRadius(38.0);
    playShadow->setOffset(0.0, 12.0);
    playShadow->setColor(QColor(0, 0, 0, 72));
    playArea->setGraphicsEffect(playShadow);

    auto* sidebar = new QFrame(root);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(344);
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(24, 24, 24, 22);
    sideLayout->setSpacing(12);

    auto* statusCard = new QFrame(sidebar);
    statusCard->setObjectName(QStringLiteral("statusCard"));
    auto* statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(18, 17, 18, 16);
    statusLayout->setSpacing(6);
    statusTitle_ = new QLabel(QStringLiteral("准备开始"), statusCard);
    statusTitle_->setObjectName(QStringLiteral("statusTitle"));
    statusDetail_ = new QLabel(statusCard);
    statusDetail_->setObjectName(QStringLiteral("statusDetail"));
    statusDetail_->setWordWrap(true);
    phaseLabel_ = new QLabel(QStringLiteral("选择一枚棋子"), statusCard);
    phaseLabel_->setObjectName(QStringLiteral("phasePill"));
    phaseLabel_->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(statusTitle_);
    statusLayout->addWidget(statusDetail_);
    statusLayout->addSpacing(5);
    statusLayout->addWidget(phaseLabel_);
    sideLayout->addWidget(statusCard);

    sideLayout->addWidget(makeSectionLabel(QStringLiteral("对局设置"), sidebar));
    modeCombo_ = new QComboBox(sidebar);
    modeCombo_->addItem(QStringLiteral("人机对战"), true);
    modeCombo_->addItem(QStringLiteral("本地双人"), false);
    sideCombo_ = new QComboBox(sidebar);
    sideCombo_->addItem(QStringLiteral("我执黑 · 先手"), Black);
    sideCombo_->addItem(QStringLiteral("我执白 · 后手"), White);
    difficultyCombo_ = new QComboBox(sidebar);
    difficultyCombo_->addItem(QStringLiteral("轻快 · 350 ms"), 350);
    difficultyCombo_->addItem(QStringLiteral("均衡 · 800 ms"), 800);
    difficultyCombo_->addItem(QStringLiteral("深入 · 1500 ms"), 1500);
    difficultyCombo_->setCurrentIndex(1);
    sideLayout->addWidget(modeCombo_);
    sideLayout->addWidget(sideCombo_);
    sideLayout->addWidget(difficultyCombo_);

    auto* actionRow = new QWidget(sidebar);
    auto* actionLayout = new QHBoxLayout(actionRow);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(9);
    newGameButton_ = new QPushButton(QStringLiteral("新对局"), actionRow);
    newGameButton_->setObjectName(QStringLiteral("primaryButton"));
    undoButton_ = new QPushButton(QStringLiteral("悔棋"), actionRow);
    hintButton_ = new QPushButton(QStringLiteral("提示"), actionRow);
    actionLayout->addWidget(newGameButton_, 2);
    actionLayout->addWidget(undoButton_, 1);
    actionLayout->addWidget(hintButton_, 1);
    sideLayout->addWidget(actionRow);

    auto* logHeader = new QWidget(sidebar);
    auto* logHeaderLayout = new QHBoxLayout(logHeader);
    logHeaderLayout->setContentsMargins(0, 4, 0, 0);
    auto* logTitle = makeSectionLabel(QStringLiteral("走子记录"), sidebar);
    moveCountLabel_ = new QLabel(QStringLiteral("0 PLY"), sidebar);
    moveCountLabel_->setObjectName(QStringLiteral("counterLabel"));
    logHeaderLayout->addWidget(logTitle);
    logHeaderLayout->addStretch();
    logHeaderLayout->addWidget(moveCountLabel_);
    sideLayout->addWidget(logHeader);

    moveLog_ = new QListWidget(sidebar);
    moveLog_->setObjectName(QStringLiteral("moveLog"));
    moveLog_->setFocusPolicy(Qt::NoFocus);
    moveLog_->setSelectionMode(QAbstractItemView::NoSelection);
    sideLayout->addWidget(moveLog_, 1);

    auto* help = new QLabel(
        QStringLiteral(
            "操作：点击棋子 → 点击终点 → 点击箭点\n"
            "右键或 Esc 可取消当前选择"
        ),
        sidebar
    );
    help->setObjectName(QStringLiteral("helpText"));
    help->setWordWrap(true);
    sideLayout->addWidget(help);

    auto* sideShadow = new QGraphicsDropShadowEffect(sidebar);
    sideShadow->setBlurRadius(34.0);
    sideShadow->setOffset(0.0, 10.0);
    sideShadow->setColor(QColor(0, 0, 0, 68));
    sidebar->setGraphicsEffect(sideShadow);

    rootLayout->addWidget(playArea, 1);
    rootLayout->addWidget(sidebar);

    auto* newGameShortcut = new QShortcut(
        QKeySequence(QStringLiteral("Ctrl+N")),
        this
    );
    auto* undoShortcut = new QShortcut(
        QKeySequence(QStringLiteral("Ctrl+Z")),
        this
    );
    connect(newGameShortcut, &QShortcut::activated,
            this, &MainWindow::startNewGame);
    connect(undoShortcut, &QShortcut::activated,
            this, &MainWindow::undoMove);
}

void MainWindow::connectInterface() {
    connect(
        boardWidget_,
        &BoardWidget::moveChosen,
        this,
        &MainWindow::handleMoveChosen
    );
    connect(
        boardWidget_,
        &BoardWidget::animationFinished,
        this,
        &MainWindow::handleAnimationFinished
    );
    connect(
        boardWidget_,
        &BoardWidget::interactionHintChanged,
        phaseLabel_,
        &QLabel::setText
    );
    connect(
        boardWidget_,
        &BoardWidget::selectionTransitionChanged,
        this,
        [this](bool active) {
            boardTransition_ = active;
            updateInterface();
        }
    );
    connect(newGameButton_, &QPushButton::clicked,
            this, &MainWindow::startNewGame);
    connect(undoButton_, &QPushButton::clicked,
            this, &MainWindow::undoMove);
    connect(hintButton_, &QPushButton::clicked,
            this, &MainWindow::requestHint);
    connect(&aiWatcher_, &QFutureWatcher<std::optional<Move>>::finished,
            this, &MainWindow::finishAiTurn);
    connect(&hintWatcher_, &QFutureWatcher<std::optional<Move>>::finished,
            this, &MainWindow::finishHint);

    connect(modeCombo_, &QComboBox::currentIndexChanged, this, [this] {
        const bool enabled = modeCombo_->currentData().toBool();
        sideCombo_->setEnabled(enabled && !busy_);
        difficultyCombo_->setEnabled(enabled && !busy_);
    });
}

void MainWindow::startNewGame() {
    if (busy_ || boardTransition_) {
        return;
    }

    versusAi_ = modeCombo_->currentData().toBool();
    humanColor_ = sideCombo_->currentData().toInt();
    game_.reset();
    moveLog_->clear();
    boardWidget_->resetSelection();
    boardWidget_->setGame(&game_);
    statusTitle_->setText(QStringLiteral("新对局已就绪"));
    statusDetail_->setText(
        versusAi_
            ? QStringLiteral("你执%1，点击棋子开始。")
                  .arg(humanColor_ == Black ? QStringLiteral("黑")
                                            : QStringLiteral("白"))
            : QStringLiteral("本地双人模式，黑方先手。")
    );
    updateInterface();

    if (versusAi_ && !isHumanTurn()) {
        QTimer::singleShot(260, this, &MainWindow::startAiTurn);
    }
}

void MainWindow::handleMoveChosen(const Move& move) {
    if (busy_ || boardTransition_ || !isHumanTurn()) {
        return;
    }

    const int movingPlayer = game_.currentPlayer();
    if (!game_.playMove(move)) {
        statusDetail_->setText(QStringLiteral("这一步不合法，请重新选择。"));
        boardWidget_->resetSelection();
        return;
    }

    appendMoveToLog(movingPlayer, move);
    setBusy(true, QStringLiteral("落子动画进行中"));
    boardWidget_->animateArrow(move);
    updateInterface();
}

void MainWindow::handleAnimationFinished() {
    busy_ = false;
    updateInterface();
    if (checkGameOver()) {
        return;
    }

    if (versusAi_ && !isHumanTurn()) {
        QTimer::singleShot(180, this, &MainWindow::startAiTurn);
    } else {
        statusTitle_->setText(QStringLiteral("%1行动").arg(
            playerName(game_.currentPlayer())
        ));
        statusDetail_->setText(QStringLiteral("选择棋子，光轨会展示所有合法方向。"));
        boardWidget_->setInputEnabled(true);
    }
}

void MainWindow::startAiTurn() {
    if (busy_ || !versusAi_ || isHumanTurn()) {
        return;
    }

    const auto board = game_.board();
    const int player = game_.currentPlayer();
    const int thinkTime = selectedThinkTime();
    setBusy(
        true,
        QStringLiteral("AI 正在评估领地、机动性与对手回应…")
    );
    statusTitle_->setText(QStringLiteral("ZZmazon 思考中"));
    phaseLabel_->setText(QStringLiteral("SEARCHING · %1 MS").arg(thinkTime));

    aiWatcher_.setFuture(QtConcurrent::run([board, player, thinkTime] {
        return AmazonsGame::chooseMove(board, player, thinkTime);
    }));
}

void MainWindow::finishAiTurn() {
    const std::optional<Move> move = aiWatcher_.result();
    if (!move) {
        busy_ = false;
        (void)checkGameOver();
        updateInterface();
        return;
    }

    const int movingPlayer = game_.currentPlayer();
    if (!game_.playMove(*move)) {
        busy_ = false;
        statusTitle_->setText(QStringLiteral("AI 状态已变化"));
        statusDetail_->setText(QStringLiteral("请开始一个新对局。"));
        updateInterface();
        return;
    }

    appendMoveToLog(movingPlayer, *move);
    setBusy(true, QStringLiteral("ZZmazon 已落子"));
    boardWidget_->animateMove(*move);
    updateInterface();
}

void MainWindow::undoMove() {
    if (busy_ || boardTransition_ || !game_.canUndo()) {
        return;
    }

    const int plies = versusAi_ ? std::min(2, game_.moveCount()) : 1;
    if (!game_.undoPlies(plies)) {
        return;
    }
    removeMovesFromLog(plies);
    boardWidget_->resetSelection();
    statusTitle_->setText(QStringLiteral("已回到上一局面"));
    statusDetail_->setText(
        versusAi_ ? QStringLiteral("已撤销你和 AI 的最近落子。")
                  : QStringLiteral("已撤销最近一步。")
    );
    updateInterface();
}

void MainWindow::requestHint() {
    if (busy_ || boardTransition_ || !isHumanTurn()) {
        return;
    }

    const auto board = game_.board();
    const int player = game_.currentPlayer();
    const int thinkTime = std::min(520, selectedThinkTime());
    setBusy(true, QStringLiteral("正在生成局面提示…"));
    statusTitle_->setText(QStringLiteral("分析当前局面"));
    phaseLabel_->setText(QStringLiteral("CALCULATING HINT"));

    hintWatcher_.setFuture(QtConcurrent::run([board, player, thinkTime] {
        return AmazonsGame::chooseMove(board, player, thinkTime);
    }));
}

void MainWindow::finishHint() {
    busy_ = false;
    const std::optional<Move> move = hintWatcher_.result();
    if (move) {
        boardWidget_->showHint(*move);
        statusTitle_->setText(QStringLiteral("建议路线已标出"));
        statusDetail_->setText(QString::fromStdString(
            AmazonsGame::formatMove(*move)
        ));
        phaseLabel_->setText(QStringLiteral("金色虚线为建议路线"));
    }
    updateInterface();
}

void MainWindow::updateInterface() {
    const int turn = game_.currentPlayer();
    turnBadge_->setText(
        QStringLiteral("%1  %2 TO MOVE")
            .arg(turn == Black ? QStringLiteral("●") : QStringLiteral("○"),
                 playerEnglishName(turn))
    );
    turnBadge_->setProperty("side", turn == Black ? "black" : "white");
    refreshStyle(turnBadge_);

    moveCountLabel_->setText(QStringLiteral("%1 PLY").arg(game_.moveCount()));
    const bool humanTurn = isHumanTurn();
    const bool controlsLocked = busy_ || boardTransition_;
    boardWidget_->setInputEnabled(!controlsLocked && humanTurn);

    const bool canUndoInMode = !controlsLocked && game_.canUndo() &&
        (!versusAi_ || game_.moveCount() >= 2);
    undoButton_->setEnabled(canUndoInMode);
    hintButton_->setEnabled(!controlsLocked && humanTurn);
    newGameButton_->setEnabled(!controlsLocked);
    modeCombo_->setEnabled(!controlsLocked);
    sideCombo_->setEnabled(!controlsLocked && modeCombo_->currentData().toBool());
    difficultyCombo_->setEnabled(
        !controlsLocked && modeCombo_->currentData().toBool()
    );

    if (!busy_ && humanTurn && !boardWidget_->hasFocus()) {
        boardWidget_->setFocus(Qt::OtherFocusReason);
    }
}

void MainWindow::setBusy(bool busy, const QString& message) {
    busy_ = busy;
    boardWidget_->setInputEnabled(!busy && isHumanTurn());
    if (!message.isEmpty()) {
        statusDetail_->setText(message);
    }
    updateInterface();
}

void MainWindow::appendMoveToLog(int player, const Move& move) {
    const int ply = game_.moveCount();
    auto* item = new QListWidgetItem(
        QStringLiteral("%1  %2  %3")
            .arg(ply, 2, 10, QLatin1Char('0'))
            .arg(player == Black ? QStringLiteral("●") : QStringLiteral("○"))
            .arg(QString::fromStdString(AmazonsGame::formatMove(move))),
        moveLog_
    );
    item->setData(Qt::UserRole, player);
    moveLog_->scrollToBottom();
}

void MainWindow::removeMovesFromLog(int count) {
    while (count-- > 0 && moveLog_->count() > 0) {
        delete moveLog_->takeItem(moveLog_->count() - 1);
    }
}

bool MainWindow::checkGameOver() {
    const int winningPlayer = game_.winner();
    if (winningPlayer == Empty) {
        return false;
    }

    boardWidget_->setInputEnabled(false);
    statusTitle_->setText(QStringLiteral("%1获胜").arg(playerName(winningPlayer)));
    statusDetail_->setText(QStringLiteral("对手已没有任何合法移动。"));
    phaseLabel_->setText(QStringLiteral("GAME COMPLETE"));

    QMessageBox message(this);
    message.setWindowTitle(QStringLiteral("对局结束"));
    message.setIcon(QMessageBox::NoIcon);
    message.setText(QStringLiteral("%1获胜").arg(playerName(winningPlayer)));
    message.setInformativeText(QStringLiteral("对手已无合法走法。可以悔棋复盘，或开启新对局。"));
    message.setStandardButtons(QMessageBox::Ok);
    message.exec();
    return true;
}

bool MainWindow::isHumanTurn() const {
    return !versusAi_ || game_.currentPlayer() == humanColor_;
}

int MainWindow::selectedThinkTime() const {
    return difficultyCombo_->currentData().toInt();
}

}  // namespace zzmazon
