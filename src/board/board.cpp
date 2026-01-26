#include "board.h"
#include "../core/magic.h"
#include "../core/zobrist.h"
#include "../eval/pst.h" // Incremental Eval
#include <cassert>
#include <cctype>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

namespace IroonRook {

// Helper to split string
std::vector<std::string> split(const std::string &s, char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

// Helper to make piece
Piece make_piece(Color c, PieceType pt) {
  return Piece((int)pt + (c == WHITE ? 0 : 8));
}

// Helper PST lookups
int get_pst_mg(PieceType pt, Square sq, Color c) {
  Square s = (c == WHITE) ? sq : (Square)(sq ^ 56);
  switch (pt) {
  case PAWN:
    return PST::mg_pawn_table[s];
  case KNIGHT:
    return PST::mg_knight_table[s];
  case BISHOP:
    return PST::mg_bishop_table[s];
  case ROOK:
    return PST::mg_rook_table[s];
  case QUEEN:
    return PST::mg_queen_table[s];
  case KING:
    return PST::mg_king_table[s];
  default:
    return 0;
  }
}

int get_pst_eg(PieceType pt, Square sq, Color c) {
  Square s = (c == WHITE) ? sq : (Square)(sq ^ 56);
  switch (pt) {
  case PAWN:
    return PST::eg_pawn_table[s];
  case KNIGHT:
    return PST::eg_knight_table[s];
  case BISHOP:
    return PST::eg_bishop_table[s];
  case ROOK:
    return PST::eg_rook_table[s];
  case QUEEN:
    return PST::eg_queen_table[s];
  case KING:
    return PST::eg_king_table[s];
  default:
    return 0;
  }
}

Board::Board() {
  for (auto &bb : by_type_bb)
    bb = 0;
  for (auto &bb : by_color_bb)
    bb = 0;
  for (auto &p : board_array)
    p = NO_PIECE;
  side = WHITE;
  ep_square = SQ_NONE;
  castle_rights = 0;
  for (int i = 0; i < 4; ++i)
    castling_rooks[i] = SQ_NONE;
  half_moves = 0;
  full_moves = 1;
  key = 0;
  mg_value = 0;
  eg_value = 0;
  phase_value = 0;
}

void Board::put_piece(Piece p, Square s) {
  if (p == NO_PIECE)
    return;

  Color c = (p >= B_PAWN) ? BLACK : WHITE;
  PieceType type = PieceType(0);
  int p_val = (int)p;
  if (c == WHITE)
    type = PieceType(p_val);
  else
    type = PieceType(p_val - 8);

  Bitboards::set_bit(by_type_bb[type], s);
  Bitboards::set_bit(by_color_bb[c], s);
  board_array[s] = p;

  key ^= Zobrist::piece_keys[p][s];

  // Incremental Update
  int sign = (c == WHITE) ? 1 : -1;
  mg_value += sign * (PST::Material[type][0] + get_pst_mg(type, s, c));
  eg_value += sign * (PST::Material[type][1] + get_pst_eg(type, s, c));
  phase_value += PST::PhaseWeights[type];
}

void Board::remove_piece(Square s) {
  Piece p = board_array[s];
  if (p == NO_PIECE)
    return;

  Color c = (p >= B_PAWN) ? BLACK : WHITE;
  PieceType type = PieceType(0);
  int p_val = (int)p;
  if (c == WHITE)
    type = PieceType(p_val);
  else
    type = PieceType(p_val - 8);

  Bitboards::pop_bit(by_type_bb[type], s);
  Bitboards::pop_bit(by_color_bb[c], s);
  board_array[s] = NO_PIECE;

  key ^= Zobrist::piece_keys[p][s];

  // Incremental Update
  int sign = (c == WHITE) ? 1 : -1;
  mg_value -= sign * (PST::Material[type][0] + get_pst_mg(type, s, c));
  eg_value -= sign * (PST::Material[type][1] + get_pst_eg(type, s, c));
  phase_value -= PST::PhaseWeights[type];
}

void Board::set_fen(const std::string &fen) {
  // Clear board
  for (auto &bb : by_type_bb)
    bb = 0;
  for (auto &bb : by_color_bb)
    bb = 0;
  for (auto &p : board_array)
    p = NO_PIECE;
  side = WHITE;
  ep_square = SQ_NONE;
  castle_rights = 0;
  half_moves = 0;
  full_moves = 1;
  mg_value = 0;
  eg_value = 0;
  phase_value = 0;
  history.clear();

  auto tokens = split(fen, ' ');
  if (tokens.empty())
    return;

  // 1. Piece placement
  std::string placement = tokens[0];
  int rank = 7;
  int file = 0;
  for (char c : placement) {
    if (c == '/') {
      rank--;
      file = 0;
    } else if (std::isdigit(c)) {
      file += (c - '0');
    } else {
      PieceType pt = NO_PIECE_TYPE;
      Color color = std::isupper(c) ? WHITE : BLACK;
      char lower = std::tolower(c);
      if (lower == 'p')
        pt = PAWN;
      else if (lower == 'n')
        pt = KNIGHT;
      else if (lower == 'b')
        pt = BISHOP;
      else if (lower == 'r')
        pt = ROOK;
      else if (lower == 'q')
        pt = QUEEN;
      else if (lower == 'k')
        pt = KING;

      if (pt != NO_PIECE_TYPE) {
        Square sq = make_square(File(file), Rank(rank));
        Piece p = make_piece(color, pt);
        put_piece(p, sq);
        file++;
      }
    }
  }

  // 2. Side to move
  if (tokens.size() > 1) {
    if (tokens[1] == "w")
      side = WHITE;
    else
      side = BLACK;
  }

  // 3. Castling rights
  // 3. Castling rights
  // X-FEN support: Parse K/Q/k/q but also A-H/a-h for rook files.
  // Standard K/Q imply outermost rooks if in 960, or H1/A1 in standard.
  if (tokens.size() > 2) {
    std::string castling = tokens[2];
    if (castling != "-") {
      // Find Kings first
      Square wk_sq = SQ_NONE, bk_sq = SQ_NONE;
      for (int s = 0; s < 64; ++s) {
        if (board_array[s] == W_KING)
          wk_sq = (Square)s;
        if (board_array[s] == B_KING)
          bk_sq = (Square)s;
      }

      auto get_file = [](Square s) { return s % 8; };

      for (char c : castling) {
        if (c == 'K') {
          // Rightmost White Rook (or H1 if not found/Standard)
          bool found = false;
          if (wk_sq != SQ_NONE) {
            for (int f = 7; f > get_file(wk_sq); --f) {
              Square sq = make_square(File(f), RANK_1);
              if (board_array[sq] == W_ROOK) {
                castle_rights |= 1;
                castling_rooks[0] = sq;
                found = true;
                break;
              }
            }
          }
          if (!found) {
            castle_rights |= 1;
            castling_rooks[0] = SQ_H1;
          }
        } else if (c == 'Q') {
          // Leftmost White Rook
          bool found = false;
          if (wk_sq != SQ_NONE) {
            for (int f = 0; f < get_file(wk_sq); ++f) {
              Square sq = make_square(File(f), RANK_1);
              if (board_array[sq] == W_ROOK) {
                castle_rights |= 2;
                castling_rooks[1] = sq;
                found = true;
                break;
              }
            }
          }
          if (!found) {
            castle_rights |= 2;
            castling_rooks[1] = SQ_A1;
          }
        } else if (c == 'k') {
          // Rightmost Black Rook (or H8)
          bool found = false;
          if (bk_sq != SQ_NONE) {
            for (int f = 7; f > get_file(bk_sq); --f) {
              Square sq = make_square(File(f), RANK_8);
              if (board_array[sq] == B_ROOK) {
                castle_rights |= 4;
                castling_rooks[2] = sq;
                found = true;
                break;
              }
            }
          }
          if (!found) {
            castle_rights |= 4;
            castling_rooks[2] = SQ_H8;
          }
        } else if (c == 'q') {
          // Leftmost Black Rook
          bool found = false;
          if (bk_sq != SQ_NONE) {
            for (int f = 0; f < get_file(bk_sq); ++f) {
              Square sq = make_square(File(f), RANK_8);
              if (board_array[sq] == B_ROOK) {
                castle_rights |= 8;
                castling_rooks[3] = sq;
                found = true;
                break;
              }
            }
          }
          if (!found) {
            castle_rights |= 8;
            castling_rooks[3] = SQ_A8;
          }
        } else if (c >= 'A' && c <= 'H') {
          // White Rook on File c
          File f = File(c - 'A');
          Square sq = make_square(f, RANK_1);
          if (wk_sq != SQ_NONE && f > get_file(wk_sq)) {
            castle_rights |= 1;
            castling_rooks[0] = sq;
          } else {
            castle_rights |= 2;
            castling_rooks[1] = sq;
          }
        } else if (c >= 'a' && c <= 'h') {
          // Black Rook on File c
          File f = File(c - 'a');
          Square sq = make_square(f, RANK_8);
          if (bk_sq != SQ_NONE && f > get_file(bk_sq)) {
            castle_rights |= 4;
            castling_rooks[2] = sq;
          } else {
            castle_rights |= 8;
            castling_rooks[3] = sq;
          }
        }
      }
    }
  }

  // 4. En passant
  if (tokens.size() > 3) {
    std::string ep = tokens[3];
    if (ep != "-") {
      // Parse file/rank
      // e.g. e3
      if (ep.size() >= 2) {
        File f = File(ep[0] - 'a');
        Rank r = Rank(ep[1] - '1');
        if (f >= FILE_A && f <= FILE_H && r >= RANK_1 && r <= RANK_8)
          ep_square = make_square(f, r);
      }
    }
  }

  // Calculate initial Zobrist Key
  key = 0;
  for (int sq = 0; sq < 64; ++sq) {
    Piece p = board_array[sq];
    if (p != NO_PIECE) {
      key ^= Zobrist::piece_keys[p][sq];
    }
  }

  if (side == BLACK)
    key ^= Zobrist::side_key;

  key ^= Zobrist::castle_keys[castle_rights];

  if (ep_square != SQ_NONE)
    key ^= Zobrist::en_passant_keys[ep_square % 8];

  if (side == BLACK)
    key ^= Zobrist::side_key;
  if (ep_square != SQ_NONE)
    key ^= Zobrist::en_passant_keys[File(ep_square % 8)];
  key ^= Zobrist::castle_keys[castle_rights];
}

bool Board::is_square_attacked(Square sq, Color by_side) const {
  Bitboard occ = all_pieces();

  // Check leapers
  // Pawns
  if (by_side == WHITE) {
    if (File(sq % 8) > FILE_A && Rank(sq / 8) > RANK_1) {
      if (pieces(PAWN, WHITE) & (1ULL << (sq - 9)))
        return true;
    }
    if (File(sq % 8) < FILE_H && Rank(sq / 8) > RANK_1) {
      if (pieces(PAWN, WHITE) & (1ULL << (sq - 7)))
        return true;
    }
  } else {
    if (File(sq % 8) > FILE_A && Rank(sq / 8) < RANK_8) {
      if (pieces(PAWN, BLACK) & (1ULL << (sq + 7)))
        return true;
    }
    if (File(sq % 8) < FILE_H && Rank(sq / 8) < RANK_8) {
      if (pieces(PAWN, BLACK) & (1ULL << (sq + 9)))
        return true;
    }
  }

  // Knights
  if (Magic::get_knight_attacks(sq) & pieces(KNIGHT, by_side))
    return true;

  // King
  if (Magic::get_king_attacks(sq) & pieces(KING, by_side))
    return true;

  // Sliders
  if ((pieces(ROOK, by_side) | pieces(QUEEN, by_side)) &
      Magic::get_rook_attacks(sq, occ))
    return true;
  if ((pieces(BISHOP, by_side) | pieces(QUEEN, by_side)) &
      Magic::get_bishop_attacks(sq, occ))
    return true;

  return false;
}

bool Board::make_move(Move m) {
  assert(Bitboards::popcount(pieces(KING, side)) == 1);
  assert(Bitboards::popcount(pieces(KING, ~side)) == 1);

  int flags = m.flags();
  // Chess960 Castling Logic
  if (flags == KING_CASTLE || flags == QUEEN_CASTLE) {
    history.push_back(key);
    Square from_sq = m.from();
    Square to_sq = m.to(); // In our generator, this is RookSq

    // Determine target squares
    Square k_dst, r_dst;
    if (side == WHITE) {
      k_dst = (flags == KING_CASTLE) ? SQ_G1 : SQ_C1;
      r_dst = (flags == KING_CASTLE) ? SQ_F1 : SQ_D1;
    } else {
      k_dst = (flags == KING_CASTLE) ? SQ_G8 : SQ_C8;
      r_dst = (flags == KING_CASTLE) ? SQ_F8 : SQ_D8;
    }

    // Pieces
    Piece king = piece_on(from_sq);
    Piece rook = piece_on(to_sq);

    // Remove first (crucial if k_dst == r_sq or similar overlap)
    remove_piece(from_sq);
    remove_piece(to_sq);

    // Place
    put_piece(king, k_dst);
    put_piece(rook, r_dst);

    // Update keys
    key ^= Zobrist::side_key;
    if (ep_square != SQ_NONE)
      key ^= Zobrist::en_passant_keys[File(ep_square % 8)];
    key ^= Zobrist::castle_keys[castle_rights];
    ep_square = SQ_NONE;

    // Revoke rights
    if (side == WHITE)
      castle_rights &= ~3;
    else
      castle_rights &= ~12;

    key ^= Zobrist::castle_keys[castle_rights];

    // Check legality (King safety)
    Bitboard k = pieces(KING, side);
    if (k) {
      Square k_pos = Bitboards::lsb(k);
      if (is_square_attacked(k_pos, ~side)) {
        // Revert is hard without full undo. Return false implies illegal.
        // Since we modify 'this', checking 'this' is correct.
        // Movegen should prevent this, but perft might trigger it.
        // For now, return false. Caller must handle state corruption or use
        // copy. Note: existing engine uses make_move on copy.
        return false;
      }
    }

    side = ~side;
    return true;
  }
  history.push_back(key);

  Square from = m.from();
  Square to = m.to();

  Piece p = piece_on(from);
  Piece capture = piece_on(to);

  remove_piece(from);
  if (capture != NO_PIECE)
    remove_piece(to);
  PieceType promo = m.promotion_type();
  if (promo != NO_PIECE_TYPE) {
    p = make_piece(side, promo);
  }
  put_piece(p, to);

  // Update Zobrist for State (Side, EP, Castle)
  key ^= Zobrist::side_key;

  if (ep_square != SQ_NONE) {
    key ^= Zobrist::en_passant_keys[File(ep_square % 8)];
  }
  key ^= Zobrist::castle_keys[castle_rights];

  ep_square = SQ_NONE;
  if (p == W_PAWN && (to - from == 16))
    ep_square = Square(from + 8);
  else if (p == B_PAWN && (from - to == 16))
    ep_square = Square(from - 8);

  if (p == W_KING)
    castle_rights &= ~3;
  if (p == B_KING)
    castle_rights &= ~12;

  // Revoke rights if a rook moves or is captured
  if (from == castling_rooks[0] || to == castling_rooks[0])
    castle_rights &= ~1;
  if (from == castling_rooks[1] || to == castling_rooks[1])
    castle_rights &= ~2;
  if (from == castling_rooks[2] || to == castling_rooks[2])
    castle_rights &= ~4;
  if (from == castling_rooks[3] || to == castling_rooks[3])
    castle_rights &= ~8;

  if (ep_square != SQ_NONE) {
    key ^= Zobrist::en_passant_keys[File(ep_square % 8)];
  }
  key ^= Zobrist::castle_keys[castle_rights];

  Bitboard k = pieces(KING, side);
  if (k == 0) {
    return false;
  }
  Square k_sq = Bitboards::lsb(k);

  if (is_square_attacked(k_sq, ~side)) {
    // std::cout << "Debug: Move left king in check: " << m.to_uci() <<
    // std::endl;
    remove_piece(to);
    put_piece(p, from);
    if (capture != NO_PIECE)
      put_piece(capture, to);
    return false;
  }

  side = ~side;
  return true;
}

} // namespace IroonRook

void IroonRook::Board::make_null_move() {
  key ^= Zobrist::side_key;

  if (ep_square != SQ_NONE) {
    key ^= Zobrist::en_passant_keys[File(ep_square % 8)];
    ep_square = SQ_NONE;
  }

  side = ~side;
}

bool IroonRook::Board::is_repetition() const {
  int end = std::max(0, (int)history.size() - half_moves);
  for (int i = (int)history.size() - 2; i >= end; i -= 2) {
    if (history[i] == key)
      return true;
  }
  return false;
}

namespace IroonRook {

// SEE Values
static const int see_values[] = {
    100, 300, 300, 500, 900, 20000 // P, N, B, R, Q, K
};

int get_see_val(Piece p) {
  if (p == NO_PIECE)
    return 0;
  int type = (int)p;
  if (type >= 8)
    type -= 8;
  return see_values[type];
}

bool Board::see(Move m, int threshold) const {
  // Handle special cases
  if (m.flags() == KING_CASTLE || m.flags() == QUEEN_CASTLE) {
    return 0 >= threshold;
  }

  Square from = m.from();
  Square to = m.to();

  int gain[32];
  int d = 0;

  Piece victim = piece_on(to);
  int val_victim = get_see_val(victim);

  int flags = m.flags();
  if (flags == EP_CAPTURE) {
    val_victim = see_values[PAWN];
  }

  gain[d] = val_victim;

  Piece attacker = piece_on(from);
  int val_attacker = get_see_val(attacker);

  // Promotions
  if (flags & 8) { // Promotion bit set (8-15)
    int promo_val = 0;
    // Queen promo is flag ending in 3 (11 or 15) -> 3 & 3 == 3
    // Rook: 10/14 -> 2
    // Bishop: 9/13 -> 1
    // Knight: 8/12 -> 0
    int promo_type = (flags & 3);

    // Approximate values relative to Knight/Pawn base?
    // Let's use exact SEE values:
    // P(100) -> Q(900). Gain +800.
    // P -> N(300). Gain +200.
    // P -> B(300).
    // P -> R(500).

    switch (promo_type) {
    case 0:
      promo_val = 200;
      break; // N - P
    case 1:
      promo_val = 200;
      break; // B - P
    case 2:
      promo_val = 400;
      break; // R - P
    case 3:
      promo_val = 800;
      break; // Q - P
    }

    gain[d] += promo_val;
    val_attacker += promo_val;
  }

  Bitboard occ = all_pieces();

  // Remove "from" piece
  occ &= ~(1ULL << from);
  if (flags == EP_CAPTURE) {
    if (side == WHITE)
      occ &= ~(1ULL << (to - 8));
    else
      occ &= ~(1ULL << (to + 8));
  }

  Color stm = ~side;

  while (true) {
    d++;
    gain[d] = val_attacker - gain[d - 1];

    if (std::max(-gain[d - 1], gain[d]) < threshold) {
      break;
    }

    Bitboard attackers = 0;

    // Pawn
    if (stm == WHITE) {
      // White pawns attacking 'to' must be at to-9, to-7
      Bitboard pad = 0;
      if (File(to % 8) > FILE_A && Rank(to / 8) > RANK_1)
        pad |= (1ULL << (to - 9));
      if (File(to % 8) < FILE_H && Rank(to / 8) > RANK_1)
        pad |= (1ULL << (to - 7));
      attackers = pad & pieces(PAWN, WHITE);
    } else {
      Bitboard pad = 0;
      if (File(to % 8) > FILE_A && Rank(to / 8) < RANK_8)
        pad |= (1ULL << (to + 7));
      if (File(to % 8) < FILE_H && Rank(to / 8) < RANK_8)
        pad |= (1ULL << (to + 9));
      attackers = pad & pieces(PAWN, BLACK);
    }

    if (attackers & occ) {
      val_attacker = see_values[PAWN];
      Square sq = Bitboards::lsb(attackers & occ);
      occ &= ~(1ULL << sq);
      stm = ~stm;
      continue;
    }

    // Knight
    attackers = Magic::get_knight_attacks(to) & pieces(KNIGHT, stm) & occ;
    if (attackers) {
      val_attacker = see_values[KNIGHT];
      Square sq = Bitboards::lsb(attackers);
      occ &= ~(1ULL << sq);
      stm = ~stm;
      continue;
    }

    // Bishop/Slider
    // Refined LVA logic:
    // Bishop (only BISHOP type)
    attackers = Magic::get_bishop_attacks(to, occ) & pieces(BISHOP, stm) & occ;
    if (attackers) {
      val_attacker = see_values[BISHOP];
      Square sq = Bitboards::lsb(attackers);
      occ &= ~(1ULL << sq);
      stm = ~stm;
      continue;
    }

    // Rook (only ROOK type)
    attackers = Magic::get_rook_attacks(to, occ) & pieces(ROOK, stm) & occ;
    if (attackers) {
      val_attacker = see_values[ROOK];
      Square sq = Bitboards::lsb(attackers);
      occ &= ~(1ULL << sq);
      stm = ~stm;
      continue;
    }

    // Queen
    attackers = (Magic::get_bishop_attacks(to, occ) |
                 Magic::get_rook_attacks(to, occ)) &
                pieces(QUEEN, stm) & occ;
    if (attackers) {
      val_attacker = see_values[QUEEN];
      Square sq = Bitboards::lsb(attackers);
      occ &= ~(1ULL << sq);
      stm = ~stm;
      continue;
    }

    // King
    attackers = Magic::get_king_attacks(to) & pieces(KING, stm) & occ;
    if (attackers) {
      val_attacker = see_values[KING];
      Square sq = Bitboards::lsb(attackers);
      occ &= ~(1ULL << sq);
      stm = ~stm;
      continue;
    }

    // No attackers found
    break;
  }

  // Minimax back
  while (--d > 0) {
    gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
  }

  return gain[0] >= threshold;
}

std::string Board::to_fen() const {
  std::string fen = "";
  for (int r = 7; r >= 0; --r) {
    int empty = 0;
    for (int f = 0; f < 8; ++f) {
      Square s = make_square(File(f), Rank(r));
      Piece p = piece_on(s);
      if (p == NO_PIECE) {
        empty++;
      } else {
        if (empty > 0) {
          fen += std::to_string(empty);
          empty = 0;
        }
        char c = '?';
        // Manual type/color extraction based on Piece enum
        // W_PAWN=1...W_KING=6. B_PAWN=9...B_KING=14.
        int p_val = (int)p;
        PieceType pt = (p_val > 8) ? PieceType(p_val - 8) : PieceType(p_val);
        Color color = (p_val > 8) ? BLACK : WHITE;

        if (color == WHITE) {
          if (pt == PAWN)
            c = 'P';
          else if (pt == KNIGHT)
            c = 'N';
          else if (pt == BISHOP)
            c = 'B';
          else if (pt == ROOK)
            c = 'R';
          else if (pt == QUEEN)
            c = 'Q';
          else if (pt == KING)
            c = 'K';
        } else {
          if (pt == PAWN)
            c = 'p';
          else if (pt == KNIGHT)
            c = 'n';
          else if (pt == BISHOP)
            c = 'b';
          else if (pt == ROOK)
            c = 'r';
          else if (pt == QUEEN)
            c = 'q';
          else if (pt == KING)
            c = 'k';
        }
        fen += c;
      }
    }
    if (empty > 0)
      fen += std::to_string(empty);
    if (r > 0)
      fen += "/";
  }

  fen += (side == WHITE ? " w " : " b ");

  // Castling
  std::string castling = "";
  if (castle_rights & 1)
    castling += "K";
  if (castle_rights & 2)
    castling += "Q";
  if (castle_rights & 4)
    castling += "k";
  if (castle_rights & 8)
    castling += "q";
  if (castling == "")
    castling = "-";
  fen += castling;

  // En Passant
  fen += " ";
  if (ep_square != SQ_NONE) {
    std::string sq_str = "";
    int file = ep_square % 8;
    int rank = ep_square / 8;
    sq_str += (char)('a' + file);
    sq_str += (char)('1' + rank);
    fen += sq_str;
  } else {
    fen += "-";
  }

  // Clocks
  fen += " " + std::to_string(half_moves) + " " + std::to_string(full_moves);

  return fen;
}

} // namespace IroonRook
