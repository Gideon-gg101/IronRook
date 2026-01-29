#ifndef TUNER_H
#define TUNER_H

#include "../board/board.h"
#include <iostream>
#include <string>
#include <vector>

namespace Prometheus {
namespace Tuning {

struct DataPoint {
  Board board;
  double result; // 0.0, 0.5, 1.0
};

class Tuner {
public:
  static void init();
  static void load_file(const std::string &path);
  static void run(int iterations);

private:
  static std::vector<DataPoint> dataset;
  static double compute_error();
  static double static_eval_sigmoid(const Board &board);
};

} // namespace Tuning
} // namespace Prometheus

#endif
