# IronBook: Experience Learning System

The **IronBook** is a persistent experience machine that allows the engine to "learn" from its own games and analysis. As the engine plays, it records the evaluations of positions it searches into a persistent file. When it encounters those positions again, it uses the stored knowledge to make better decisions or prune bad lines faster.

## 1. How it Works

- **Storage:** Data is saved in a binary file (e.g., `IronBook.exp`).
- **Learning:** Every time the engine completes a search to `depth >= 8`, it records the score and depth.
- **Improvement:** If a position is searched again to a greater depth, the book is updated.
- **Usage:** During search, the engine probes the book. If a match is found with sufficient depth, it uses the stored score to guide the search (Root) or prune/reduce (Search).

## 2. Configuration

To enable the IronBook, simply point the engine to the file using the UCI option:

```ini
option name ExperiencePath value "IronBook.exp"
```

If the file does not exist, the engine will create it when it first saves data.

## 3. Creating an Initial Book

You can seed the IronBook with a set of opening positions (EPD) or games.

### A. Generate Openings
First, generate a set of random opening positions to learn from:
```bash
python tools/generate_book.py my_book.epd 1000
```
*(This generates 1000 positions with 8-ply random walks, vetted by the engine)*

### B. Train the Experience File
Run the training tool to have the engine analyze these positions and save the results:
```bash
python tools/train_experience.py my_book.epd IronBook.exp
```
*(This runs a depth-10 search on each position and builds the .exp file)*

## 4. Continuous Learning (Reinforcement Loop)

You can run the engine in a continuous self-play loop where it updates the book after every game.

```bash
python tools/selfplay.py ./build/IronRook games.pgn 10000 IronBook.exp
```

- **./build/IronRook**: Path to your engine executable.
- **games.pgn**: Where to save the game records.
- **10000**: Number of games to play.
- **IronBook.exp**: The experience file to use and update.

**Note:** The engine only saves the updated book to disk when it quits (i.e., after the script finishes or is stopped gracefully). Ideally, run for a set number of games, then restart to ensure data is flushed frequently.
