# Prometheus Engine - Benchmarking Guide

Guide for tracking and analyzing engine performance over time.

## 📊 Key Performance Metrics

### 1. Nodes Per Second (NPS)
**What it measures**: Raw search speed  
**Target**: 600,000+ NPS  
**Command**:
```bash
.\build\IronRook.exe bench 12
```

**Interpreting Results**:
- **< 400K NPS**: Performance regression - investigate immediately
- **400-600K NPS**: Normal range
- **> 600K NPS**: Excellent performance

### 2. Search Depth
**What it measures**: How deep the engine searches in fixed time  
**Target**: Depth 18+ @ 5 seconds  
**Command**:
```bash
# UCI position test
uci
position startpos
go movetime 5000
```

### 3. Test Suite Pass Rate
**What it measures**: Tactical/positional correctness  
**Target**: 80%+ across all suites  
**Command**:
```bash
python scripts/run_test_suites.py build/IronRook.exe \
    --suites tests/suites/*.epd \
    --time 5000 \
    --output test_results.json
```

### 4. Elo Rating
**What it measures**: Playing strength  
**Target**: 3000+ Elo (HCE), 3200+ (NNUE)  
**Method**: Self-play against baseline or CCRL testing

---

## 🎯 Benchmark Suite Positions

### Standard Bench Positions
Located in: `src/bench.cpp`

Positions include:
1. Starting position
2. Tactical middlegame
3. Endgame (KRPvKR)
4. Passed pawn race
5. Complex middlegame
6. King safety test
7. Zugzwang position
8. Fortress position
9. Open position
10. Closed position
11. Mate in 3
12. Perft position

### Running Standard Bench
```bash
# Default depth 12
.\build\IronRook.exe bench

# Custom depth
.\build\IronRook.exe bench 15

# Save to file
.\build\IronRook.exe bench 12 > benchmark_$(date +%Y%m%d).txt
```

---

## 📈 Tracking Performance Over Time

### Benchmark History CSV
**Location**: `docs/benchmark_history.csv`

**Format**:
```csv
Date,Commit,NPS,Depth,TestPassRate,Notes
2026-01-20,abc1234,582000,17,78.5,Baseline
2026-01-25,def5678,627000,18,82.4,Post-SPSA tuning
2026-01-31,ghi9012,645000,18,85.1,Time management update
```

### Recording Benchmarks
```bash
#!/bin/bash
# save_benchmark.sh

DATE=$(date +%Y-%m-%d)
COMMIT=$(git rev-parse --short HEAD)

# Run bench
OUTPUT=$(.\build\IronRook.exe bench 12)
NPS=$(echo "$OUTPUT" | grep "Nodes/second" | awk '{print $2}')

# Run tests
python scripts/run_test_suites.py build/IronRook.exe \
    --suites tests/suites/*.epd > test_output.txt
PASS_RATE=$(cat test_output.txt | grep "Pass Rate" | awk '{print $3}')

# Append to history
echo "$DATE,$COMMIT,$NPS,18,$PASS_RATE,Auto-logged" >> docs/benchmark_history.csv
```

---

## 🔍 Profiling Performance Issues

### 1. Identify Bottlenecks

#### Using Visual Studio Profiler (Windows)
```bash
# 1. Build with profiling enabled
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --config RelWithDebInfo

# 2. Run profiler in Visual Studio
# Set IronRook.exe as startup project
# Start with Performance Profiler (Alt+F2)
```

#### Using perf (Linux)
```bash
# 1. Run with perf
perf record -g ./build/IronRook bench 12

# 2. Analyze results
perf report
```

### 2. Common Bottlenecks

| Bottleneck | Symptoms | Fix |
|------------|----------|-----|
| **TT Contention** | Low NPS in multi-threaded | Increase TT size |
| **Move Generation** | High % in movegen.cpp | Optimize bitboard ops |
| **Evaluation** | High % in eval.cpp | Add lazy eval |
| **Search Overhead** | Many small functions | Inline hot paths |

### 3. Memory Profiling
```bash
# Valgrind (Linux)
valgrind --tool=massif ./build/IronRook bench 12
ms_print massif.out.* > memory_profile.txt

# Visual Studio Diagnostic Tools (Windows)
# Use Memory Usage profiler
```

---

## 📉 Regression Detection

### Automated Regression Tests

**Script**: `scripts/regression_check.sh`

```bash
#!/bin/bash
# Compares current build vs baseline

BASELINE=build/IronRook_baseline.exe
CURRENT=build/IronRook.exe

# 1. NPS comparison
BASELINE_NPS=$(.\$BASELINE bench 12 | grep "Nodes/second" | awk '{print $2}')
CURRENT_NPS=$(.\$CURRENT bench 12 | grep "Nodes/second" | awk '{print $2}')

# 2. Calculate delta
DELTA=$(echo "scale=2; ($CURRENT_NPS - $BASELINE_NPS) / $BASELINE_NPS * 100" | bc)

echo "NPS Delta: $DELTA%"

# 3. Fail if regression > 5%
if (( $(echo "$DELTA < -5" | bc -l) )); then
    echo "❌ REGRESSION DETECTED: $DELTA%"
    exit 1
else
    echo "✅ No regression: $DELTA%"
fi
```

### CI Integration
```yaml
# .github/workflows/regression_tests.yml
- name: Benchmark Regression Check
  run: |
    ./scripts/regression_check.sh
    if [ $? -ne 0 ]; then
      echo "Performance regression detected!"
      exit 1
    fi
```

---

## 🎮 Playing Strength Testing

### Self-Play SPRT
**Command**:
```bash
python scripts/sprt_test.py \
    build/IronRook_baseline.exe \
    build/IronRook_new.exe \
    --elo0 -5 \
    --elo1 0 \
    --tc 10+0.1 \
    --max-games 1000
```

**Interpretation**:
- **PASS**: New version is not weaker (can promote)
- **FAIL**: Regression detected (revert changes)
- **TIMEOUT**: Inconclusive (need more games)

### External Testing

#### CCRL (Computer Chess Rating Lists)
1. Submit engine to CCRL: http://ccrl.chessdom.com/
2. Wait for rating (2-4 weeks)
3. Track Elo progression

#### Lichess Bot
1. Deploy as Lichess bot
2. Play rated games
3. Monitor Lichess rating

---

## 📋 Benchmark Checklist

### Before Release
- [ ] Run `bench 12` - Record NPS
- [ ] Run all test suites - Record pass rate
- [ ] Run perft tests - Verify correctness
- [ ] Self-play 100 games vs baseline - Check W/D/L
- [ ] Profile for memory leaks
- [ ] Test on different hardware (if available)
- [ ] Verify UCI compliance

### After Major Change
- [ ] Compare NPS vs previous build
- [ ] Run regression SPRT (500 games minimum)
- [ ] Check test suite delta
- [ ] Update benchmark history CSV
- [ ] Tag commit if improvement confirmed

---

## 🎯 Performance Targets by Version

| Version | NPS Target | Test Pass | Estimated Elo | Status |
|---------|-----------|-----------|---------------|--------|
| v1.0 | 200K | 60% | 2400 | ✅ Released |
| v1.1 | 400K | 70% | 2700 | ✅ Released |
| v1.2 | 600K | 80% | 3000 | ✅ **Current** |
| v1.3 | 650K | 85% | 3100 | 🎯 Target |
| v2.0 (NNUE) | 500K | 90% | 3300+ | 🔮 Future |

---

## 📞 Reporting Issues

If you encounter:
- **NPS < 500K**: Open issue with `perf` profile
- **Test pass rate drop**: Include failing positions
- **Elo regression**: Provide SPRT results

**Last Updated**: 2026-01-31
