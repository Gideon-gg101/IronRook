#include "book.h"
#include "movegen.h"
#include "polyglot_keys.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>

namespace IroonRook {

// Helper to swap endianness (Polyglot is Big Endian)
uint16_t swap16(uint16_t v) { return (v << 8) | (v >> 8); }
uint32_t swap32(uint32_t v) {
  return (v << 24) | ((v << 8) & 0x00FF0000) | ((v >> 8) & 0x0000FF00) |
         (v >> 24);
}
uint64_t swap64(uint64_t v) {
  v = ((v << 8) & 0xFF00FF00FF00FF00ULL) | ((v >> 8) & 0x00FF00FF00FF00FFULL);
  v = ((v << 16) & 0xFFFF0000FFFF0000ULL) | ((v >> 16) & 0x0000FFFF0000FFFFULL);
  return (v << 32) | (v >> 32);
}

bool Book::load(const std::string &path) {
  file_path = path;
  std::ifstream file(file_path, std::ios::binary);
  return file.good();
}

Move Book::probe(const Board &board, bool pick_best) {
  std::ifstream file(file_path, std::ios::binary);
  if (!file.is_open())
    return Move::NONE;

  uint64_t key = Polyglot::compute_key(board);
  // Polyglot keys are big-endian in file
  uint64_t key_be = swap64(key);

  // Binary search
  file.seekg(0, std::ios::end);
  size_t file_size = file.tellg();
  size_t num_entries = file_size / 16;

  size_t low = 0, high = num_entries - 1;
  size_t found_idx = -1;

  while (low <= high) {
    size_t mid = low + (high - low) / 2;
    file.seekg(mid * 16, std::ios::beg);

    uint64_t entry_key;
    file.read(reinterpret_cast<char *>(&entry_key), 8);

    if (entry_key == key_be) {
      found_idx = mid;
      break;
    } else if (entry_key < key_be) {
      low = mid + 1;
    } else {
      if (mid == 0)
        break;
      high = mid - 1;
    }
  }

  if (found_idx == (size_t)-1)
    return Move::NONE;

  // Find first entry (scan backwards)
  while (found_idx > 0) {
    file.seekg((found_idx - 1) * 16, std::ios::beg);
    uint64_t entry_key;
    file.read(reinterpret_cast<char *>(&entry_key), 8);
    if (entry_key != key_be)
      break;
    found_idx--;
  }

  std::vector<BookEntry> entries;
  // Read all matching entries
  file.seekg(found_idx * 16, std::ios::beg);
  while (true) {
    BookEntry entry;
    file.read(reinterpret_cast<char *>(&entry.key), 8);
    if (entry.key != key_be)
      break;

    file.read(reinterpret_cast<char *>(&entry.move), 2);
    file.read(reinterpret_cast<char *>(&entry.weight), 2);
    file.read(reinterpret_cast<char *>(&entry.learn), 4);

    entry.key = swap64(entry.key);
    entry.move = swap16(entry.move);
    entry.weight = swap16(entry.weight);
    entry.learn = swap32(entry.learn);

    entries.push_back(entry);
    if (file.eof())
      break;
  }

  if (entries.empty())
    return Move::NONE;

  // Select move
  if (pick_best) {
    std::sort(entries.begin(), entries.end(),
              [](const BookEntry &a, const BookEntry &b) {
                return a.weight > b.weight;
              });
    return convert_move(entries[0].move, board);
  } else {
    // Weighted random
    int total_weight = 0;
    for (const auto &e : entries)
      total_weight += e.weight;

    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, total_weight - 1);
    int r = dist(rng);

    for (const auto &e : entries) {
      r -= e.weight;
      if (r < 0)
        return convert_move(e.move, board);
    }
  }

  return convert_move(entries[0].move, board);
}

Move Book::convert_move(uint16_t poly_move, const Board &board) {
  int to_file = poly_move & 0x7;
  int to_rank = (poly_move >> 3) & 0x7;
  int from_file = (poly_move >> 6) & 0x7;
  int from_rank = (poly_move >> 9) & 0x7;
  int promo = (poly_move >> 12) & 0x7;

  Square from = make_square(File(from_file), Rank(from_rank));
  Square to = make_square(File(to_file), Rank(to_rank));

  MoveList ml;
  MoveGen::generate_all(board, ml);

  for (int i = 0; i < ml.count; ++i) {
    const Move &m = ml.moves[i];
    if (m.from() == from && m.to() == to) {
      if (m.flags() & 8) {
        PieceType pt = PieceType((m.flags() & 3) + KNIGHT);
        if (promo == 1 && pt == KNIGHT)
          return m;
        if (promo == 2 && pt == BISHOP)
          return m;
        if (promo == 3 && pt == ROOK)
          return m;
        if (promo == 4 && pt == QUEEN)
          return m;
      } else {
        return m;
      }
    }
  }
  return Move::NONE;
}

namespace Polyglot {
uint64_t compute_key(const Board &board) {
  uint64_t key = 0;

  // Pieces
  // Polyglot encoding:
  // White: P=0, N=1, B=2, R=3, Q=4, K=5
  // Black: P=6, N=7, B=8, R=9, Q=10, K=11
  for (int s = 0; s < 64; ++s) {
    Piece p = board.piece_on(Square(s));
    if (p != NO_PIECE) {
      int pt = (p & 7); // 1..6
      int c = (p >> 3); // 0=White, 1=Black

      int poly_piece = -1;
      if (c == WHITE) {
        poly_piece = pt - 1;
      } else {
        poly_piece = pt - 1 + 6;
      }

      // Offset: 64 * piece + square
      key ^= Random64[64 * poly_piece + s];
    }
  }

  // Castling
  // 768: WK, 769: WQ, 770: BK, 771: BQ
  int rights = board.castling_rights();
  if (rights & 1)
    key ^= Random64[768];
  if (rights & 2)
    key ^= Random64[769];
  if (rights & 4)
    key ^= Random64[770];
  if (rights & 8)
    key ^= Random64[771];

  // En Passant
  // Offset 772 + file (0..7)
  // Note: Polyglot stricter check (candidates exist) is ideal,
  // but for simple integration, hashing the valid EP square is standard.
  Square ep = board.en_passant_square();
  if (ep != SQ_NONE) {
    // Is it a valid En Passant?
    // Polyglot requires that a pawn can accurately capture.
    // We will blindly hash it for now.
    int f = ep % 8;
    key ^= Random64[772 + f];
  }

  // Turn
  // White to move: XOR Random64[780]
  if (board.side_to_move() == WHITE) {
    key ^= Random64[780];
  }

  return key;
}
} // namespace Polyglot

} // namespace IroonRook
