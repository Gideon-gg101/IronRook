#ifndef PAWN_EVAL_H
#define PAWN_EVAL_H

#include "../board/board.h"
#include "../core/types.h"


namespace IroonRook {

namespace Eval {

struct PawnEntry {
  uint64_t key;
  int score_mg;
  int score_eg;
  // We can cache bitboards for passers/weaknesses if needed later
};

// Simple Pawn Hash Table
class PawnTable {
public:
  PawnTable();
  ~PawnTable();

  void resize(size_t mb);
  void clear();

  PawnEntry *probe(uint64_t pawnKey);
  void save(uint64_t pawnKey, int mg, int eg);

private:
  PawnEntry *entries;
  size_t count;
};

// Global Pawn Table (singleton-like or global instance)
extern PawnTable GlobalPawnTable;

// Main Pawn Evaluation Function
// Returns score {mg, eg} encoded or just int score?
// Ideally returns score pair or adds to eval accumulator.
// For now, let's return a struct or pair.
struct ScorePair {
  int mg;
  int eg;
};

ScorePair evaluate_pawns(const Board &board, PawnTable &pt);

} // namespace Eval
} // namespace IroonRook

#endif // PAWN_EVAL_H
