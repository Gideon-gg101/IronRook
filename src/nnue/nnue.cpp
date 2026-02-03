#include "nnue.h"
#include "simd.h"
#include <cstring>
#include <iostream>
#include <vector>


namespace NNUE {

// Global weights (to be loaded from file)
// For Half-KP:
// FeatureTransformer: [InputSize -> HiddenSize]
//   - Biases: [HiddenSize]
//   - Weights: [InputSize * HiddenSize]
// OutputLayer: [HiddenSize * 2 -> 1]
//   - Biases: [1]
//   - Weights: [HiddenSize * 2 * 1]

alignas(64) int16_t feature_biases[HIDDEN_SIZE];
alignas(64) int16_t
    feature_weights[INPUT_SIZE *
                    HIDDEN_SIZE]; // This is huge: 768 * 256 * 2 bytes = ~400KB
// Wait, for Half-KP with 41024 inputs (standard std::nnue), it's 20MB.
// But we are defining INPUT_SIZE as 768 in the header for now as a
// placeholder/custom simple net? If we use standard Half-KP, INPUT_SIZE should
// be 41024 (King-Piece buckets). Let's stick to the header definition for now
// and update later when we settle on architecture.

alignas(64) int16_t output_biases[OUTPUT_SIZE];
alignas(64) int16_t output_weights[HIDDEN_SIZE * 2 * OUTPUT_SIZE];

bool is_initialized = false;

void init() {
  if (is_initialized)
    return;
  // Initialize weights to 0 or random if training from scratch (but we will
  // load)
  std::memset(feature_biases, 0, sizeof(feature_biases));
  std::memset(feature_weights, 0, sizeof(feature_weights));
  std::memset(output_biases, 0, sizeof(output_biases));
  std::memset(output_weights, 0, sizeof(output_weights));
  is_initialized = true;
}

bool load_model(std::string filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Error: Cannot open NNUE model file: " << filename
              << std::endl;
    return false;
  }

  // TODO: Implement actual binary format loading
  // For now, just pretend to read
  // file.read((char*)feature_biases, sizeof(feature_biases));
  // ...

  std::cout << "NNUE model loaded: " << filename << std::endl;
  return true;
}

void refresh_accumulator(const Board &board, int color, Accumulator &acc) {
  // Full refresh of the accumulator
  // 1. Start with biases
  for (int i = 0; i < HIDDEN_SIZE; ++i) {
    acc.accumulation[i] = feature_biases[i];
  }

  // 2. Add active features
  // Loop through board pieces
  // This depends on the Input Feature Set definition (e.g. King-Relative Piece
  // Squares)

  // Stub for now
  acc.computed = true;
}

int evaluate(const Board &board) {
  if (!is_initialized)
    return 0; // Fallback

  // 1. Update Accumulators (Incremental or Refresh)
  // For now, just refresh
  Accumulator acc_white, acc_black;
  // refresh_accumulator(board, WHITE, acc_white);
  // refresh_accumulator(board, BLACK, acc_black);

  // 2. Forward Propagation (Hidden -> Output)
  // ReLU / Clipper on Accumulators

  // 3. Output Layer
  int32_t output = 0; // output_biases[0];

  // Add weighted sum from hidden layers (White + Black perspectives)

  return output / SCALE; // Scale back to centipawns
}

} // namespace NNUE
