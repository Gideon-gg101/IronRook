#include "tuning.h"
#include "../eval/eval.h"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace Prometheus {
namespace Tuning {

ParameterTuner GlobalTuner;

void ParameterTuner::add_param(const std::string &name, int *ptr, int min,
                               int max, int step) {
  params.push_back({name, ptr, min, max, *ptr, step});
}

void ParameterTuner::print_params() {
  for (const auto &p : params) {
    std::cout << "option name " << p.name << " type spin default "
              << p.default_val << " min " << p.min_val << " max " << p.max_val
              << std::endl;
  }
}

void ParameterTuner::register_params() {
  params.clear();
  add_param("BishopPairMG", &Eval::BishopPairMG, 0, 50);
  add_param("BishopPairEG", &Eval::BishopPairEG, 0, 100);
  add_param("OutpostBonus", &Eval::OutpostBonus, 0, 40);
  add_param("PermanentOutpostBonus", &Eval::PermanentOutpostBonus, 0, 50);
  add_param("HarassedByPawnPenalty", &Eval::HarassedByPawnPenalty, 0, 100);
  add_param("OpenFileBonus", &Eval::OpenFileBonus, 0, 30);
  add_param("SemiOpenFileBonus", &Eval::SemiOpenFileBonus, 0, 20);
  add_param("SpaceSquareBonus", &Eval::SpaceSquareBonus, 0, 15);

  for (int pt = KNIGHT; pt <= QUEEN; ++pt) {
    std::string name = "Mobility_" + std::to_string(pt);
    add_param(name, &Eval::MobilityBonus[pt], 0, 10);

    std::string h_name = "Hanging_" + std::to_string(pt);
    add_param(h_name, &Eval::HangingPiecePenalty[pt], 0, 1500);
  }

  // Singular Extensions
  add_param("SingularMargin", &Eval::SingularMarginMultiplier, 1, 4);
  add_param("SingularMinDepth", &Eval::SingularMinDepth, 6, 12);

  // Lazy Evaluation & Eval Noise Control
  add_param("LazyEvalMargin", &Eval::LazyEvalMargin, 50, 300);
  add_param("MaxNonMateEval", &Eval::MaxNonMateEval, 1500, 3000);

  // LMR (Late Move Reductions)
  add_param("LMRBaseReduction", &Eval::LMRBaseReduction, 50, 100);
  add_param("LMRDepthDivisor", &Eval::LMRDepthDivisor, 150, 300);
  add_param("LMRHistoryDivisor", &Eval::LMRHistoryDivisor, 1000, 4000);
  add_param("LMRPVReduction", &Eval::LMRPVReduction, 1, 4);
  add_param("LMRImprovingBonus", &Eval::LMRImprovingBonus, 0, 2);

  // Eval Hysteresis
  add_param("EvalHysteresis", &Eval::EvalHysteresis, 0, 15);

  // Internal Iterative Deepening (IID)
  add_param("IIDMinDepthPV", &Eval::IIDMinDepthPV, 4, 8);
  add_param("IIDMinDepthNonPV", &Eval::IIDMinDepthNonPV, 6, 10);
  add_param("IIDReductionPV", &Eval::IIDReductionPV, 1, 3);
  add_param("IIDReductionNonPV", &Eval::IIDReductionNonPV, 2, 4);
}

void ParameterTuner::export_params(const std::string &filename) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Error: Could not open file for writing: " << filename
              << std::endl;
    return;
  }

  file << "{\n";
  for (size_t i = 0; i < params.size(); ++i) {
    const auto &p = params[i];
    file << "  \"" << p.name << "\": " << *p.value_ptr;
    if (i < params.size() - 1)
      file << ",";
    file << "\n";
  }
  file << "}\n";
  file.close();
  std::cout << "Exported " << params.size() << " parameters to " << filename
            << std::endl;
}

void ParameterTuner::import_params(const std::string &filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Error: Could not open file for reading: " << filename
              << std::endl;
    return;
  }

  std::string line;
  int updated = 0;
  while (std::getline(file, line)) {
    // Simple parser: look for "name": value
    size_t quote1 = line.find('"');
    size_t quote2 = line.find('"', quote1 + 1);
    size_t colon = line.find(':', quote2);

    if (quote1 == std::string::npos || quote2 == std::string::npos ||
        colon == std::string::npos)
      continue;

    std::string name = line.substr(quote1 + 1, quote2 - quote1 - 1);
    std::string value_str = line.substr(colon + 1);

    // Remove commas, spaces, etc
    value_str.erase(std::remove_if(value_str.begin(), value_str.end(),
                                   [](char c) {
                                     return c == ',' || c == ' ' || c == '\t' ||
                                            c == '\r' || c == '\n';
                                   }),
                    value_str.end());

    if (value_str.empty())
      continue;

    int value = std::stoi(value_str);

    // Find and update parameter
    for (auto &p : params) {
      if (p.name == name) {
        // Clamp to bounds
        value = std::max(p.min_val, std::min(p.max_val, value));
        *p.value_ptr = value;
        updated++;
        break;
      }
    }
  }

  file.close();
  std::cout << "Imported " << updated << " parameters from " << filename
            << std::endl;
}

void ParameterTuner::run_spsa(int iterations) {
  std::cout << "SPSA tuning not yet implemented." << std::endl;
}

} // namespace Tuning
} // namespace Prometheus
