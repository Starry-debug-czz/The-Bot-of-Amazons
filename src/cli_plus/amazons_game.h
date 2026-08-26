#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace zzmazon {

constexpr int kBoardSize = 8;

enum Cell : int {
    Empty = 0,
    Black = 1,
    White = 2,
    Arrow = 3,
};

struct Coord {
    int row = -1;
    int col = -1;

    [[nodiscard]] bool valid() const;
    bool operator==(const Coord& other) const;
};

struct Move {
    Coord from;
    Coord to;
    Coord arrow;

    bool operator==(const Move& other) const;
};

class AmazonsGame {
public:
    using Board = std::array<int, kBoardSize * kBoardSize>;

    AmazonsGame();

    void reset();

    [[nodiscard]] const Board& board() const;
    [[nodiscard]] int at(Coord coord) const;
    [[nodiscard]] int currentPlayer() const;
    [[nodiscard]] int moveCount() const;
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] int winner() const;

    [[nodiscard]] std::vector<Coord> legalDestinations(Coord from) const;
    [[nodiscard]] std::vector<Coord> legalArrows(Coord from, Coord to) const;
    [[nodiscard]] std::vector<Move> legalMoves(int player) const;
    [[nodiscard]] bool isLegal(const Move& move) const;

    bool playMove(const Move& move);
    bool undoPlies(int count = 1);

    [[nodiscard]] static std::optional<Move> chooseMove(
        Board board,
        int player,
        int timeLimitMs
    );

    [[nodiscard]] static std::string formatMove(const Move& move);
    [[nodiscard]] static int opponentOf(int player);

private:
    struct Snapshot {
        Board board;
        int currentPlayer;
    };

    Board board_{};
    int currentPlayer_ = Black;
    std::vector<Snapshot> history_;
};

}  // namespace zzmazon
