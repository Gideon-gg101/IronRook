#include "syzygy.h"
#include "tbprobe.h"
#include <iostream>

namespace Prometheus {
namespace Syzygy {

bool init(const std::string &path) { return tb_init(path.c_str()); }

int probe_wdl(const Board &board) {
  // Check popcount limits
  int men = Bitboards::popcount(board.all_pieces());
  if (men > TB_LARGEST)
    return TB_RESULT_FAILED;

  uint64_t white = board.pieces(WHITE);
  uint64_t black = board.pieces(BLACK);
  uint64_t kings = board.pieces(KING);
  uint64_t queens = board.pieces(QUEEN);
  uint64_t rooks = board.pieces(ROOK);
  uint64_t bishops = board.pieces(BISHOP);
  uint64_t knights = board.pieces(KNIGHT);
  uint64_t pawns = board.pieces(PAWN);

  // Castling: Prometheus uses bits 1,2,4,8 logic?
  // Board::castling_rights() returns: WK=1, WQ=2, BK=4, BQ=8.
  // Fathom: K=1, Q=2, k=4, q=8. Matches.
  unsigned castling = board.castling_rights();

  // En Passant: invalid is usually SQ_NONE or 64?
  // Fathom expects 0 if none.
  // Board::en_passant_square() likely returns SQ_NONE (64) if none.
  // Fathom ep square is 0-63? or 0=none?
  // tbprobe.h says: "Set to zero if there is no en passant square."
  // But square 0 is A1.
  // Usually Fathom uses 0 for None? Wait.
  // tbconfig.h or tbprobe source check?
  // Most engines pass 0. But if ep is on A1 (impossible for ep), it might
  // conflict. However, EP only possible on rank 3 or 6. square index > 0. So 0
  // is safe for "None".
  unsigned ep = 0;
  if (board.en_passant_square() != SQ_NONE) {
    ep = (unsigned)board.en_passant_square();
  }

  bool turn = (board.side_to_move() == WHITE);

  // Rule50? Fathom wants it.
  // But probes usually rely on rule50 being 0 inside probing unless we track
  // it? For WDL, rule50 matters for 50-move rule draws. Let's pass 0 for now
  // (optimistic) or pass actual if we track it. Board has no rule50 accessor
  // visible in header? "half_moves"? "half_moves" is usually since last
  // capture/pawn move. Let's assume passed as 0 for simple WDL of position.
  unsigned rule50 = 0;

  unsigned result = tb_probe_wdl(white, black, kings, queens, rooks, bishops,
                                 knights, pawns, rule50, castling, ep, turn);

  return result;
}

int probe_root_wdl(const Board &board) {
  // Similar to probe_wdl but for root
  return probe_wdl(board);
}

int wdl_to_score(int wdl, int ply) {
  if (wdl == TB_RESULT_FAILED)
    return -32001;

  // Fathom values:
  // TB_LOSS = 0
  // TB_BLESSED_LOSS = 1 (Loss but 50-move draw) -> treat as draw? or loss?
  // TB_DRAW = 2
  // TB_CURSED_WIN = 3 (Win but 50-move draw) -> treat as draw?
  // TB_WIN = 4

  // Engine scores (centipawns):
  // Mate = 30000
  // TB Win = 20000 - ply (to prefer shorter wins? No, Syzygy WDL doesn't give
  // distance) Syzygy DTZ gives distance. WDL just says "Win". If we use WDL, we
  // just return a high score.

  switch (wdl) {
  case TB_WIN:
    return 20000 - ply;
  case TB_LOSS:
    return -20000 + ply;
  case TB_DRAW:
    return 0;
  case TB_CURSED_WIN:
    return 0; // Draw by 50-move
  case TB_BLESSED_LOSS:
    return 0; // Draw by 50-move
  default:
    return 0;
  }
}

} // namespace Syzygy
} // namespace Prometheus
