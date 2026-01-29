#ifndef ENDGAME_H
#define ENDGAME_H

#include "../board/board.h"
#include "../core/types.h"

namespace Prometheus {
namespace Eval {

// Endgame Scaling Factor (0-64, where 64 = normal, 0 = dead draw)
struct EndgameScale {
  int factor; // 0..64
};

// Endgame Evaluation Function
// Returns a ScorePair or modifier to the main evaluation?
// Best to return a scale factor and an additional score bonus.
struct EndgameScore {
  int score_bonus;
  int scale_factor; // 0 to 64
};

EndgameScore evaluate_endgame(const Board &board, int score);

} // namespace Eval
} // namespace Prometheus

#endif // ENDGAME_H
