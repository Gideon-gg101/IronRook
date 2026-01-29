cmake_minimum_required(VERSION 3.15)
project(IronRook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Release build by default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Optimization flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
elseif(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG /GL")
  set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files (excluding Syzygy tablebase for now due to missing dependencies)
set(SOURCES
  src/main.cpp
  src/bench.cpp
  src/board/board.cpp
  src/board/book.cpp
  src/board/movegen.cpp
  src/board/perft.cpp
  src/core/bitboard.cpp
  src/core/magic.cpp
  src/core/tt.cpp
  src/core/zobrist.cpp
  src/eval/endgame.cpp
  src/eval/eval.cpp
  src/eval/pawn_eval.cpp
  src/interface/selfplay.cpp
  src/interface/uci.cpp
  src/search/learning.cpp
  src/search/search.cpp
  src/search/thread_pool.cpp
  src/search/time_manager.cpp
  src/tuning/tuner.cpp
  src/tuning/tuning.cpp
)

# Create executable
add_executable(IronRook ${SOURCES})

# Link threading library
find_package(Threads REQUIRED)
target_link_libraries(IronRook PRIVATE Threads::Threads)

# Platform-specific libraries
if(WIN32)
  target_link_libraries(IronRook PRIVATE ws2_32)
endif()
