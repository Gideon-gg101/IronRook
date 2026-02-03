# OpenBench Setup Guide for Prometheus

Prometheus is configured to be compatible with [OpenBench](https://github.com/AndyGrant/OpenBench), a distributed testing framework for chess engines.

## 1. Requirements

- **UCI Protocol**: Supported.
- **Bench Command**: `bench [depth]` is supported and outputs `Total Nodes: X`.
- **Makefile**: Root `Makefile` provided for Linux builds.
- **Compiler**: `g++` (GCC) is standard.

## 2. Configuration (`manifest.json` or Test Definition)

When submitting Prometheus to an OpenBench instance, use the following configuration templates.

### Engine Configuration
```json
{
  "command": "./IronRook",
  "name": "Prometheus",
  "protocol": "uci",
  "options": [
    { "name": "Hash", "value": "128" },
    { "name": "Threads", "value": "1" }
  ]
}
```

### Build Configuration (Makefile)
Prometheus includes a root `Makefile` that wraps the CMake build process.
```bash
# Verify it builds
make release
./IronRook bench
```

## 3. Connecting to a Client

If you are running an OpenBench client:

1.  **Clone the Repo**:
    ```bash
    git clone https://github.com/YourRepo/Prometheus.git engines/Prometheus
    ```

2.  **Configure Client**:
    Add the engine options in your client configuration if running locally, or rely on the server's compilation script.

## 4. Troubleshooting

- **"Bench command failed"**: Ensure `Total Nodes:` is in the output.
- **"Build failed"**: Check `cmake` version (3.10+ required).
