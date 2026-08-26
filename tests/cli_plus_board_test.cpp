#include "amazons_game.h"
#include "board_widget.h"

#include <QApplication>
#include <QPoint>
#include <QTest>

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

QPoint cellCenter(const zzmazon::BoardWidget& board, zzmazon::Coord coord) {
    constexpr double padding = 42.0;
    const double size = std::min(
        board.width() - padding * 2.0,
        board.height() - padding * 2.0
    );
    const double left = (board.width() - size) / 2.0;
    const double top = (board.height() - size) / 2.0;
    const double cell = size / zzmazon::kBoardSize;
    return {
        static_cast<int>(left + (coord.col + 0.5) * cell),
        static_cast<int>(top + (coord.row + 0.5) * cell),
    };
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    using namespace zzmazon;

    AmazonsGame game;
    BoardWidget board;
    board.resize(700, 700);
    board.setGame(&game);
    board.setInputEnabled(true);
    board.show();
    expect(QTest::qWaitForWindowExposed(&board),
           "offscreen board window should become exposed");

    std::vector<QString> hints;
    std::optional<Move> chosenMove;
    bool animationCompleted = false;
    QObject::connect(
        &board,
        &BoardWidget::interactionHintChanged,
        [&hints](const QString& hint) { hints.push_back(hint); }
    );
    QObject::connect(
        &board,
        &BoardWidget::moveChosen,
        [&chosenMove](const Move& move) { chosenMove = move; }
    );
    QObject::connect(
        &board,
        &BoardWidget::animationFinished,
        [&animationCompleted] { animationCompleted = true; }
    );

    const Coord source{0, 2};
    const Coord destination{0, 3};
    QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier,
                      cellCenter(board, source));
    expect(!hints.empty() && hints.back() == QStringLiteral("选择移动终点"),
           "clicking a queen should enter destination selection");

    QTest::mouseMove(&board, cellCenter(board, destination));
    const QString screenshotPath = qEnvironmentVariable("ZZMAZON_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        board.grab().save(screenshotPath);
    }
    QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier,
                      cellCenter(board, destination));
    expect(hints.back() == QStringLiteral("棋子移动中…"),
           "destination click should start the queen movement first");
    QTest::qWait(460);
    expect(hints.back() == QStringLiteral("选择箭的落点"),
           "arrow targets should appear only after the queen has landed");
    expect(!animationCompleted,
           "preview movement must not finish the complete turn");

    QTest::mouseMove(&board, cellCenter(board, source));
    const QString arrowScreenshotPath =
        qEnvironmentVariable("ZZMAZON_ARROW_SCREENSHOT");
    if (!arrowScreenshotPath.isEmpty()) {
        board.grab().save(arrowScreenshotPath);
    }
    QTest::mouseClick(&board, Qt::LeftButton, Qt::NoModifier,
                      cellCenter(board, source));
    expect(chosenMove.has_value(), "third click should emit a complete move");
    expect(*chosenMove == Move{source, destination, source},
           "emitted move should preserve all three selected coordinates");

    expect(game.playMove(*chosenMove), "chosen GUI move should be legal");
    board.animateArrow(*chosenMove);
    QTest::qWait(560);
    expect(animationCompleted, "arrow landing animation should complete");
    expect(game.at(destination) == Black && game.at(source) == Arrow,
           "animated move should end on the committed board state");

    std::cout << "cli_plus_board_test: all checks passed\n";
    return EXIT_SUCCESS;
}
