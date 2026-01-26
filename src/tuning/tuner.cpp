#include "tuner.h"
#include "../eval/eval.h"
#include "../eval/pst.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace IroonRook {
namespace Tuning {

std::vector<DataPoint> Tuner::dataset;

void Tuner::init() { dataset.clear(); }

void Tuner::load_file(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Failed to open dataset: " << path << std::endl;
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    DataPoint dp;
    double result = 0.5;

    // Check for " | " delimiter
    size_t delimiter = line.find(" | ");
    if (delimiter != std::string::npos) {
      std::string res_str = line.substr(delimiter + 3);
      if (res_str == "1.0")
        result = 1.0;
      else if (res_str == "0.0")
        result = 0.0;
      else
        result = 0.5; // Default to draw if unknown or 0.5

      std::string fen = line.substr(0, delimiter);
      dp.board.set_fen(fen);
      dp.result = result;
      dataset.push_back(dp);
    } else {
      // Fallback for EPD/PGNish if needed, or skip
      // Current generation produces " | ".
      continue;
    }
  }
  std::cout << "Loaded " << dataset.size() << " positions." << std::endl;
}

double Tuner::static_eval_sigmoid(const Board &board) {
  int score = Eval::evaluate(board);
  return 1.0 / (1.0 + std::pow(10.0, -(double)score / 400.0));
}

double Tuner::compute_error() {
  double total_error = 0.0;
  for (const auto &dp : dataset) {
    double eval_prob = static_eval_sigmoid(dp.board);
    double diff = dp.result - eval_prob;
    total_error += diff * diff;
  }
  return total_error / dataset.size();
}

struct Parameter {
  int *ptr;
  std::string name;
};

void Tuner::run(int iterations) {
  std::vector<Parameter> params;

  // Register params
  for (int pt = 1; pt <= 5; ++pt) {
    params.push_back({&PST::Material[pt][0], "MatMG_" + std::to_string(pt)});
    params.push_back({&PST::Material[pt][1], "MatEG_" + std::to_string(pt)});
  }

  for (int i = 0; i < 64; ++i) {
    params.push_back({&PST::mg_pawn_table[i], "PST_P_MG_" + std::to_string(i)});
    params.push_back({&PST::eg_pawn_table[i], "PST_P_EG_" + std::to_string(i)});
    params.push_back(
        {&PST::mg_knight_table[i], "PST_N_MG_" + std::to_string(i)});
    params.push_back(
        {&PST::eg_knight_table[i], "PST_N_EG_" + std::to_string(i)});
  }

  std::cout << "Optimizing " << params.size() << " parameters..." << std::endl;

  double best_error = compute_error();
  std::cout << "Initial Error: " << best_error << std::endl;

  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> idx_dist(0, params.size() - 1);
  std::uniform_int_distribution<int> delta_dist(0, 1);

  for (int it = 1; it <= iterations; ++it) {
    int idx = idx_dist(rng);
    int delta = (delta_dist(rng) == 0) ? -1 : 1;

    *params[idx].ptr += delta;
    double new_error = compute_error();

    if (new_error < best_error) {
      best_error = new_error;
      if (it % 100 == 0)
        std::cout << "Iter " << it << " Error: " << best_error << std::endl;
    } else {
      *params[idx].ptr -= delta;
    }
  }
  std::cout << "Final Error: " << best_error << std::endl;

  // Output tuned values (Simplified dump)
  std::cout << "--- Tuned Material ---" << std::endl;
  for (int pt = 1; pt <= 5; ++pt) {
    std::cout << pt << ": " << PST::Material[pt][0] << ", "
              << PST::Material[pt][1] << std::endl;
  }
}

} // namespace Tuning
} // namespace IroonRook
