# Games Directory Structure

This directory contains game collections organized by purpose.

## Structure

```
Games/
├── spsa_phase1/          # Phase 1: Core Evaluation (7 params)
│   └── tuning_games.pgn  # 125,000 games @ 3K nodes/move
│
├── spsa_phase2/          # Phase 2: Search Parameters (13 params)
│   └── tuning_games.pgn  # 100,000 games @ 3K nodes/move
│
├── spsa_phase3/          # Phase 3: Advanced Evaluation (8 params)
│   └── tuning_games.pgn  # 75,000 games @ 5K nodes/move
│
└── *.pgn                 # Original game collections for opening book
```

## Phase Breakdown

### Phase 1 - Core Evaluation (spsa_phase1/)
**Parameters**: BishopPair, Outpost, OpenFile, HarassedByPawn (7 total)
**Games**: 125,000
**Timeline**: 33 days @ 3K nodes/move
**Expected Elo**: +50-80

### Phase 2 - Search Parameters (spsa_phase2/)
**Parameters**: SE, NMP, LMR, IID, Futility (13 total)
**Games**: 100,000
**Timeline**: 26 days @ 3K nodes/move
**Expected Elo**: +80-140

### Phase 3 - Advanced Evaluation (spsa_phase3/)
**Parameters**: King Safety, Pawn Structure, Material (8 total)
**Games**: 75,000
**Timeline**: 28 days @ 5K nodes/move
**Expected Elo**: +40-80

## Storage Requirements

| Phase | Games | PGN Size | Total |
|-------|-------|----------|-------|
| Phase 1 | 125K | ~10GB | ~10GB |
| Phase 2 | 100K | ~8GB | ~8GB |
| Phase 3 | 75K | ~6GB | ~6GB |
| **Total** | **300K** | **~24GB** | **~24GB** |

## Cleanup

Old games can be deleted after:
1. Parameters extracted and applied
2. Validation SPRT completed
3. Source code updated with new values

```bash
# Keep only last phase
Remove-Item Games/spsa_phase1/*.pgn
Remove-Item Games/spsa_phase2/*.pgn
```

## Archive

For long-term storage, compress completed phases:
```bash
# Compress Phase 1 after completion
7z a Games/spsa_phase1.7z Games/spsa_phase1/*.pgn
Remove-Item Games/spsa_phase1/*.pgn
```
