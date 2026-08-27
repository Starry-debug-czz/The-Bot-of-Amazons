#include "main_window.h"

#include "board_widget.h"

#include <QtConcurrent/QtConcurrentRun>

#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPropertyAnimation>
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

QString playerMark(int player) {
    return player == Black ? QStringLiteral("黑") : QStringLiteral("白");
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
    setWindowTitle(QStringLiteral("ZZmazon · 弈棋斋 · 亚马逊棋"));
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
    auto* eyebrow = new QLabel(QStringLiteral("GAME OF THE AMAZONS · 弈"), brandBlock);
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    auto* brand = new QLabel(QStringLiteral("ZZmazon 弈棋斋"), brandBlock);
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
    modeCombo_->addItem(QStringLiteral("人机对弈"), true);
    modeCombo_->addItem(QStringLiteral("双人对弈"), false);
    sideCombo_ = new QComboBox(sidebar);
    sideCombo_->addItem(QStringLiteral("吾执黑子 · 先行"), Black);
    sideCombo_->addItem(QStringLiteral("吾执白子 · 后行"), White);
    difficultyCombo_ = new QComboBox(sidebar);
    difficultyCombo_->addItem(QStringLiteral("初窥门径 · 350ms"), 350);
    difficultyCombo_->addItem(QStringLiteral("渐入佳境 · 800ms"), 800);
    difficultyCombo_->addItem(QStringLiteral("登峰造极 · 1500ms"), 1500);
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
    hintButton_ = new QPushButton(QStringLiteral("锦囊"), actionRow);
    actionLayout->addWidget(newGameButton_, 2);
    actionLayout->addWidget(undoButton_, 1);
    actionLayout->addWidget(hintButton_, 1);
    sideLayout->addWidget(actionRow);

    auto* logHeader = new QWidget(sidebar);
    auto* logHeaderLayout = new QHBoxLayout(logHeader);
    logHeaderLayout->setContentsMargins(0, 4, 0, 0);
    auto* logTitle = makeSectionLabel(QStringLiteral("手谈实录"), sidebar);
    moveCountLabel_ = new QLabel(QStringLiteral("共 0 手"), sidebar);
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
            "先点一子定去向，再点空处移玉步，\n"
            "末点一处降朱障。右键或 Esc 收回成命。"
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
    setBusy(true, QStringLiteral("落子进行中…"));
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
        statusTitle_->setText(QStringLiteral("轮到%1").arg(
            playerName(game_.currentPlayer())
        ));
        statusDetail_->setText(QStringLiteral("点选己方棋子，青色墨点皆为可行之处。"));
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
        QStringLiteral("AI 运筹帷幄，推演领地与杀势…")
    );
    statusTitle_->setText(QStringLiteral("AI 沉思中"));
    phaseLabel_->setText(QStringLiteral("运筹 · %1 毫秒").arg(thinkTime));

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
    setBusy(true, QStringLiteral("AI 已落子"));
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
    setBusy(true, QStringLiteral("推演当前局面…"));
    statusTitle_->setText(QStringLiteral("锦囊推演中"));
    phaseLabel_->setText(QStringLiteral("推演中…"));

    hintWatcher_.setFuture(QtConcurrent::run([board, player, thinkTime] {
        return AmazonsGame::chooseMove(board, player, thinkTime);
    }));
}

void MainWindow::finishHint() {
    busy_ = false;
    const std::optional<Move> move = hintWatcher_.result();
    if (move) {
        boardWidget_->showHint(*move);
        statusTitle_->setText(QStringLiteral("锦囊已开"));
        statusDetail_->setText(QString::fromStdString(
            AmazonsGame::formatMove(*move)
        ));
        phaseLabel_->setText(QStringLiteral("赭金虚线乃推荐着法"));
    }
    updateInterface();
}

void MainWindow::updateInterface() {
    const int turn = game_.currentPlayer();
    turnBadge_->setText(
        QStringLiteral("%1 %2 行棋")
            .arg(turn == Black ? QStringLiteral("●") : QStringLiteral("○"),
                 playerName(turn))
    );
    turnBadge_->setProperty("side", turn == Black ? "black" : "white");
    refreshStyle(turnBadge_);

    moveCountLabel_->setText(QStringLiteral("共 %1 手").arg(game_.moveCount()));
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
        QStringLiteral("%1  %2 · %3")
            .arg(ply, 2, 10, QLatin1Char('0'))
            .arg(playerMark(player))
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
    phaseLabel_->setText(QStringLiteral("此局终了"));

    showGameOverDialog(winningPlayer);
    return true;
}

void MainWindow::showGameOverDialog(int winningPlayer) {
    QDialog dialog(this);
    dialog.setModal(true);
    dialog.setFixedSize(396, 432);
    dialog.setAttribute(Qt::WA_TranslucentBackground);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    // QFrame 才能可靠承载样式表背景(纯 QDialog 在部分平台不绘背景)
    auto* card = new QFrame(&dialog);
    card->setObjectName(QStringLiteral("gameOverCard"));

    const bool humanWon = !versusAi_ || winningPlayer == humanColor_;

    auto* outerLayout = new QVBoxLayout(&dialog);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(card);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(32, 34, 32, 26);
    layout->setSpacing(7);

    // 朱印:胜局钤朱砂,负局钤焦墨
    auto* seal = new QLabel(humanWon ? QStringLiteral("胜") : QStringLiteral("负"),
                            &dialog);
    seal->setObjectName(QStringLiteral("gameOverSeal"));
    seal->setProperty("result", humanWon ? QStringLiteral("win")
                                         : QStringLiteral("loss"));
    seal->setAlignment(Qt::AlignCenter);
    seal->setFixedSize(112, 112);
    refreshStyle(seal);
    layout->addWidget(seal, 0, Qt::AlignHCenter);

    auto* title = new QLabel(&dialog);
    title->setObjectName(QStringLiteral("gameOverTitle"));
    title->setAlignment(Qt::AlignCenter);
    title->setText(
        versusAi_
            ? (humanWon ? QStringLiteral("棋高一着，妙手功成")
                        : QStringLiteral("惜败半筹，再战可期"))
            : QStringLiteral("%1获胜").arg(playerName(winningPlayer))
    );

    auto* subtitle = new QLabel(&dialog);
    subtitle->setObjectName(QStringLiteral("gameOverSubtitle"));
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setText(QStringLiteral("%1 · 共 %2 手")
                          .arg(playerName(winningPlayer))
                          .arg(game_.moveCount()));

    auto* detail = new QLabel(&dialog);
    detail->setObjectName(QStringLiteral("gameOverDetail"));
    detail->setAlignment(Qt::AlignCenter);
    detail->setText(QStringLiteral("对手已无合法着法。可悔棋复盘，或另开新局。"));

    auto* divider = new QFrame(&dialog);
    divider->setObjectName(QStringLiteral("gameOverDivider"));
    divider->setFrameShape(QFrame::HLine);

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(detail);
    layout->addSpacing(6);
    layout->addWidget(divider);
    layout->addStretch();

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(10);
    auto* againButton = new QPushButton(QStringLiteral("再来一局"), &dialog);
    againButton->setObjectName(QStringLiteral("primaryButton"));
    auto* undoButton = new QPushButton(QStringLiteral("悔棋复盘"), &dialog);
    auto* closeButton = new QPushButton(QStringLiteral("关 闭"), &dialog);
    buttons->addWidget(againButton, 2);
    buttons->addWidget(undoButton, 1);
    buttons->addWidget(closeButton, 1);
    layout->addLayout(buttons);

    const int kUndoResult = QDialog::Accepted + 1;
    connect(againButton, &QPushButton::clicked, &dialog,
            [&dialog] { dialog.done(QDialog::Accepted); });
    connect(undoButton, &QPushButton::clicked, &dialog,
            [&dialog] { dialog.done(kUndoResult); });
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    // 入场:卡片淡入,朱印自上而落
    auto* sealEffect = new QGraphicsOpacityEffect(seal);
    sealEffect->setOpacity(0.0);
    seal->setGraphicsEffect(sealEffect);
    auto* sealFade = new QPropertyAnimation(sealEffect, "opacity");
    sealFade->setDuration(300);
    sealFade->setStartValue(0.0);
    sealFade->setEndValue(1.0);
    sealFade->setEasingCurve(QEasingCurve::OutCubic);
    sealFade->start(QAbstractAnimation::DeleteWhenStopped);

    dialog.setWindowOpacity(0.0);
    dialog.show();
    auto* cardFade = new QPropertyAnimation(&dialog, "windowOpacity");
    cardFade->setDuration(240);
    cardFade->setStartValue(0.0);
    cardFade->setEndValue(1.0);
    cardFade->setEasingCurve(QEasingCurve::OutCubic);
    cardFade->start(QAbstractAnimation::DeleteWhenStopped);

    const QPoint sealHome = seal->pos();
    seal->move(sealHome - QPoint(0, 30));
    auto* stampDrop = new QPropertyAnimation(seal, "pos");
    stampDrop->setDuration(300);
    stampDrop->setStartValue(seal->pos());
    stampDrop->setEndValue(sealHome);
    stampDrop->setEasingCurve(QEasingCurve::InQuad);
    stampDrop->start(QAbstractAnimation::DeleteWhenStopped);

    dialog.exec();

    if (dialog.result() == QDialog::Accepted) {
        startNewGame();
    } else if (dialog.result() == kUndoResult) {
        undoMove();
    }
}

bool MainWindow::isHumanTurn() const {
    return !versusAi_ || game_.currentPlayer() == humanColor_;
}

int MainWindow::selectedThinkTime() const {
    return difficultyCombo_->currentData().toInt();
}

}  // namespace zzmazon
