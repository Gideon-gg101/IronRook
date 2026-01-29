#include "zobrist.h"
#include <random>

namespace Prometheus {

namespace Zobrist {

uint64_t piece_keys[PIECE_NB][SQUARE_NB];
uint64_t en_passant_keys[FILE_NB];
uint64_t castle_keys[16];
uint64_t side_key;

void init() {
  // Use fixed seed for reproducibility
  std::mt19937_64 rng(123456789ULL);
  std::uniform_int_distribution<uint64_t> dist;

  for (int p = 0; p < PIECE_NB; ++p) {
    for (int s = 0; s < SQUARE_NB; ++s) {
      piece_keys[p][s] = dist(rng);
    }
  }

  for (int f = 0; f < FILE_NB; ++f) {
    en_passant_keys[f] = dist(rng);
  }

  for (int c = 0; c < 16; ++c) {
    castle_keys[c] = dist(rng);
  }

  side_key = dist(rng);
}

} // namespace Zobrist

} // namespace Prometheus
