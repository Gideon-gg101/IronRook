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
    return Eval::evaluate(board);
  }

  nodes++;
  int stand_pat = Eval::evaluate(board);
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

      Board copy = board;
      if (copy.make_move(m)) {
        moves_searched++;
        int score;

        if (moves_searched == 1) {
          score = -alpha_beta(copy, depth - 1, 1, -beta, -alpha, nodes,
                              Move::NONE, m);
        } else {
          score = -alpha_beta(copy, depth - 1, 1, -alpha - 1, -alpha, nodes,
                              Move::NONE, m);
          if (score > alpha && score < beta) {
            score = -alpha_beta(copy, depth - 1, 1, -beta, -alpha, nodes,
                                Move::NONE, m);
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

      Board copy = board;
      if (copy.make_move(m)) {
        moves_searched++;
        int score;

        if (moves_searched == 1) {
          score = -alpha_beta(copy, depth - 1, 1, -beta, -alpha, nodes,
                              Move::NONE, m);
        } else {
          score = -alpha_beta(copy, depth - 1, 1, -alpha - 1, -alpha, nodes,
                              Move::NONE, m);
          if (score > alpha && score < beta) {
            score = -alpha_beta(copy, depth - 1, 1, -beta, -alpha, nodes,
                                Move::NONE, m);
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
    TT.save(board.key, best_score, BOUND_UPPER, depth, best_move, 0, 0);
  } else {
    TT.save(board.key, best_score, BOUND_EXACT, depth, best_move, 0, 0);
  }

  return best_score;
}

int SearchWorker::alpha_beta(Board &board, int depth, int ply, int alpha,
                             int beta, uint64_t &nodes, Move excludedMove,
                             Move prevMove) {
  if (should_stop)
    return 0;

  if (Timer.should_stop(nodes)) {
    should_stop = true;
    return 0;
  }

  if (ply >= 64) {
    return Eval::evaluate(board);
  }

  // Draw Detection (Repetition / 50-move)
  if (board.half_move_clock() >= 100 || board.is_repetition()) {
    return -limits.contempt;
  }

  // Check Extensions
  bool inCheck = board.is_square_attacked(
      Bitboards::lsb(board.pieces(KING, board.side_to_move())),
      ~board.side_to_move());
  if (inCheck) {
    depth += 1; // Extend
  }

  bool pvNode = (beta - alpha > 1);

  int alphaOrig = alpha;
  int staticEval = 30001; // Sentinel

  // Probe TT
  Move ttMove = Move::NONE;
  TTEntry tte;
  if (TT.probe(board.key, tte)) {
    ttMove = tte.move();
    int ttScore = tte.score();
    // Mate score normalization: transform from storage (ply-independent) to
    // search (ply-relative)
    if (ttScore > MATE_BOUND)
      ttScore -= ply;
    else if (ttScore < -MATE_BOUND)
      ttScore += ply;

    if (tte.depth() >= depth) {
      if (excludedMove == Move::NONE) {
        if (!pvNode || tte.type() == BOUND_EXACT) {
          if (tte.type() == BOUND_EXACT)
            return ttScore;
          if (tte.type() == BOUND_LOWER && ttScore >= beta)
            return ttScore;
          if (tte.type() == BOUND_UPPER && ttScore <= alpha)
            return ttScore;
        }
      }
    }
  }

  // Singular Extensions (Skipped for now, complexity)

  // Internal Iterative Deepening (IID)
  // Only if PV node or we rely on TT for ordering
  if (depth >= 6 && ttMove == Move::NONE && !inCheck &&
      (pvNode || depth >= 8)) {
    int iidDepth = depth - 2;
    alpha_beta(board, iidDepth, ply, alpha, beta, nodes);
    if (TT.probe(board.key, tte)) {
      ttMove = tte.move();
    }
  }

  nodes++;
  if (depth <= 0) {
    return quiescence(board, alpha, beta, nodes, 0, ply);
  }

  // Razoring (Commented out in original, leaving out for now)

  if (staticEval == 30001) {
    staticEval = Eval::evaluate(board);
    staticEval += get_correction(board.side_to_move(), board);
  }

  // Position Trend
  if (ply < 256)
    static_evals[ply] = staticEval;
  bool improving = (ply >= 2 && staticEval > static_evals[ply - 2]);

  // ProbCut
  // Absolutely NO ProbCut in PV nodes
  if (!pvNode && depth >= 5 &&
      ((int)beta >= 0 ? (int)beta : -(int)beta) < MATE && !inCheck) {
    int margin = 200;
    int reducedDepth = depth - 4;
    if (reducedDepth < 1)
      reducedDepth = 1;

    int probBeta = beta + margin;

    int score = alpha_beta(board, reducedDepth, ply + 1, probBeta - 1, probBeta,
                           nodes, Move::NONE, prevMove);

    if (score >= probBeta) {
      TT.save(board.key, score, BOUND_LOWER, depth, Move::NONE, 0, ply);
      return beta;
    }
  }

  // Null Move Pruning
  // Absolutely NO NMP in PV nodes
  if (!pvNode && depth >= 3 &&
      !board.is_square_attacked(
          Bitboards::lsb(board.pieces(KING, board.side_to_move())),
          ~board.side_to_move())) {

    // Adaptive R: Increase reduction if score is high above beta
    int R = 3 + depth / 6 + std::min(3, (staticEval - beta) / 200);

    // Suppression in endgames to avoid zugzwang risks
    bool zugzwang_risk = (board.phase_value < 4); // Very few pieces left

    if (staticEval >= beta && (!zugzwang_risk || staticEval >= beta + 200)) {
      Board copy = board;
      copy.make_null_move();
      int score = -alpha_beta(copy, depth - 1 - R, ply + 1, -beta, -beta + 1,
                              nodes, Move::NONE, Move::NONE);
      if (score >= beta)
        return (score >= MATE_BOUND) ? beta : score;
    }
  }

  // Use pre-allocated heap list
  MoveList &list = moveLists[ply];
  list.count = 0; // Reset
  MoveGen::generate_all(board, list);

  score_moves(board, list, ttMove, depth, prevMove);

  int best_score = -INF;
  Move best_move = Move::NONE;

  int moves_searched = 0;

  for (int i = 0; i < list.count; ++i) {
    Move m = pick_next_move(list, i);
    if (m == Move::NONE)
      break;

    bool is_quiet = (board.piece_on(m.to()) == NO_PIECE) &&
                    (m.flags() != MoveFlags::EP_CAPTURE);

    // SEE Pruning (Bad Captures)
    // If capture is static loss (see < -margin), prune it
    if (!is_quiet && depth > 0 && !inCheck) {
      // G+ 4: SEE Scaling
      int see_margin = 50 * depth;
      if (!board.see(m, -see_margin)) {
        continue;
      }
    }

    // Dynamic LMP (Late Move Pruning)
    // G+ 3: Smarter LMP with eval margin
    if (!pvNode && !inCheck && is_quiet && depth <= 8) {
      int eval_margin = staticEval - alpha;
      int lmp_count = (3 + depth * depth);
      if (eval_margin > 0)
        lmp_count += eval_margin / 100;

      if (moves_searched >= lmp_count) {
        continue;
      }
    }

    // History Pruning
    // Prune moves with very bad history
    // G+ 1.4: History Pruning
    if (!pvNode && !inCheck && is_quiet && depth <= 3) {
      int hist = history[board.side_to_move()][m.from()][m.to()];
      if (hist < -2000) // Threshold
        continue;
    }

    // Futility Pruning
    // Absolutely NO Futility in PV nodes, In Check, or (implied) tactical
    // moves Request: "Absolutely no ... when: PV node" G+ 1: Disable futility
    // in PV
    if (!pvNode && depth <= 7 && is_quiet && !inCheck && alpha < 20000 &&
        beta < 20000 && moves_searched > 0) {

      int fMargin = 120 * depth;
      if (staticEval + fMargin <= alpha) {
        continue;
      }
    }

    Board copy = board;
    if (copy.make_move(m)) {
      moves_searched++;

      int extension = 0;
      if (is_quiet && depth < 16) {
        Piece p = board.piece_on(m.from());
        int to_rank = m.to() / 8;
        if (p == W_PAWN && to_rank == 6)
          extension = 1;
        if (p == B_PAWN && to_rank == 1)
          extension = 1;
      }

      int score;
      int reduction = 0;
      if (depth >= 3 && moves_searched > 1 && is_quiet) {
        reduction = 1 + std::log(depth) * std::log(moves_searched) / 2;
        reduction -= extension; // Extend instead of reduce

        // History-based reduction
        int hist = history[board.side_to_move()][m.from()][m.to()];
        reduction -= hist / 2000; // Good history = less reduction

        // PV nodes reduce less
        if (pvNode)
          reduction -=
              2; // Aggressive reduction decrease for PV tactical accuracy

        // Check/Promotion already handled by is_quiet check (promotions are
        // !is_quiet usually? No, m.flags()) is_quiet defined as:
        // board.piece_on(to) == NO_PIECE && flags != EP_CAPTURE. Promotions
        // are NOT quiet for LMR purposes usually.
        if (m.flags() >= MoveFlags::PROMOTION_KNIGHT)
          reduction = 0;

        // Not Improving? Increase reduction
        if (!improving) {
          reduction += 1;
        }

        if (reduction < 0)
          reduction = 0;
      }

      // Safe check for mate scores in TT/Search to normalize distance
      // We do this by passing 'ply' to search, but signature is fixed.
      // Standard: alpha/beta are "mated_in(ply)" relative.
      // Prometheus uses raw scores relative to root?
      // Current: MATE = 30000.
      // We need to handle mate score adjustment in search return and call.
      // Currently: return best_score.
      // Let's rely on standard mate score propagation: MATE - ply.
      // But we need to un-normalize when saving to TT/Reading from TT.
      // For now, let's stick to LMR implementation.

      if (moves_searched == 1) {
        score = -alpha_beta(copy, depth - 1, ply + 1, -beta, -alpha, nodes);
      } else {
        // PVS with LMR
        score = -alpha_beta(copy, depth - 1 - reduction, ply + 1, -alpha - 1,
                            -alpha, nodes);

        // Re-search if failed high
        if (score > alpha && reduction > 0) {
          score =
              -alpha_beta(copy, depth - 1, ply + 1, -alpha - 1, -alpha, nodes);
        }

        if (score > alpha && score < beta) {
          score = -alpha_beta(copy, depth - 1, ply + 1, -beta, -alpha, nodes);
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

        // Quiet move checks
        if (board.piece_on(best_move.to()) == NO_PIECE &&
            best_move.flags() != MoveFlags::EP_CAPTURE) {

          if (depth < 64) {
            if (best_move != killers[depth][0]) {
              killers[depth][1] = killers[depth][0];
              killers[depth][0] = best_move;
            }
          }

          // History Bonus
          update_history(best_move, depth * depth, board.side_to_move());

          // Counter Move Update
          if (prevMove != Move::NONE) {
            counter_moves[board.side_to_move()][prevMove.to()] = best_move;
          }

          if (prevMove != Move::NONE) {
            Piece prevPiece = board.piece_on(prevMove.to());
            Piece currPiece = board.piece_on(
                best_move
                    .from()); // Before move? No, board is not changed here.
            // Wait, 'best_move' is from 'board'. Board is not changed in this
            // scope (we undid or copied?) In alpha_beta, 'board' is CONST?
            // NO, 'board' is reference. In Loop: 'Board copy = board;
            // copy.make_move(m)'. So 'board' is the state BEFORE 'best_move'
            // is made. So board.piece_on(best_move.from()) is the piece
            // moving. Correct.
            update_continuation_history(prevMove, prevPiece, best_move,
                                        currPiece, depth * depth);
          }
        }

        if (staticEval != 30001 && !inCheck) {
          update_correction_history(board.side_to_move(), depth, beta,
                                    staticEval, board);
        }
        return beta;
      }
      if (score > alpha) {
        alpha = score;
      }
    }
  }

  if (moves_searched == 0) {
    if (excludedMove != Move::NONE)
      return alphaOrig;
    if (inCheck)
      return -MATE + ply;
    else
      return 0;
  }

  if (moves_searched == 0) {
    if (excludedMove != Move::NONE)
      return alphaOrig;
    if (inCheck)
      return -MATE + ply;
    else
      return 0;
  }

  if (best_score <= alphaOrig) {
    TT.save(board.key, best_score, BOUND_UPPER, depth, best_move, 0, ply);
  } else {
    TT.save(board.key, best_score, BOUND_EXACT, depth, best_move, 0, ply);
    // G+ 2.7: Experience Cache Record (Only exact scores at sufficient depth)
    if (depth >= 10 && !should_stop) {
      GlobalExperience.record(board.key, best_score, depth);
    }
  }

  if (staticEval != 30001 && !inCheck) {
    update_correction_history(board.side_to_move(), depth, best_score,
                              staticEval, board);
  }
  return best_score;
}

void SearchWorker::iter_deep() {
  Board board = rootBoard; // Worker's copy
  int max_depth = (limits.depth > 0) ? limits.depth : 64;
  nodes_searched = 0;
  root_stability_counter = 0;
  last_best_move = Move::NONE;

  if (thread_id == 0) {
    TT.set_generation((TT.get_generation() + 1) & 0xF);
  }

  Move bestMoveHistory[128];
  for (int i = 0; i < 128; ++i)
    bestMoveHistory[i] = Move::NONE;

  auto start_time = std::chrono::high_resolution_clock::now();

  // Syzygy probe (Thread 0 only)
  if (thread_id == 0 && Bitboards::popcount(board.all_pieces()) <= 7) {
    int wdl = Syzygy::probe_root_wdl(board);
    if (wdl != -32001) {
      int score = Syzygy::wdl_to_score(wdl, 0);
      std::cout << "info string Syzygy WDL: " << score << " (Win/Loss/Draw)"
                << std::endl;
    }
  }

  int score = 0; // Initialize score for aspiration window
  rootMoves.clear();

  for (int depth = 1; depth <= max_depth; ++depth) {
    if (depth == 1) {
      MoveList list;
      MoveGen::generate_all(board, list);
      for (int i = 0; i < list.count; ++i) {
        if (board.make_move(list.moves[i])) {
          RootMove rm;
          rm.move = list.moves[i];
          rm.score = -INF;
          rootMoves.push_back(rm);
        }
      }
    }

    if (should_stop)
      break;

    // MultiPV Loop
    std::vector<Move> excludedMoves;
    int pv_count = limits.multipv;
    if (pv_count < 1)
      pv_count = 1;
    // Don't produce more PVs than legal moves
    // (Optimization: verify legal move count, but alpha_beta_root handles it)

    for (int mpv = 0; mpv < pv_count; ++mpv) {
      // Aspiration Windows (Reset for each PV line or share?)
      // Usually reset.
      if (should_stop)
        break;

      // If not first PV, we might want to relax aspiration or just search
      // full window For simplicity, use aspiration for first PV, full window
      // for others? Or same logic.

      int current_score = 0;
      if (depth >= 5 && mpv == 0) {
        int delta = 16;
        while (true) {
          int alpha = std::max(-INF, score - delta);
          int beta = std::min(INF, score + delta);

          current_score = alpha_beta_root(board, depth, alpha, beta,
                                          nodes_searched, excludedMoves);

          if (should_stop)
            break;

          if (current_score <= alpha) {
            // Fail Low (Horizon Panic?)
            if (current_score < score - 50)
              Timer.extend_time(1.5);
            delta += delta / 2 + 12;
          } else if (current_score >= beta) {
            // Fail High
            delta += delta / 2 + 12;
          } else {
            break;
          }
          if (delta > 2000) {
            current_score = alpha_beta_root(board, depth, -INF, INF,
                                            nodes_searched, excludedMoves);
            break;
          }
        }
      } else {
        current_score = alpha_beta_root(board, depth, -INF, INF, nodes_searched,
                                        excludedMoves);
      }

      if (should_stop)
        break;

      // Find the best move for this PV
      // We rely on TT to have stored the best move from alpha_beta_root
      Move bestMove = Move::NONE;
      TTEntry tte;
      if (TT.probe(board.key, tte)) {
        bestMove = tte.move();
      }
      this->bestMove = bestMove; // Store for external use

      // IMPORTANT: If we excluded moves, the TT entry might still be old if
      // we didn't save? alpha_beta_root saves to TT. So bestMove should be
      // the one from THIS search.

      if (bestMove != Move::NONE) {
        excludedMoves.push_back(bestMove);

        if (thread_id == 0) {
          auto now = std::chrono::high_resolution_clock::now();
          uint64_t duration =
              std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                    start_time)
                  .count();

          // Construct PV Line
          std::string pv_line = bestMove.to_uci(Threads.chess960);
          Board pv_board = board;
          pv_board.make_move(bestMove);

          // Probe TT for subsequent moves
          for (int d = 0; d < depth; ++d) {
            TTEntry tte_pv;
            if (TT.probe(pv_board.key, tte_pv)) {
              Move m_pv = tte_pv.move();
              if (m_pv == Move::NONE)
                break;

              // Verify legality to be safe (avoid illegal moves in PV)
              MoveList legitimate_moves;
              MoveGen::generate_legal(pv_board, legitimate_moves);
              bool found = false;
              for (int k = 0; k < legitimate_moves.count; ++k) {
                if (legitimate_moves.moves[k] == m_pv) {
                  found = true;
                  break;
                }
              }

              if (found) {
                pv_line += " " + m_pv.to_uci(Threads.chess960);
                pv_board.make_move(m_pv);
              } else {
                break;
              }
            } else {
              break;
            }
          }

          std::cout << "info depth " << depth << " multipv " << (mpv + 1)
                    << " score cp " << current_score << " nodes "
                    << nodes_searched << " time " << duration << " nps "
                    << (nodes_searched * 1000 / std::max(duration, 1ULL))
                    << " pv " << pv_line << std::endl;
        }
      } else {
        break; // No more moves?
      }

      // Update main score for next aspiration window prediction (only for
      // first PV)
      if (mpv == 0) {
        // Panic Check (Score Drop)
        if (depth > 8 && current_score < score - 40) {
          Timer.extend_time(1.3);
        }

        score = current_score;
        // G+ 2.2: Root Move Stability Tracking
        if (bestMove == last_best_move && bestMove != Move::NONE) {
          root_stability_counter++;
        } else {
          root_stability_counter = 0;
        }
        last_best_move = bestMove;

        bestMoveHistory[depth] = bestMove;

        if (depth > 4) {
          // Check if the best move has changed from the previous depth
          if (bestMoveHistory[depth] != bestMoveHistory[depth - 1]) {
            // Unstable! Extend time
            Timer.extend_time(1.5); // Give 50% more time
          }

          // G+ 2.2 Predictive Finish Early
          if (depth > 10 && root_stability_counter >= 5) {
            Timer.finish_early();
          }
        }
      }
    }

    // Sort root moves for next depth reordering
    if (!rootMoves.empty()) {
      std::sort(rootMoves.begin(), rootMoves.end(),
                [](const RootMove &a, const RootMove &b) {
                  return a.score > b.score;
                });
    }
  }

  if (thread_id == 0) {
    Move bestMove = Move::NONE;
    TTEntry tte;
    if (TT.probe(board.key, tte)) {
      bestMove = tte.move();
    }
    MoveList legal_moves;
    MoveGen::generate_legal(board, legal_moves);

    bool legal = false;
    for (int i = 0; i < legal_moves.count; ++i) {
      if (legal_moves.moves[i] == bestMove) {
        legal = true;
        break;
      }
    }

    if (!legal && legal_moves.count > 0) {
      // Fallback: Pick first legal move (safety net)
      // Ideally picking best scoring, but we don't have list scores here
      // easily
      bestMove = legal_moves.moves[0];
    } else if (legal_moves.count == 0) {
      bestMove = Move::NONE;
    }

    if (bestMove != Move::NONE) {
      this->bestMove =
          bestMove; // Ensure member is updated even if loop incomplete
      std::cout << "bestmove " << bestMove.to_uci(Threads.chess960)
                << std::endl;
    } else {
      std::cout << "bestmove (none)" << std::endl;
    }
  }
}

void ThreadPool::clear() {
  for (auto *worker : workers) {
    worker->clear_history();
  }
  TT.clear();
}
} // namespace Search

} // namespace Prometheus
