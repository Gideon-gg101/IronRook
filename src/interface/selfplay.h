#ifndef SELFPLAY_H
#define SELFPLAY_H

#include <string>

namespace Prometheus {
void run_selfplay(int games, int depth, const std::string &outputFile = "");
}

#endif
