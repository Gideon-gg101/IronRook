# Prometheus Engine - Architecture

Technical overview of the engine's code structure and design decisions.

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     UCI Interface                        │
│                    (interface/uci.cpp)                   │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│                   Thread Pool                            │
│              (search/thread_pool.cpp)                    │
└────────┬──────────────────────────────────┬─────────────┘
         │                                  │
         ▼                                  ▼
┌─────────────────────┐          ┌─────────────────────┐
│   Search Worker     │          │   Time Manager      │
│  (search/search.cpp)│          │ (search/time_mgr.cpp)│
└──────┬──────────────┘          └─────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────┐
│                  Alpha-Beta Search                       │
│         (IID, SE, NMP, LMR, Futility, Probcut)          │
└────────┬────────────────────────────────┬───────────────┘
         │                                │
         ▼                                ▼
┌─────────────────────┐          ┌─────────────────────┐
│   Move Generator    │          │    Evaluation       │
│ (board/movegen.cpp) │          │   (eval/eval.cpp)   │
└──────┬──────────────┘          └──────┬──────────────┘
       │                                │
       ▼                                ▼
┌─────────────────────┐          ┌─────────────────────┐
│    Board State      │          │   Eval Components   │
│  (board/board.cpp)  │          │ (pawn, king, endgame)│
└──────┬──────────────┘          └─────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────┐
│              Core Components                             │
│    (Bitboards, TT, Zobrist, Magic, Types)               │
└─────────────────────────────────────────────────────────┘
```

---

## Module Breakdown

### 1. UCI Interface (`src/interface/`)

**Files**:
- `uci.cpp/h` - UCI protocol implementation

**Responsibilities**:
- Parse UCI commands (`uci`, `isready`, `position`, `go`, `setoption`)
- Manage engine options
- Output info/bestmove
- Handle pondering

**Key Functions**:
```cpp
void UCI::loop();                    // Main UCI loop
void UCI::position(const string&);   // Set position
void UCI::go(const SearchLimits&);   // Start search
```

---

### 2. Search (`src/search/`)

#### 2.1 Thread Pool (`thread_pool.cpp/h`)

**Responsibilities**:
- Manage search threads (1-128 threads)
- Coordinate parallel search
- Distribute work across threads

**Key Classes**:
```cpp
class ThreadPool {
    void start_search(const Board&, const SearchLimits&);
    void stop();
    vector<SearchWorker*> workers;
};

class SearchWorker {
    void iter_deep();  // Iterative deepening loop
    void search();     // Worker thread main loop
};
```

#### 2.2 Main Search (`search.cpp/h`)

**Responsibilities**:
- Alpha-beta search with PVS
- Implementing all search enhancements
- Move ordering
- History heuristics

**Key Functions**:
```cpp
int alpha_beta(Board&, int alpha, int beta, int depth, bool pvNode);
int quiescence(Board&, int alpha, int beta);
void update_killers(Move, int ply);
void update_history(Move, int depth, bool good);
```

**Search Flow**:
```
iter_deep() 
  └─> alpha_beta_root()
       └─> alpha_beta()
            ├─> Null Move Pruning
            ├─> Singular Extensions
            ├─> Internal Iterative Deepening (IID)
            ├─> Multi-Cut
            ├─> Futility Pruning
            ├─> Late Move Reductions (LMR)
            ├─> Probcut
            └─> quiescence()
```

#### 2.3 Time Manager (`time_manager.cpp/h`)

**Responsibilities**:
- Calculate time allocation per move
- Phase-aware time management
- EMA smoothing across moves
- Panic/extension handling

**Key Features**:
```cpp
void init(const SearchLimits&, Color, const Board&);
bool should_stop(uint64_t nodes);  // Check every 2048 nodes
void extend_time(double factor);   // Volatility extensions
```

**Time Allocation**:
- Opening/Midgame: 1/20 of remaining time
- Transition: 1/15 of remaining time
- Endgame: 1/10 of remaining time
- EMA smoothing: `alpha = 0.7`

---

### 3. Board Representation (`src/board/`)

#### 3.1 Board State (`board.cpp/h`)

**Representation**:
- **Bitboards**: 8 (piece types) + 2 (colors) = 10 bitboards
- **Mailbox**: 64-element array for fast `piece_on(square)`
- **State**: side to move, castling rights, en passant, half-move clock

**Key Methods**:
```cpp
void make_move(Move);
void take_move();
bool is_legal(Move);
bool in_check(Color);
Bitboard attackers_to(Square, Bitboard occupied);
```

**Performance**: Incremental updates (no copy on make_move)

#### 3.2 Move Generation (`movegen.cpp/h`)

**Types**:
```cpp
enum MoveGenType {
    ALL_MOVES,      // Legal moves (pseudo-legal + check filter)
    CAPTURES,       // Captures only
    QUIETS,         // Non-captures
    EVASIONS,       // Check evasions
    LEGAL           // Fully legal (use sparingly)
};
```

**Magic Bitboards**:
- Rook attacks: Fancy magic
- Bishop attacks: Fancy magic
- Precomputed attack tables

---

### 4. Evaluation (`src/eval/`)

#### 4.1 Main Eval (`eval.cpp/h`)

**Tapering**:
```cpp
int evaluate(const Board& board) {
    ScorePair score = {0, 0};  // {midgame, endgame}
    
    score += evaluate_material(board);
    score += evaluate_pawns(board, pawn_table);
    score += evaluate_pieces(board);
    score += evaluate_king_safety(board);
    
    EndgameScore es = evaluate_endgame(board, score);
    int final = taper(score, phase);
    final = (final * es.scale_factor) / 64;
    
    return clamp(final);
}
```

**Phase Calculation**:
```cpp
phase = TotalPhase - PawnPhase * pawn_count 
                   - KnightPhase * knight_count 
                   - BishopPhase * bishop_count 
                   - RookPhase * rook_count 
                   - QueenPhase * queen_count;
```

#### 4.2 Pawn Eval (`pawn_eval.cpp/h`)

**Features**:
- Passed pawns (bonus by rank, support)
- Doubled pawns (penalty)
- Isolated pawns (penalty)
- Backward pawns (penalty)
- Pawn chains (bonus)
- Candidate passed pawns

**Pawn Hash Table**: 16K entries

#### 4.3 King Safety (`king_safety.cpp` - in eval.cpp)

**Attack Units**:
```cpp
int attacks = 0;
attacks += count_attacks(queen) * 4;
attacks += count_attacks(rook) * 2;
attacks += count_attacks(bishop) * 1;
attacks += count_attacks(knight) * 1;

int safety = -(attacks * attacks) / 256;
```

**Pawn Shield**: Bonus for pawns in front of king

#### 4.4 Endgame Knowledge (`endgame.cpp/h`)

**Special Cases**:
- KBNvK (mate with bishop+knight)
- KPvK (pawn endgames, opposition)
- KRPvKR (Philidor, Lucena)
- OCB (Opposite-colored bishops - drawish)
- Fortress detection

**Scaling Factors**:
```cpp
if (opposite_colored_bishops && no_other_pieces)
    scale_factor = 32;  // 50% scaling
```

---

### 5. Core Components (`src/core/`)

#### 5.1 Bitboards (`bitboard.cpp/h`)

**Operations**:
```cpp
constexpr Bitboard set_bit(Square);
constexpr Bitboard clear_bit(Square);
constexpr int popcount(Bitboard);
constexpr Square lsb(Bitboard);
constexpr Square msb(Bitboard);
```

**Precomputed**:
- Pawn attacks
- Knight attacks
- King attacks
- Between bitboards
- Line bitboards

#### 5.2 Transposition Table (`tt.cpp/h`)

**Entry Structure**:
```cpp
struct TTEntry {
    uint64_t key;     // Zobrist hash
    int16_t score;    // Evaluation
    Move best_move;   // Best move found
    uint8_t depth;    // Search depth
    uint8_t age;      // Generation
    uint8_t bound;    // EXACT, LOWER, UPPER
};
```

**Replacement Scheme**:
- Always replace if depth >= old_depth
- Age-based replacement for collisions
- 4-bucket system for better hit rate

**Size**: Configurable via UCI (default 128MB)

#### 5.3 Zobrist Hashing (`zobrist.cpp/h`)

**Hash Components**:
```cpp
hash ^= zobrist_piece[piece][square];
hash ^= zobrist_castling[castling_rights];
hash ^= zobrist_en_passant[ep_square];
hash ^= zobrist_side_to_move;
```

**Incremental Updates**: Hash updated on make/take move

---

### 6. Tuning (`src/tuning/`)

**Files**:
- `tuning.cpp/h` - Parameter registration
- `tuner.cpp/h` - Texel tuning implementation

**Parameter System**:
```cpp
struct TunableParam {
    string name;
    int* value_ptr;
    int default_value;
    int min_value;
    int max_value;
};

void register_param(const string& name, int* ptr, int def, int min, int max);
```

**Current**: 42 tunable parameters

---

## Data Structures

### Move Representation
```cpp
class Move {
    uint16_t data;  // 16 bits
    // bits 0-5:   from square (0-63)
    // bits 6-11:  to square (0-63)
    // bits 12-13: promotion piece
    // bits 14-15: move type (normal, castle, en passant, promotion)
};
```

### Score Pair (Tapered Eval)
```cpp
struct ScorePair {
    int mg;  // Midgame score
    int eg;  // Endgame score
    
    ScorePair operator+(ScorePair other);
    int taper(int phase);
};
```

---

## Performance Optimizations

### 1. Lazy Evaluation
```cpp
if (alpha >= beta + LazyEvalMargin)
    return alpha;  // Fail high without full eval
```

### 2. Bulk Counting
```cpp
// Count pawns in single popcount
int pawn_count = popcount(pieces(PAWN, WHITE));
```

### 3. Move Ordering
```
1. Hash move (from TT)
2. Winning captures (MVV-LVA)
3. Killer moves (2 per ply)
4. Counter moves
5. History heuristic (sorted)
6. Quiets (sorted by history)
```

### 4. Parallel Search
- Shared TT across threads
- No locking during search (atomic TT writes)
- Per-thread history tables

---

## Build System

### CMake Configuration
```cmake
project(Prometheus CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")
```

### Compiler Optimizations
- **-O3**: Maximum optimization
- **-march=native**: CPU-specific optimizations
- **-flto**: Link-time optimization
- **NDEBUG**: Disable asserts in release

---

## Testing Infrastructure

### 1. Perft Testing
```cpp
uint64_t perft(Board& board, int depth) {
    if (depth == 0) return 1;
    uint64_t nodes = 0;
    for (Move m : generate_moves(board))
        nodes += perft(board_after(m), depth - 1);
    return nodes;
}
```

### 2. EPD Test Suites
```
Format: FEN; bm <best_move>; id "<description>";
Runner: scripts/run_test_suites.py
```

### 3. Self-Play SPRT
```python
# scripts/sprt_test.py
llr = calculate_llr(wins, losses, draws)
if llr >= upper_bound: return "PASS"
if llr <= lower_bound: return "FAIL"
```

---

## Memory Layout

### Stack Usage (per thread)
- Board state: ~500 bytes
- Move list: ~5KB (max 256 moves × 128 ply)
- History tables: ~32KB
- Killer moves: ~2KB
- PV line: ~1KB

**Total per thread**: ~40KB

### Heap Usage
- Transposition Table: 128MB-64GB (configurable)
- Pawn Hash Table: 16KB
- Thread pool: ~40KB × thread_count

---

## Concurrency Model

### Lock-Free Design
- TT writes: Atomic compare-exchange
- Search: No locks (thread-local data)
- UCI: Single-threaded command loop

### Thread Safety
- ✅ TT: Thread-safe (atomic operations)
- ✅ Search: Independent worker threads
- ❌ Board: Not thread-safe (per-thread copy)

---

## Future Architecture (v2.0 - NNUE)

```
┌────────────────────────────────────┐
│         NNUE Evaluation            │
│   (768→256→1 neural network)       │
└────────┬───────────────────────────┘
         │
         ▼
┌────────────────────────────────────┐
│      Incremental Updates           │
│  (Accumulator-based inference)     │
└────────────────────────────────────┘
         │
         ▼
┌────────────────────────────────────┐
│      SIMD Optimizations            │
│     (AVX2/AVX-512 intrinsics)      │
└────────────────────────────────────┘
```

**Performance Impact**: 
- HCE: 600K NPS (current)
- NNUE: 500K NPS (expected, but +200-300 Elo)

---

## Code Metrics

| Metric | Count |
|--------|-------|
| Total Lines | ~15,000 |
| Source Files | 25 |
| Header Files | 20 |
| Functions | ~300 |
| Classes | ~15 |
| UCI Parameters | 42 |

---

## Design Principles

1. **Performance First**: Every decision optimized for speed
2. **Simplicity**: Avoid over-engineering
3. **Testability**: Comprehensive test coverage
4. **Tunability**: All magic numbers are UCI parameters
5. **Portability**: Windows/Linux/Mac support

---

**Last Updated**: 2026-01-31  
**Version**: v1.2.0
