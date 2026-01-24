#ifndef TUNING_H
#define TUNING_H

#include <string>
#include <vector>

namespace Prometheus {
namespace Tuning {

struct TunableParam {
  std::string name;
  int *value_ptr;
  int min_val;
  int max_val;
  int default_val;
  int step;
};

class ParameterTuner {
public:
  void add_param(const std::string &name, int *ptr, int min, int max,
                 int step = 1);
  void register_params();
  void print_params();

  std::vector<TunableParam> &get_params_list() { return params; }

  // Future: SPSA implementation
  void run_spsa(int iterations);

private:
  std::vector<TunableParam> params;
};

extern ParameterTuner GlobalTuner;

} // namespace Tuning
} // namespace Prometheus

#endif
