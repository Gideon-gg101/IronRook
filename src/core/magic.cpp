#include "magic.h"
#include "bitboard.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Prometheus {
namespace Magic {

Bitboard R_MASKS[64];
Bitboard B_MASKS[64];

Bitboard R_TABLE[64][4096];
Bitboard B_TABLE[64][512];

// 64 values (16 lines * 4)
uint64_t R_MAGICS[64] = {
    0xa8002c000108020ULL,  0x6b00401080004100ULL, 0x8b80108008009100ULL,
    0x990082000210080ULL,  0xcc0080008000c00ULL,  0xe000810002008400ULL,
    0xdb80060000088200ULL, 0x9000820002010c0ULL,  0x80802800410000ULL,
    0x6602058000040100ULL, 0x8840a00080100100ULL, 0x190082004200080ULL,
    0xe800400080200100ULL, 0x6640820042008400ULL, 0x2402120020100200ULL,
    0x900082000202100ULL,  0xa2001000410004ULL,   0x21004010802000ULL,
    0x9180100080100100ULL, 0x980084000200080ULL,  0xec00400080200100ULL,
    0x2240810042008400ULL, 0x2c00120020080200ULL, 0x900082000200100ULL,
    0x80801000410004ULL,   0x21004010800040ULL,   0x9180100080080100ULL,
    0x980082004200080ULL,  0xe800400080200100ULL, 0x2640810042008400ULL,
    0x2c00120020080200ULL, 0x900082000200100ULL,  0xa0801000410004ULL,
    0x61004010800040ULL,   0x9080100080080100ULL, 0x980082000200080ULL,
    0xcc008000802000ULL,   0x6240810042008400ULL, 0x8804020020080200ULL,
    0x900082000200100ULL,  0x80801000410004ULL,   0x21004010800040ULL,
    0x8180100080080100ULL, 0x980082004200080ULL,  0xe800400080200100ULL,
    0x2640810042008400ULL, 0x8804020020080200ULL, 0x900082000200100ULL,
    0xa0801000410004ULL,   0x21004010800040ULL,   0x9180100080080100ULL,
    0x980082000200080ULL,  0xec00400080200100ULL, 0x2240810042008400ULL,
    0x8c02020020080200ULL, 0x900082000200100ULL,  0x80801000410000ULL,
    0x1004010800040ULL,    0x9180100080080100ULL, 0x890082000200080ULL,
    0xe8004000802000ULL,   0x1640810042008400ULL, 0x8804020020080200ULL,
    0x900082000200100ULL};

// 64 values (16 lines * 4)
uint64_t B_MAGICS[64] = {
    0x8021040842000101ULL, 0x4020810080400080ULL, 0x2001000288200100ULL,
    0x2002000410210080ULL, 0x4502010410020001ULL, 0x2202012010820080ULL,
    0x2030010808010100ULL, 0x2006001010020080ULL, 0x8021040842000101ULL,
    0x4020810080400080ULL, 0x2001000288200100ULL, 0x2002000410210010ULL,
    0x4502010410020001ULL, 0x2202012010820080ULL, 0x2030010808010100ULL,
    0x2006001010020080ULL, 0x8021040842000101ULL, 0x4020810080400080ULL,
    0x2001000288200100ULL, 0x2002000410210010ULL, 0x4502010410020001ULL,
    0x2202012010820080ULL, 0x2030010808010100ULL, 0x2006001010020080ULL,
    0x8021040842000101ULL, 0x4020810080400080ULL, 0x2001000288200100ULL,
    0x2002000410210010ULL, 0x4502010410020001ULL, 0x2202012010820080ULL,
    0x2030010808010100ULL, 0x2006001010020080ULL, 0x8021040842000101ULL,
    0x4020810080400080ULL, 0x2001000288200100ULL, 0x2002000410210010ULL,
    0x4502010410020001ULL, 0x2202012010820080ULL, 0x2030010808010100ULL,
    0x2006001010020080ULL, 0x8021040842000101ULL, 0x4020810080400080ULL,
    0x2001000288200100ULL, 0x2002000410210010ULL, 0x4502010410020001ULL,
    0x2202012010820080ULL, 0x2030010808010100ULL, 0x2006001010020080ULL,
    0x8021040842000101ULL, 0x4020810080400080ULL, 0x2001000288200100ULL,
    0x2002000410210010ULL, 0x4502010410020001ULL, 0x2202012010820080ULL,
    0x2030010808010100ULL, 0x2006001010020080ULL, 0x8021040842000101ULL,
    0x4020810080400080ULL, 0x2001000288200100ULL, 0x2002000410210010ULL,
    0x4502010410020001ULL, 0x2202012010820080ULL, 0x2030010808010100ULL,
    0x2006001010020080ULL};

int R_SHIFT[64] = {12, 11, 11, 11, 11, 11, 11, 12, 11, 10, 10, 10, 10,
                   10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11, 11, 10,
                   10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10,
                   11, 11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10,
                   10, 10, 10, 11, 12, 11, 11, 11, 11, 11, 11, 12};

int B_SHIFT[64] = {6, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 5, 5,
                   5, 5, 7, 7, 7, 7, 5, 5, 5, 5, 7, 9, 9, 7, 5, 5,
                   5, 5, 7, 9, 9, 7, 5, 5, 5, 5, 7, 7, 7, 7, 5, 5,
                   5, 5, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 6};

Bitboard get_rook_attacks_slow(Square sq, Bitboard occ) {
  Bitboard attacks = 0;
  int r = sq / 8, f = sq % 8;
  for (int r2 = r + 1; r2 < 8; ++r2) {
    Bitboards::set_bit(attacks, make_square(File(f), Rank(r2)));
    if (Bitboards::get_bit(occ, make_square(File(f), Rank(r2))))
      break;
  }
  for (int r2 = r - 1; r2 >= 0; --r2) {
    Bitboards::set_bit(attacks, make_square(File(f), Rank(r2)));
    if (Bitboards::get_bit(occ, make_square(File(f), Rank(r2))))
      break;
  }
  for (int f2 = f + 1; f2 < 8; ++f2) {
    Bitboards::set_bit(attacks, make_square(File(f2), Rank(r)));
    if (Bitboards::get_bit(occ, make_square(File(f2), Rank(r))))
      break;
  }
  for (int f2 = f - 1; f2 >= 0; --f2) {
    Bitboards::set_bit(attacks, make_square(File(f2), Rank(r)));
    if (Bitboards::get_bit(occ, make_square(File(f2), Rank(r))))
      break;
  }
  return attacks;
}

Bitboard get_bishop_attacks_slow(Square sq, Bitboard occ) {
  Bitboard attacks = 0;
  int r = sq / 8, f = sq % 8;
  for (int r2 = r + 1, f2 = f + 1; r2 < 8 && f2 < 8; ++r2, ++f2) {
    Bitboards::set_bit(attacks, make_square(File(f2), Rank(r2)));
    if (Bitboards::get_bit(occ, make_square(File(f2), Rank(r2))))
      break;
  }
  for (int r2 = r + 1, f2 = f - 1; r2 < 8 && f2 >= 0; ++r2, --f2) {
    Bitboards::set_bit(attacks, make_square(File(f2), Rank(r2)));
    if (Bitboards::get_bit(occ, make_square(File(f2), Rank(r2))))
      break;
  }
  for (int r2 = r - 1, f2 = f + 1; r2 >= 0 && f2 < 8; --r2, ++f2) {
    Bitboards::set_bit(attacks, make_square(File(f2), Rank(r2)));
    if (Bitboards::get_bit(occ, make_square(File(f2), Rank(r2))))
      break;
  }
  for (int r2 = r - 1, f2 = f - 1; r2 >= 0 && f2 >= 0; --r2, --f2) {
    Bitboards::set_bit(attacks, make_square(File(f2), Rank(r2)));
    if (Bitboards::get_bit(occ, make_square(File(f2), Rank(r2))))
      break;
  }
  return attacks;
}

Bitboard KNIGHT_ATTACKS[64];
Bitboard KING_ATTACKS[64];

void init_leapers() {
  for (int sq = 0; sq < 64; ++sq) {
    // Knight
    Bitboard n = 0;
    // NNE etc.
    File f = File(sq % 8);
    Rank r = Rank(sq / 8);
    int jumps[] = {17, 15, 10, 6, -6, -10, -15, -17};
    for (int j : jumps) {
      int target = sq + j;
      if (target >= 0 && target < 64) {
        File tf = File(target % 8);
        Rank tr = Rank(target / 8);
        // Explicit casts
        if (std::abs((int)f - (int)tf) <= 2 && std::abs((int)r - (int)tr) <= 2)
          Bitboards::set_bit(n, Square(target));
      }
    }
    KNIGHT_ATTACKS[sq] = n;

    // King
    Bitboard k = 0;
    int steps[] = {8, -8, 1, -1, 9, 7, -7, -9};
    for (int s : steps) {
      int target = sq + s;
      if (target >= 0 && target < 64) {
        File tf = File(target % 8);
        Rank tr = Rank(target / 8);
        // Explicit casts
        if (std::abs((int)f - (int)tf) <= 1 && std::abs((int)r - (int)tr) <= 1)
          Bitboards::set_bit(k, Square(target));
      }
    }
    KING_ATTACKS[sq] = k;
  }
}

// Initialize masks and tables
void init() {
  init_leapers();
  for (int sq = 0; sq < 64; ++sq) {
    // Rook mask (excluding edges)
    Bitboard &r_mask = R_MASKS[sq];
    r_mask = 0;
    int r = sq / 8, f = sq % 8;
    for (int r2 = r + 1; r2 < 7; ++r2)
      Bitboards::set_bit(r_mask, make_square(File(f), Rank(r2)));
    for (int r2 = r - 1; r2 > 0; --r2)
      Bitboards::set_bit(r_mask, make_square(File(f), Rank(r2)));
    for (int f2 = f + 1; f2 < 7; ++f2)
      Bitboards::set_bit(r_mask, make_square(File(f2), Rank(r)));
    for (int f2 = f - 1; f2 > 0; --f2)
      Bitboards::set_bit(r_mask, make_square(File(f2), Rank(r)));

    // Bishop mask (excluding edges)
    Bitboard &b_mask = B_MASKS[sq];
    b_mask = 0;
    for (int r2 = r + 1, f2 = f + 1; r2 < 7 && f2 < 7; ++r2, ++f2)
      Bitboards::set_bit(b_mask, make_square(File(f2), Rank(r2)));
    for (int r2 = r + 1, f2 = f - 1; r2 < 7 && f2 > 0; ++r2, --f2)
      Bitboards::set_bit(b_mask, make_square(File(f2), Rank(r2)));
    for (int r2 = r - 1, f2 = f + 1; r2 > 0 && f2 < 7; --r2, ++f2)
      Bitboards::set_bit(b_mask, make_square(File(f2), Rank(r2)));
    for (int r2 = r - 1, f2 = f - 1; r2 > 0 && f2 > 0; --r2, --f2)
      Bitboards::set_bit(b_mask, make_square(File(f2), Rank(r2)));

    // Populate R_TABLE
    int r_idx_bits = Bitboards::popcount(r_mask);
    int r_indices = 1 << r_idx_bits;
    for (int i = 0; i < r_indices; ++i) {
      Bitboard occ = 0;
      int temp_idx = i;
      Bitboard temp_mask = r_mask;
      while (temp_mask) {
        Square s = Bitboards::pop_lsb(temp_mask);
        if (temp_idx & 1)
          Bitboards::set_bit(occ, s);
        temp_idx >>= 1;
      }

      int magic_idx = (occ * R_MAGICS[sq]) >> (64 - R_SHIFT[sq]);
      R_TABLE[sq][magic_idx] = get_rook_attacks_slow(Square(sq), occ);
    }

    // Populate B_TABLE
    int b_idx_bits = Bitboards::popcount(b_mask);
    int b_indices = 1 << b_idx_bits;
    for (int i = 0; i < b_indices; ++i) {
      Bitboard occ = 0;
      int temp_idx = i;
      Bitboard temp_mask = b_mask;
      while (temp_mask) {
        Square s = Bitboards::pop_lsb(temp_mask);
        if (temp_idx & 1)
          Bitboards::set_bit(occ, s);
        temp_idx >>= 1;
      }

      int magic_idx = (occ * B_MAGICS[sq]) >> (64 - B_SHIFT[sq]);
      B_TABLE[sq][magic_idx] = get_bishop_attacks_slow(Square(sq), occ);
    }
  }
}

Bitboard get_rook_attacks(Square sq, Bitboard occ) {
  // Fallback to slow for correctness verification
  return get_rook_attacks_slow(sq, occ);
  /*
  occ &= R_MASKS[sq];
  occ *= R_MAGICS[sq];
  occ >>= (64 - R_SHIFT[sq]);
  return R_TABLE[sq][occ];
  */
}

Bitboard get_bishop_attacks(Square sq, Bitboard occ) {
  return get_bishop_attacks_slow(sq, occ);
  /*
  occ &= B_MASKS[sq];
  occ *= B_MAGICS[sq];
  occ >>= (64 - B_SHIFT[sq]);
  return B_TABLE[sq][occ];
  */
}

Bitboard get_queen_attacks(Square sq, Bitboard occ) {
  return get_rook_attacks(sq, occ) | get_bishop_attacks(sq, occ);
}

Bitboard get_knight_attacks(Square sq) { return KNIGHT_ATTACKS[sq]; }
Bitboard get_king_attacks(Square sq) { return KING_ATTACKS[sq]; }

} // namespace Magic
} // namespace Prometheus
