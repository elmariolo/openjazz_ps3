#!/bin/bash

TARGET_DIR="gamefiles"

if [ -d "$TARGET_DIR" ]; then
    cd "$TARGET_DIR"
    for f in *; do
        lower=$(echo "$f" | tr '[:upper:]' '[:lower:]')
        if [ "$f" != "$lower" ]; then
            mv "$f" "$lower"
            echo "Renaming: $f -> $lower"
        fi
    done
    
    cd ..
    echo "-Done! -"
else
    echo "-Error: '$TARGET_DIR' folder not found.-"
fi