#!/bin/bash

# Navigate to the base directory where the directories are located
cd /mnt/sda4/LDC/setup/results || exit

# List all directories and loop through them
for dir in */; do
    echo "Processing directory: $dir"
    cd "$dir" || continue

    # Find and delete all files except the specified latency results files
    find . -type f ! -name 'latency_results_0.json' ! -name 'latency_results_1.json' ! -name 'latency_results_2.json' -delete
    
    # Optionally, delete directories if needed, this part is commented out because
    # your structure does not indicate subdirectories within these directories
    # and deleting might not be safe without more context.
    # find . -type d ! -path . -exec rm -rf {} +

    cd - || exit # Return to the base directory before processing the next one
done

echo "Cleanup completed."
