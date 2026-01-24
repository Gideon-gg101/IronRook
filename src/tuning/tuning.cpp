#include "tuning.h"
#include "../eval/eval.h"
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
}

void ParameterTuner::run_spsa(int iterations) {
  std::cout << "SPSA tuning not yet implemented." << std::endl;
}

} // namespace Tuning
} // namespace Prometheus
