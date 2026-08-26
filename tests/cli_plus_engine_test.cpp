#include "amazons_game.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int countCells(const zzmazon::AmazonsGame::Board& board, int cell) {
    return static_cast<int>(std::count(board.begin(), board.end(), cell));
}

}  // namespace

int main() {
    using namespace zzmazon;

    AmazonsGame game;
    expect(game.currentPlayer() == Black, "black must move first");
    expect(countCells(game.board(), Black) == 4, "initial black queen count");
    expect(countCells(game.board(), White) == 4, "initial white queen count");
    expect(countCells(game.board(), Arrow) == 0, "initial arrow count");

    const Coord source{0, 2};
    const Coord destination{0, 3};
    const auto destinations = game.legalDestinations(source);
    expect(
        std::find(destinations.begin(), destinations.end(), destination) !=
            destinations.end(),
        "queen should move horizontally to an empty square"
    );

    const auto arrows = game.legalArrows(source, destination);
    expect(
        std::find(arrows.begin(), arrows.end(), source) != arrows.end(),
        "arrow may land on the queen's vacated source"
    );

    const Move move{source, destination, source};
    expect(game.isLegal(move), "known opening move must be legal");
    expect(game.playMove(move), "legal move must be applied");
    expect(game.at(source) == Arrow, "source should contain the fired arrow");
    expect(game.at(destination) == Black, "destination should contain queen");
    expect(game.currentPlayer() == White, "turn should alternate");
    expect(game.moveCount() == 1, "history should contain one ply");

    expect(game.undoPlies(), "undo should restore the previous snapshot");
    expect(game.at(source) == Black, "undo should restore source queen");
    expect(game.at(destination) == Empty, "undo should clear destination");
    expect(game.currentPlayer() == Black, "undo should restore turn");

    const auto aiMove = AmazonsGame::chooseMove(game.board(), Black, 120);
    expect(aiMove.has_value(), "AI should find an opening move");
    expect(game.isLegal(*aiMove), "AI move must pass the public validator");

    std::cout << "cli_plus_engine_test: all checks passed\n";
    return EXIT_SUCCESS;
}
