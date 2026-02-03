#!/usr/bin/env python3
"""
OpenBench Worker - Documentation & Setup Guide

IMPORTANT: This is a documentation file, not an implementation.
OpenBench integration requires significant infrastructure:
- Public GitHub repository
- Dedicated server/VM for worker operation  
- OpenBench server registration

For full implementation, visit: https://github.com/AndyGrant/OpenBench

"""

# ========================================
# OPENBENCH WORKER SETUP GUIDE
# ========================================

"""
## Overview
OpenBench is a distributed testing framework for chess engines.
It enables community-contributed compute power for regression testing and tuning.

## Prerequisites
1. **Public Repository**: Engine code must be publicly accessible on GitHub
2. **UCI Compliance**: Engine must fully support UCI protocol
3. **Cutechess Integration**: Must work with cutechess-cli
4. **OpenBench Account**: Register at http://chess.grantnet.us/

## Architecture
- **Server**: Manages test queue, collects results, computes Elo
- **Workers**: Download binaries, run games, upload results
- **Repository**: Hosts engine source code and build scripts

## Worker Setup

### 1. Install Dependencies
```bash
pip install requests chess python-chess cutechess
```

### 2. Register Worker
Visit http://chess.grantnet.us/ and create an account.
Generate API key from your profile.

### 3. Configure Worker
Create `openbench_config.json`:
```json
{
  "server_url": "http://chess.grantnet.us/",
  "api_key": "YOUR_API_KEY_HERE",
  "username": "your_username",
  "max_threads": 4,
  "max_memory_mb": 8192,
  "engine_name": "IronRook"
}
```

### 4. Run Worker
```bash
python tools/openbench_worker.py --config openbench_config.json
```

## Engine Registration

### 1. Create OpenBench Profile
Add `openbench.json` to repository root:
```json
{
  "name": "IronRook",
  "source": {
    "repo": "https://github.com/YOUR_USERNAME/IronRook",
    "branch": "main"
  },
  "build": {
    "system": "cmake",
    "commands": [
      "cmake -B build -DCMAKE_BUILD_TYPE=Release .",
      "cmake --build build --config Release"
    ],
    "binary": "build/IronRook"
  },
  "test": {
    "time_control": "10+0.1",
    "book": "UHO_XXL_+0.90_+1.19.epd",
    "games_per_test": 10000
  }
}
```

### 2. Submit Test
Visit OpenBench dashboard and submit a test comparing two commits.

## Worker Implementation (Conceptual)

```python
import requests
import subprocess
import json

class OpenBenchWorker:
    def __init__(self, config):
        self.server_url = config['server_url']
        self.api_key = config['api_key']
        self.username = config['username']
    
    def get_task(self):
        '''Request a test task from server'''
        response = requests.get(
            f'{self.server_url}/api/task',
            headers={'Authorization': f'Bearer {self.api_key}'}
        )
        return response.json()
    
    def run_games(self, task):
        '''Run cutechess-cli games for the task'''
        cmd = [
            'cutechess-cli',
            '-engine', f'cmd={task["baseline_binary"]}', 'name=Baseline',
            '-engine', f'cmd={task["candidate_binary"]}', 'name=Candidate',
            '-each', f'tc={task["time_control"]}', 'proto=uci',
            '-games', str(task['games']),
            '-pgnout', 'results.pgn'
        ]
        subprocess.run(cmd)
    
    def upload_results(self, task_id, results):
        '''Upload game results to server'''
        requests.post(
            f'{self.server_url}/api/results/{task_id}',
            headers={'Authorization': f'Bearer {self.api_key}'},
            json=results
        )
    
    def run(self):
        '''Main worker loop'''
        while True:
            task = self.get_task()
            if task:
                self.run_games(task)
                results = parse_pgn('results.pgn')
                self.upload_results(task['id'], results)
```

## Best Practices
1. **Stability**: Ensure worker has stable internet and power
2. **Resources**: Allocate sufficient RAM/CPU for testing
3. **Monitoring**: Log worker activity for debugging
4. **Rate Limiting**: Respect server rate limits

## Alternative: fishtest
If OpenBench access is unavailable, consider Stockfish's fishtest:
- URL: https://tests.stockfishchess.org/
- Similar distributed testing framework
- Requires Stockfish-compatible engine

## For IronRook Integration
To enable OpenBench for IronRook:
1. Make repository public on GitHub
2. Add openbench.json to repo root
3. Register engine on OpenBench server
4. Deploy workers (local or cloud VMs)
5. Submit regression tests via dashboard

## Contact
OpenBench: https://github.com/AndyGrant/OpenBench
IronRook: [Your contact/repo here]
"""

# This is a documentation file - actual implementation requires
# infrastructure setup and is beyond scope of local development.

if __name__ == '__main__':
    print(__doc__)
