#ifndef NNUE_H
#define NNUE_H

#include "../board/board.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>


#ifdef __AVX2__
#include <immintrin.h>
#endif

// NNUE Architecture: Half-KP (King-Piece)
// Input: 768 (King bucket * 12 * 64) -> Simplified to relative King-Piece (32
// buckets x 64 squares x 12 pieces)?? Wait, Standard Half-KP is 41024 inputs
// (64 king squares * 641 [10 * 64 + 1])... Actually, for Half-KP: Input size =
// 12 * 64 = 768 features per piece per king. Standard architecture:
// (Accumulator[768] -> Hidden[256]) x 2 -> Output[1]

namespace NNUE {

// Constants for Half-KP Architecture
constexpr int INPUT_SIZE = 768; // 12 piece types * 64 squares
constexpr int HIDDEN_SIZE = 256;
constexpr int OUTPUT_SIZE = 1;

// Fixed-point quantization
constexpr int QA = 255;
constexpr int QB = 64;
constexpr int QAB = QA * QB;
constexpr int SCALE = 400;

// Accumulator struct (updated incrementally)
struct Accumulator {
  alignas(64) int16_t accumulation[HIDDEN_SIZE];
  bool computed;
};

// State maintained per node/ply
struct NNUE_State {
  Accumulator accumulators[2]; // 0 = White, 1 = Black
  // Pointers/indices for dirty tracking can go here
};

// Main Interface
void init();
bool load_model(std::string filename);
int evaluate(const Board &board);

// Incremental Updates (to be called from Board::make_move)
// For now, we might just do a full refresh to start, then optimize.
void refresh_accumulator(const Board &board, int color, Accumulator &acc);

} // namespace NNUE

#endif // NNUE_H
