#pragma once

#include "amazons_game.h"

#include <QFutureWatcher>
#include <QMainWindow>

#include <optional>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace zzmazon {

class BoardWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void startNewGame();
    void handleMoveChosen(const zzmazon::Move& move);
    void handleAnimationFinished();
    void undoMove();
    void requestHint();
    void finishAiTurn();
    void finishHint();

private:
    void buildInterface();
    void connectInterface();
    void startAiTurn();
    void updateInterface();
    void setBusy(bool busy, const QString& message = {});
    void appendMoveToLog(int player, const Move& move);
    void removeMovesFromLog(int count);
    [[nodiscard]] bool checkGameOver();
    [[nodiscard]] bool isHumanTurn() const;
    [[nodiscard]] int selectedThinkTime() const;

    AmazonsGame game_;
    bool versusAi_ = true;
    int humanColor_ = Black;
    bool busy_ = false;
    bool boardTransition_ = false;

    BoardWidget* boardWidget_ = nullptr;
    QLabel* turnBadge_ = nullptr;
    QLabel* statusTitle_ = nullptr;
    QLabel* statusDetail_ = nullptr;
    QLabel* phaseLabel_ = nullptr;
    QLabel* moveCountLabel_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QComboBox* sideCombo_ = nullptr;
    QComboBox* difficultyCombo_ = nullptr;
    QPushButton* newGameButton_ = nullptr;
    QPushButton* undoButton_ = nullptr;
    QPushButton* hintButton_ = nullptr;
    QListWidget* moveLog_ = nullptr;

    QFutureWatcher<std::optional<Move>> aiWatcher_;
    QFutureWatcher<std::optional<Move>> hintWatcher_;
};

}  // namespace zzmazon
