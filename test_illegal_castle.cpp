#include "src/board/board.h"
#include "src/board/movegen.h"
#include <iostream>
#include <string>
#include <vector>


using namespace Prometheus;

int main() {
  Board board;
  board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  std::vector<std::string> moves = {
      "f1g3", "d7d5", "b1c3", "d5d4", "c3d4", "d8d4", "a2a3", "e7e5",
      "e2e3", "d4d6", "d1f3", "g8f6", "f1c4", "b8c6", "c4d3", "c8g4",
      "c3e4", "d6d3", "e4f6", "g7f6", "f3c6", "b7c6", "c2d3", "h8g8",
      "h1g1", "e8c8", "b2b3", "d8d3", "b3b4", "d3b3", "g1h1", "g4e6"};

  // Note: The move notation in the user request was algebraic.
  // I need to convert them touci or use a helper.
  // Nf3 = g1f3
  // d5 = d7d5
  // Nc3 = b1c3
  // d4 = d5d4
  // Nxd4 = c3d4
  // Qxd4 = d8d4
  // a3 = a2a3
  // e5 = e7e5
  // e3 = e2e3
  // Qd6 = d4d6
  // Qf3 = d1f3
  // Nf6 = g8f6
  // Bc4 = f1c4
  // Nc6 = b8c6
  // Bd3 = c4d3
  // Bg4 = c8g4
  // Ne4 = c3e4
  // Qxd3 = d6d3
  // Nxf6+ = e4f6
  // gxf6 = g7f6
  // Qxc6+ = f3c6
  // bxc6 = b7c6
  // cxd3 = c2d3
  // Rg8 = h8g8
  // Rg1 = h1g1
  // O-O-O = e8c8
  // b3 = b2b3
  // Rxd3 = d8d3
  // b4 = b2b4
  // Rb3 = d3b3
  // Rh1 = g1h1
  // Be6 = g4e6

  // Corrected move list from my manual mapping:
  std::vector<std::string> uci_moves = {
      "g1f3", "d7d5", "b1c3", "d5d4", "c3d4", "d8d4", "a2a3", "e7e5",
      "e2e3", "d4d6", "d1f3", "g8f6", "f1c4", "b8c6", "c4d3", "c8g4",
      "c3e4", "d6d3", "e4f6", "g7f6", "f3c6", "b7c6", "c2d3", "h8g8",
      "h1g1", "e8c8", "b2b3", "d8d3", "b2b4", "d3b3", "g1h1", "g4e6"};

  for (const std::string &m_str : uci_moves) {
    MoveList list;
    MoveGen::generate_all(board, list);
    bool found = false;
    for (int i = 0; i < list.count; ++i) {
      if (list.moves[i].to_uci() == m_str) {
        if (board.make_move(list.moves[i])) {
          found = true;
          break;
        }
      }
    }
    if (!found) {
      std::cerr << "Failed to make move: " << m_str << std::endl;
      return 1;
    }
  }

  std::cout << "Position after 16...Be6: " << board.to_fen() << std::endl;
  std::cout << "Castling rights (bitmask): " << board.castling_rights()
            << std::endl;

  MoveList list;
  MoveGen::generate_all(board, list);
  bool can_castle = false;
  for (int i = 0; i < list.count; ++i) {
    if (list.moves[i].to_uci() == "e1g1") {
      can_castle = true;
      break;
    }
  }

  if (can_castle) {
    std::cout << "BUG: White CAN still castle e1g1!" << std::endl;
  } else {
    std::cout << "FIXED: White cannot castle e1g1." << std::endl;
  }

  return 0;
}
