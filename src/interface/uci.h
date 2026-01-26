#ifndef UCI_H
#define UCI_H

#include "../board/board.h"
#include "../search/search.h"
#include <string>

namespace IroonRook {

namespace UCI {

void loop();

// Parsers
void position(const std::string &command, Board &board);
void go(const std::string &command, Board &board);

} // namespace UCI

} // namespace IroonRook

#endif // UCI_H
