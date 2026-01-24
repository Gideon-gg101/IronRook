#ifndef BOOK_H
#define BOOK_H

#include "../core/types.h"
#include "board.h"
#include <string>
#include <vector>

namespace Prometheus {

struct BookEntry {
  uint64_t key;
  uint16_t move;
  uint16_t weight;
  uint32_t learn;
};

class Book {
public:
  Book() = default;

  // Load book from file
  bool load(const std::string &path);

  // Probe the book for a move
  // Returns MOVE_NONE if no move found
  Move probe(const Board &board, bool pick_best = false);

private:
  std::string file_path;
  // We don't load the whole file into memory to save RAM,
  // we use binary search on file or mmap if needed.
  // For simplicity/performance trade-off, we can load it if < 100MB.
  // For now, let's assume we read from file or load small books.
  // Standard Polyglot books can be large (hundreds of MB).
  // `std::ifstream` with seekg is fine for standard usage.

  Move convert_move(uint16_t poly_move, const Board &board);
};

namespace Polyglot {
// Compute the Polyglot key for the board.
// Currently uses Engine's internal keys (Custom mode).
// TODO: Implement standard Polyglot constants.
uint64_t compute_key(const Board &board);
} // namespace Polyglot

} // namespace Prometheus

#endif // BOOK_H
