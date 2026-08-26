#include "board_widget.h"

#include <QBrush>
#include <QColor>
#include <QConicalGradient>
#include <QCursor>
#include <QEasingCurve>
#include <QFont>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

namespace zzmazon {
namespace {

constexpr qreal kOuterPadding = 42.0;
constexpr qreal kCellGap = 5.0;
constexpr qreal kPi = 3.14159265358979323846;

qreal clamp01(qreal value) {
    return std::clamp(value, 0.0, 1.0);
}

qreal smoothStep(qreal value) {
    value = clamp01(value);
    return value * value * (3.0 - 2.0 * value);
}

QPointF interpolate(QPointF from, QPointF to, qreal progress) {
    return from + (to - from) * progress;
}

bool contains(const std::vector<Coord>& coords, Coord wanted) {
    return std::find(coords.begin(), coords.end(), wanted) != coords.end();
}

int signOf(int value) {
    return (value > 0) - (value < 0);
}

QString phaseText(BoardWidget::SelectionPhase phase) {
    switch (phase) {
        case BoardWidget::SelectionPhase::Piece:
            return QStringLiteral("选择一枚棋子");
        case BoardWidget::SelectionPhase::Destination:
            return QStringLiteral("选择移动终点");
        case BoardWidget::SelectionPhase::MovingQueen:
            return QStringLiteral("棋子移动中…");
        case BoardWidget::SelectionPhase::ArrowTarget:
            return QStringLiteral("选择箭的落点");
    }
    return {};
}

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
    animation_.durationMs = 920;
    animationClock_.restart();
    animationTimer_.start();
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
    animation_.durationMs = 470;
    animationClock_.restart();
    animationTimer_.start();
    update();
}

void BoardWidget::showHint(const Move& move) {
    hintMove_ = move;
    update();
}

void BoardWidget::clearHint() {
    hintMove_.reset();
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
    animation_.durationMs = 390;
    animationClock_.restart();
    animationTimer_.start();
    emit selectionTransitionChanged(true);
    emit interactionHintChanged(phaseText(phase_));
    update();
}

void BoardWidget::chooseArrow(Coord coord) {
    if (!isValidTarget(coord)) {
        return;
    }
    emit moveChosen({selected_, destination_, coord});
}

void BoardWidget::updateCursorFor(Coord coord) {
    if (isInteractive(coord)) {
        setCursor(Qt::PointingHandCursor);
    } else {
        unsetCursor();
    }
}

void BoardWidget::mouseMoveEvent(QMouseEvent* event) {
    const Coord nextHover = coordAt(event->position());
    if (!(nextHover == hovered_)) {
        hovered_ = nextHover;
        updateCursorFor(hovered_);
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
            updateCursorFor(hovered_);
            update();
            return;
        }
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
    const QRectF board = boardRect();
    const QRectF card = board.adjusted(-18.0, -18.0, 18.0, 18.0);

    for (int layer = 18; layer > 0; layer -= 3) {
        QColor shadow(2, 8, 20, 7);
        painter.setPen(Qt::NoPen);
        painter.setBrush(shadow);
        painter.drawRoundedRect(
            card.translated(0.0, layer * 0.55).adjusted(
                -layer * 0.25,
                -layer * 0.1,
                layer * 0.25,
                layer * 0.35
            ),
            28.0,
            28.0
        );
    }

    QLinearGradient cardGradient(card.topLeft(), card.bottomRight());
    cardGradient.setColorAt(0.0, QColor("#1b2b43"));
    cardGradient.setColorAt(1.0, QColor("#101b2e"));
    painter.setBrush(cardGradient);
    painter.setPen(QPen(QColor(255, 255, 255, 22), 1.0));
    painter.drawRoundedRect(card, 24.0, 24.0);
}

void BoardWidget::drawBoard(QPainter& painter) const {
    const QRectF board = boardRect();
    const qreal cellSize = board.width() / kBoardSize;

    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const Coord coord{row, col};
            const QRectF cell = cellRect(coord);
            QLinearGradient surface(cell.topLeft(), cell.bottomRight());
            if ((row + col) % 2 == 0) {
                surface.setColorAt(0.0, QColor("#f5ead4"));
                surface.setColorAt(1.0, QColor("#dfc89e"));
            } else {
                surface.setColorAt(0.0, QColor("#caa876"));
                surface.setColorAt(1.0, QColor("#ad8253"));
            }

            painter.setPen(QPen(QColor(255, 255, 255, 22), 1.0));
            painter.setBrush(surface);
            painter.drawRoundedRect(cell, cellSize * 0.08, cellSize * 0.08);

            const QRectF sheen = cell.adjusted(4.0, 4.0, -4.0, -cell.height() * 0.58);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 13));
            painter.drawRoundedRect(sheen, 5.0, 5.0);
        }
    }
}

void BoardWidget::drawCoordinates(QPainter& painter) const {
    const QRectF board = boardRect();
    const qreal cell = board.width() / kBoardSize;
    QFont font = painter.font();
    font.setPointSizeF(std::max(9.0, cell * 0.13));
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    painter.setPen(QColor("#70839f"));

    for (int index = 0; index < kBoardSize; ++index) {
        const QRectF topLabel(
            board.left() + index * cell,
            board.top() - 28.0,
            cell,
            22.0
        );
        painter.drawText(topLabel, Qt::AlignCenter, QString::number(index));

        const QRectF leftLabel(
            board.left() - 30.0,
            board.top() + index * cell,
            22.0,
            cell
        );
        painter.drawText(leftLabel, Qt::AlignCenter, QString::number(index));
    }
}

void BoardWidget::drawHighlights(QPainter& painter) const {
    if (!game_ || animation_.active) {
        return;
    }

    const QColor targetColor = phase_ == SelectionPhase::ArrowTarget
                                   ? QColor("#ff8b72")
                                   : QColor("#43d9c2");
    const Coord rayOrigin = phase_ == SelectionPhase::ArrowTarget
                                ? destination_
                                : selected_;

    if (rayOrigin.valid() && !validTargets_.empty()) {
        painter.save();
        painter.setPen(QPen(QColor(targetColor.red(), targetColor.green(),
                                  targetColor.blue(), 23), 2.0));
        for (const Coord target : validTargets_) {
            painter.drawLine(cellCenter(rayOrigin), cellCenter(target));
        }
        painter.restore();
    }

    const auto path = highlightedPath();
    if (!path.empty()) {
        QLinearGradient beam(cellCenter(rayOrigin), cellCenter(path.back()));
        beam.setColorAt(0.0, QColor(targetColor.red(), targetColor.green(),
                                   targetColor.blue(), 70));
        beam.setColorAt(1.0, QColor(targetColor.red(), targetColor.green(),
                                   targetColor.blue(), 225));
        painter.setPen(QPen(QBrush(beam), std::max(5.0, boardRect().width() / 105.0),
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(cellCenter(rayOrigin), cellCenter(path.back()));
    }

    for (const Coord target : validTargets_) {
        QRectF marker = cellRect(target).adjusted(9.0, 9.0, -9.0, -9.0);
        const bool hovered = target == hovered_;
        QColor fill = targetColor;
        fill.setAlpha(hovered ? 110 : 42);
        painter.setBrush(fill);
        painter.setPen(QPen(QColor(targetColor.red(), targetColor.green(),
                                  targetColor.blue(), hovered ? 245 : 120),
                            hovered ? 2.5 : 1.2));
        painter.drawRoundedRect(marker, marker.width() * 0.24, marker.width() * 0.24);

        const qreal dotSize = hovered ? marker.width() * 0.22 : marker.width() * 0.13;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(targetColor.red(), targetColor.green(),
                                targetColor.blue(), hovered ? 245 : 170));
        painter.drawEllipse(target == hovered_ ? marker.center() + QPointF(0, -1)
                                               : marker.center(),
                            dotSize, dotSize);
    }

    if (selected_.valid()) {
        const Coord focusCoord =
            (phase_ == SelectionPhase::MovingQueen ||
             phase_ == SelectionPhase::ArrowTarget)
                ? destination_
                : selected_;
        QRectF selectedRect = cellRect(focusCoord)
                                  .adjusted(3.0, 3.0, -3.0, -3.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor("#ffd36a"), 3.0));
        painter.drawRoundedRect(selectedRect, 12.0, 12.0);
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

    Coord preview = selected_;
    qreal lift = 7.0;
    qreal opacity = 1.0;
    qreal previewScale = 1.08;
    if (phase_ == SelectionPhase::Destination && isValidTarget(hovered_)) {
        preview = hovered_;
        lift = 9.0;
    } else if (phase_ == SelectionPhase::ArrowTarget) {
        preview = destination_;
        lift = 0.0;
        previewScale = 1.0;
    }

    const qreal pulse = 1.0 + 0.025 *
        std::sin(static_cast<qreal>(animationClock_.isValid()
                                       ? animationClock_.elapsed()
                                       : 0) / 160.0);
    drawPiece(
        painter,
        cellCenter(preview),
        cellRect(preview).width() * 0.72,
        game_->currentPlayer(),
        previewScale * (phase_ == SelectionPhase::ArrowTarget ? 1.0 : pulse),
        lift,
        opacity
    );

    if (phase_ == SelectionPhase::ArrowTarget && isValidTarget(hovered_)) {
        drawArrowStone(
            painter,
            cellCenter(hovered_),
            cellRect(hovered_).width() * 0.58,
            0.92,
            0.72
        );
    }
}

void BoardWidget::drawAnimation(QPainter& painter) const {
    if (!animation_.active || !game_) {
        return;
    }

    const qreal progress = clamp01(
        static_cast<qreal>(animationClock_.elapsed()) / animation_.durationMs
    );
    const qreal pieceSize = cellRect({0, 0}).width() * 0.72;
    const QPointF from = cellCenter(animation_.move.from);
    const QPointF to = cellCenter(animation_.move.to);
    const QPointF arrowTarget = cellCenter(animation_.move.arrow);

    if (animation_.kind == AnimationKind::QueenPreview) {
        const qreal travel = smoothStep(progress);
        QPointF queenCenter = interpolate(from, to, travel);
        queenCenter.ry() -= std::sin(travel * kPi) * pieceSize * 0.22;

        QLinearGradient trail(from, queenCenter);
        trail.setColorAt(0.0, QColor(67, 217, 194, 18));
        trail.setColorAt(1.0, QColor(67, 217, 194, 180));
        painter.setPen(QPen(QBrush(trail), pieceSize * 0.11,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(from, queenCenter);
        drawPiece(
            painter,
            queenCenter,
            pieceSize,
            animation_.player,
            1.0 + std::sin(travel * kPi) * 0.13,
            std::sin(travel * kPi) * 12.0
        );
        return;
    }

    if (animation_.kind == AnimationKind::ArrowOnly) {
        const qreal arrowProgress = smoothStep(progress);
        QPointF projectile = interpolate(to, arrowTarget, arrowProgress);
        projectile.ry() -= std::sin(arrowProgress * kPi) * pieceSize * 0.30;

        painter.setPen(QPen(QColor(255, 139, 114, 135), pieceSize * 0.09,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(to, projectile);
        drawArrowStone(
            painter,
            projectile,
            pieceSize * (0.58 + arrowProgress * 0.24),
            1.0,
            0.94
        );
        return;
    }

    const qreal travel = smoothStep((progress - 0.07) / 0.57);
    QPointF queenCenter = interpolate(from, to, travel);
    queenCenter.ry() -= std::sin(travel * kPi) * pieceSize * 0.20;
    const qreal lift = 4.0 + std::sin(travel * kPi) * 12.0;
    const qreal scale = 1.0 + std::sin(travel * kPi) * 0.13;

    painter.save();
    QLinearGradient trail(from, queenCenter);
    trail.setColorAt(0.0, QColor(67, 217, 194, 12));
    trail.setColorAt(1.0, QColor(67, 217, 194, 145));
    painter.setPen(QPen(QBrush(trail), pieceSize * 0.12,
                        Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(from, queenCenter);
    painter.restore();

    drawPiece(
        painter,
        progress < 0.70 ? queenCenter : to,
        pieceSize,
        animation_.player,
        progress < 0.70 ? scale : 1.0,
        progress < 0.70 ? lift : 0.0
    );

    if (progress >= 0.62 && progress < 0.94) {
        const qreal arrowProgress = smoothStep((progress - 0.62) / 0.32);
        QPointF projectile = interpolate(to, arrowTarget, arrowProgress);
        projectile.ry() -= std::sin(arrowProgress * kPi) * pieceSize * 0.28;

        painter.save();
        painter.setPen(QPen(QColor(255, 139, 114, 130), pieceSize * 0.09,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(to, projectile);
        painter.restore();

        drawArrowStone(
            painter,
            projectile,
            pieceSize * 0.60,
            0.78 + arrowProgress * 0.20,
            0.92
        );
    } else if (progress >= 0.94) {
        const qreal settle = smoothStep((progress - 0.94) / 0.06);
        drawArrowStone(
            painter,
            arrowTarget,
            pieceSize * 0.82,
            1.12 - settle * 0.12,
            1.0
        );
    }
}

void BoardWidget::drawHint(QPainter& painter) const {
    if (!hintMove_ || animation_.active || selected_.valid()) {
        return;
    }

    const QColor hintColor("#ffd36a");
    const std::array<Coord, 3> points = {
        hintMove_->from,
        hintMove_->to,
        hintMove_->arrow,
    };

    painter.save();
    painter.setPen(QPen(QColor(255, 211, 106, 185), 3.0,
                        Qt::DashLine, Qt::RoundCap));
    painter.drawLine(cellCenter(points[0]), cellCenter(points[1]));
    painter.drawLine(cellCenter(points[1]), cellCenter(points[2]));
    for (int index = 0; index < 3; ++index) {
        QRectF ring = cellRect(points[index]).adjusted(7.0, 7.0, -7.0, -7.0);
        painter.setBrush(QColor(255, 211, 106, 28));
        painter.setPen(QPen(hintColor, 2.0));
        painter.drawRoundedRect(ring, 10.0, 10.0);
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

    for (int layer = 5; layer >= 1; --layer) {
        const qreal expansion = layer * 1.8;
        QColor shadow(4, 8, 17, 9 + layer * 5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(shadow);
        painter.drawEllipse(
            pieceRect.adjusted(-expansion, -expansion + 5.0,
                               expansion, expansion + 5.0)
        );
    }

    QRadialGradient body(
        pieceRect.center() - QPointF(diameter * 0.19, diameter * 0.22),
        diameter * 0.68
    );
    if (player == Black) {
        body.setColorAt(0.0, QColor("#334862"));
        body.setColorAt(0.48, QColor("#17253a"));
        body.setColorAt(1.0, QColor("#07101f"));
    } else {
        body.setColorAt(0.0, QColor("#fffdf6"));
        body.setColorAt(0.62, QColor("#efe5cf"));
        body.setColorAt(1.0, QColor("#cbb995"));
    }
    painter.setBrush(body);
    painter.setPen(QPen(
        player == Black ? QColor("#5e7898") : QColor("#fff8df"),
        std::max(1.5, diameter * 0.026)
    ));
    painter.drawEllipse(pieceRect);

    QPainterPath crown;
    const qreal left = center.x() - diameter * 0.24;
    const qreal right = center.x() + diameter * 0.24;
    const qreal top = center.y() - diameter * 0.20;
    const qreal bottom = center.y() + diameter * 0.18;
    crown.moveTo(left, bottom);
    crown.lineTo(left + diameter * 0.03, top + diameter * 0.08);
    crown.lineTo(center.x() - diameter * 0.09, top + diameter * 0.16);
    crown.lineTo(center.x(), top);
    crown.lineTo(center.x() + diameter * 0.09, top + diameter * 0.16);
    crown.lineTo(right - diameter * 0.03, top + diameter * 0.08);
    crown.lineTo(right, bottom);
    crown.closeSubpath();

    painter.setPen(QPen(
        player == Black ? QColor("#83e4d7") : QColor("#8e7040"),
        std::max(1.2, diameter * 0.022),
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin
    ));
    painter.setBrush(
        player == Black ? QColor(67, 217, 194, 36)
                        : QColor(142, 112, 64, 24)
    );
    painter.drawPath(crown);
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
    const QRectF stone(
        center.x() - diameter / 2.0,
        center.y() - diameter / 2.0,
        diameter,
        diameter
    );

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(3, 8, 18, 48));
    painter.drawRoundedRect(stone.translated(0.0, 4.0), diameter * 0.26,
                            diameter * 0.26);

    QLinearGradient body(stone.topLeft(), stone.bottomRight());
    body.setColorAt(0.0, QColor("#536378"));
    body.setColorAt(0.46, QColor("#2a3749"));
    body.setColorAt(1.0, QColor("#111b2a"));
    painter.setBrush(body);
    painter.setPen(QPen(QColor("#738399"), std::max(1.0, diameter * 0.025)));
    painter.drawRoundedRect(stone, diameter * 0.26, diameter * 0.26);

    painter.setPen(QPen(QColor("#ff8b72"), diameter * 0.095,
                        Qt::SolidLine, Qt::RoundCap));
    const qreal inset = diameter * 0.27;
    painter.drawLine(stone.topLeft() + QPointF(inset, inset),
                     stone.bottomRight() - QPointF(inset, inset));
    painter.drawLine(stone.topRight() + QPointF(-inset, inset),
                     stone.bottomLeft() + QPointF(inset, -inset));
    painter.restore();
}

}  // namespace zzmazon
