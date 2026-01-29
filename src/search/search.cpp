#include "search.h"
#include "../board/movegen.h"
#include "../core/tt.h"
#include "../eval/eval.h"
#include "../tb/syzygy.h"
#include "learning.h"
#include "time_manager.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>

#include <cstdlib>
#include <iostream>
#include <vector>

namespace Prometheus {

namespace Search {

Book BookInstance;

constexpr int INF = 1000000;
constexpr int MATE = 30000;

// LMR reduction table [depth][moveNumber]
int LMRTable[64][64];

// Initialize LMR reduction table
void init_lmr_table() {
  for (int d = 1; d < 64; d++) {
    for (int m = 1; m < 64; m++) {
      // Formula: base + log(depth) * log(moveNum) / divisor
      // Using tunable parameters from Eval namespace
      LMRTable[d][m] =
          int(Eval::LMRBaseReduction / 100.0 +
              std::log(d) * std::log(m) / (Eval::LMRDepthDivisor / 100.0));
    }
  }
  // Depth 0 and move 0 always have 0 reduction
  for (int i = 0; i < 64; i++) {
    LMRTable[0][i] = 0;
    LMRTable[i][0] = 0;
  }
}

static int score_capture(const Board &board, Move m) {
  Piece attacker = board.piece_on(m.from());
  Piece victim = board.piece_on(m.to());
  int v_val = 0;
  int type = (int)victim;
  if (type >= 9)
    type -= 8;

  switch (type) {
  case PAWN:
    v_val = 100;
    break;
  case KNIGHT:
    v_val = 300;
    break;
  case BISHOP:
    v_val = 300;
    break;
  case ROOK:
    v_val = 500;
    break;
  case QUEEN:
    v_val = 900;
    break;
  case KING:
    v_val = 20000;
    break;
  default:
    v_val = 0;
    if (m.flags() == MoveFlags::EP_CAPTURE)
      v_val = 100;
    break;
  }

  int a_val = 0;
  type = (int)attacker;
  if (type >= 9)
    type -= 8;
  switch (type) {
  case PAWN:
    a_val = 1;
    break;
  case KNIGHT:
    a_val = 2;
    break;
  case BISHOP:
    a_val = 2;
    break;
  case ROOK:
    a_val = 3;
    break;
  case QUEEN:
    a_val = 4;
    break;
  case KING:
    a_val = 5;
    break;
  }

  return v_val * 10 - a_val + 10000;
}

void SearchWorker::clear_history() {
  for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 64; ++j)
      for (int k = 0; k < 64; ++k)
        history[i][j][k] = 0;

  for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 64; ++j)
      counter_moves[i][j] = Move::NONE;

  for (int i = 0; i < 16; ++i)
    for (int j = 0; j < 64; ++j)
      for (int k = 0; k < 16; ++k)
        for (int l = 0; l < 64; ++l)
          continuation_history[i][j][k][l] = 0;
}

void SearchWorker::update_history(Move m, int bonus, int side) {
  int &val = history[side][m.from()][m.to()];
  val += bonus;
  if (val > 2000)
    val = 2000;
  if (val < -2000)
    val = -2000;
}

void SearchWorker::update_continuation_history(Move prevMove, Piece prevPiece,
                                               Move currMove, Piece currPiece,
                                               int bonus) {
  if (prevMove == Move::NONE || currMove == Move::NONE ||
      prevPiece == NO_PIECE || currPiece == NO_PIECE)
    return;

  // Indices: prevPiece, prevTo, currPiece, currTo
  int &val = continuation_history[(int)prevPiece][prevMove.to()][(int)currPiece]
                                 [currMove.to()];
  val += bonus;
  if (val > 2000)
    val = 2000;
  if (val < -2000)
    val = -2000;
}

int SearchWorker::get_continuation_bonus(Move prevMove, Piece prevPiece,
                                         Move currMove, Piece currPiece) {
  if (prevMove == Move::NONE || currMove == Move::NONE ||
      prevPiece == NO_PIECE || currPiece == NO_PIECE)
    return 0;
  return continuation_history[(int)prevPiece][prevMove.to()][(int)currPiece]
                             [currMove.to()];
}

void SearchWorker::update_correction_history(int side, int depth, int score,
                                             int static_eval,
                                             const Board &board) {
  if (((int)score >= 0 ? (int)score : -(int)score) >= MATE - 100)
    return;

  int diff = score - static_eval;
  int weight = std::min(depth, 16);
  int val = diff * weight / 16;
  int clamped_diff = std::max(-50, std::min(50, val));

  for (int pt = 1; pt < 7; ++pt) {
    Bitboard b = board.pieces((PieceType)pt, (Color)side);
    while (b) {
      Square sq = Bitboards::pop_lsb(b);
      int &hist = correction_history[side][pt][sq];
      hist += clamped_diff;
      if (hist > 512)
        hist = 512;
      if (hist < -512)
        hist = -512;
    }
  }
}

int SearchWorker::get_correction(int side, const Board &board) {
  int correction = 0;
  for (int pt = 1; pt < 7; ++pt) {
    Bitboard b = board.pieces((PieceType)pt, (Color)side);
    while (b) {
      Square sq = Bitboards::pop_lsb(b);
      correction += correction_history[side][pt][sq];
    }
  }
  return correction / 64;
}

void SearchWorker::score_moves(const Board &board, MoveList &list, Move ttMove,
                               int depth, Move prevMove) {
  Piece prevPiece = NO_PIECE;
  if (prevMove != Move::NONE) {
    prevPiece =
        board.piece_on(prevMove.to()); // For opponent's move, piece is at 'to'
  }
  for (int i = 0; i < list.count; ++i) {
    Move m = list.moves[i];
    int score = 0;

    if (m == ttMove) {
      score = 2000000;
    } else if (board.piece_on(m.to()) != NO_PIECE ||
               m.flags() == MoveFlags::EP_CAPTURE) {
      // Use SEE to distinguish good/bad captures
      if (board.see(m, 0)) {
        score = score_capture(board, m); // Good capture (>10000)
      } else {
        score = score_capture(board, m) - 15000; // Bad capture (<0)
      }

    } else if (m.flags() >= MoveFlags::PROMOTION_KNIGHT) {
      // Promotion (Quiet push)
      // Give huge bonus to Queen promotion, less for others
      int type = m.flags() & 3; // 3=Queen, 2=Rook, 1=Bishop, 0=Knight
      if (type == 3)
        score = 30000;
      else if (type == 2)
        score = 15000;
      else
        score = 10000;
    } else {
      // Quiet
      if (depth < 64) {
        if (m == killers[depth][0])
          score = 9000;
        else if (m == killers[depth][1])
          score = 8000;
      }

      // Counter-Move Heuristic
      if (prevMove != Move::NONE) {
        Move counter = counter_moves[board.side_to_move()][prevMove.to()];
        if (m == counter) {
          score += 7000; // High bonus, below Killers but above history/quiet
        }
      }

      int side = board.side_to_move();
      score += history[side][m.from()][m.to()];

      if (prevMove != Move::NONE) {
        Piece currPiece = board.piece_on(m.from());
        score += get_continuation_bonus(prevMove, prevPiece, m, currPiece);
      }
    }
    list.scores[i] = score;
  }
}

static Move pick_next_move(MoveList &list, int startIndex) {
  int bestIndex = -1;
  int bestScore = -2000000000;

  for (int i = startIndex; i < list.count; ++i) {
    if (list.scores[i] > bestScore) {
      bestScore = list.scores[i];
      bestIndex = i;
    }
  }

  if (bestIndex != -1) {
    Move tempM = list.moves[startIndex];
    int tempS = list.scores[startIndex];
    list.moves[startIndex] = list.moves[bestIndex];
    list.scores[startIndex] = list.scores[bestIndex];
    list.moves[bestIndex] = tempM;
    list.scores[bestIndex] = tempS;
    return list.moves[startIndex];
  }
  return Move::NONE;
}

int SearchWorker::quiescence(Board &board, int alpha, int beta, uint64_t &nodes,
                             int depth, int ply) {
  if ((nodes & 2047) == 0) {
    if (should_stop)
      return 0;

    if (Timer.should_stop(nodes)) {
      should_stop = true;
      return 0;
    }
  }

  // Guard against deep recursion/stack overflow
  if (ply >= 127) {
    return Eval::evaluate(board, alpha, beta, 0);
  }

  nodes++;
  int stand_pat = Eval::evaluate(board, alpha, beta, 0);
  if (stand_pat >= beta)
    return beta;
  if (alpha < stand_pat)
    alpha = stand_pat;

  // QSearch Hardening: Quiet Checks
  // Only search quiet checks at shallow q-depths (0, -1)
  bool search_quiet_checks = (depth > -2);
  int checks_searched = 0;

  // Use pre-allocated heap list
  MoveList &list = moveLists[ply];
  list.count = 0; // Reset
  MoveGen::generate_all(board, list);
  score_moves(board, list, Move::NONE, 0);

  for (int i = 0; i < list.count; ++i) {
    Move m = pick_next_move(list, i);
    if (m == Move::NONE)
      break;

    bool is_capture = (board.piece_on(m.to()) != NO_PIECE) ||
                      (m.flags() == MoveFlags::EP_CAPTURE);

    // G+ 2.4: Termination/Safeguards for QSearch
    if (!is_capture && (!search_quiet_checks || checks_searched >= 2)) {
      continue;
    }

    // SEE Pruning
    // captures: prune if loses material
    // quiet checks: prune if loses piece for nothing (SEE < 0)
    if (!board.see(m, 0)) {
      continue;
    }

    Board copy = board;
    if (!copy.make_move(m))
      continue;

    bool gives_check = copy.is_square_attacked(
        Bitboards::lsb(copy.pieces(KING, copy.side_to_move())),
        ~copy.side_to_move());

    if (!is_capture) {
      if (!gives_check && m.flags() < MoveFlags::PROMOTION_KNIGHT)
        continue;
      checks_searched++;
    }

    // Delta Pruning (Only for captures, not checks)
    if (is_capture && !gives_check) {
      int captured_val = 0;
      Piece victim = board.piece_on(m.to());
      if (victim != NO_PIECE) {
        int type = (int)victim;
        if (type >= 9)
          type -= 8;
        switch (type) {
        case PAWN:
          captured_val = 100;
          break;
        case KNIGHT:
          captured_val = 300;
          break;
        case BISHOP:
          captured_val = 300;
          break;
        case ROOK:
          captured_val = 500;
          break;
        case QUEEN:
          captured_val = 900;
          break;
        }
      }
      if (stand_pat + captured_val + 200 < alpha &&
          m.flags() < MoveFlags::PROMOTION_KNIGHT)
        continue;
    }

    int score = -quiescence(copy, -beta, -alpha, nodes, depth - 1, ply + 1);

    if (score >= beta)
      return beta;
    if (score > alpha)
      alpha = score;
  }
  return alpha;
}

int SearchWorker::alpha_beta_root(Board &board, int depth, int alpha, int beta,
                                  uint64_t &nodes,
                                  const std::vector<Move> &excluded) {
  int alphaOrig = alpha;
  if (should_stop)
    return 0;
  if (Timer.should_stop(nodes)) {
    should_stop = true;
    return 0;
  }

  // G+ 2.7: Experience Cache Probe
  int exp_score, exp_depth;
  if (GlobalExperience.probe(board.key, exp_score, exp_depth)) {
    if (exp_depth >= depth) {
      return exp_score;
    }
  }

  // Check Extensions
  bool inCheck = board.is_square_attacked(
      Bitboards::lsb(board.pieces(KING, board.side_to_move())),
      ~board.side_to_move());
  if (inCheck) {
    depth += 1;
  }

  // Probe TT for root move ordering?
  // We can use TT move, but check if it's excluded.
  Move ttMove = Move::NONE;
  TTEntry tte;
  if (TT.probe(board.key, tte)) {
    ttMove = tte.move();
  }

  // If ttMove is excluded, ignore it
  for (size_t i = 0; i < excluded.size(); ++i) {
    if (excluded[i] == ttMove) {
      ttMove = Move::NONE;
      break;
    }
  }

  int best_score = -INF;
  Move best_move = Move::NONE;
  int moves_searched = 0;

  // Use rootMoves if already populated (Iterative Deepening carry-over)
  if (!rootMoves.empty()) {
    for (auto &rm : rootMoves) {
      Move m = rm.move;

      // Skip excluded moves
      bool skip = false;
      for (const auto &ex : excluded) {
        if (m == ex) {
          skip = true;
          break;
        }
      }
      if (skip)
        continue;

      // Root Move Count Pruning
      if (depth >= 6 && moves_searched > 16 && !inCheck) {
        if (history[board.side_to_move()][m.from()][m.to()] < -200) {
          continue;
        }
      }

      Board copy = board;
      if (copy.make_move(m)) {
        moves_searched++;
        int score;

        // Root LMR
        int reduction = 0;
        if (depth >= 3 && moves_searched > 4) {
          reduction =
              LMRTable[std::min(depth, 63)][std::min(moves_searched, 63)];
          if (reduction > 1)
            reduction = 1; // Mild cap at root
        }

        if (moves_searched == 1) {
          score = -alpha_beta(copy, depth - 1, 1, -beta, -alpha, nodes,
                              Move::NONE, m, true);
        } else {
          // PVS with LMR at root
          score = -alpha_beta(copy, depth - 1 - reduction, 1, -alpha - 1,
                              -alpha, nodes, Move::NONE, m, true);

          if (score > alpha && reduction > 0) {
            score = -alpha_beta(copy, depth - 1, 1, -alpha - 1, -alpha, nodes,
                                Move::NONE, m, true);
          }

          if (score > alpha && score < beta) {
            score = -alpha_beta(copy, depth - 1, 1, -beta, -alpha, nodes,
                                Move::NONE, m, true);
          }
        }

        if (should_stop)
          return 0;

        rm.score = score; // Update score for reordering

        if (score > best_score) {
          best_score = score;
          best_move = m;
        }

        if (score >= beta) {
          TT.save(board.key, score, BOUND_LOWER, depth, best_move, 0, 0);
          return beta;
        }
        if (score > alpha) {
          alpha = score;
        }
      }
    }
  } else {
    // Fallback: Use pre-allocated heap list (Ply 0)
    MoveList &list = moveLists[0];
    list.count = 0; // Reset
    MoveGen::generate_all(board, list);

    score_moves(board, list, ttMove, depth);

    for (int i = 0; i < list.count; ++i) {
      Move m = pick_next_move(list, i);
      if (m == Move::NONE)
        break;

      // Skip excluded moves
      bool skip = false;
      for (const auto &ex : excluded) {
        if (m == ex) {
          skip = true;
          break;
        }
      }
      if (skip)
        continue;

      // Root Move Count Pruning
      if (depth >= 6 && moves_searched > 16 && !inCheck) {
        if (history[board.side_to_move()][m.from()][m.to()] < -200) {
          continue;
        }
      }

      Board copy = board;
      if (copy.make_move(m)) {
        moves_searched++;
        int score;

        // Root LMR
        int reduction = 0;
        if (depth >= 3 && moves_searched > 4) {
          reduction =
              LMRTable[std::min(depth, 63)][std::min(moves_searched, 63)];
          if (reduction > 1)
            reduction = 1; // Mild cap at root
        }

        if (moves_searched == 1) {
          score = -alpha_beta(copy, depth - 1, 1, -beta, -alpha, nodes,
                              Move::NONE, m, true);
        } else {
          // PVS with LMR
          score = -alpha_beta(copy, depth - 1 - reduction, 1, -alpha - 1,
                              -alpha, nodes, Move::NONE, m, true);

          if (score > alpha && reduction > 0) {
            score = -alpha_beta(copy, depth - 1, 1, -alpha - 1, -alpha, nodes,
                                Move::NONE, m, true);
          }

          if (score > alpha && score < beta) {
            score = -alpha_beta(copy, depth - 1, 1, -beta, -alpha, nodes,
                                Move::NONE, m, true);
          }
        }

        if (should_stop)
          return 0;

        if (score > best_score) {
          best_score = score;
          best_move = m;
        }

        if (score >= beta) {
          TT.save(board.key, score, BOUND_LOWER, depth, best_move, 0, 0);
          return beta;
        }
        if (score > alpha) {
          alpha = score;
        }
      }
    }
  }

  if (moves_searched == 0) {
    if (excluded.empty()) {
      bool inCheckRoot = board.is_square_attacked(
          Bitboards::lsb(board.pieces(KING, board.side_to_move())),
          ~board.side_to_move());
      if (inCheckRoot)
        return -MATE;
      else
        return 0;
    } else {
      return alphaOrig;
    }
  }

  // Always exact at root if we searched all moves?
  // If we found a best move updates alpha, it is exact if we searched full
  // window. With aspiration, we might return bound.
  if (best_score <= alphaOrig) { // Fail low
    TT.save(board.key, score_to_tt(best_score, 0), BOUND_UPPER, depth,
            best_move, 0, 0);
  } else {
    TT.save(board.key, score_to_tt(best_score, 0), BOUND_EXACT, depth,
            best_move, 0, 0);
  }

  return best_score;
}

int SearchWorker::alpha_beta(Board &board, int depth, int ply, int alpha,
                             int beta, uint64_t &nodes, Move excludedMove,
                             Move prevMove, bool allowNull, bool updateStats) {
  if (should_stop)
    return 0;

  if (Timer.should_stop(nodes)) {
    should_stop = true;
    return 0;
  }

  if (ply >= 64) {
    return Eval::evaluate(board, alpha, beta, depth);
  }

  bool pvNode = (beta - alpha > 1);
  // Check Probing of TT
  Move ttMove = Move::NONE;
  TTEntry tte;
  int tt_eval = -INF;
  bool ttHit = TT.probe(board.key, tte);

  if (ttHit) {
    ttMove = tte.move();
    // Normalize score from TT perspective to search perspective
    tt_eval = score_from_tt(tte.score(), ply);

    // TT Cutoff
    if (tte.depth() >= depth && !pvNode) {
      if (tte.type() == BOUND_EXACT) {
        return tt_eval;
      }
      if (tte.type() == BOUND_LOWER && tt_eval >= beta) {
        return tt_eval;
      }
      if (tte.type() == BOUND_UPPER && tt_eval <= alpha) {
        return tt_eval;
      }
    }
  }

  // Draw Detection: Repetition / 50-move
  if (ply > 0 && (board.half_move_clock() >= 100 || board.is_repetition())) {
    return -limits.contempt; // Contempt factor for draw
  }

  // Fail-High/Low Repair (Safety Clamp)
  if (alpha < -MATE_SCORE + ply)
    alpha = -MATE_SCORE + ply;
  if (beta > MATE_SCORE - ply)
    beta = MATE_SCORE - ply;

  // Check Extensions
  bool inCheck = board.is_square_attacked(
      Bitboards::lsb(board.pieces(KING, board.side_to_move())),
      ~board.side_to_move());
  if (inCheck) {
    depth += 1; // Extend
  }

  int alphaOrig = alpha;
  int staticEval = 30001; // Sentinel

  // Singular Extensions
  // Singular Extensions
  int singularExt = 0;
  if (depth >= Eval::SingularMinDepth && ttHit && ttMove != Move::NONE &&
      ttMove != excludedMove && // Recursive SE protection
      tte.depth() >= depth - 3 &&
      (tte.type() == BOUND_LOWER || tte.type() == BOUND_EXACT) && !inCheck &&
      std::abs(tt_eval) < MATE_BOUND && std::abs(alpha) < MATE_SCORE - 100 &&
      std::abs(beta) < MATE_SCORE - 100) {

    int singularBeta = tt_eval - depth * Eval::SingularMarginMultiplier;
    int singularDepth = depth / 2 - 1;

    int seScore =
        alpha_beta(board, singularDepth, ply, singularBeta - 1, singularBeta,
                   nodes, ttMove, prevMove, allowNull, updateStats);

    if (seScore < singularBeta) {
      singularExt = 1; // TT move is singular!

      // Double Singular Extension
      if (seScore < singularBeta - Eval::DoubleSingularMargin &&
          depth >= Eval::SingularMinDepth + 2) {
        singularExt = 2;
      }
    }
  }

  // Apply Extension (Safety Clamp)
  // Move depth processing to HERE if feasible, or pass singularExt logic down?
  // Usually this extends the search of the TT move specifically, OR extends the
  // *current* node. Standard logic: Extend the search of this node if the TT
  // move is the only good one. Wait, Prometheus implementation structure is
  // slightly different. depth passed to alpha_beta calls. Correct application:
  // Update 'depth' before move loop? Yes.
  depth += singularExt;

  // Internal Iterative Deepening (IID)
  int minDepth = pvNode ? Eval::IIDMinDepthPV : Eval::IIDMinDepthNonPV;

  if (depth >= minDepth && ttMove == Move::NONE && !inCheck && !should_stop) {
    int reduction = pvNode ? Eval::IIDReductionPV : Eval::IIDReductionNonPV;

    // Adaptive reduction: deeper searches reduce more to save time
    if (depth >= 12)
      reduction++;

    int iidDepth = depth - reduction;
    if (iidDepth < 1)
      iidDepth = 1; // Safety: minimum depth

    // Perform reduced-depth search to populate TT
    // Preserve allowNull
    alpha_beta(board, iidDepth, ply, alpha, beta, nodes, Move::NONE, Move::NONE,
               allowNull, updateStats);

    // Retrieve best move from TT
    if (TT.probe(board.key, tte)) {
      ttMove = tte.move();
    }
  }

  nodes++;
  if (depth <= 0) {
    return quiescence(board, alpha, beta, nodes, 0, ply);
  }

  // Get static evaluation if we don't have it yet
  if (staticEval == 30001) {
    staticEval = Eval::evaluate(board, alpha, beta, depth);
    staticEval += get_correction(board.side_to_move(), board);
  }

  // Position Trend
  if (ply < 256)
    static_evals[ply] = staticEval;
  bool improving = (ply >= 2 && staticEval > static_evals[ply - 2]);

  // ProbCut: Early cutoff with shallow verification
  if (!pvNode && depth >= Eval::ProbcutMinDepth && std::abs(beta) < MATE &&
      !inCheck) {
    int probBeta = beta + Eval::ProbcutMargin;
    int reducedDepth = depth - Eval::ProbcutReduction;
    if (reducedDepth < 1)
      reducedDepth = 1;

    // Preserve allowNull
    int score = alpha_beta(board, reducedDepth, ply + 1, probBeta - 1, probBeta,
                           nodes, Move::NONE, prevMove, allowNull, updateStats);

    if (score >= probBeta) {
      TT.save(board.key, beta, BOUND_LOWER, depth, Move::NONE, 0, ply);
      return beta; // Verified cutoff!
    }
  }

  // Null Move Pruning
  if (allowNull && !pvNode && depth >= 3 &&
      !board.is_square_attacked(
          Bitboards::lsb(board.pieces(KING, board.side_to_move())),
          ~board.side_to_move())) {

    int R = Eval::NmpBaseReduction + depth / Eval::NmpDepthDivisor +
            std::min(3, (staticEval - beta) / Eval::NmpEvalBetaMargin);

    // Disable NMP if we have only King + Pawns (or just King)
    Bitboard non_pawns = board.pieces(board.side_to_move()) &
                         ~board.pieces(PAWN, board.side_to_move()) &
                         ~board.pieces(KING, board.side_to_move());
    bool only_pawns = (non_pawns == 0);

    if (staticEval >= beta && !only_pawns) {
      Board copy = board;
      copy.make_null_move();
      // Pass allowNull=false
      int score =
          -alpha_beta(copy, depth - 1 - R, ply + 1, -beta, -beta + 1, nodes,
                      Move::NONE, Move::NONE, false, updateStats);
      if (score >= beta) {
        // Verified Null Move Pruning (VNM)
        if (depth >= Eval::NmpVerificationDepth &&
            std::abs(beta) < MATE_BOUND) {
          int vDepth = depth - Eval::NmpVerificationReduction;
          if (vDepth < 1)
            vDepth = 1;

          int vScore =
              -alpha_beta(copy, vDepth, ply + 1, -beta, -beta + 1, nodes,
                          Move::NONE, Move::NONE, false, updateStats);
          if (vScore >= beta) {
            return (vScore >= MATE_BOUND) ? beta : vScore;
          }
        } else {
          return (score >= MATE_BOUND) ? beta : score;
        }
      }
    }
  }

  // Use pre-allocated heap list
  MoveList &list = moveLists[ply];
  list.count = 0; // Reset
  MoveGen::generate_all(board, list);

  score_moves(board, list, ttMove, depth, prevMove);

  int best_score = -INF;
  Move best_move = Move::NONE;

  int local_moves_searched = 0;

  for (int i = 0; i < list.count; ++i) {
    Move m = pick_next_move(list, i);
    if (m == Move::NONE)
      break;

    if (m == excludedMove)
      continue;

    Board copy = board;
    if (copy.make_move(m)) {
      local_moves_searched++;
      int score;

      // Reset allowNull to true for normal moves (unless there's another reason
      // to disable it?) NMP recursion is essentially checking if passing is
      // good. If we move, the next search presumably allows null move (since we
      // moved). So allowNull = true.

      if (local_moves_searched == 1) {
        score = -alpha_beta(copy, depth - 1, ply + 1, -beta, -alpha, nodes,
                            Move::NONE, m, true, updateStats);
      } else {
        // Late Move Reductions (LMR)
        int reduction = 0;
        if (depth >= 3 && local_moves_searched > 1 && !inCheck) {
          reduction =
              LMRTable[std::min(depth, 63)][std::min(local_moves_searched, 63)];

          if (m.flags() >= MoveFlags::PROMOTION_KNIGHT || board.see(m, 0) ||
              m == killers[depth][0] || m == killers[depth][1]) {
            reduction /= 2;
          }

          // PV nodes reduce less
          if (pvNode)
            reduction -= Eval::LMRPVReduction;

          // Improving bonus
          if (improving)
            reduction -= Eval::LMRImprovingBonus;

          if (reduction < 0)
            reduction = 0;

          // History LMR
          // Good history -> Reduce less
          // Bad history -> Reduce more? Or just reduce less for good moves.
          // History is typically -4096 to +4096
          // int hist_score = history[board.side_to_move()][m.from()][m.to()];
          // reduction -= hist_score / 2048;

          // Clamp again
          if (reduction < 0)
            reduction = 0;

          // Negative Singular Extensions
          // If the node is singular (one move is much better), other moves are
          // likely bad. We revert the extension (since we extended the whole
          // node) and add a penalty.
          if (singularExt > 0) {
            reduction += singularExt;
            reduction += 1;
          }
        }

        score = -alpha_beta(copy, depth - 1 - reduction, ply + 1, -alpha - 1,
                            -alpha, nodes, Move::NONE, m, true, updateStats);

        if (score > alpha && reduction > 0) {
          score = -alpha_beta(copy, depth - 1, ply + 1, -alpha - 1, -alpha,
                              nodes, Move::NONE, m, true, updateStats);
        }

        if (score > alpha && score < beta) {
          score = -alpha_beta(copy, depth - 1, ply + 1, -beta, -alpha, nodes,
                              Move::NONE, m, true, updateStats);
        }
      }

      if (should_stop)
        return 0;

      if (score > best_score) {
        best_score = score;
        best_move = m;
      }

      if (score >= beta) {
        TT.save(board.key, score, BOUND_LOWER, depth, best_move, 0, ply);
        // History updates
        if (updateStats && board.piece_on(m.to()) == NO_PIECE &&
            m.flags() < MoveFlags::EP_CAPTURE) {
          Threads.main()->update_history(m, depth * depth,
                                         board.side_to_move());
        }
        return beta;
      }
      if (score > alpha) {
        alpha = score;
      }
    }

    // IID Retry (Repair Move Ordering)
    // If the first move (TT Move) fails low, our move ordering might be broken.
    // Try to find a better move with a quick IID search.
    if (local_moves_searched == 1 && best_score <= alphaOrig &&
        ttMove != Move::NONE && depth >= Eval::IIDRetryMinDepth &&
        !should_stop) {

      int reducedDepth = depth - Eval::IIDRetryReduction;
      if (reducedDepth < 1)
        reducedDepth = 1;

      // Perform reduced depth search to find a new best move
      alpha_beta(board, reducedDepth, ply + 1, alpha, beta, nodes, Move::NONE,
                 Move::NONE, true, updateStats);

      // Probe TT to see if we found something new
      TTEntry tte_retry;
      if (TT.probe(board.key, tte_retry)) {
        Move newBest = tte_retry.move();
        if (newBest != Move::NONE && newBest != best_move) {
          // We found a new best move!
          // Swap 'newBest' to be the next move in 'list'.

          for (int k = i + 1; k < list.count; ++k) {
            if (list.moves[k] == newBest) {
              // Swap to next position (i+1)
              std::swap(list.moves[i + 1], list.moves[k]);
              std::swap(list.scores[i + 1], list.scores[k]);
              break;
            }
          }
        }
      }
    }
  }

  if (local_moves_searched == 0) {
    if (excludedMove != Move::NONE) {
      return alphaOrig;
    }
    if (inCheck)
      return -MATE_SCORE + ply; // Checkmate
    else
      return 0; // Stalemate
  }

  if (best_score <= alphaOrig) { // Fail low
    TT.save(board.key, score_to_tt(best_score, ply), BOUND_UPPER, depth,
            best_move, 0, ply);
  } else {
    TT.save(board.key, score_to_tt(best_score, ply), BOUND_EXACT, depth,
            best_move, 0, ply);
  }

  return best_score;
}

// Helper to extract PV from TT
static void get_pv(Board b, std::vector<Move> &pv) {
  TTEntry tte;
  // Limit PV length to avoid loops
  for (int i = 0; i < 64; ++i) {
    if (!TT.probe(b.key, tte))
      break;
    Move m = tte.move();
    if (m == Move::NONE)
      break;

    // Basic validity check (can we make the move?)
    // Note: This is expensive if we do full generation.
    // Trust TT for now, but handle make_move failure.

    // Check if move is distinct pseudo-legal?
    // Let's just try to make it.
    Board copy = b;
    if (!copy.make_move(m))
      break;

    pv.push_back(m);
    b = copy;

    // Loop detection?
    if (b.is_repetition())
      break;
  }
}

void SearchWorker::iter_deep() {
  clear_history(); // Clear for new game/search?
  // No, uci.cpp handles 'ucinewgame'. But standard ID often resets some stats?
  // Let's NOT clear history here to allow accumulation over moves in a game.

  // Initialize Root Moves
  rootMoves.clear();
  MoveList list;
  MoveGen::generate_legal(rootBoard, list);
  if (list.count == 0) {
    // mated or stalemate
    // Should print result?
    // UCI handles bestmove (none).
    return;
  }

  for (int i = 0; i < list.count; ++i) {
    RootMove rm;
    rm.move = list.moves[i];
    rm.score = -INF;
    rootMoves.push_back(rm);
  }

  bestMove = Move::NONE; // Reset
  int score = 0;

  // Reset nodes for this search
  nodes_searched = 0;
  // Timer.start_time is already initialized by Timer.init() in
  // Threads.start_search

  int alpha = -INF;
  int beta = INF;

  // Iterative Deepening Loop
  int max_depth = limits.depth > 0 ? limits.depth : MAX_PLY;
  for (int depth = 1; depth <= max_depth; ++depth) {
    if (should_stop)
      break;

    // Aspiration Windows
    // Only apply after first depth to have a baseline score
    int delta = Eval::AspirationWindow;
    if (depth > 1) {
      alpha = std::max(-MATE, score - delta);
      beta = std::min(MATE, score + delta);
    } else {
      alpha = -INF;
      beta = INF;
    }

    int research_cnt = 0;

    while (true) {
      if (should_stop)
        break;
      research_cnt++;

      std::vector<Move>
          excluded; // Root search doesn't use excluded moves (singular root?)
      // Call AlphaBetaRoot
      int val = alpha_beta_root(rootBoard, depth, alpha, beta, nodes_searched,
                                excluded);

      if (should_stop)
        break;

      // Sort Root Moves
      std::stable_sort(rootMoves.begin(), rootMoves.end(),
                       [](const RootMove &a, const RootMove &b) {
                         return a.score > b.score;
                       });

      // Update best move immediately
      if (!rootMoves.empty()) {
        bestMove = rootMoves[0].move;
        score = rootMoves[0].score;
      }

      // Fail Low: Widen Alpha
      if (val <= alpha) {
        Timer.fail_low(); // Time Extension
        beta = (alpha + beta) / 2;
        int growth = (Eval::AspirationGrowth * delta) / 100;
        if (growth < 5)
          growth = 5;
        delta += growth;
        alpha = std::max(-MATE, val - delta);

        // Panic check
        if (delta > Eval::AspirationPanic) {
          alpha = -INF;
          beta = INF;
        }
        continue;
      }

      // Fail High: Widen Beta
      if (val >= beta) {
        alpha = (alpha + beta) / 2;
        int growth = (Eval::AspirationGrowth * delta) / 100;
        if (growth < 5)
          growth = 5;
        delta += growth;
        beta = std::min(MATE, val + delta);

        // Panic check
        if (delta > Eval::AspirationPanic) {
          alpha = -INF;
          beta = INF;
        }
        continue;
      }

      // Within window
      score = val;
      break;
    }

    if (should_stop)
      break;

    // UCI Info Output
    long long duration = Timer.elapsed();
    if (duration == 0)
      duration = 1;

    std::cout << "info depth " << depth << " seldepth "
              << depth // approximation
              << " score cp " << score << " nodes " << nodes_searched << " nps "
              << (nodes_searched * 1000 / duration) << " time " << duration
              << " pv";

    std::vector<Move> pv;
    get_pv(rootBoard, pv);
    for (auto m : pv) {
      std::cout << " " << m.to_uci();
    }
    std::cout << std::endl;

    // Time Management Updates
    if (!rootMoves.empty()) {
      Timer.update_best_move(rootMoves[0].move, depth);
    }

    // Check Time
    if (Timer.should_stop(nodes_searched)) {
      should_stop = true;
      break;
    }
  }

  // Print Best Move
  // If we stopped before completing depth 1? (e.g. instant input)
  // Use whatever we have (Move Gen order if nothing else)
  if (bestMove == Move::NONE && !rootMoves.empty()) {
    bestMove = rootMoves[0].move;
  }

  if (thread_id == 0) {
    std::cout << "bestmove "
              << (bestMove != Move::NONE ? bestMove.to_uci() : "0000")
              << std::endl;
  }
}

} // namespace Search

} // namespace Prometheus
