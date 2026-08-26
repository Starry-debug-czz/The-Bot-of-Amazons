#include "amazons_game.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <queue>
#include <random>
#include <sstream>
#include <utility>

namespace zzmazon {
namespace {

constexpr std::array<Coord, 8> kDirections = {{
    {-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
    {0, 1},   {1, -1}, {1, 0},  {1, 1},
}};

constexpr int kUnreachable = 99;

int indexOf(Coord coord) {
    return coord.row * kBoardSize + coord.col;
}

bool inBounds(Coord coord) {
    return coord.row >= 0 && coord.row < kBoardSize &&
           coord.col >= 0 && coord.col < kBoardSize;
}

Coord step(Coord coord, Coord direction, int distance = 1) {
    return {
        coord.row + direction.row * distance,
        coord.col + direction.col * distance,
    };
}

std::vector<Coord> destinationsFor(
    const AmazonsGame::Board& board,
    Coord from
) {
    std::vector<Coord> result;
    if (!inBounds(from) || board[indexOf(from)] == Empty ||
        board[indexOf(from)] == Arrow) {
        return result;
    }

    result.reserve(28);
    for (const Coord direction : kDirections) {
        for (int distance = 1; distance < kBoardSize; ++distance) {
            const Coord target = step(from, direction, distance);
            if (!inBounds(target) || board[indexOf(target)] != Empty) {
                break;
            }
            result.push_back(target);
        }
    }
    return result;
}

std::vector<Coord> arrowsFor(
    AmazonsGame::Board board,
    Coord from,
    Coord to
) {
    std::vector<Coord> result;
    if (!inBounds(from) || !inBounds(to) || board[indexOf(to)] != Empty) {
        return result;
    }

    const int piece = board[indexOf(from)];
    if (piece != Black && piece != White) {
        return result;
    }

    board[indexOf(from)] = Empty;
    board[indexOf(to)] = piece;
    return destinationsFor(board, to);
}

bool containsCoord(const std::vector<Coord>& coords, Coord wanted) {
    return std::find(coords.begin(), coords.end(), wanted) != coords.end();
}

AmazonsGame::Board applyUnchecked(
    AmazonsGame::Board board,
    const Move& move,
    int player
) {
    board[indexOf(move.from)] = Empty;
    board[indexOf(move.to)] = player;
    board[indexOf(move.arrow)] = Arrow;
    return board;
}

std::vector<Move> generateMoves(
    const AmazonsGame::Board& board,
    int player
) {
    std::vector<Move> result;
    result.reserve(1800);

    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const Coord from{row, col};
            if (board[indexOf(from)] != player) {
                continue;
            }
            for (const Coord to : destinationsFor(board, from)) {
                for (const Coord arrow : arrowsFor(board, from, to)) {
                    result.push_back({from, to, arrow});
                }
            }
        }
    }
    return result;
}

int queenMobility(const AmazonsGame::Board& board, int player) {
    int mobility = 0;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const Coord coord{row, col};
            if (board[indexOf(coord)] == player) {
                mobility += static_cast<int>(destinationsFor(board, coord).size());
            }
        }
    }
    return mobility;
}

std::array<int, kBoardSize * kBoardSize> queenDistances(
    const AmazonsGame::Board& board,
    int player
) {
    std::array<int, kBoardSize * kBoardSize> distances{};
    distances.fill(kUnreachable);
    std::queue<Coord> frontier;

    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const Coord coord{row, col};
            if (board[indexOf(coord)] == player) {
                distances[indexOf(coord)] = 0;
                frontier.push(coord);
            }
        }
    }

    while (!frontier.empty()) {
        const Coord current = frontier.front();
        frontier.pop();
        const int nextDistance = distances[indexOf(current)] + 1;

        for (const Coord direction : kDirections) {
            for (int distance = 1; distance < kBoardSize; ++distance) {
                const Coord target = step(current, direction, distance);
                if (!inBounds(target) || board[indexOf(target)] != Empty) {
                    break;
                }
                if (nextDistance < distances[indexOf(target)]) {
                    distances[indexOf(target)] = nextDistance;
                    frontier.push(target);
                }
            }
        }
    }
    return distances;
}

double evaluateBoard(const AmazonsGame::Board& board, int perspective) {
    const int opponent = AmazonsGame::opponentOf(perspective);
    const int ownMobility = queenMobility(board, perspective);
    const int opponentMobility = queenMobility(board, opponent);

    if (ownMobility == 0) {
        return -100000.0;
    }
    if (opponentMobility == 0) {
        return 100000.0;
    }

    const auto ownDistance = queenDistances(board, perspective);
    const auto opponentDistance = queenDistances(board, opponent);
    double territory = 0.0;
    double reach = 0.0;

    for (int index = 0; index < static_cast<int>(board.size()); ++index) {
        if (board[index] != Empty) {
            continue;
        }

        const int own = ownDistance[index];
        const int theirs = opponentDistance[index];
        if (own < theirs) {
            territory += 1.0;
        } else if (theirs < own) {
            territory -= 1.0;
        }

        if (own < kUnreachable) {
            reach += 1.0 / (1.0 + own);
        }
        if (theirs < kUnreachable) {
            reach -= 1.0 / (1.0 + theirs);
        }
    }

    return territory * 15.0 + reach * 2.5 +
           static_cast<double>(ownMobility - opponentMobility) * 0.45;
}

}  // namespace

bool Coord::valid() const {
    return inBounds(*this);
}

bool Coord::operator==(const Coord& other) const {
    return row == other.row && col == other.col;
}

bool Move::operator==(const Move& other) const {
    return from == other.from && to == other.to && arrow == other.arrow;
}

AmazonsGame::AmazonsGame() {
    reset();
}

void AmazonsGame::reset() {
    board_.fill(Empty);

    for (const Coord coord : std::array<Coord, 4>{{
             {0, 2}, {2, 0}, {5, 0}, {7, 2},
         }}) {
        board_[indexOf(coord)] = Black;
    }
    for (const Coord coord : std::array<Coord, 4>{{
             {0, 5}, {2, 7}, {5, 7}, {7, 5},
         }}) {
        board_[indexOf(coord)] = White;
    }

    currentPlayer_ = Black;
    history_.clear();
}

const AmazonsGame::Board& AmazonsGame::board() const {
    return board_;
}

int AmazonsGame::at(Coord coord) const {
    return coord.valid() ? board_[indexOf(coord)] : Arrow;
}

int AmazonsGame::currentPlayer() const {
    return currentPlayer_;
}

int AmazonsGame::moveCount() const {
    return static_cast<int>(history_.size());
}

bool AmazonsGame::canUndo() const {
    return !history_.empty();
}

int AmazonsGame::winner() const {
    if (!generateMoves(board_, currentPlayer_).empty()) {
        return Empty;
    }
    return opponentOf(currentPlayer_);
}

std::vector<Coord> AmazonsGame::legalDestinations(Coord from) const {
    if (at(from) != currentPlayer_) {
        return {};
    }
    return destinationsFor(board_, from);
}

std::vector<Coord> AmazonsGame::legalArrows(Coord from, Coord to) const {
    if (at(from) != currentPlayer_ ||
        !containsCoord(destinationsFor(board_, from), to)) {
        return {};
    }
    return arrowsFor(board_, from, to);
}

std::vector<Move> AmazonsGame::legalMoves(int player) const {
    return generateMoves(board_, player);
}

bool AmazonsGame::isLegal(const Move& move) const {
    if (at(move.from) != currentPlayer_) {
        return false;
    }
    const auto destinations = destinationsFor(board_, move.from);
    if (!containsCoord(destinations, move.to)) {
        return false;
    }
    return containsCoord(arrowsFor(board_, move.from, move.to), move.arrow);
}

bool AmazonsGame::playMove(const Move& move) {
    if (!isLegal(move)) {
        return false;
    }

    history_.push_back({board_, currentPlayer_});
    board_ = applyUnchecked(board_, move, currentPlayer_);
    currentPlayer_ = opponentOf(currentPlayer_);
    return true;
}

bool AmazonsGame::undoPlies(int count) {
    if (history_.empty() || count < 1) {
        return false;
    }

    count = std::min(count, static_cast<int>(history_.size()));
    while (count-- > 0) {
        board_ = history_.back().board;
        currentPlayer_ = history_.back().currentPlayer;
        history_.pop_back();
    }
    return true;
}

std::optional<Move> AmazonsGame::chooseMove(
    Board board,
    int player,
    int timeLimitMs
) {
    auto moves = generateMoves(board, player);
    if (moves.empty()) {
        return std::nullopt;
    }
    if (moves.size() == 1) {
        return moves.front();
    }

    thread_local std::mt19937 randomEngine(
        static_cast<unsigned int>(
            std::chrono::steady_clock::now().time_since_epoch().count()
        )
    );
    std::shuffle(moves.begin(), moves.end(), randomEngine);

    struct Candidate {
        Move move;
        Board boardAfterMove;
        double staticScore = -std::numeric_limits<double>::infinity();
        double replyScore = -std::numeric_limits<double>::infinity();
    };

    std::vector<Candidate> candidates;
    candidates.reserve(moves.size());
    for (const Move& move : moves) {
        Board next = applyUnchecked(board, move, player);
        candidates.push_back({move, next, evaluateBoard(next, player)});
    }

    constexpr std::size_t kCandidateLimit = 40;
    if (candidates.size() > kCandidateLimit) {
        std::nth_element(
            candidates.begin(),
            candidates.begin() + static_cast<std::ptrdiff_t>(kCandidateLimit),
            candidates.end(),
            [](const Candidate& left, const Candidate& right) {
                return left.staticScore > right.staticScore;
            }
        );
        candidates.resize(kCandidateLimit);
    }
    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const Candidate& left, const Candidate& right) {
            return left.staticScore > right.staticScore;
        }
    );

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(std::max(80, timeLimitMs));
    const int opponent = opponentOf(player);
    const int replySamples = timeLimitMs >= 1200 ? 96 :
                             (timeLimitMs >= 600 ? 56 : 28);

    for (Candidate& candidate : candidates) {
        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }

        auto replies = generateMoves(candidate.boardAfterMove, opponent);
        if (replies.empty()) {
            return candidate.move;
        }
        std::shuffle(replies.begin(), replies.end(), randomEngine);

        double worstReply = std::numeric_limits<double>::infinity();
        const int sampleCount = std::min(
            replySamples,
            static_cast<int>(replies.size())
        );
        for (int sample = 0; sample < sampleCount; ++sample) {
            if (std::chrono::steady_clock::now() >= deadline) {
                break;
            }
            const Board replyBoard = applyUnchecked(
                candidate.boardAfterMove,
                replies[sample],
                opponent
            );
            worstReply = std::min(worstReply, evaluateBoard(replyBoard, player));
        }

        if (std::isfinite(worstReply)) {
            candidate.replyScore = candidate.staticScore * 0.58 +
                                   worstReply * 0.42;
        } else {
            candidate.replyScore = candidate.staticScore;
        }
    }

    return std::max_element(
               candidates.begin(),
               candidates.end(),
               [](const Candidate& left, const Candidate& right) {
                   const double leftScore = std::isfinite(left.replyScore)
                                                ? left.replyScore
                                                : left.staticScore;
                   const double rightScore = std::isfinite(right.replyScore)
                                                 ? right.replyScore
                                                 : right.staticScore;
                   return leftScore < rightScore;
               }
           )->move;
}

std::string AmazonsGame::formatMove(const Move& move) {
    std::ostringstream output;
    output << '(' << move.from.row << ',' << move.from.col << ") → ("
           << move.to.row << ',' << move.to.col << ") · 箭 ("
           << move.arrow.row << ',' << move.arrow.col << ')';
    return output.str();
}

int AmazonsGame::opponentOf(int player) {
    return player == Black ? White : Black;
}

}  // namespace zzmazon
