#ifndef SYZYGY_H
#define SYZYGY_H

#include "../board/board.h"
#include <string>

namespace Prometheus {
namespace Syzygy {

// Initialize Syzygy tablebases (load files)
// Returns true if successful (found some files)
bool init(const std::string &path);

// Probe the tablebase at the root
// Returns a score in centipawns (e.g. +MATE, -MATE, 0 for draw) if found.
// Returns -32001 (or some INVALID constant) if not found.
// Also useful for WDL.
// Standard WDL values:
// 0=Loss, 1=Blessed Loss, 2=Draw, 3=Cursed Win, 4=Win
int probe_root_wdl(const Board &board);

// Probe WDL during search
int probe_wdl(const Board &board);

// Convert TB_RESULT to engine score
// bound: alpha/beta/exact?
// Typically returns:
// > 0 for WIN
// < 0 for LOSS
// 0 for DRAW
// Returns -32001 (INVALID) if TB_RESULT_FAILED or if result is not decisive
// (should not happen if probed)
int wdl_to_score(int wdl, int ply);

} // namespace Syzygy
} // namespace Prometheus

#endif // SYZYGY_H
