#ifndef EVAL_H
#define EVAL_H
#include "../board/board.h"
#include "../core/types.h"
#include <atomic>
#include <vector>

namespace Prometheus {

namespace Eval {

// Tunable Parameters
// Material and Phase moved to PST
// extern int Material[PIECE_TYPE_NB][2];
// extern int PhaseWeights[PIECE_TYPE_NB];
extern int SafetyTable[100];
extern int MobilityBonus[PIECE_TYPE_NB];

// Tunable Evaluation Weights
extern int BishopPairMG, BishopPairEG;
extern int OutpostBonus, PermanentOutpostBonus;
extern int HangingPiecePenalty[PIECE_TYPE_NB];
extern int HarassedByPawnPenalty;
extern int OpenFileBonus, SemiOpenFileBonus;
extern int SpaceSquareBonus;

// Simple Material Values (centipawns)
constexpr int VALUE_PAWN = 100;
constexpr int VALUE_KNIGHT = 320;
constexpr int VALUE_BISHOP = 330;
constexpr int VALUE_ROOK = 500;
constexpr int VALUE_QUEEN = 900;
constexpr int VALUE_KING = 20000;

// Eval Cache Entry
struct EvalEntry {
  int16_t score;
  uint16_t pad; // Alignment
  std::atomic<uint64_t>
      key; // Key logic: Write score, then Key. Read Key, then Score.
};

class EvalTT {
public:
  EvalTT(size_t size_mb = 4);
  ~EvalTT();

  void resize(size_t size_mb);
  void clear();

  // Probe: If key matches, return true and score.
  bool probe(uint64_t key, int &score) const;

  // Save: Store score for key.
  void save(uint64_t key, int score);

private:
  EvalEntry *table;
  size_t size;
};

extern EvalTT EvalCache;

// Main Evaluation
int evaluate(const Board &board);

} // namespace Eval

} // namespace Prometheus

#endif // EVAL_H
