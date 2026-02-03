@echo off
REM Game Generation Script for SPSA Phase 1
REM Generates 175,000 games at 3000 nodes/move

set "CUTECHESS=C:\Users\Administrator\Desktop\GideonInspired\Chess\Engine based\cutechess-1.4.0-win64\cutechess-cli.exe"
set ENGINE=build\IronRook.exe
set OUTPUT=Games\spsa_phase1\raw_games.pgn
set GAMES=175000
set NODES=3000
set CONCURRENCY=5

echo Starting game generation...
echo Target: %GAMES% games
echo Nodes/move: %NODES%
echo Concurrency: %CONCURRENCY%
echo Output: %OUTPUT%
echo.

mkdir Games\spsa_phase1 2>nul

"%CUTECHESS%" ^
  -engine cmd=%ENGINE% name=IronRook ^
  -engine cmd=%ENGINE% name=IronRook ^
  -each proto=uci tc=inf/3000 ^
  -games %GAMES% ^
  -concurrency %CONCURRENCY% ^
  -pgnout %OUTPUT% ^
  -recover ^
  -repeat

echo.
echo Done! Generated %GAMES% games
echo Saved to: %OUTPUT%
pause
