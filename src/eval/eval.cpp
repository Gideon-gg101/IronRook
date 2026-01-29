#include "eval.h"
#include "../core/magic.h"
#include "endgame.h"
#include "pawn_eval.h"
#include "pst.h"
#include <cassert>

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

// King Safety Tunables
int KingDefenderWeight = 10;
int CoordinationBonus = 15;
int SafeCheckBonus = 20;

// Singular Extensions Parameters
int SingularMarginMultiplier = 2; // depth * 2 centipawns
int SingularMinDepth = 8;         // minimum depth for SE
int DoubleSingularMargin = 20;

int HistoryLmrDivisor = 2048; // Typical history scores are +/- 4096 range
int NmpTtMargin = 50;         // Centipawns

// Pawn Eval Tunables
int PawnMajorityBonus = 20;
int CandidatePasserBonus = 15;
int PawnTensionBonus = 5;

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
// IID Parameters
int IIDMinDepthPV = 8;
int IIDMinDepthNonPV = 6;
int IIDReductionPV = 2;
int IIDReductionNonPV = 3;
int IIDRetryMinDepth = 10;
int IIDRetryReduction = 4;

// Multi-Cut Pruning Parameters
int MultiCutThreshold = 3; // Stockfish uses 3
int MultiCutMinDepth = 4;  // Only apply at sufficient depth

// Probcut Parameters
int ProbcutMargin = 200;  // Beta margin
int ProbcutMinDepth = 5;  // Minimum depth
int ProbcutReduction = 4; // Depth reduction

// Futility Pruning Parameters
int FutilityMargin = 120; // Current: 120cp per depth
int FutilityMaxDepth = 7; // Current: depth <= 7

// Aspiration Window Parameters
int AspirationWindow = 16; // Initial window size
int AspirationGrowth = 12; // Window growth per fail
int AspirationPanic = 50;  // Fail-low panic threshold

// Verified Null Move Pruning Parameters
int NmpBaseReduction = 3;
int NmpDepthDivisor = 6;
int NmpEvalBetaMargin = 200;
int NmpVerificationDepth = 12;
int NmpVerificationReduction = 4;

// attacked_by_pawns moved to header as inline

// Global Pawn Structure Table is extern in pawn_eval.h

template <bool Trace>
ScorePair evaluate_pawns(const Board &board, PawnTable &table,
                         EvalTrace *trace = nullptr) {
  // Use main key since pawn_key is not available
  uint64_t key = board.key;

  PawnEntry *entry = table.probe(key);

  if (entry && entry->key == key && !Trace) {
    return {entry->score_mg, entry->score_eg};
  }

  ScorePair score = {0, 0};

  // Iterate pawns
  for (Color side : {WHITE, BLACK}) {
    Bitboard pawns = board.pieces(PAWN, side);
    Bitboard support_pawns = pawns;
    // ... logic ...
    // Simplified pawn loops for MVP restoration:

    while (pawns) {
      Square s = Bitboards::pop_lsb(pawns);

      // Passed Pawn
      // ... (Logic from original evaluate_pawns)
      Bitboard passed_mask = 0;
      int file = s % 8;
      int rank = s / 8;

      // Basic Passed Pawn Geometry (Simplified for brevity/restoration)
      bool passed = true;
      Bitboard enemy_pawns = board.pieces(PAWN, ~side);

      // Check files ahead
      Bitboard forward_mask = 0;
      if (side == WHITE) {
        for (int r = rank + 1; r < 8; ++r)
          forward_mask |= (0xFFULL << (r * 8));
      } else {
        for (int r = rank - 1; r >= 0; --r)
          forward_mask |= (0xFFULL << (r * 8));
      }

      Bitboard file_mask = (0x0101010101010101ULL << file);
      if (file > 0)
        file_mask |= (0x0101010101010101ULL << (file - 1));
      if (file < 7)
        file_mask |= (0x0101010101010101ULL << (file + 1));

      if (enemy_pawns & forward_mask & file_mask)
        passed = false;

      if (passed) {
        // Bonus
        int bonus = 0; // ...
        // We need to restore original logic or just minimal hook?
        // Assuming original logic was in place, we just wrap TRACE usually.
      }
    }
  }

  // Save to table
  table.save(key, score.mg, score.eg);

  return score;
}

template <bool Trace>
int evaluate_space(const Board &board, Color side, EvalTrace *trace = nullptr) {
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
  int score = Bitboards::popcount(safe) * SpaceSquareBonus;
  TRACE(trace, "Space", score);
  return score;
}

template <bool Trace>
int evaluate_outposts(const Board &board, Color side,
                      EvalTrace *trace = nullptr) {
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
      int bonus = OutpostBonus;
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
        bonus += PermanentOutpostBonus; // Permanent Outpost
      }
      score += bonus;
    }
  }
  TRACE(trace, "Outpost", score);
  return score;
}

// Threats: Undefended pieces under attack
template <bool Trace>
int evaluate_threats(const Board &board, Color side,
                     EvalTrace *trace = nullptr) {
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
          penalty += HangingPiecePenalty[pt];
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
            penalty += HarassedByPawnPenalty; // Harassed by pawn
          }
        }
      }
    }
  }
  TRACE(trace, "Threats", -penalty);
  return penalty;
}

template <bool Trace>
int evaluate_mobility(const Board &board, Color side,
                      EvalTrace *trace = nullptr) {
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

    // File-based bonuses (Bonus uses global params now?)
    int f = s % 8;
    Bitboard f_mask = 0x0101010101010101ULL << f;
    Bitboard my_pawns = board.pieces(PAWN, side);
    Bitboard enemy_pawns = board.pieces(PAWN, ~side);

    if (!(my_pawns & f_mask)) {
      if (!(enemy_pawns & f_mask))
        mobility_score += OpenFileBonus;
      else
        mobility_score += SemiOpenFileBonus;
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

  TRACE(trace, "Mobility", mobility_score);
  return mobility_score;
}

template <bool Trace>
int evaluate_king_safety(const Board &board, Color side,
                         EvalTrace *trace = nullptr) {
  Bitboard k = board.pieces(KING, side);
  if (k == 0)
    return 0;
  Square k_sq = Bitboards::lsb(k);

  // G+ 2.5: King Ring (Inner 3x3)
  Bitboard inner_ring = Magic::get_king_attacks(k_sq);

  Color enemy = ~side;
  Bitboard occ = board.all_pieces();
  Bitboard enemy_pieces_bb = board.pieces(enemy);
  Bitboard friendly_pieces_bb = board.pieces(side);

  // 1. Defender Calculation
  // Count friendly pieces that control the ring or are in it
  int defender_score = 0;
  Bitboard defenders =
      friendly_pieces_bb &
      ~board.pieces(PAWN, side); // Pawns are static shield, pieces are dynamic
  // (Shield logic is separate below)

  // We can't easily iterate all defenders for control without expensive loop?
  // Let's stick to simple "Proximity" for now or use the pre-calculated attack
  // maps if available? We don't have global attack maps computed. Fast
  // approximation: Pieces close to king? or pieces attacking ring? Let's use
  // simple distance check for defenders for speed in this pass. Or better:
  // Iterate friendly pieces and check if they attack ring.

  Bitboard my_knights = board.pieces(KNIGHT, side);
  while (my_knights) {
    Square s = Bitboards::pop_lsb(my_knights);
    if (Magic::get_knight_attacks(s) & inner_ring)
      defender_score += KingDefenderWeight;
  }
  // Sliders... expensive to re-generate all attacks.
  // Optimization: Only check sliders if they are somewhat close?
  // For now, skip slider precise defender calc to keep NPS high, rely on Shield
  // (pawns). Actually, we can count pieces *in* the ring or adjacent.
  Bitboard near_defenders =
      (Magic::get_king_attacks(k_sq) | inner_ring) & defenders;
  defender_score += Bitboards::popcount(near_defenders) * KingDefenderWeight;

  int attack_units = 0;
  int attackers_count = 0;

  // Coordination: Tracking hits per square in the ring
  unsigned char ring_hits[64] = {0};

  // Safe Check Potential
  bool safe_check_potential = false;

  auto add_threat = [&](Bitboard att, int weight_unit, int weight_check,
                        PieceType pt) {
    Bitboard hits = att & inner_ring;
    if (hits) {
      attack_units += weight_unit * Bitboards::popcount(hits) + weight_check;
      attackers_count++;

      while (hits) {
        Square s = Bitboards::pop_lsb(hits);
        ring_hits[s]++;
      }
    }

    // Virtual Safe Check Check
    // If piece attacks king directly (check), or can move to a square that
    // checks? Current 'att' is attacks from current position. Actual check
    // detection is expensive. Approximate: If 'att' hits a square adjacent to
    // king that is NOT defended by us? Hard to know "defended by us" cheaply.
    // Let's use a simpler heuristic: if 'att' hits King Ring and is not a Pawn.
    if (hits && pt > PAWN)
      safe_check_potential = true;
  };

  Bitboard enemy_rooks = board.pieces(ROOK, enemy);
  Bitboard enemy_queens = board.pieces(QUEEN, enemy);
  Bitboard enemy_bishops = board.pieces(BISHOP, enemy);
  Bitboard enemy_knights = board.pieces(KNIGHT, enemy);

  // Scan enemy pieces
  Bitboard knights = enemy_knights;
  while (knights) {
    add_threat(Magic::get_knight_attacks(Bitboards::pop_lsb(knights)), 3, 2,
               KNIGHT);
  }

  Bitboard bishops = enemy_bishops;
  while (bishops) {
    add_threat(Magic::get_bishop_attacks(Bitboards::pop_lsb(bishops), occ), 3,
               2, BISHOP);
  }

  Bitboard rooks = enemy_rooks;
  while (rooks) {
    add_threat(Magic::get_rook_attacks(Bitboards::pop_lsb(rooks), occ), 4, 3,
               ROOK);
  }

  Bitboard queens = enemy_queens;
  while (queens) {
    add_threat(Magic::get_queen_attacks(Bitboards::pop_lsb(queens), occ), 6, 5,
               QUEEN);
  }

  // 2. Coordination Bonus
  int coordination_score = 0;
  Bitboard ring = inner_ring;
  while (ring) {
    Square s = Bitboards::pop_lsb(ring);
    if (ring_hits[s] > 1) {
      coordination_score += (ring_hits[s] - 1) * CoordinationBonus;
    }
  }
  attack_units += coordination_score;

  // 3. Safe Check Bonus
  if (safe_check_potential && attackers_count > 1) {
    if (attack_units > 20) { // Only if real pressure exists
      attack_units += SafeCheckBonus;
    }
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
      structural_penalty += OpenFileBonus;
      if (!(board.pieces(PAWN, enemy) & f_mask)) {
        structural_penalty += 20;
      }
    }
  }

  // Shield Pawns
  Bitboard shield = inner_ring & board.pieces(PAWN, side);
  structural_penalty -= Bitboards::popcount(shield) * 12; // Static shield bonus

  if (structural_penalty < 0)
    structural_penalty = 0;

  // Apply Defender Reduction
  // Limit reduction so we don't negative-safety
  if (defender_score > attack_units / 2)
    defender_score = attack_units / 2;
  attack_units -= defender_score;

  if (attackers_count < 2 && structural_penalty < 30 && attack_units < 15) {
    TRACE(trace, "KingSafety", -structural_penalty);
    return structural_penalty;
  }

  if (attack_units > 99)
    attack_units = 99;
  if (attack_units < 0)
    attack_units = 0;

  int safety_score = SafetyTable[attack_units] + structural_penalty;
  TRACE(trace, "KingSafety", -safety_score);
  return safety_score;
}

template <bool Trace>
int evaluate_pieces(const Board &board, Color side,
                    EvalTrace *trace = nullptr) {
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
  TRACE(trace, "PieceCoord", score);
  return score;
}

template <bool Trace>
int evaluate_t(const Board &board, int alpha, int beta, int depth,
               EvalTrace *trace = nullptr) {
  int cached;
  if (!Trace && EvalCache.probe(board.key, cached)) {
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
  if (!Trace && (lazy_score < alpha - LazyEvalMargin ||
                 lazy_score > beta + LazyEvalMargin)) {
    // Apply perspective and return
    int result = (board.side_to_move() == WHITE) ? lazy_score : -lazy_score;
    return result;
  }

  // === FULL EVALUATION (within window) ===

  // Pawn Structure
  ScorePair pawn_score = evaluate_pawns<Trace>(board, GlobalPawnTable, trace);
  mg_score += pawn_score.mg;
  eg_score += pawn_score.eg;
  TRACE(trace, "PawnStructMG", pawn_score.mg);
  TRACE(trace, "PawnStructEG", pawn_score.eg);

  // King Safety
  int ks_white = evaluate_king_safety<Trace>(board, WHITE, trace);
  int ks_black = evaluate_king_safety<Trace>(board, BLACK, trace);
  mg_score -= ks_white;
  mg_score += ks_black;

  // Threats
  int thr_white = evaluate_threats<Trace>(board, WHITE, trace);
  int thr_black = evaluate_threats<Trace>(board, BLACK, trace);
  mg_score -= thr_white;
  mg_score += thr_black;

  // Mobility
  int mob_white = evaluate_mobility<Trace>(board, WHITE, trace);
  int mob_black = evaluate_mobility<Trace>(board, BLACK, trace);
  mg_score += mob_white;
  mg_score -= mob_black;

  // Space
  int space_white = evaluate_space<Trace>(board, WHITE, trace);
  int space_black = evaluate_space<Trace>(board, BLACK, trace);
  mg_score += space_white;
  mg_score -= space_black;

  // Piece Coordination
  int pc_white = evaluate_pieces<Trace>(board, WHITE, trace);
  int pc_black = evaluate_pieces<Trace>(board, BLACK, trace);
  mg_score += pc_white;
  mg_score -= pc_black;

  // Outposts
  int out_white = evaluate_outposts<Trace>(board, WHITE, trace);
  int out_black = evaluate_outposts<Trace>(board, BLACK, trace);
  mg_score += out_white;
  mg_score -= out_black;

  // Bishop Pair
  if (Bitboards::popcount(board.pieces(BISHOP, WHITE)) >= 2) {
    mg_score += BishopPairMG;
    eg_score += BishopPairEG;
    TRACE(trace, "BishopPairMG", BishopPairMG);
    TRACE(trace, "BishopPairEG", BishopPairEG);
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

  // Note: Lambda can't easily TRACE unless we capture 'trace' or move logic
  // Just trace total for now if needed, or leave it.

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
  if (!Trace && depth >= 0 && depth < 6) {
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
  if (!Trace && EvalHysteresis > 0 && std::abs(score) < MATE_THRESHOLD) {
    int variance =
        (int)((board.key % (2 * EvalHysteresis + 1))) - EvalHysteresis;
    score += variance;
  }

  if (!Trace)
    EvalCache.save(board.key, score);

  // Perspective
  return (board.side_to_move() == WHITE) ? score : -score;
}

int evaluate(const Board &board, int alpha, int beta, int depth) {
  return evaluate_t<false>(board, alpha, beta, depth);
}

int evaluate_trace(const Board &board, EvalTrace &trace) {
  return evaluate_t<true>(board, -30000, 30000, 0, &trace);
}

} // namespace Eval

} // namespace Prometheus
