# Prometheus Engine - Maintenance Guide

This guide outlines all items that require regular updates to keep the engine performing optimally.

## 🔄 Continuous Maintenance Schedule

### 1. Parameter Tuning (Weekly/Monthly)

**Location**: `src/tuning/tuning.cpp`, `scripts/spsa_tuner.py`, `scripts/texel_tuner.py`

#### What to Update
- ✅ Run SPSA tuning after major changes
- ✅ Re-run Texel tuning when evaluation changes
- ✅ Update parameter ranges in `tuner_params.json`
- ✅ Track parameter evolution in version control

#### Commands
```bash
# Weekly SPSA run (quick)
python scripts/spsa_tuner.py --config configs/spsa_weekly.json --iterations 50

# Monthly comprehensive SPSA
python scripts/spsa_tuner.py --config configs/spsa_production.json --iterations 200

# Texel tuning (after eval changes)
python scripts/texel_tuner.py --positions data/quiet_positions.epd --epochs 100
```

#### Expected Results
- +10-50 Elo per tuning cycle
- Converged parameters (< 5% change between runs)
- Improved tactical/positional test scores

---

### 2. Test Suite Expansion (Bi-weekly)

**Location**: `tests/suites/*.epd`

#### What to Update
- ✅ Add positions where engine fails
- ✅ Add new tactical/endgame studies
- ✅ Update baseline pass rates after improvements
- ✅ Remove obsolete tests that are now trivial
- ✅ Balance suite difficulty

#### Current Status
- **Total Positions**: 85
- **Target**: 150+ positions (comprehensive coverage)
- **Categories**: Tactical, Endgame, Fortress, Zugzwang, Mate, Strategic, Pruning

#### Adding New Positions
```epd
# Format: FEN; bm <best_move>; id "<description>";
r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 0 1; bm Bxf7+; id "Legal's mate";
```

#### Running Tests
```bash
# Run all suites
python scripts/run_test_suites.py build/IronRook.exe \
    --suites tests/suites/*.epd \
    --time 5000 \
    --min-pass-rate 75.0

# Individual suite
python scripts/run_test_suites.py build/IronRook.exe \
    --suites tests/suites/tactical.epd \
    --time 3000
```

---

### 3. Opening Book Updates (Monthly)

**Location**: `data/books/*.pgn`

#### What to Update
- ✅ Add games from recent tournaments
- ✅ Update book based on self-play discoveries
- ✅ Remove losing lines identified by engine
- ✅ Balance opening variety (avoid over-specialization)

#### Update Process
```bash
# 1. Download recent high-level games
wget https://www.pgnmentor.com/files/TwicLatest.pgn

# 2. Filter quality games (2700+ Elo, decisive results)
python scripts/filter_games.py --input TwicLatest.pgn --min-elo 2700 --output high_quality.pgn

# 3. Merge with existing book
python scripts/update_opening_book.py --input high_quality.pgn --existing data/books/main_book.pgn

# 4. Rebuild binary book
polyglot make-book -pgn data/books/main_book.pgn -bin data/books/main_book.bin -max-ply 20
```

#### Book Statistics Target
- **Unique Positions**: 10,000+
- **Average Depth**: 10-12 ply
- **Coverage**: All major openings (e4, d4, c4, Nf3)

---

### 4. Baseline Benchmarks (After Each Major Change)

**Location**: Build logs, `benchmark_results.json`

#### Metrics to Track
| Metric | Target | Command |
|--------|--------|---------|
| **NPS** | 600K+ | `.\build\IronRook.exe bench 12` |
| **Test Pass Rate** | 80%+ | `python scripts/run_test_suites.py` |
| **Depth @ 5s** | 18+ | Custom position test |
| **Memory Usage** | < 256MB | Task Manager |

#### Benchmark Commands
```bash
# 1. Standard bench (NPS baseline)
.\build\IronRook.exe bench 12 > benchmark_log.txt

# 2. Test suite pass rate
python scripts/run_test_suites.py build/IronRook.exe --suites tests/suites/*.epd --output results.json

# 3. Perft test (move generation correctness)
python scripts/run_perft.py build/IronRook.exe --max-depth 5

# 4. Self-play quick test (50 games)
python scripts/continuous_selfplay.py --games 50 --tc 10+0.1 --quick-test
```

#### Recording Results
```bash
# Append to benchmark history
echo "$(date +%Y-%m-%d), $(git rev-parse --short HEAD), 627500" >> docs/benchmark_history.csv
```

---

### 5. Self-Play Data Generation (Continuous)

**Location**: `scripts/continuous_selfplay.py`

#### Monitoring Metrics
- **W/D/L Ratio**: Target ~35/45/20 (draw-heavy is normal)
- **Elo Trend**: Should increase over time
- **Time Loss Rate**: Should be 0%
- **Game Diversity**: Check opening variance

#### Running Continuous Self-Play
```bash
# Background continuous self-play
nohup python scripts/continuous_selfplay.py \
    --games 1000 \
    --tc 60+0.6 \
    --rounds 100 \
    --baseline build/IronRook_baseline.exe \
    --candidate build/IronRook.exe &

# Check progress
tail -f selfplay.log
```

#### Weekly Review
```bash
# Analyze self-play results
python scripts/analyze_selfplay.py --log selfplay_history.json

# Expected output:
# - Elo change: +15 ± 8 Elo
# - Time losses: 0/1000 (0.1%)
# - Opening diversity: 87% unique
```

---

### 6. CI/CD Pipeline Health (Weekly)

**Location**: `.github/workflows/*.yml`

#### What to Monitor
- ✅ Build success rate (Target: 100%)
- ✅ Test pass rates over time
- ✅ Artifact upload success
- ✅ Workflow execution time (< 10 mins)

#### GitHub Actions Dashboard
1. Go to **Actions** tab
2. Check **regression_tests.yml** workflow
3. Review recent runs for failures
4. Download test artifacts for analysis

#### Fixing Common Issues
```yaml
# If builds are slow, increase cache usage
- name: Cache dependencies
  uses: actions/cache@v3
  with:
    path: |
      build
      ~/.cargo
    key: ${{ runner.os }}-${{ hashFiles('**/CMakeLists.txt') }}
```

---

### 7. Documentation Updates (After Major Features)

**Location**: `README.md`, `docs/*.md`

#### What to Update
- ✅ **README.md**: Feature list, installation, quick start
- ✅ **TUNING.md**: Parameter guides when parameters change
- ✅ **TESTING.md**: Test suite management
- ✅ **BENCHMARKING.md**: Performance tracking
- ✅ Code comments for complex algorithms

#### Documentation Checklist
```markdown
When adding a new feature:
- [ ] Update README.md feature list
- [ ] Add UCI option documentation
- [ ] Create walkthrough artifact
- [ ] Update parameter count in docs
- [ ] Add example usage in appropriate guide
```

---

### 8. Version Tagging (After Elo Milestones)

**Location**: Git tags, `src/main.cpp`

#### When to Tag
- ✅ After +50 Elo improvement
- ✅ After completing major feature
- ✅ Before/after SPSA tuning runs
- ✅ Before breaking changes

#### Version Numbering
```
v{MAJOR}.{MINOR}.{PATCH}-{SUFFIX}

Examples:
- v1.0.0          (Initial release)
- v1.1.0          (New feature: Time management)
- v1.1.1          (Bug fix)
- v1.2.0-tuned   (After SPSA run)
- v2.0.0          (NNUE implementation)
```

#### Tagging Process
```bash
# 1. Update version in source
# Edit src/main.cpp: ENGINE_VERSION = "v1.3.2"

# 2. Commit version bump
git add src/main.cpp
git commit -m "Bump version to v1.3.2: Post-SPSA tuning +75 Elo"

# 3. Create annotated tag
git tag -a v1.3.2 -m "Release v1.3.2

Changes:
- SPSA parameter tuning (+75 Elo)
- Enhanced time management
- Expanded test suites (85 positions)

Benchmark: 627K NPS @ depth 18
Test Pass Rate: 82.4%
"

# 4. Push tag
git push origin v1.3.2
```

---

## 📊 Recommended Update Schedule

| Item | Frequency | Effort | Impact | Priority |
|------|----------|--------|--------|----------|
| **SPSA/Texel Tuning** | Weekly | High | 🔥 **Critical** | **1** |
| **Self-Play Monitoring** | Daily | Low | High | **2** |
| **Test Suite Expansion** | Bi-weekly | Medium | High | **3** |
| **Baseline Benchmarks** | After changes | Low | Medium | **4** |
| **Opening Book** | Monthly | Medium | Medium | **5** |
| **CI/CD Health** | Weekly | Low | Low | **6** |
| **Documentation** | As needed | Medium | Low | **7** |
| **Version Tags** | Per milestone | Low | Low | **8** |

---

## 🎯 Quick Start: Weekly Maintenance Routine

### Monday Morning (30 mins)
```bash
# 1. Check CI/CD health
# Visit: https://github.com/YOUR_REPO/actions

# 2. Pull latest changes
git pull origin main

# 3. Run quick benchmark
.\build\IronRook.exe bench 12

# 4. Review self-play results
python scripts/analyze_selfplay.py --log selfplay_history.json
```

### Wednesday Evening (2-3 hours)
```bash
# 1. Run weekly SPSA tuning
python scripts/spsa_tuner.py --config configs/spsa_weekly.json

# 2. Start background self-play
python scripts/continuous_selfplay.py --games 500 --tc 60+0.6 &
```

### Friday Afternoon (1 hour)
```bash
# 1. Add new test positions (if any failures found)
# Edit: tests/suites/tactical.epd

# 2. Run full test suite
python scripts/run_test_suites.py build/IronRook.exe --suites tests/suites/*.epd

# 3. Record benchmark
echo "$(date +%Y-%m-%d),$(git rev-parse --short HEAD),627500" >> docs/benchmark_history.csv

# 4. Commit weekly progress
git add .
git commit -m "Weekly update: SPSA tuning, new test positions"
git push
```

---

## 📁 File Organization

```
Prometheus/
├── docs/
│   ├── MAINTENANCE.md          (This file)
│   ├── TUNING.md              (Parameter tuning guide)
│   ├── TESTING.md             (Test suite management)
│   ├── BENCHMARKING.md        (Performance tracking)
│   └── benchmark_history.csv   (Historical NPS data)
├── scripts/
│   ├── spsa_tuner.py
│   ├── texel_tuner.py
│   ├── continuous_selfplay.py
│   ├── run_test_suites.py
│   └── analyze_selfplay.py
├── tests/
│   └── suites/
│       ├── tactical.epd
│       ├── endgame.epd
│       └── ...
└── data/
    └── books/
        ├── main_book.pgn
        └── main_book.bin
```

---

## 🚨 Critical Alerts

### When to Take Immediate Action

1. **NPS Drop > 10%**
   - Investigate recent commits
   - Profile with `perf` or `gprof`
   - Revert if necessary

2. **Test Pass Rate < 70%**
   - Review failing positions
   - Check for search regressions
   - Validate parameter changes

3. **Time Losses in Self-Play**
   - Check time management logic
   - Verify hard_limit safety margin
   - Test under low-time conditions

4. **Build Failures in CI**
   - Fix immediately before other work
   - Ensure all platforms build
   - Update dependencies if needed

---

## 📞 Contact & Support

- **Issues**: Use GitHub Issues for bug reports
- **Discussions**: GitHub Discussions for questions
- **Contributions**: See CONTRIBUTING.md

**Last Updated**: 2026-01-31  
**Maintainer**: Prometheus Engine Team
