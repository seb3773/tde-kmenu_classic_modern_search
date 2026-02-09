#!/bin/bash
# Integration script for KMenu Modern Search Integration
# This script injects the new classic menu search system into a tdebase-trinity directory.

if [ -z "$1" ]; then
    echo "Usage: $0 <path_to_target_tdebase_root>"
    exit 1
fi

TARGET_DIR="$1"

if [ ! -d "$TARGET_DIR/kicker/kicker/ui" ]; then
    echo "Error: $TARGET_DIR does not look like a tdebase-trinity root."
    exit 1
fi

echo "Integrating KMenu Modern Search into $TARGET_DIR..."

# Copy files
cp -v src/kicker/kicker/ui/k_mnu.cpp "$TARGET_DIR/kicker/kicker/ui/"
cp -v src/kicker/kicker/ui/k_mnu.h "$TARGET_DIR/kicker/kicker/ui/"

echo "Success! Modern Search system integrated."
echo "You can now rebuild kicker in the target tree."
