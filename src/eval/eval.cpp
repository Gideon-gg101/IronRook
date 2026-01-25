#include "eval.h"
#include "../core/magic.h"
#include "endgame.h"
#include "pawn_eval.h"
#include "pst.h"

namespace Prometheus {

namespace Eval {

// Material Values (PeSTO-compatible)
// [PieceType][MG/EG]
// Material Values (PeSTO-compatible)
// [PieceType][MG/EG]
// Material and Phase are now in pst.h

// EvalTT Implementation
EvalTT EvalCache;

EvalTT::EvalTT(size_t size_mb) : table(nullptr), size(0) { resize(size_mb); }

EvalTT::~EvalTT() {
  if (table)
    delete[] table;
}

void EvalTT::resize(size_t size_mb) {
  if (table)
    delete[] table;
  size_t count = (size_mb * 1024 * 1024) / sizeof(EvalEntry);
  table = new EvalEntry[count];
  size = count;
  clear();
}

void EvalTT::clear() {
  for (size_t i = 0; i < size; ++i) {
    table[i].key.store(0, std::memory_order_relaxed);
    table[i].score = 0;
  }
}

bool EvalTT::probe(uint64_t key, int &score) const {
  if (size == 0)
    return false;
  size_t idx = key % size;
  uint64_t entry_key = table[idx].key.load(std::memory_order_acquire);
  if (entry_key == key) {
    score = table[idx].score;
    return true;
  }
  return false;
}

void EvalTT::save(uint64_t key, int score) {
  if (size == 0)
    return;
  size_t idx = key % size;
  table[idx].score = (int16_t)score;
  table[idx].key.store(key, std::memory_order_release);
}

// King Safety Table: Quadratic penalty for attack units
// Index: Attack Units. Value: Penalty (positive integer, to be subtracted)
int SafetyTable[100] = {
    0,   0,   1,   2,   3,   5,   7,    9,    12,   15,   18,   22,   26,
    30,  35,  39,  44,  50,  56,  62,   68,   75,   82,   89,   96,   104,
    112, 120, 129, 137, 146, 155, 165,  174,  184,  194,  204,  215,  226,
    237, 248, 260, 272, 285, 298, 311,  324,  338,  352,  367,  382,  397,
    412, 427, 442, 458, 474, 490, 506,  523,  540,  557,  574,  591,  609,
    627, 645, 663, 682, 701, 720, 739,  759,  779,  799,  819,  839,  860,
    881, 902, 923, 944, 965, 987, 1009, 1031, 1053, 1075, 1098, 1121,
};

// Mobility Bonus (Simple: per safe square)
int MobilityBonus[PIECE_TYPE_NB] = {0, 0, 4, 3, 2, 1, 0, 0};

// Tunable Evaluation Weights
int BishopPairMG = 20;
int BishopPairEG = 40;
int OutpostBonus = 20;
int PermanentOutpostBonus = 25;
int HangingPiecePenalty[PIECE_TYPE_NB] = {0, 60, 350, 350, 550, 1000, 0, 0};
int HarassedByPawnPenalty = 50;
int OpenFileBonus = 15;
int SemiOpenFileBonus = 8;
int SpaceSquareBonus = 5;

// Singular Extensions Parameters
int SingularMarginMultiplier = 2; // depth * 2 centipawns
int SingularMinDepth = 8;         // minimum depth for SE

// Lazy Evaluation & Noise Control Parameters
int LazyEvalMargin = 150;  // Margin for lazy eval (in centipawns)
int MaxNonMateEval = 2000; // Max eval for non-mate positions (+/- 20 pawns)

// LMR (Late Move Reductions) Parameters
int LMRBaseReduction = 75;    // Base: 0.75 (×100)
int LMRDepthDivisor = 225;    // Divisor: 2.25 (×100)
int LMRHistoryDivisor = 2000; // History scaling
int LMRPVReduction = 2;       // PV reduction decrease
int LMRImprovingBonus = 1;    // Not improving penalty

// Hysteresis Parameter (Eval Noise Phase 4)
int EvalHysteresis =
    5; // Small deterministic noise (0-10 cp) to smooth eval transitions

// Internal Iterative Deepening (IID) Parameters
int IIDMinDepthPV = 6;     // Min depth for IID on PV nodes
int IIDMinDepthNonPV = 8;  // Min depth for IID on non-PV nodes
int IIDReductionPV = 2;    // Depth reduction for PV IID
int IIDReductionNonPV = 3; // Depth reduction for non-PV IID

// Helper: Get squares attacked by pawns of a given color
Bitboard attacked_by_pawns(const Board &board, Color side) {
  Bitboard pawns = board.pieces(PAWN, side);
  if (side == WHITE) {
    // White attacks NorthWest (+7) and NorthEast (+9)
    // Mask File H for +7 (avoid A->H wrap? No, A->H is +7? 8->15. Yes. Mask H)
    // Mask File A for +9 (avoid H->A wrap? 15->24. Yes)
    // Note: My previous comment analysis was slightly confused but code logic
    // held. Let's rely on standard: (p << 9) & ~FileA (p << 7) & ~FileH
    return ((pawns << 9) & 0xFEFEFEFEFEFEFEFEULL) |
           ((pawns << 7) & 0x7F7F7F7F7F7F7F7FULL);
  } else {
    // Black attacks SouthEast (-7) and SouthWest (-9)
    // (p >> 9) & ~FileH
    // (p >> 7) & ~FileA
    return ((pawns >> 9) & 0x7F7F7F7F7F7F7F7FULL) |
           ((pawns >> 7) & 0xFEFEFEFEFEFEFEFEULL);
  }
}

int evaluate_space(const Board &board, Color side) {
  // Only evaluate if we have enough material (non-pawn material > ~6000?)
  // Simplified: Just calculate.

  Bitboard space_mask;
  Bitboard center_files = 0x3C3C3C3C3C3C3C3CULL; // Files C,D,E,F

  // Safe: Not attacked by enemy pawns
  Bitboard unsafe = attacked_by_pawns(board, ~side);
  Bitboard all_pawns = board.pieces(PAWN, WHITE) | board.pieces(PAWN, BLACK);

  if (side == WHITE) {
    // Rank 2-4 (Black camp central) - wait, taking space means controlling
    // their side? Usually Space is defined as squares on OUR side of the board
    // (rank 2,3,4) that are safe? Or controlling the center? Stockfish
    // definition: Space = squares in camps (Rank 2-4 for White relative) that
    // are not attacked by enemy pawns and are supported. Let's stick to
    // "Central ranks 2-4 for side".

    // Let's count squares in Rank 2, 3, 4 (0-based) for White.
    // Ranks 2,3,4 = (0xFF << 16) | (0xFF << 24) | (0xFF << 32)
    space_mask =
        center_files & ((0xFFULL << 16) | (0xFFULL << 24) | (0xFFULL << 32));

  } else {
    // Black Relative Rank 2-4: Ranks 5,4,3 (from top)?
    // Ranks 5,4,3 = (0xFF << 40) | (0xFF << 32) | (0xFF << 24)
    space_mask =
        center_files & ((0xFFULL << 40) | (0xFFULL << 32) | (0xFFULL << 24));
  }

  Bitboard safe = space_mask & ~unsafe & ~all_pawns;
  return Bitboards::popcount(safe) * 5; // 5 cp per square?
}

int evaluate_outposts(const Board &board, Color side) {
  int score = 0;
  Color enemy = ~side;
  Bitboard knights = board.pieces(KNIGHT, side);
  Bitboard my_pawns = board.pieces(PAWN, side);
  Bitboard enemy_pawns = board.pieces(PAWN, enemy);

  // Outpost ranks: 4, 5, 6 for White; 3, 4, 5 for Black
  Bitboard outpost_mask = (side == WHITE)
                              ? (0xFFULL << 24 | 0xFFULL << 32 | 0xFFULL << 40)
                              : (0xFFULL << 32 | 0xFFULL << 24 | 0xFFULL << 16);

  while (knights) {
    Square s = Bitboards::pop_lsb(knights);
    if (!(Bitboards::square_bb(s) & outpost_mask))
      continue;

    Bitboard pawn_support = 0;
    if (side == WHITE) {
      pawn_support =
          ((Bitboards::square_bb(s) >> 9) & ~0x8080808080808080ULL & my_pawns) |
          ((Bitboards::square_bb(s) >> 7) & ~0x0101010101010101ULL & my_pawns);
    } else {
      pawn_support =
          ((Bitboards::square_bb(s) << 9) & ~0x0101010101010101ULL & my_pawns) |
          ((Bitboards::square_bb(s) << 7) & ~0x8080808080808080ULL & my_pawns);
    }

    if (pawn_support) {
      int bonus = 20;
      int f = s % 8;
      Bitboard adjacent_files = 0;
      if (f > 0)
        adjacent_files |= (0x0101010101010101ULL << (f - 1));
      if (f < 7)
        adjacent_files |= (0x0101010101010101ULL << (f + 1));

      Bitboard unreachable_mask = 0;
      int r = s / 8;
      if (side == WHITE) {
        for (int rank = r + 1; rank < 8; ++rank)
          unreachable_mask |= (0xFFULL << (rank * 8));
      } else {
        for (int rank = r - 1; rank >= 0; --rank)
          unreachable_mask |= (0xFFULL << (rank * 8));
      }

      if (!(enemy_pawns & adjacent_files & unreachable_mask)) {
        bonus += 25; // Permanent Outpost
      }
      score += bonus;
    }
  }
  return score;
}

// Threats: Undefended pieces under attack
int evaluate_threats(const Board &board, Color side) {
  int penalty = 0;
  Color enemy = ~side;

  for (int pt = PAWN; pt <= QUEEN; ++pt) {
    Bitboard p = board.pieces((PieceType)pt, side);
    while (p) {
      Square s = Bitboards::pop_lsb(p);

      if (board.is_square_attacked(s, enemy)) {
        bool defended = board.is_square_attacked(s, side);

        if (!defended) {
          // Hanging Piece Penalty
          switch (pt) {
          case PAWN:
            penalty += 60;
            break;
          case KNIGHT:
            penalty += 350;
            break;
          case BISHOP:
            penalty += 350;
            break;
          case ROOK:
            penalty += 550;
            break;
          case QUEEN:
            penalty += 1000;
            break;
          }
        } else {
          // Attacked but defended. Check for "Tempo/Weakness"
          // E.g. minor piece attacked by a pawn.
          Bitboard enemy_pawns = board.pieces(PAWN, enemy);
          Bitboard pawn_attacks =
              (enemy == WHITE) ? (((enemy_pawns << 9) & 0xFEFEFEFEFEFEFEFEULL) |
                                  ((enemy_pawns << 7) & 0x7F7F7F7F7F7F7F7FULL))
                               : (((enemy_pawns >> 9) & 0x7F7F7F7F7F7F7F7FULL) |
                                  ((enemy_pawns >> 7) & 0xFEFEFEFEFEFEFEFEULL));

          if (pt > PAWN && (Bitboards::square_bb(s) & pawn_attacks)) {
            penalty += 50; // Harassed by pawn
          }
        }
      }
    }
  }
  return penalty;
}

int evaluate_mobility(const Board &board, Color side) {
  int mobility_score = 0;
  Bitboard my_pieces = board.pieces(side);
  Bitboard enemy_pieces = board.pieces(~side);
  Bitboard occupied = my_pieces | enemy_pieces;

  // Safety mask: Squares not attacked by enemy PAWNS (simplified safety)
  // Ideally we avoid all attacks, but pawn attacks are the most dangerous for
  // valuable pieces.
  Bitboard unsafe = attacked_by_pawns(board, ~side);

  Bitboard valid_destinations =
      ~my_pieces & ~unsafe; // Can move to empty or capture enemy, but not to
                            // pawn-attacked squares.

  // Knights
  Bitboard knights = board.pieces(KNIGHT, side);
  while (knights) {
    Square s = Bitboards::pop_lsb(knights);
    Bitboard att = Magic::get_knight_attacks(s);
    mobility_score +=
        Bitboards::popcount(att & valid_destinations) * MobilityBonus[KNIGHT];
  }

  // Bishops
  Bitboard bishops = board.pieces(BISHOP, side);
  while (bishops) {
    Square s = Bitboards::pop_lsb(bishops);
    Bitboard att = Magic::get_bishop_attacks(s, occupied);
    mobility_score +=
        Bitboards::popcount(att & valid_destinations) * MobilityBonus[BISHOP];
  }

  // Rooks
  Bitboard rooks = board.pieces(ROOK, side);
  while (rooks) {
    Square s = Bitboards::pop_lsb(rooks);
    Bitboard att = Magic::get_rook_attacks(s, occupied);
    Bitboard moves = att & valid_destinations;
    mobility_score += Bitboards::popcount(moves) * MobilityBonus[ROOK];

    // File-based bonuses
    int f = s % 8;
    Bitboard f_mask = 0x0101010101010101ULL << f;
    Bitboard my_pawns = board.pieces(PAWN, side);
    Bitboard enemy_pawns = board.pieces(PAWN, ~side);

    if (!(my_pawns & f_mask)) {
      if (!(enemy_pawns & f_mask))
        mobility_score += 15; // Open file
      else
        mobility_score += 8; // Semi-open file
    }
  }

  // Queens
  Bitboard queens = board.pieces(QUEEN, side);
  while (queens) {
    Square s = Bitboards::pop_lsb(queens);
    Bitboard att = Magic::get_queen_attacks(s, occupied);
    mobility_score +=
        Bitboards::popcount(att & valid_destinations) * MobilityBonus[QUEEN];
  }

  return mobility_score;
}

int evaluate_king_safety(const Board &board, Color side) {
  Bitboard k = board.pieces(KING, side);
  if (k == 0)
    return 0;
  Square k_sq = Bitboards::lsb(k);

  // G+ 2.5: King Ring (Inner 3x3)
  Bitboard inner_ring = Magic::get_king_attacks(k_sq);

  Color enemy = ~side;
  Bitboard occ = board.all_pieces();

  int attack_units = 0;
  int attackers_count = 0;

  auto add_threat = [&](Bitboard att, int weight_unit, int weight_check) {
    Bitboard hits = att & inner_ring;
    if (hits) {
      attack_units += weight_unit * Bitboards::popcount(hits) + weight_check;
      attackers_count++;
    }
  };

  Bitboard enemy_rooks = board.pieces(ROOK, enemy);
  Bitboard enemy_queens = board.pieces(QUEEN, enemy);
  Bitboard enemy_bishops = board.pieces(BISHOP, enemy);
  Bitboard enemy_knights = board.pieces(KNIGHT, enemy);

  // Scan enemy pieces
  Bitboard knights = enemy_knights;
  while (knights) {
    add_threat(Magic::get_knight_attacks(Bitboards::pop_lsb(knights)), 3, 2);
  }

  Bitboard bishops = enemy_bishops;
  while (bishops) {
    add_threat(Magic::get_bishop_attacks(Bitboards::pop_lsb(bishops), occ), 3,
               2);
  }

  Bitboard rooks = enemy_rooks;
  while (rooks) {
    add_threat(Magic::get_rook_attacks(Bitboards::pop_lsb(rooks), occ), 4, 3);
  }

  Bitboard queens = enemy_queens;
  while (queens) {
    add_threat(Magic::get_queen_attacks(Bitboards::pop_lsb(queens), occ), 6, 5);
  }

  // G+ 2.5: File Alignment Penalties (X-ray threats)
  int k_file = k_sq % 8;
  int k_rank = k_sq / 8;
  Bitboard file_mask = 0x0101010101010101ULL << k_file;
  Bitboard rank_mask = 0xFFULL << (k_rank * 8);

  // Penalize enemy sliders on same file/rank even if blocked
  if (enemy_rooks & file_mask)
    attack_units += 10;
  if (enemy_queens & file_mask)
    attack_units += 15;
  if (enemy_rooks & rank_mask)
    attack_units += 5;
  if (enemy_queens & rank_mask)
    attack_units += 10;

  // Open File Penalty
  int structural_penalty = 0;
  for (int f = std::max(0, k_file - 1); f <= std::min(7, k_file + 1); ++f) {
    Bitboard f_mask = 0x0101010101010101ULL << f;
    if (!(board.pieces(PAWN, side) & f_mask)) {
      structural_penalty += 15;
      if (!(board.pieces(PAWN, enemy) & f_mask)) {
        structural_penalty += 20;
      }
    }
  }

  // Shield Pawns
  Bitboard shield = inner_ring & board.pieces(PAWN, side);
  structural_penalty -= Bitboards::popcount(shield) * 12;

  if (structural_penalty < 0)
    structural_penalty = 0;

  if (attackers_count < 2 && structural_penalty < 30)
    return structural_penalty;

  if (attack_units > 99)
    attack_units = 99;
  return SafetyTable[attack_units] + structural_penalty;
}

int evaluate_pieces(const Board &board, Color side) {
  int score = 0;
  Bitboard rooks = board.pieces(ROOK, side);
  Bitboard queens = board.pieces(QUEEN, side);

  // Rook on 7th (Relative)
  int rank7 = (side == WHITE) ? 6 : 1;
  Bitboard r7 = rooks & (0xFFULL << (rank7 * 8));
  if (r7) {
    // Bonus only if opponent king is on relative rank 8 or 7 (pinned)
    Bitboard k = board.pieces(KING, ~side);
    int rank8 = (side == WHITE) ? 7 : 0;
    if (k & ((0xFFULL << (rank8 * 8)) | (0xFFULL << (rank7 * 8)))) {
      score += Bitboards::popcount(r7) * 20;
    }
  }

  // Connected Rooks
  if (Bitboards::popcount(rooks) > 1) {
    // Simple check: do they attack each other?
    // We iterate rooks.
    Bitboard temp = rooks;
    while (temp) {
      Square s = Bitboards::pop_lsb(temp);
      // If this rook attacks another rook
      if (Magic::get_rook_attacks(s, board.all_pieces()) & rooks) {
        score += 10;
        // Don't double count excessively (simplification: 10 per connected
        // rook)
      }
    }
  }

  return score;
}

int evaluate(const Board &board, int alpha, int beta, int depth) {
  int cached;
  if (EvalCache.probe(board.key, cached)) {
    return (board.side_to_move() == WHITE) ? cached : -cached;
  }

  int mg_score = board.mg_value;
  int eg_score = board.eg_value;
  int phase = board.phase_value;

  // === LAZY EVALUATION ===
  // Quick tapered eval from material + PST only
  if (phase > PST::TotalPhaseMax)
    phase = PST::TotalPhaseMax;

  int lazy_score =
      (mg_score * phase + eg_score * (PST::TotalPhaseMax - phase)) /
      PST::TotalPhaseMax;

  // If score is far outside [alpha-margin, beta+margin], return early
  // This skips expensive evaluation (king safety, mobility, threats, etc.)
  if (lazy_score < alpha - LazyEvalMargin ||
      lazy_score > beta + LazyEvalMargin) {
    // Apply perspective and return
    int result = (board.side_to_move() == WHITE) ? lazy_score : -lazy_score;
    return result;
  }

  // === FULL EVALUATION (within window) ===

  // Pawn Structure
  ScorePair pawn_score = evaluate_pawns(board, GlobalPawnTable);
  mg_score += pawn_score.mg;
  eg_score += pawn_score.eg;

  // King Safety
  mg_score -= evaluate_king_safety(board, WHITE); // Penalty for White
  mg_score += evaluate_king_safety(board, BLACK);

  // Threats
  mg_score -= evaluate_threats(board, WHITE);
  mg_score += evaluate_threats(board, BLACK);

  // Mobility
  mg_score += evaluate_mobility(board, WHITE);
  mg_score -= evaluate_mobility(board, BLACK);

  // Space
  mg_score += evaluate_space(board, WHITE);
  mg_score -= evaluate_space(board, BLACK);

  // Piece Coordination
  mg_score += evaluate_pieces(board, WHITE);
  mg_score -= evaluate_pieces(board, BLACK);

  // Outposts
  mg_score += evaluate_outposts(board, WHITE);
  mg_score -= evaluate_outposts(board, BLACK);

  // Bishop Pair
  if (Bitboards::popcount(board.pieces(BISHOP, WHITE)) >= 2) {
    mg_score += BishopPairMG;
    eg_score += BishopPairEG;
  }
  if (Bitboards::popcount(board.pieces(BISHOP, BLACK)) >= 2) {
    mg_score -= BishopPairMG;
    eg_score -= BishopPairEG;
  }

  // G+ 2.6: King Activity & Box-In (Endgame Only)
  auto king_activity = [&](Color side) {
    Square k = Bitboards::lsb(board.pieces(KING, side));
    int r = k / 8;
    int c = k % 8;
    // Center dist: 0..7
    int dist = std::max(std::abs(r * 2 - 7), std::abs(c * 2 - 7));
    int activity = (7 - dist) * 15; // Increased bonus to 105 max

    // Box-in penalty (edge of board in endgame is risky)
    if (r == 0 || r == 7 || c == 0 || c == 7)
      activity -= 20;

    return activity;
  };

  eg_score += king_activity(WHITE);
  eg_score -= king_activity(BLACK);

  if (phase > PST::TotalPhaseMax)
    phase = PST::TotalPhaseMax; // Safety

  // Tapered Eval
  // Phase 0 = Endgame, 24 = Midgame
  int score = (mg_score * phase + eg_score * (PST::TotalPhaseMax - phase)) /
              PST::TotalPhaseMax;

  // Endgame Knowledge
  EndgameScore es = evaluate_endgame(board, score);

  score += es.score_bonus;
  score = (score * es.scale_factor) / 64;

  // === EVAL CLAMPING (Noise Control) ===
  // Clamp non-mate scores to prevent extreme outliers
  // This prevents pruning misfires from unstable eval
  constexpr int MATE_THRESHOLD = 29000; // Mate scores start at ±30000
  if (std::abs(score) < MATE_THRESHOLD) {
    score = std::max(-MaxNonMateEval, std::min(MaxNonMateEval, score));
  }

  // === DEPTH DAMPENING (Noise Control) ===
  // Reduce positional evaluation components at shallow depths
  // King safety and threats are less reliable with limited lookahead
  if (depth >= 0 && depth < 6) {
    // Separate material from positional components
    int material_score = (board.mg_value * phase +
                          board.eg_value * (PST::TotalPhaseMax - phase)) /
                         PST::TotalPhaseMax;
    int positional = score - material_score;

    // Dampen positional score: 50% at d=0, 60% at d=1, ... 100% at d=5+
    int dampen_factor = 50 + depth * 10; // Linear scaling
    positional = (positional * dampen_factor) / 100;

    score = material_score + positional;
  }

  // === EVAL HYSTERESIS (Noise Control Phase 4) ===
  // Add small deterministic variance to prevent rapid eval oscillation
  // Uses zobrist key for determinism (same position always gets same variance)
  if (EvalHysteresis > 0 && std::abs(score) < MATE_THRESHOLD) {
    int variance =
        (int)((board.key % (2 * EvalHysteresis + 1))) - EvalHysteresis;
    score += variance;
  }

  EvalCache.save(board.key, score);

  // Perspective
  return (board.side_to_move() == WHITE) ? score : -score;
}

} // namespace Eval

} // namespace Prometheus
