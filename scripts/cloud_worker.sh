#!/bin/bash
set -e

# Configuration (These can be overridden by environment variables)
GAMES=${GAMES:-1000}
DEPTH=${DEPTH:-8}
S3_BUCKET=${S3_BUCKET:-"s3://prometheus-chess-data"}
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
HOSTNAME=$(hostname)
OUTPUT_FILE="games_${TIMESTAMP}_${HOSTNAME}.epd"

echo "Starting Prometheus Worker on $HOSTNAME"
echo "Target: $GAMES games at depth $DEPTH"
echo "Output: $OUTPUT_FILE"

# Run Self-Play
# The selfplay command is exposed in src/main.cpp
# Usage: ./IronRook selfplay <games> <depth> <output_file>
/app/IronRook selfplay $GAMES $DEPTH $OUTPUT_FILE

echo "Self-play complete. Generated $(wc -l < $OUTPUT_FILE) positions."

# Upload to S3
if [ -n "$S3_BUCKET" ]; then
    echo "Uploading to $S3_BUCKET..."
    aws s3 cp $OUTPUT_FILE $S3_BUCKET/$OUTPUT_FILE
else
    echo "S3_BUCKET not set. Skipping upload."
fi

echo "Worker finished successfully."
