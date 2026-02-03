#!/bin/bash
set -e
echo "Starting SPSA Tuning on Cloud..."
echo "Configuration: tools/spsa_config_cloud.json"

# Run SPSA Tuner with Python 3
# Ensure we are in /app
cd /app
python3 tools/spsa_tuner.py tools/spsa_config_cloud.json
