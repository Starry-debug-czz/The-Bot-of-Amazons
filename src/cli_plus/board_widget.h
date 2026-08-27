#pragma once

#include "amazons_game.h"

#include <QElapsedTimer>
#include <QPointF>
#include <QRectF>
#include <QTimer>
#include <QWidget>

#include <optional>
#include <vector>

class QKeyEvent;
class QMouseEvent;
class QPainter;
class QPixmap;

namespace zzmazon {

class BoardWidget final : public QWidget {
    Q_OBJECT

public:
    enum class SelectionPhase {
        Piece,
        Destination,
        MovingQueen,
        ArrowTarget,
    };

    explicit BoardWidget(QWidget* parent = nullptr);

    void setGame(const AmazonsGame* game);
    void setInputEnabled(bool enabled);
    void resetSelection();
    void animateMove(const Move& move);
    void animateArrow(const Move& move);
    void showHint(const Move& move);
    void clearHint();

signals:
    void moveChosen(const zzmazon::Move& move);
    void animationFinished();
    void selectionTransitionChanged(bool active);
    void interactionHintChanged(const QString& hint);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    enum class AnimationKind {
        FullMove,
        QueenPreview,
        ArrowOnly,
    };

    struct AnimationState {
        bool active = false;
        AnimationKind kind = AnimationKind::FullMove;
        Move move;
        int player = Empty;
        int durationMs = 920;
        // 音效节点只触发一次的标记
        bool soundPlace = false;
        bool soundArrow = false;
        bool soundImpact = false;
    };

    [[nodiscard]] QRectF boardRect() const;
    [[nodiscard]] QRectF cellRect(Coord coord) const;
    [[nodiscard]] QRectF tileRect(Coord coord) const;
    [[nodiscard]] qreal hoverLift(Coord coord) const;
    [[nodiscard]] QPointF cellCenter(Coord coord) const;
    [[nodiscard]] Coord coordAt(QPointF position) const;
    [[nodiscard]] bool isValidTarget(Coord coord) const;
    [[nodiscard]] bool isInteractive(Coord coord) const;
    [[nodiscard]] std::vector<Coord> highlightedPath() const;

    void selectPiece(Coord coord);
    void chooseDestination(Coord coord);
    void chooseArrow(Coord coord);
    void updateAnimation();

    void drawBackdrop(QPainter& painter) const;
    void drawBoard(QPainter& painter) const;
    void drawCoordinates(QPainter& painter) const;
    void drawHighlights(QPainter& painter) const;
    void drawStaticContents(QPainter& painter) const;
    void drawSelectionPreview(QPainter& painter) const;
    void drawAnimation(QPainter& painter) const;
    void drawHint(QPainter& painter) const;
    void drawPiece(
        QPainter& painter,
        QPointF center,
        qreal size,
        int player,
        qreal scale = 1.0,
        qreal lift = 0.0,
        qreal opacity = 1.0
    ) const;
    void drawArrowStone(
        QPainter& painter,
        QPointF center,
        qreal size,
        qreal scale = 1.0,
        qreal opacity = 1.0
    ) const;

    [[nodiscard]] QColor brushTargetColor() const;
    void syncIdleMotion();
    void ensurePaperTexture() const;

    const AmazonsGame* game_ = nullptr;
    bool inputEnabled_ = false;
    SelectionPhase phase_ = SelectionPhase::Piece;
    Coord selected_;
    Coord destination_;
    Coord hovered_;
    std::vector<Coord> validTargets_;
    std::optional<Move> hintMove_;

    mutable QPixmap paperTexture_;
    mutable QSize paperTextureSize_;
    QElapsedTimer shimmerClock_;
    QElapsedTimer hoverClock_;
    QTimer idleTimer_;

    AnimationState animation_;
    QElapsedTimer animationClock_;
    QTimer animationTimer_;
};

}  // namespace zzmazon
