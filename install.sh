#!/bin/bash
# Install script for KMenu Modern Search
# Overlays the modern search files onto a standard tdebase-trinity source tree.

if [ -z "$1" ]; then
    echo "Usage: $0 <path_to_tdebase_root>"
    exit 1
fi

TARGET="$(realpath $1)"

if [ ! -d "$TARGET/kicker/kicker/ui" ]; then
    echo "Error: Target does not look like tdebase source: $TARGET"
    exit 1
fi

echo "Installing KMenu Modern Search to: $TARGET"

# Copy sources
cp -v src/kicker/kicker/ui/* "$TARGET/kicker/kicker/ui/"

# Copy CMakeLists (auto-sstrip)
cp -v src/kicker/kicker/CMakeLists.txt    "$TARGET/kicker/kicker/"
cp -v src/kicker/libkicker/CMakeLists.txt "$TARGET/kicker/libkicker/"

# Copy Icons
mkdir -p "$TARGET/kicker/data/icons"
cp -v src/kicker/data/icons/* "$TARGET/kicker/data/icons/"

# Copy Tools
cp -v create_deb.sh "$TARGET/kicker/"
cp -v README.md     "$TARGET/kicker/"

echo "--------------------------------------------------------"
echo "Success! Sources updated."
echo "Now you can build:"
echo "  cd $TARGET/build"
echo "  cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_KICKER=ON"
echo "  make kicker"
echo "  sudo make install  (Will auto-strip to 3KB)"
echo "--------------------------------------------------------"
