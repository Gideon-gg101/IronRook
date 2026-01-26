#ifndef TT_H
#define TT_H

#include "types.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace Prometheus {

// Packed 16-byte TT Entry
struct TTEntry {
  uint64_t key;
  uint64_t data;
  // Data layout:
  // Move: 16 bits
  // Score: 16 bits
  // Eval: 16 bits
  // Depth: 8 bits
  // Type: 4 bits
  // Gen: 4 bits
  // Total: 64 bits matches uint64_t

  Move move() const { return Move((uint16_t)(data & 0xFFFF)); }
  int16_t score() const { return (int16_t)((data >> 16) & 0xFFFF); }
  int16_t eval() const { return (int16_t)((data >> 32) & 0xFFFF); }
  uint8_t depth() const { return (uint8_t)((data >> 48) & 0xFF); }
  uint8_t type() const { return (uint8_t)((data >> 56) & 0xF); }
  uint8_t gen() const { return (uint8_t)((data >> 60) & 0xF); }

  void save(Move m, int16_t s, int16_t e, uint8_t d, uint8_t t, uint8_t g) {
    data = (uint64_t)m.data | ((uint64_t)(uint16_t)s << 16) |
           ((uint64_t)(uint16_t)e << 32) | ((uint64_t)d << 48) |
           ((uint64_t)(t & 0xF) << 56) | ((uint64_t)(g & 0xF) << 60);
  }
};

enum TTBound {
  BOUND_NONE = 0,
  BOUND_UPPER = 1,
  BOUND_LOWER = 2,
  BOUND_EXACT = 3
};

constexpr int MATE_SCORE = 30000;
constexpr int MATE_MAX_PLY = 128; // Assuming 128 max ply

// Mate Score Normalization
// We store mate scores relative to current ply to avoid off-by-one errors
// when using TT entries from different depths/paths.
inline int16_t score_to_tt(int score, int ply) {
  if (score >= MATE_SCORE - MATE_MAX_PLY) {
    return (int16_t)(score + ply);
  }
  if (score <= -MATE_SCORE + MATE_MAX_PLY) {
    return (int16_t)(score - ply);
  }
  return (int16_t)score;
}

inline int score_from_tt(int16_t score, int ply) {
  if (score >= MATE_SCORE - MATE_MAX_PLY) {
    return (int)(score - ply);
  }
  if (score <= -MATE_SCORE + MATE_MAX_PLY) {
    return (int)(score + ply);
  }
  return (int)score;
}
constexpr int MATE_BOUND = 29000;

class TranspositionTable {
public:
  TranspositionTable(size_t size_mb = 16);
  ~TranspositionTable();

  void resize(size_t size_mb);
  void clear();

  // Thread-safe probe
  // Returns true if found and matches key
  // Fills entry data
  bool probe(uint64_t key, TTEntry &entry) const;

  // Thread-safe save
  void save(uint64_t key, int16_t score, uint8_t bound, uint8_t depth,
            Move move, int16_t eval, int ply);

  void set_generation(uint8_t g) { generation = g; }
  uint8_t get_generation() const { return generation; }

  int hashfull() const;

private:
  // Use raw pointer for alignment control if needed, but vector is okay.
  // Ideally, use a cluster struct.
  struct Cluster {
    TTEntry entry[3];    // Bucket of 3? or just 1?
    uint8_t padding[16]; // Pad to 64 bytes? 3*16=48. 48+16=64.
  };
  // Simplest: std::vector<TTEntry>
  std::vector<TTEntry> table;
  size_t size;
  uint8_t generation = 0;
};

extern TranspositionTable TT;

} // namespace Prometheus

#endif // TT_H
