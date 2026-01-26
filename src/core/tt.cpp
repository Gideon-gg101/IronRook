#include "tt.h"
#include <cstring>
#include <iostream>

namespace IroonRook {

TranspositionTable TT; // Global Instance

TranspositionTable::TranspositionTable(size_t size_mb) { resize(size_mb); }

TranspositionTable::~TranspositionTable() {}

void TranspositionTable::resize(size_t size_mb) {
  size_t bytes = size_mb * 1024 * 1024;
  size_t count = bytes / sizeof(TTEntry);
  table.resize(count);
  size = count;
  clear();
  // std::cout << "TT resized to " << size_mb << "MB (" << count << " entries)"
  //           << std::endl;
}

void TranspositionTable::clear() {
  memset(table.data(), 0, table.size() * sizeof(TTEntry));
}

bool TranspositionTable::probe(uint64_t key, TTEntry &outEntry) const {
  size_t index = key % size;
  const TTEntry &entry = table[index];

  // Read key first (atomic-ish on x64)
  if (entry.key == key) {
    outEntry = entry;
    return true;
  }
  return false;
}

void TranspositionTable::save(uint64_t key, int16_t score, uint8_t bound,
                              uint8_t depth, Move move, int16_t eval, int ply) {
  size_t index = key % size;
  TTEntry &entry = table[index];

  // Mate score normalization: transform to ply-independent score
  if (score > MATE_BOUND)
    score += ply;
  else if (score < -MATE_BOUND)
    score -= ply;

  // Replacement strategy:
  // 1. Always replace if different key
  // 2. Same key: Replace if depth >= entry depth
  // 3. Same key: Replace if entry is from a previous generation

  if (entry.key != key || depth >= entry.depth() || entry.gen() != generation) {
    entry.key = key;
    entry.save(move, score, eval, depth, bound, generation);
  }
}

int TranspositionTable::hashfull() const {
  // Estimates permill full
  int used = 0;
  for (int i = 0; i < 1000; ++i) {
    if (table[i].key != 0)
      used++;
    // Sample first 1000
  }
  return used;
}

} // namespace IroonRook
