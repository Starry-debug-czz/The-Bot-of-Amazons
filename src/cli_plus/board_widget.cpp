#include "board_widget.h"

#include <QBrush>
#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QProcess>
#include <QRadialGradient>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace zzmazon {
namespace {

constexpr qreal kOuterPadding = 42.0;
constexpr qreal kCellGap = 5.0;
constexpr qreal kTileInset = 3.5;   // 分块瓷砖与格位的内缩
constexpr qreal kPi = 3.14159265358979323846;
constexpr quint32 kPaperSeed = 20260827u;

qreal clamp01(qreal value) {
    return std::clamp(value, 0.0, 1.0);
}

qreal smoothStep(qreal value) {
    value = clamp01(value);
    return value * value * (3.0 - 2.0 * value);
}

QPointF lerpPoint(QPointF from, QPointF to, qreal t) {
    return from + (to - from) * t;
}

bool contains(const std::vector<Coord>& coords, Coord wanted) {
    return std::find(coords.begin(), coords.end(), wanted) != coords.end();
}

int signOf(int value) {
    return (value > 0) - (value < 0);
}

// ── 水墨调色板 ──────────────────────────────────────────────
const QColor kPaperLight{247, 240, 222};
const QColor kPaper{241, 232, 211};
const QColor kPaperDeep{230, 218, 193};
const QColor kInk{64, 56, 44};
const QColor kInkSoft{106, 92, 68};
const QColor kGridInk{92, 80, 60};
const QColor kCinnabar{166, 54, 34};
const QColor kCinnabarDeep{122, 38, 20};
const QColor kAzure{62, 96, 90};
const QColor kAzureDeep{43, 70, 66};
const QColor kOchre{150, 116, 50};
const QColor kPaperChar{243, 231, 207};

// 缓动曲线:落子节奏感的骨架
qreal span(qreal p, qreal start, qreal end) {
    return clamp01((p - start) / (end - start));
}

qreal easeInOutCubic(qreal x) {
    x = clamp01(x);
    return x < 0.5 ? 4*x*x*x : 1 - std::pow(-2*x + 2, 3) / 2;
}

qreal easeOutCubic(qreal x) {
    x = clamp01(x);
    return 1 - std::pow(1 - x, 3);
}

qreal easeOutBack(qreal t) {
    t = clamp01(t);
    const qreal c1 = 1.45;
    const qreal c3 = c1 + 1;
    return 1 + c3*std::pow(t - 1, 3) + c1*std::pow(t - 1, 2);
}

qreal easeInQuad(qreal x) {
    x = clamp01(x);
    return x * x;
}

// 确定性噪声:同一格子的纹样每帧一致
quint32 hash32(quint32 s) {
    s ^= s >> 16;
    s *= 0x7feb352du;
    s ^= s >> 15;
    s *= 0x846ca68bu;
    s ^= s >> 16;
    return s;
}

quint32 coordSeed(int row, int col, quint32 salt) {
    return hash32(quint32(row * 1315423911u) ^ quint32(col * 2654435761u) ^ salt);
}

qreal noise01(quint32 seedValue) {
    return qreal(hash32(seedValue)) / 4294967295.0;
}

QColor inkFontColor(qreal alpha = 255.0) {
    return QColor(int(kInk.red()), int(kInk.green()), int(kInk.blue()), int(alpha));
}

QFont cjkFont(qreal pointSize, QFont::Weight weight = QFont::Normal) {
    QFont font;
    font.setFamilies({
        QStringLiteral("Kaiti SC"),
        QStringLiteral("STKaiti"),
        QStringLiteral("KaiTi"),
        QStringLiteral("Kaiti TC"),
        QStringLiteral("Songti SC"),
        QStringLiteral("Noto Serif CJK SC"),
        QStringLiteral("PingFang SC"),
    });
    font.setPointSizeF(std::max(6.5, pointSize));
    font.setWeight(weight);
    return font;
}

QString phaseText(BoardWidget::SelectionPhase phase) {
    switch (phase) {
        case BoardWidget::SelectionPhase::Piece:
            return QStringLiteral("选择一枚棋子");
        case BoardWidget::SelectionPhase::Destination:
            // 该文案被 tests/cli_plus_board_test.cpp 断言,不可修改
            return QStringLiteral("选择移动终点");
        case BoardWidget::SelectionPhase::MovingQueen:
            return QStringLiteral("棋子移动中…");
        case BoardWidget::SelectionPhase::ArrowTarget:
            return QStringLiteral("选择箭的落点");
    }
    return {};
}

// 音效合成与播放(实现见文件后部的匿名命名空间)
QByteArray buildWav(const std::vector<float>& samples);
std::vector<float> synthPlace();
std::vector<float> synthWhoosh();
std::vector<float> synthThud();
void playSfx(const QString& key, const std::vector<float>& samples);

}  // namespace

BoardWidget::BoardWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    animationTimer_.setInterval(16);
    animationTimer_.setTimerType(Qt::PreciseTimer);
    connect(
        &animationTimer_,
        &QTimer::timeout,
        this,
        &BoardWidget::updateAnimation
    );

    shimmerClock_.start();
    hoverClock_.start();
    idleTimer_.setInterval(33);
    idleTimer_.setTimerType(Qt::CoarseTimer);
    connect(&idleTimer_, &QTimer::timeout, this, [this] { update(); });
}

void BoardWidget::setGame(const AmazonsGame* game) {
    game_ = game;
    resetSelection();
    update();
}

void BoardWidget::setInputEnabled(bool enabled) {
    inputEnabled_ = enabled && !animation_.active;
    if (!inputEnabled_) {
        hovered_ = {};
        unsetCursor();
    }
    syncIdleMotion();
    update();
}

void BoardWidget::resetSelection() {
    const bool cancelledPreview =
        animation_.active && animation_.kind == AnimationKind::QueenPreview;
    if (cancelledPreview) {
        animation_.active = false;
        animationTimer_.stop();
        inputEnabled_ = game_ != nullptr;
        emit selectionTransitionChanged(false);
    }
    phase_ = SelectionPhase::Piece;
    selected_ = {};
    destination_ = {};
    hovered_ = {};
    validTargets_.clear();
    hintMove_.reset();
    emit interactionHintChanged(phaseText(phase_));
    syncIdleMotion();
    update();
}

void BoardWidget::animateMove(const Move& move) {
    if (!game_) {
        return;
    }

    resetSelection();
    inputEnabled_ = false;
    animation_.active = true;
    animation_.kind = AnimationKind::FullMove;
    animation_.move = move;
    animation_.player = game_->at(move.to);
    animation_.durationMs = 1150;
    animationClock_.restart();
    animationTimer_.start();
    syncIdleMotion();
    update();
}

void BoardWidget::animateArrow(const Move& move) {
    if (!game_) {
        return;
    }

    resetSelection();
    inputEnabled_ = false;
    animation_.active = true;
    animation_.kind = AnimationKind::ArrowOnly;
    animation_.move = move;
    animation_.player = game_->at(move.to);
    // 需在 tests 的 560ms 等待内完成
    animation_.durationMs = 520;
    animationClock_.restart();
    animationTimer_.start();
    syncIdleMotion();
    update();
}

void BoardWidget::showHint(const Move& move) {
    hintMove_ = move;
    syncIdleMotion();
    update();
}

void BoardWidget::clearHint() {
    hintMove_.reset();
    syncIdleMotion();
    update();
}

QSize BoardWidget::sizeHint() const {
    return {760, 760};
}

QSize BoardWidget::minimumSizeHint() const {
    return {520, 520};
}

QRectF BoardWidget::boardRect() const {
    const qreal usableWidth = std::max(1.0, width() - kOuterPadding * 2.0);
    const qreal usableHeight = std::max(1.0, height() - kOuterPadding * 2.0);
    const qreal size = std::min(usableWidth, usableHeight);
    return {
        (width() - size) / 2.0,
        (height() - size) / 2.0,
        size,
        size,
    };
}

QRectF BoardWidget::cellRect(Coord coord) const {
    const QRectF board = boardRect();
    const qreal cell = board.width() / kBoardSize;
    return {
        board.left() + coord.col * cell + kCellGap / 2.0,
        board.top() + coord.row * cell + kCellGap / 2.0,
        cell - kCellGap,
        cell - kCellGap,
    };
}

QPointF BoardWidget::cellCenter(Coord coord) const {
    return cellRect(coord).center();
}

QRectF BoardWidget::tileRect(Coord coord) const {
    return cellRect(coord).adjusted(
        kTileInset, kTileInset, -kTileInset, -kTileInset
    );
}

qreal BoardWidget::hoverLift(Coord coord) const {
    if (!inputEnabled_ || !coord.valid() || !(coord == hovered_) ||
        !isInteractive(coord)) {
        return 0.0;
    }
    return 3.0 * easeOutCubic(span(qreal(hoverClock_.elapsed()), 0.0, 150.0));
}

Coord BoardWidget::coordAt(QPointF position) const {
    const QRectF board = boardRect();
    if (!board.contains(position)) {
        return {};
    }
    const qreal cell = board.width() / kBoardSize;
    return {
        std::clamp(static_cast<int>((position.y() - board.top()) / cell), 0, 7),
        std::clamp(static_cast<int>((position.x() - board.left()) / cell), 0, 7),
    };
}

bool BoardWidget::isValidTarget(Coord coord) const {
    return coord.valid() && contains(validTargets_, coord);
}

bool BoardWidget::isInteractive(Coord coord) const {
    if (!inputEnabled_ || !game_ || !coord.valid()) {
        return false;
    }
    if (game_->at(coord) == game_->currentPlayer()) {
        return true;
    }
    return phase_ != SelectionPhase::Piece && isValidTarget(coord);
}

std::vector<Coord> BoardWidget::highlightedPath() const {
    Coord from;
    Coord to;
    if (phase_ == SelectionPhase::Destination && isValidTarget(hovered_)) {
        from = selected_;
        to = hovered_;
    } else if (phase_ == SelectionPhase::ArrowTarget &&
               isValidTarget(hovered_)) {
        from = destination_;
        to = hovered_;
    } else {
        return {};
    }

    std::vector<Coord> result;
    const int rowStep = signOf(to.row - from.row);
    const int colStep = signOf(to.col - from.col);
    Coord current{from.row + rowStep, from.col + colStep};
    while (current.valid()) {
        result.push_back(current);
        if (current == to) {
            break;
        }
        current.row += rowStep;
        current.col += colStep;
    }
    return result;
}

void BoardWidget::selectPiece(Coord coord) {
    if (!game_ || game_->at(coord) != game_->currentPlayer()) {
        return;
    }
    selected_ = coord;
    destination_ = {};
    phase_ = SelectionPhase::Destination;
    validTargets_ = game_->legalDestinations(selected_);
    hintMove_.reset();
    emit interactionHintChanged(phaseText(phase_));
    syncIdleMotion();
    update();
}

void BoardWidget::chooseDestination(Coord coord) {
    if (!game_) {
        return;
    }
    if (game_->at(coord) == game_->currentPlayer()) {
        selectPiece(coord);
        return;
    }
    if (!isValidTarget(coord)) {
        return;
    }

    destination_ = coord;
    phase_ = SelectionPhase::MovingQueen;
    validTargets_.clear();
    hovered_ = {};
    inputEnabled_ = false;
    animation_.active = true;
    animation_.kind = AnimationKind::QueenPreview;
    animation_.move = {selected_, destination_, {}};
    animation_.player = game_->currentPlayer();
    // 需在 tests 的 460ms 等待内完成并切换到落箭阶段
    animation_.durationMs = 380;
    animationClock_.restart();
    animationTimer_.start();
    emit selectionTransitionChanged(true);
    emit interactionHintChanged(phaseText(phase_));
    syncIdleMotion();
    update();
}

void BoardWidget::chooseArrow(Coord coord) {
    if (!isValidTarget(coord)) {
        return;
    }
    emit moveChosen({selected_, destination_, coord});
}

void BoardWidget::mouseMoveEvent(QMouseEvent* event) {
    const Coord nextHover = coordAt(event->position());
    if (!(nextHover == hovered_)) {
        hovered_ = nextHover;
        hoverClock_.restart();
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void BoardWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    if (event->button() == Qt::RightButton) {
        resetSelection();
        return;
    }
    if (event->button() != Qt::LeftButton || !inputEnabled_ ||
        animation_.active || !game_) {
        return;
    }

    const Coord clicked = coordAt(event->position());
    if (!clicked.valid()) {
        resetSelection();
        return;
    }

    switch (phase_) {
        case SelectionPhase::Piece:
            selectPiece(clicked);
            break;
        case SelectionPhase::Destination:
            if (clicked == selected_) {
                resetSelection();
            } else {
                chooseDestination(clicked);
            }
            break;
        case SelectionPhase::MovingQueen:
            break;
        case SelectionPhase::ArrowTarget:
            chooseArrow(clicked);
            break;
    }
}

void BoardWidget::leaveEvent(QEvent* event) {
    hovered_ = {};
    hoverClock_.restart();
    unsetCursor();
    update();
    QWidget::leaveEvent(event);
}

void BoardWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        resetSelection();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void BoardWidget::updateAnimation() {
    if (!animation_.active) {
        animationTimer_.stop();
        return;
    }

    // 音效在动作的节拍点上触发,每枚一次
    const qreal progress = clamp01(
        qreal(animationClock_.elapsed()) / qreal(animation_.durationMs)
    );
    if (animation_.kind == AnimationKind::QueenPreview) {
        if (!animation_.soundPlace && progress >= 0.78) {
            animation_.soundPlace = true;
            playSfx(QStringLiteral("place"), synthPlace());
        }
    } else if (animation_.kind == AnimationKind::ArrowOnly) {
        if (!animation_.soundArrow && progress >= 0.14) {
            animation_.soundArrow = true;
            playSfx(QStringLiteral("whoosh"), synthWhoosh());
        }
        if (!animation_.soundImpact && progress >= 0.80) {
            animation_.soundImpact = true;
            playSfx(QStringLiteral("thud"), synthThud());
        }
    } else {
        if (!animation_.soundPlace && progress >= 0.56) {
            animation_.soundPlace = true;
            playSfx(QStringLiteral("place"), synthPlace());
        }
        if (!animation_.soundArrow && progress >= 0.72) {
            animation_.soundArrow = true;
            playSfx(QStringLiteral("whoosh"), synthWhoosh());
        }
        if (!animation_.soundImpact && progress >= 0.94) {
            animation_.soundImpact = true;
            playSfx(QStringLiteral("thud"), synthThud());
        }
    }

    if (animationClock_.elapsed() >= animation_.durationMs) {
        const AnimationKind completedKind = animation_.kind;
        animation_.active = false;
        animationTimer_.stop();
        if (completedKind == AnimationKind::QueenPreview) {
            phase_ = SelectionPhase::ArrowTarget;
            validTargets_ = game_
                ? game_->legalArrows(selected_, destination_)
                : std::vector<Coord>{};
            inputEnabled_ = game_ != nullptr;
            emit selectionTransitionChanged(false);
            emit interactionHintChanged(phaseText(phase_));
            syncIdleMotion();
            update();
            return;
        }
        syncIdleMotion();
        update();
        emit animationFinished();
        return;
    }
    update();
}

void BoardWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    drawBackdrop(painter);
    drawBoard(painter);
    drawHighlights(painter);
    drawStaticContents(painter);
    drawSelectionPreview(painter);
    drawHint(painter);
    drawAnimation(painter);
    drawCoordinates(painter);
}

void BoardWidget::drawBackdrop(QPainter& painter) const {
    ensurePaperTexture();
    painter.drawPixmap(0, 0, paperTexture_);

    const QRectF board = boardRect();
    const QRectF card = board.adjusted(-18.0, -18.0, 18.0, 18.0);

    for (int layer = 14; layer > 0; --layer) {
        QColor shadow(88, 72, 46, 6);
        painter.setPen(Qt::NoPen);
        painter.setBrush(shadow);
        painter.drawRoundedRect(
            card.translated(0.0, layer * 0.7).adjusted(
                -layer * 0.35,
                -layer * 0.1,
                layer * 0.35,
                layer * 0.4
            ),
            26.0,
            26.0
        );
    }

    QLinearGradient mount(card.topLeft(), card.bottomRight());
    mount.setColorAt(0.0, QColor(249, 243, 228));
    mount.setColorAt(1.0, QColor(238, 227, 202));
    painter.setPen(QPen(QColor(120, 102, 74, 150), 1.2));
    painter.setBrush(mount);
    painter.drawRoundedRect(card, 22.0, 22.0);

    painter.setPen(QPen(QColor(140, 122, 90, 85), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(card.adjusted(5.0, 5.0, -5.0, -5.0), 17.0, 17.0);

    painter.setBrush(QColor(150, 118, 70, 175));
    painter.setPen(QPen(QColor(110, 86, 50, 165), 1.0));
    const QPointF corners[4] = {
        card.topLeft() + QPointF(12.5, 12.5),
        card.topRight() + QPointF(-12.5, 12.5),
        card.bottomLeft() + QPointF(12.5, -12.5),
        card.bottomRight() + QPointF(-12.5, -12.5),
    };
    for (const QPointF& corner : corners) {
        painter.save();
        painter.translate(corner);
        painter.rotate(45.0);
        painter.drawRect(QRectF(-3.1, -3.1, 6.2, 6.2));
        painter.restore();
    }

    painter.save();
    painter.translate(card.right() - 32.0, card.bottom() + 3.0);
    painter.rotate(-4.0);
    const QRectF sealRect(-12.5, -12.5, 25.0, 25.0);
    QLinearGradient sealGrad(sealRect.topLeft(), sealRect.bottomLeft());
    sealGrad.setColorAt(0.0, QColor(179, 64, 39));
    sealGrad.setColorAt(1.0, kCinnabarDeep);
    painter.setPen(QPen(QColor(96, 28, 14, 225), 1.4));
    painter.setBrush(sealGrad);
    painter.drawRoundedRect(sealRect, 4.0, 4.0);
    painter.setPen(QPen(QColor(243, 231, 207, 55), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(sealRect.adjusted(2.6, 2.6, -2.6, -2.6), 2.4, 2.4);
    painter.setFont(cjkFont(13.5, QFont::Bold));
    painter.setPen(kPaperChar);
    painter.drawText(sealRect, Qt::AlignCenter, QStringLiteral("弈"));
    painter.restore();
}

void BoardWidget::drawBoard(QPainter& painter) const {
    const QRectF board = boardRect();
    const qreal pitch = board.width() / kBoardSize;

    painter.save();
    painter.setClipRect(board);

    // 分块瓷砖在墨晕与帘纹之后统一绘制(见函数末尾)

    for (int blotch = 0; blotch < 9; ++blotch) {
        const quint32 seedValue = coordSeed(blotch, blotch * 3 + 1, 911u);
        const QPointF center(
            board.left() + noise01(seedValue) * board.width(),
            board.top() + noise01(seedValue ^ 0xA5A5u) * board.height()
        );
        const qreal radius = pitch * (1.1 + noise01(seedValue >> 8) * 1.3);
        QRadialGradient wash(center, radius);
        wash.setColorAt(0.0, QColor(88, 76, 54, 0));
        wash.setColorAt(0.62, QColor(88, 76, 54, 8));
        wash.setColorAt(0.85, QColor(88, 76, 54, 13));
        wash.setColorAt(1.0, QColor(88, 76, 54, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(wash);
        painter.drawEllipse(center, radius, radius);
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
    QPen laidPen(QColor(255, 252, 240, 20), 1.0);
    painter.setPen(laidPen);
    for (qreal y = board.top(); y < board.bottom(); y += 6.0) {
        painter.drawLine(QPointF(board.left(), y), QPointF(board.right(), y));
    }
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.restore();

    // 分块瓷砖:每格独立悬浮,缝隙透出下方墨晕宣纸
    auto tinted = [](QColor base, qreal t) {
        return QColor(
            int(base.red() + (150 - base.red()) * t),
            int(base.green() + (132 - base.green()) * t),
            int(base.blue() + (94 - base.blue()) * t)
        );
    };

    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const Coord coord{row, col};
            QRectF tile = tileRect(coord);
            const qreal lift = hoverLift(coord);
            tile.translate(0.0, -lift);
            const qreal cornerRadius = tile.width() * 0.10;

            for (int layer = 3; layer >= 1; --layer) {
                QColor shadow(70, 56, 34, 9 + layer * 4 + int(lift * 3.0));
                painter.setPen(Qt::NoPen);
                painter.setBrush(shadow);
                painter.drawRoundedRect(
                    tile.translated(0.0, 2.0 + lift * 0.5 + layer * 0.7)
                        .adjusted(-layer * 0.7, -layer * 0.5,
                                  layer * 0.7, layer * 0.9),
                    cornerRadius, cornerRadius
                );
            }

            const qreal jitter = noise01(coordSeed(row, col, 77u));
            QLinearGradient surface(tile.topLeft(), tile.bottomRight());
            surface.setColorAt(0.0, tinted(QColor(252, 247, 235), jitter * 0.7));
            surface.setColorAt(1.0, tinted(kPaperDeep, 0.35 + jitter * 0.4));
            painter.setPen(QPen(QColor(120, 104, 76, 80), 1.0));
            painter.setBrush(surface);
            painter.drawRoundedRect(tile, cornerRadius, cornerRadius);
        }
    }
}

void BoardWidget::drawCoordinates(QPainter& painter) const {
    const QRectF board = boardRect();
    const qreal cell = board.width() / kBoardSize;
    painter.setFont(cjkFont(std::max(9.0, cell * 0.15)));
    painter.setPen(QColor(112, 96, 70, 220));

    for (int index = 0; index < kBoardSize; ++index) {
        const QRectF topLabel(
            board.left() + index * cell,
            board.top() - 29.0,
            cell,
            22.0
        );
        painter.drawText(topLabel, Qt::AlignCenter, QString::number(index));

        const QRectF leftLabel(
            board.left() - 31.0,
            board.top() + index * cell,
            22.0,
            cell
        );
        painter.drawText(leftLabel, Qt::AlignCenter, QString::number(index));
    }
}

namespace {

// 毛笔笔触:主锋 + 侧锋飞白,数层叠加模拟枯润
void drawBrushStroke(
    QPainter& painter,
    const QPointF& from,
    const QPointF& to,
    const QColor& baseColor,
    qreal width,
    int alpha
) {
    const qreal dx = to.x() - from.x();
    const qreal dy = to.y() - from.y();
    const qreal length = std::hypot(dx, dy);
    if (length < 1e-6) {
        return;
    }
    const QPointF perp(-dy / length, dx / length);
    struct StrokeLayer {
        qreal widthFactor;
        qreal offsetFactor;
        int layerAlpha;
    };
    const StrokeLayer layers[4] = {
        {1.00,  0.00, int(alpha * 0.30)},
        {0.45,  0.20, int(alpha * 0.55)},
        {0.16,  0.06, alpha},
        {0.52, -0.24, int(alpha * 0.28)},
    };
    for (const StrokeLayer& layer : layers) {
        QColor color = baseColor;
        color.setAlpha(std::min(255, layer.layerAlpha));
        painter.setPen(QPen(
            color,
            std::max(1.1, width * layer.widthFactor),
            Qt::SolidLine,
            Qt::RoundCap
        ));
        const QPointF shift = perp * (width * layer.offsetFactor);
        painter.drawLine(from + shift, to + shift);
    }
}

// 涟漪墨圈
void drawRipple(
    QPainter& painter,
    const QPointF& center,
    qreal radius,
    int alpha,
    const QColor& color
) {
    if (radius <= 0.5 || alpha <= 2) {
        return;
    }
    const qreal weight = std::clamp(radius * 0.09, 1.1, 3.4);
    QColor outer = color;
    outer.setAlpha(std::min(255, alpha));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(outer, weight));
    painter.drawEllipse(center, radius, radius);
    QColor inner = color;
    inner.setAlpha(int(alpha * 0.5));
    painter.setPen(QPen(inner, std::max(0.9, weight * 0.6)));
    painter.drawEllipse(center, radius * 0.66, radius * 0.66);
}

// 待选落点的"墨点"呼吸值:-1..1,周期约 2.6s,逐点错相
qreal breatheAt(qint64 tMs, quint32 seedValue) {
    const qreal phase = noise01(seedValue) * 2 * kPi;
    return qreal(std::sin(tMs / 2600.0 * 2 * kPi + phase));
}

// ── 音效:纯代码合成 WAV,交由系统播放器异步播放 ──────────────
constexpr int kSfxSampleRate = 22050;

QByteArray buildWav(const std::vector<float>& samples) {
    QByteArray data;
    auto appendLe16 = [&data](quint16 value) {
        data.append(char(value & 0xFF));
        data.append(char(value >> 8));
    };
    auto appendLe32 = [&](quint32 value) {
        appendLe16(quint16(value & 0xFFFF));
        appendLe16(quint16(value >> 16));
    };
    data.append("RIFF");
    appendLe32(quint32(36 + samples.size() * 2));
    data.append("WAVEfmt ");
    appendLe32(16);                   // fmt 块长
    appendLe16(1);                    // PCM
    appendLe16(1);                    // 单声道
    appendLe32(kSfxSampleRate);
    appendLe32(kSfxSampleRate * 2);   // 字节率
    appendLe16(2);                    // 块对齐
    appendLe16(16);                   // 位深
    data.append("data");
    appendLe32(quint32(samples.size() * 2));
    for (float sample : samples) {
        const auto clamped = std::clamp(sample, -1.0f, 1.0f);
        appendLe16(quint16(qint16(clamped * 32767.0f)));
    }
    return data;
}

// 落子:木石相叩的短促"嗒"声
std::vector<float> synthPlace() {
    std::vector<float> out;
    const int total = int(0.09 * kSfxSampleRate);
    out.reserve(total);
    qreal phase = 0.0;
    for (int i = 0; i < total; ++i) {
        const qreal t = qreal(i) / kSfxSampleRate;
        const qreal freq = 320.0 + 620.0 * std::exp(-t / 0.016);
        phase += 2 * kPi * freq / kSfxSampleRate;
        const qreal envelope = std::exp(-t / 0.020);
        qreal value = std::sin(phase) * envelope * 0.62;
        if (i < 40) {  // 起笔的噪声脆头
            value += (noise01(quint32(i) * 2654435761u) - 0.5)
                * 0.5 * (1.0 - qreal(i) / 40.0);
        }
        out.push_back(float(value));
    }
    return out;
}

// 箭坠:低沉的闷响
std::vector<float> synthThud() {
    std::vector<float> out;
    const int total = int(0.16 * kSfxSampleRate);
    out.reserve(total);
    qreal phase = 0.0;
    for (int i = 0; i < total; ++i) {
        const qreal t = qreal(i) / kSfxSampleRate;
        const qreal freq = 82.0 + 150.0 * std::exp(-t / 0.028);
        phase += 2 * kPi * freq / kSfxSampleRate;
        const qreal envelope = std::exp(-t / 0.055);
        qreal value = std::sin(phase) * envelope * 0.60;
        if (i < 60) {
            value += (noise01(quint32(i) * 40503u) - 0.5)
                * 0.30 * (1.0 - qreal(i) / 60.0);
        }
        out.push_back(float(value));
    }
    return out;
}

// 破空:噪声扫频低通,如箭啸掠纸面
std::vector<float> synthWhoosh() {
    std::vector<float> out;
    const int total = int(0.22 * kSfxSampleRate);
    out.reserve(total);
    qreal lowpassed = 0.0;
    for (int i = 0; i < total; ++i) {
        const qreal t = qreal(i) / kSfxSampleRate;
        const qreal raw =
            noise01(quint32(i) * 747796405u + 2891336453u) - 0.5;
        const qreal bell = std::sin(t / 0.22 * kPi);
        const qreal openness = 0.05 + 0.45 * bell * bell;
        lowpassed += openness * (raw - lowpassed);
        out.push_back(float(lowpassed * std::pow(bell, 1.4) * 1.5));
    }
    return out;
}

void playSfx(const QString& key, const std::vector<float>& samples) {
    // 测试与截图等离屏环境保持安静
    if (qGuiApp == nullptr ||
        QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        return;
    }
    static const QString player = [] {
        for (const QString& candidate : {QStringLiteral("afplay"),
                                         QStringLiteral("paplay"),
                                         QStringLiteral("aplay")}) {
            const QString found = QStandardPaths::findExecutable(candidate);
            if (!found.isEmpty()) {
                return found;
            }
        }
        return QString();
    }();
    if (player.isEmpty()) {
        return;
    }
    static QHash<QString, QString> sfxFiles;
    QString& path = sfxFiles[key];
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            + QStringLiteral("/zzmazon_%1.wav").arg(key);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(buildWav(samples)) < 0) {
            path.clear();
            return;
        }
        file.close();  // 先落盘再交予播放器
    }
    QProcess::startDetached(player, {path});
}

}  // namespace

void BoardWidget::drawHighlights(QPainter& painter) const {
    if (!game_ || animation_.active) {
        return;
    }

    const qint64 tMs = shimmerClock_.elapsed();
    const QColor& accent = brushTargetColor();

    auto drawTileWash = [&](Coord cellCoord, int alpha, int rimAlpha,
                            qreal rimWidth) {
        QRectF wash = tileRect(cellCoord);
        wash.translate(0.0, -hoverLift(cellCoord));
        QLinearGradient washGrad(wash.topLeft(), wash.bottomLeft());
        washGrad.setColorAt(0.0, QColor(accent.red(), accent.green(),
                                        accent.blue(), std::min(255, alpha + 18)));
        washGrad.setColorAt(1.0, QColor(accent.red(), accent.green(),
                                        accent.blue(), std::max(20, alpha - 2)));
        painter.setPen(QPen(QColor(accent.red(), accent.green(),
                                   accent.blue(), rimAlpha), rimWidth));
        painter.setBrush(washGrad);
        painter.drawRoundedRect(wash, wash.width() * 0.10, wash.width() * 0.10);
    };

    if (hovered_.valid() && inputEnabled_ && isInteractive(hovered_)) {
        const bool hoverTarget = isValidTarget(hovered_);
        drawTileWash(hovered_, hoverTarget ? 118 : 44,
                     hoverTarget ? 245 : 120, hoverTarget ? 2.6 : 1.5);
        QRectF glow = tileRect(hovered_).adjusted(-4.5, -4.5, 4.5, 4.5);
        glow.translate(0.0, -hoverLift(hovered_));
        painter.setPen(QPen(QColor(accent.red(), accent.green(),
                                   accent.blue(), hoverTarget ? 110 : 55), 4.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(glow, glow.width() * 0.115, glow.width() * 0.115);
    }

    // 点击之前不画任何路径连线,悬停反馈只作用于整块瓷砖


    for (const Coord target : validTargets_) {
        if (target == hovered_) {
            continue;
        }
        const qreal breath = breatheAt(tMs, coordSeed(target.row, target.col, 5u));
        drawTileWash(target, int(60.0 + 22.0 * breath), 175, 1.8);
    }

    const Coord focusCoord =
        (phase_ == SelectionPhase::MovingQueen ||
         phase_ == SelectionPhase::ArrowTarget)
            ? destination_
            : selected_;
    if (focusCoord.valid()) {
        const QPointF focusCenter = cellCenter(focusCoord);
        const qreal ringRadius = cellRect(focusCoord).width() * 0.47;
        const qreal rotationDeg = tMs * 0.010;

        QRadialGradient glow(focusCenter, ringRadius * 1.6);
        glow.setColorAt(0.0, QColor(kCinnabar.red(), kCinnabar.green(),
                                    kCinnabar.blue(), 30));
        glow.setColorAt(0.7, QColor(kCinnabar.red(), kCinnabar.green(),
                                    kCinnabar.blue(), 12));
        glow.setColorAt(1.0, QColor(kCinnabar.red(), kCinnabar.green(),
                                    kCinnabar.blue(), 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(focusCenter, ringRadius * 1.6, ringRadius * 1.6);

        painter.save();
        painter.translate(focusCenter);
        painter.rotate(rotationDeg);
        QPen orbitPen(QColor(kCinnabar.red(), kCinnabar.green(),
                             kCinnabar.blue(), 195), 2.2);
        orbitPen.setStyle(Qt::DashLine);
        orbitPen.setDashPattern({9.0, 7.0});
        orbitPen.setCapStyle(Qt::RoundCap);
        painter.setPen(orbitPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(0, 0), ringRadius, ringRadius);
        painter.restore();

        painter.save();
        painter.translate(focusCenter);
        painter.rotate(-rotationDeg * 1.35);
        painter.setPen(QPen(QColor(kCinnabar.red(), kCinnabar.green(),
                                   kCinnabar.blue(), 105), 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(0, 0), ringRadius * 1.11, ringRadius * 1.11);
        painter.restore();
    }
}

void BoardWidget::drawStaticContents(QPainter& painter) const {
    if (!game_) {
        return;
    }

    const qreal pieceSize = cellRect({0, 0}).width() * 0.72;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const Coord coord{row, col};
            const int value = game_->at(coord);
            bool skip = false;

            if (animation_.active) {
                if (animation_.kind == AnimationKind::FullMove &&
                    (coord == animation_.move.to ||
                     coord == animation_.move.arrow)) {
                    skip = true;
                } else if (animation_.kind == AnimationKind::ArrowOnly &&
                           coord == animation_.move.arrow) {
                    skip = true;
                } else if (animation_.kind == AnimationKind::QueenPreview &&
                           coord == selected_) {
                    skip = true;
                }
            }
            if (!animation_.active && selected_.valid() && coord == selected_ &&
                phase_ != SelectionPhase::Piece) {
                skip = true;
            }
            if (skip) {
                continue;
            }

            if (value == Black || value == White) {
                const bool hoveredPiece = inputEnabled_ && coord == hovered_ &&
                                          value == game_->currentPlayer();
                drawPiece(
                    painter,
                    cellCenter(coord),
                    pieceSize,
                    value,
                    hoveredPiece ? 1.055 : 1.0,
                    hoveredPiece ? 3.0 : 0.0
                );
            } else if (value == Arrow) {
                drawArrowStone(painter, cellCenter(coord), pieceSize * 0.82);
            }
        }
    }
}

void BoardWidget::drawSelectionPreview(QPainter& painter) const {
    if (!game_ || animation_.active || !selected_.valid() ||
        phase_ == SelectionPhase::Piece) {
        return;
    }

    const qint64 tMs = shimmerClock_.elapsed();
    const qreal pulse = 1.0 + 0.02 * std::sin(tMs / 2200.0 * 2 * kPi);

    if (phase_ == SelectionPhase::ArrowTarget) {
        // 棋子已随确认动画落于新位,静候射箭指令
        drawPiece(painter, cellCenter(destination_),
                  cellRect(destination_).width() * 0.72,
                  game_->currentPlayer(), 1.0, 0.0, 1.0);
        return;
    }

    // 选择终点阶段:棋子留在原位轻轻浮起,点击后才真正移动
    const qreal floatBob = 1.6 * std::sin(tMs / 1900.0 * 2 * kPi);
    drawPiece(painter, cellCenter(selected_),
              cellRect(selected_).width() * 0.72,
              game_->currentPlayer(), 1.05 * pulse, 6.5 + floatBob, 0.97);
}

void BoardWidget::drawAnimation(QPainter& painter) const {
    if (!animation_.active || !game_) {
        return;
    }

    const qreal progress = clamp01(
        static_cast<qreal>(animationClock_.elapsed()) / animation_.durationMs
    );
    const qreal pieceSize = cellRect({0, 0}).width() * 0.72;
    const qreal pitch = boardRect().width() / kBoardSize;
    const QPointF from = cellCenter(animation_.move.from);
    const QPointF to = cellCenter(animation_.move.to);
    const QPointF arrowTarget = cellCenter(animation_.move.arrow);

    auto splashParticles =
        [&](const QPointF& origin,
            quint32 seedValue,
            qreal t,
            qreal maxDist,
            int alphaPeak,
            const QColor& color) {
            if (t <= 0.0 || t >= 1.0) {
                return;
            }
            const qreal eased = easeOutCubic(t);
            for (int index = 0; index < 7; ++index) {
                const qreal angle =
                    noise01(seedValue + quint32(index) * 97u) * 2 * kPi;
                const qreal speedFactor =
                    0.7 + noise01(seedValue + quint32(index) * 131u) * 0.55;
                const QPointF position = origin +
                    QPointF(std::cos(angle), std::sin(angle)) *
                        maxDist * speedFactor * eased;
                QColor colorDrop = color;
                colorDrop.setAlpha(
                    int(alphaPeak * std::pow(1.0 - t, 1.3))
                );
                painter.setPen(Qt::NoPen);
                painter.setBrush(colorDrop);
                painter.drawEllipse(position, 2.6 - 1.9 * eased,
                                    2.6 - 1.9 * eased);
            }
        };

    const quint32 destSeed = coordSeed(animation_.move.to.row,
                                       animation_.move.to.col, 21u);

    if (animation_.kind == AnimationKind::QueenPreview) {
        const qreal anticipate = span(progress, 0.0, 0.16);
        const qreal glideT = easeInOutCubic(span(progress, 0.16, 0.78));
        const qreal landU = span(progress, 0.78, 1.0);

        QPointF queenCenter = lerpPoint(from, to, glideT);
        qreal lift = 0.0;
        qreal scale = 1.0;
        if (progress < 0.16) {
            queenCenter = from;
            scale = 1.0 + 0.06 * std::sin(anticipate * kPi);
            lift = -2.0 * std::sin(anticipate * kPi);
        } else if (progress < 0.78) {
            const qreal arc = std::sin(glideT * kPi);
            lift = pieceSize * 0.28 * arc;
            scale = 1.0 + arc * 0.08;
            drawBrushStroke(painter, from, queenCenter, kAzure,
                            pieceSize * 0.11,
                            int(smoothStep(span(progress, 0.16, 0.26)) * 90));
        } else {
            queenCenter = to;
            scale = 0.93 + 0.07 * easeOutBack(landU);
            drawRipple(painter, to, pitch * (0.20 + landU * 0.24),
                       int((1.0 - landU) * 80), kAzureDeep);
        }

        drawPiece(painter, queenCenter, pieceSize, animation_.player,
                  scale, lift);
        return;
    }

    if (animation_.kind == AnimationKind::ArrowOnly) {
        const qreal windup = span(progress, 0.0, 0.14);
        const qreal flightT = easeInQuad(span(progress, 0.14, 0.80));
        const QColor& flame = brushTargetColor();

        if (progress < 0.14) {
            QRadialGradient gather(to, pitch * 0.34);
            gather.setColorAt(0.0, QColor(kCinnabar.red(), kCinnabar.green(),
                                          kCinnabar.blue(),
                                          int(windup * 46)));
            gather.setColorAt(1.0, QColor(kCinnabar.red(), kCinnabar.green(),
                                          kCinnabar.blue(), 0));
            painter.setPen(Qt::NoPen);
            painter.setBrush(gather);
            painter.drawEllipse(to, pitch * 0.34, pitch * 0.34);
            return;
        }

        if (progress < 0.80) {
            QPointF projectile = lerpPoint(to, arrowTarget, flightT);
            projectile.ry() -= std::sin(flightT * kPi) * pieceSize * 0.26;
            drawBrushStroke(painter, to, projectile, kCinnabar,
                            pieceSize * 0.08, int(40 + flightT * 70));
            drawArrowStone(painter, projectile,
                           pieceSize * (0.58 + flightT * 0.26), 1.0, 0.96);
            return;
        }

        const qreal impactU = span(progress, 0.80, 0.97);
        drawArrowStone(
            painter,
            arrowTarget,
            pieceSize * 0.82 *
                (1.0 + 0.22 * (1.0 - easeOutCubic(impactU)))
        );
        const qreal rippleU = span(progress, 0.82, 1.0);
        drawRipple(painter, arrowTarget, pitch * (0.20 + rippleU * 0.52),
                   int((1.0 - rippleU) * 100), kCinnabar);
        splashParticles(arrowTarget, destSeed,
                        span(progress, 0.80, 1.0), pitch * 0.55, 140,
                        kCinnabarDeep);
        return;
    }

    // 完整回合:蓄势 → 疾行 → 顿落墨溅 → 一拍停顿 → 引弓疾发 → 朱印砸落
    const qreal anticipate = span(progress, 0.0, 0.10);
    const qreal travelT = easeInOutCubic(span(progress, 0.10, 0.56));
    const qreal settleU = span(progress, 0.56, 0.66);

    QPointF queenCenter = to;
    qreal lift = 0.0;
    qreal scale = 1.0;
    if (progress < 0.10) {
        queenCenter = from;
        scale = 1.0 + 0.085 * std::sin(anticipate * kPi);
        lift = -2.5 * std::sin(anticipate * kPi);

        QRadialGradient charge(from, pieceSize * 1.1);
        charge.setColorAt(0.55, QColor(kAzure.red(), kAzure.green(),
                                       kAzure.blue(), int(anticipate * 30)));
        charge.setColorAt(1.0, QColor(kAzure.red(), kAzure.green(),
                                      kAzure.blue(), 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(charge);
        painter.drawEllipse(from, pieceSize * 1.1, pieceSize * 1.1);
    } else if (progress < 0.56) {
        queenCenter = lerpPoint(from, to, travelT);
        const qreal arc = std::sin(travelT * kPi);
        lift = pieceSize * 0.34 * arc;
        scale = 1.0 + arc * 0.10;
        const int trailAlpha =
            int(smoothStep(span(progress, 0.10, 0.22)) * 95 *
                (1.0 - span(progress, 0.48, 0.62)));
        if (trailAlpha > 3) {
            drawBrushStroke(painter, from, queenCenter, kAzure,
                            pieceSize * 0.12, trailAlpha);
        }
    } else {
        scale = 0.90 + 0.10 * easeOutBack(settleU);
    }

    drawPiece(painter, queenCenter, pieceSize, animation_.player, scale, lift);

    if (progress >= 0.56 && progress < 0.95) {
        for (int ringIndex = 0; ringIndex < 2; ++ringIndex) {
            const qreal rippleU =
                span(progress, 0.56 + ringIndex * 0.03, 0.92);
            drawRipple(painter, to,
                       pitch * (0.30 + rippleU * 0.66) *
                           (ringIndex ? 1.28 : 1.0),
                       int(std::pow(1.0 - rippleU, 1.4) *
                           (ringIndex ? 60 : 105)),
                       kAzureDeep);
        }
        splashParticles(to, destSeed, span(progress, 0.56, 0.86),
                        pitch * 0.80, 150, QColor(58, 74, 68));
    }

    if (progress >= 0.72 && progress < 0.94) {
        const qreal flightT = easeInQuad(span(progress, 0.72, 0.90));
        QPointF projectile = lerpPoint(to, arrowTarget, flightT);
        projectile.ry() -= std::sin(flightT * kPi) * pieceSize * 0.24;
        drawBrushStroke(painter, to, projectile, kCinnabar,
                        pieceSize * 0.075, int(40 + flightT * 70));
        drawArrowStone(painter, projectile,
                       pieceSize * (0.56 + flightT * 0.26), 1.0, 0.95);
    } else if (progress >= 0.94) {
        const qreal impactU = span(progress, 0.94, 0.985);
        drawArrowStone(
            painter,
            arrowTarget,
            pieceSize * 0.82 *
                (1.0 + 0.24 * (1.0 - easeOutCubic(impactU)))
        );
        const qreal rippleU = span(progress, 0.94, 1.0);
        drawRipple(painter, arrowTarget, pitch * (0.20 + rippleU * 0.52),
                   int((1.0 - rippleU) * 90), kCinnabar);
        splashParticles(arrowTarget, destSeed,
                        span(progress, 0.94, 1.0), pitch * 0.50, 120,
                        kCinnabarDeep);
    }
}

void BoardWidget::drawHint(QPainter& painter) const {
    if (!hintMove_ || animation_.active || selected_.valid()) {
        return;
    }

    const qint64 tMs = shimmerClock_.elapsed();
    const int breathAlpha = int(150 + 35 *
        std::sin(tMs / 2400.0 * 2 * kPi));
    const std::array<Coord, 3> points = {
        hintMove_->from,
        hintMove_->to,
        hintMove_->arrow,
    };

    painter.save();
    QPen dashPen(QColor(kOchre.red(), kOchre.green(), kOchre.blue(),
                        breathAlpha), 2.6);
    dashPen.setStyle(Qt::DashLine);
    dashPen.setDashPattern({9.0, 6.0});
    dashPen.setCapStyle(Qt::RoundCap);
    painter.setPen(dashPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(cellCenter(points[0]), cellCenter(points[1]));
    painter.drawLine(cellCenter(points[1]), cellCenter(points[2]));
    for (int index = 0; index < 3; ++index) {
        QRectF ring = cellRect(points[index]).adjusted(6.0, 6.0, -6.0, -6.0);
        painter.setBrush(QColor(kOchre.red(), kOchre.green(),
                                kOchre.blue(), 26));
        painter.setPen(QPen(QColor(kOchre.red(), kOchre.green(),
                                   kOchre.blue(), breathAlpha), 2.0));
        painter.drawRoundedRect(ring, ring.width() / 2.0, ring.width() / 2.0);
    }
    painter.restore();
}

void BoardWidget::drawPiece(
    QPainter& painter,
    QPointF center,
    qreal size,
    int player,
    qreal scale,
    qreal lift,
    qreal opacity
) const {
    painter.save();
    painter.setOpacity(opacity);
    center.ry() -= lift;
    const qreal diameter = size * scale;
    const QRectF pieceRect(
        center.x() - diameter / 2.0,
        center.y() - diameter / 2.0,
        diameter,
        diameter
    );

    for (int layer = 4; layer >= 1; --layer) {
        const qreal expansion = layer * 2.1;
        QColor shadow(72, 58, 36, 8 + layer * 4);
        painter.setPen(Qt::NoPen);
        painter.setBrush(shadow);
        painter.drawEllipse(
            pieceRect.adjusted(-expansion, -expansion + 4.0 + lift * 0.3,
                               expansion, expansion + 4.0 + lift * 0.3)
        );
    }

    QRadialGradient body(
        pieceRect.center() - QPointF(diameter * 0.20, diameter * 0.23),
        diameter * 0.75
    );
    if (player == Black) {
        body.setColorAt(0.0, QColor(88, 82, 74));
        body.setColorAt(0.45, QColor(46, 42, 38));
        body.setColorAt(1.0, QColor(17, 15, 13));
    } else {
        body.setColorAt(0.0, QColor(255, 253, 245));
        body.setColorAt(0.60, QColor(240, 229, 205));
        body.setColorAt(1.0, QColor(199, 183, 148));
    }
    painter.setBrush(body);
    painter.setPen(QPen(
        player == Black ? QColor(70, 63, 54) : QColor(160, 143, 112),
        std::max(1.4, diameter * 0.024)
    ));
    painter.drawEllipse(pieceRect);

    QRadialGradient sheen(
        pieceRect.center() - QPointF(diameter * 0.26, diameter * 0.30),
        diameter * 0.30
    );
    sheen.setColorAt(0.0, player == Black ? QColor(255, 250, 238, 34)
                                          : QColor(255, 255, 250, 175));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(sheen);
    painter.drawEllipse(pieceRect.adjusted(diameter * 0.06, diameter * 0.06,
                                           -diameter * 0.30,
                                           -diameter * 0.30));

    painter.setFont(cjkFont(diameter * 0.46, QFont::DemiBold));
    painter.setPen(player == Black ? QColor(232, 221, 196, 205)
                                   : QColor(74, 64, 50, 220));
    painter.drawText(pieceRect, Qt::AlignCenter, QStringLiteral("后"));
    painter.restore();
}

void BoardWidget::drawArrowStone(
    QPainter& painter,
    QPointF center,
    qreal size,
    qreal scale,
    qreal opacity
) const {
    painter.save();
    painter.setOpacity(opacity);
    const qreal diameter = size * scale;

    QRadialGradient shadowGrad(
        center + QPointF(0.0, diameter * 0.10), diameter * 0.62
    );
    shadowGrad.setColorAt(0.0, QColor(60, 46, 28, 88));
    shadowGrad.setColorAt(1.0, QColor(60, 46, 28, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadowGrad);
    painter.drawEllipse(center + QPointF(0.0, diameter * 0.12),
                        diameter * 0.62, diameter * 0.52);

    painter.save();
    painter.translate(center);
    const quint32 seedValue = coordSeed(int(center.y()), int(center.x()), 33u);
    painter.rotate(-4.5 + noise01(seedValue) * 9.0);

    const QRectF sealRect(-diameter / 2.0, -diameter / 2.0, diameter, diameter);
    QLinearGradient seal(sealRect.topLeft(), sealRect.bottomLeft());
    seal.setColorAt(0.0, QColor(184, 66, 40));
    seal.setColorAt(0.55, kCinnabar);
    seal.setColorAt(1.0, kCinnabarDeep);
    painter.setPen(QPen(QColor(94, 27, 13, 230), std::max(1.2, diameter * 0.05)));
    painter.setBrush(seal);
    painter.drawRoundedRect(sealRect, diameter * 0.16, diameter * 0.16);

    painter.setPen(QPen(QColor(243, 231, 207, 70), std::max(0.9, diameter * 0.03)));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(sealRect.adjusted(
        diameter * 0.09, diameter * 0.09,
        -diameter * 0.09, -diameter * 0.09
    ), diameter * 0.11, diameter * 0.11);

    painter.setFont(cjkFont(diameter * 0.52, QFont::Bold));
    painter.setPen(kPaperChar);
    painter.drawText(sealRect, Qt::AlignCenter, QStringLiteral("障"));
    painter.restore();
    painter.restore();
}

QColor BoardWidget::brushTargetColor() const {
    // 高亮用更饱和的色:青绿示可行,朱红示落箭
    return phase_ == SelectionPhase::ArrowTarget
        ? QColor(186, 48, 24)
        : QColor(26, 120, 100);
}

void BoardWidget::syncIdleMotion() {
    const bool need = game_ != nullptr && !animation_.active &&
        (hintMove_.has_value() || !validTargets_.empty() ||
         (inputEnabled_ && hovered_.valid()));
    if (need && !idleTimer_.isActive()) {
        idleTimer_.start();
    } else if (!need && idleTimer_.isActive()) {
        idleTimer_.stop();
    }
}

void BoardWidget::ensurePaperTexture() const {
    const qreal dpr = std::clamp(devicePixelRatioF(), 1.0, 3.0);
    const QSize pixelSize(qCeil(width() * dpr), qCeil(height() * dpr));
    if (paperTextureSize_ == pixelSize && !paperTexture_.isNull()) {
        return;
    }

    paperTexture_ = QPixmap(pixelSize);
    paperTexture_.setDevicePixelRatio(dpr);
    paperTexture_.fill(Qt::transparent);

    QPainter painter(&paperTexture_);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(dpr, dpr);
    const QRectF area(0.0, 0.0, width(), height());

    QLinearGradient base(area.topLeft(), area.bottomLeft());
    base.setColorAt(0.0, QColor(243, 234, 214));
    base.setColorAt(0.55, kPaper);
    base.setColorAt(1.0, QColor(233, 222, 197));
    painter.setPen(Qt::NoPen);
    painter.setBrush(base);
    painter.drawRect(area);

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(120, 104, 74, 7), 1.0));
    for (qreal y = 0.0; y < height(); y += 5.0) {
        painter.drawLine(QPointF(0.0, y), QPointF(width(), y));
    }
    painter.setRenderHint(QPainter::Antialiasing, true);

    std::mt19937 generator(kPaperSeed);
    auto uniform = [&generator](qreal low, qreal high) {
        const qreal unit = qreal(generator() - generator.min()) /
                           qreal(generator.max() - generator.min());
        return low + (high - low) * unit;
    };

    const int fiberCount = int(width() * height() / 700.0);
    for (int index = 0; index < fiberCount; ++index) {
        const QPointF start(uniform(0.0, width()), uniform(0.0, height()));
        const qreal angle = uniform(0.0, kPi);
        const qreal length = uniform(2.5, 8.5);
        const bool bright = uniform(0.0, 1.0) < 0.45;
        QColor fiber = bright ? QColor(255, 250, 235)
                              : QColor(112, 96, 68);
        fiber.setAlpha(int(uniform(7.0, 24.0)));
        painter.setPen(QPen(fiber, 0.9, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(
            start,
            start + QPointF(std::cos(angle), std::sin(angle)) * length
        );
    }

    const int fleckCount = int(width() * height() / 8000.0);
    for (int index = 0; index < fleckCount; ++index) {
        const QPointF spot(uniform(0.0, width()), uniform(0.0, height()));
        QColor fleck = uniform(0.0, 1.0) < 0.6 ? QColor(126, 108, 76)
                                               : QColor(90, 74, 48);
        fleck.setAlpha(int(uniform(14.0, 34.0)));
        painter.setPen(Qt::NoPen);
        painter.setBrush(fleck);
        painter.drawEllipse(spot, uniform(0.7, 1.7), uniform(0.7, 1.7));
    }

    QRadialGradient vignette(area.center(), area.width() * 0.78);
    vignette.setColorAt(0.55, QColor(118, 98, 64, 0));
    vignette.setColorAt(1.0, QColor(118, 98, 64, 34));
    painter.setPen(Qt::NoPen);
    painter.setBrush(vignette);
    painter.drawRect(area);
    painter.end();
}

}  // namespace zzmazon
