# Prometheus Engine - Directory Structure

```
Prometheus/
├── README.md                    # Main documentation
├── CMakeLists.txt              # Build configuration
├── Dockerfile                  # Docker container
│
├── configs/                    # Configuration files
│   ├── config_ci.json         # CI/CD configuration
│   ├── spsa_weekly.json       # Weekly SPSA tuning
│   └── texel_material.json    # Material value tuning
│
├── docs/                       # Documentation
│   ├── ARCHITECTURE.md        # Code structure & design
│   ├── BENCHMARKING.md        # Performance tracking
│   ├── MAINTENANCE.md         # Update schedules
│   ├── TUNING.md             # Parameter tuning guide
│   ├── GITHUB_ACTIONS_SETUP.md # CI/CD setup
│   └── benchmark_history.csv  # Historical NPS data
│
├── src/                        # Source code
│   ├── board/                 # Board representation
│   │   ├── board.cpp/h       # Board state
│   │   ├── movegen.cpp/h     # Move generation
│   │   ├── perft.cpp/h       # Perft testing
│   │   └── book.cpp/h        # Opening book
│   │
│   ├── search/                # Search algorithms
│   │   ├── search.cpp/h      # Alpha-beta search
│   │   ├── thread_pool.cpp/h # Threading
│   │   ├── time_manager.cpp/h # Time management
│   │   └── learning.cpp/h    # Learning
│   │
│   ├── eval/                  # Evaluation
│   │   ├── eval.cpp/h        # Main evaluation
│   │   ├── pawn_eval.cpp/h   # Pawn structure
│   │   └── endgame.cpp/h     # Endgame knowledge
│   │
│   ├── core/                  # Core components
│   │   ├── bitboard.cpp/h    # Bitboard operations
│   │   ├── tt.cpp/h          # Transposition table
│   │   ├── zobrist.cpp/h     # Zobrist hashing
│   │   └── magic.cpp/h       # Magic bitboards
│   │
│   ├── interface/             # External interfaces
│   │   ├── uci.cpp/h         # UCI protocol
│   │   └── selfplay.cpp/h    # Self-play
│   │
│   ├── tuning/                # Parameter tuning
│   │   ├── tuner.cpp/h       # Texel tuning
│   │   └── tuning.cpp/h      # Parameter registration
│   │
│   ├── main.cpp              # Entry point
│   └── bench.cpp             # Benchmark suite
│
├── scripts/                   # Python utilities
│   ├── spsa_tuner.py         # SPSA parameter optimization
│   ├── texel_tuner.py        # Texel tuning
│   ├── continuous_selfplay.py # Automatic testing
│   ├── sprt_test.py          # Regression testing
│   ├── run_test_suites.py    # EPD test runner
│   ├── run_perft.py          # Perft validation
│   ├── sensitivity_analysis.py # Parameter analysis
│   ├── multi_objective_spsa.py # Multi-objective tuning
│   └── README.md             # Scripts documentation
│
├── tests/                     # Test suites
│   └── suites/               # EPD test positions
│       ├── tactical.epd      # Tactical puzzles
│       ├── endgame.epd       # Endgame positions
│       ├── strategic.epd     # Strategic positions
│       ├── mate.epd          # Mate-in-N
│       ├── fortress.epd      # Fortress draws
│       ├── zugzwang.epd      # Zugzwang positions
│       └── pruning.epd       # Pruning stress tests
│
├── tools/                     # Development tools
│   └── openbench_setup.py    # OpenBench integration
│
├── Games/                     # PGN game collection
│   └── *.pgn                 # Training/opening book games
│
├── cloud_data/               # Cloud-generated data
│   └── *.epd                # Self-play positions
│
├── build/                    # Build artifacts (gitignored)
│   └── IronRook.exe         # Compiled engine
│
└── .github/                  # GitHub configuration
    └── workflows/            # CI/CD workflows
        └── regression_tests.yml # Automated testing
```

## Key Directories

### `/src` - Source Code
All C++20 engine code organized by functionality:
- **board/**: Board representation and move generation
- **search/**: Search algorithms and time management
- **eval/**: Evaluation function components
- **core/**: Low-level data structures
- **interface/**: UCI and external interfaces
- **tuning/**: Parameter optimization

### `/scripts` - Automation
Python scripts for tuning, testing, and CI/CD:
- SPSA/Texel parameter optimization
- Self-play testing and regression detection
- Test suite runners
- Analysis tools

### `/tests` - Test Suites
EPD test positions (85 total):
- Tactical puzzles
- Endgame studies
- Strategic positions
- Edge cases (zugzwang, fortress, etc.)

### `/docs` - Documentation
Complete engine documentation:
- Architecture and design
- Tuning guides
- Maintenance schedules
- Performance benchmarks

### `/configs` - Configuration
Tuning and CI/CD configurations:
- SPSA parameter configs
- Texel tuning configs
- CI testing configs

### `/tools` - Development
Development utilities and integrations:
- OpenBench setup
- Debugging tools
- Custom utilities

## Build Artifacts

```
build/
├── IronRook.exe              # Main executable
├── CMakeCache.txt            # CMake configuration
├── CMakeFiles/               # Build intermediates
└── *.obj                     # Object files
```

## Git Structure

```
.git/                         # Version control
.gitignore                    # Ignored files
.gitattributes                # Git attributes
.github/workflows/            # CI/CD pipelines
```

## File Naming Conventions

- **C++ Source**: `snake_case.cpp`
- **Headers**: `snake_case.h`
- **Python**: `snake_case.py`
- **Configs**: `lowercase_descriptive.json`
- **Docs**: `UPPERCASE.md`
- **Tests**: `lowercase.epd`

## Total Project Size

- **Source Lines**: ~15,000 LOC
- **Files**: ~80 total
- **Build Size**: ~2MB (Release)
- **Repository**: ~50MB (with games)

---

**Last Updated**: 2026-01-31
