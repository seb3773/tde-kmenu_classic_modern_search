#!/bin/bash

# Configuration
PACKAGE_NAME="tde-kicker-q4win10"
PACKAGE_VERSION="14.1.1"
DEB_VERSION="14.1.1-1"
ARCH=$(dpkg --print-architecture)
MAINTAINER="seb3773 <seb3773@github.com>"
DESCRIPTION="Optimized Kicker (TDE Panel) with Windows 10 Search logic"
BUILD_DIR="package_build"

# Detect Trinity version from parent directory or system
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TDE_BASE_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_ROOT="$TDE_BASE_DIR/build"

# Package name format
DEB_NAME="${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCH}.deb"

echo "Creating .deb package for $PACKAGE_NAME..."
echo "  Architecture: $ARCH"

# Cleanup and setup
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/opt/trinity/bin"
mkdir -p "$BUILD_DIR/opt/trinity/lib/trinity"
mkdir -p "$BUILD_DIR/opt/trinity/share/apps/kicker"
mkdir -p "$BUILD_DIR/DEBIAN"

# Install directly from build system to ensure consistency
echo "Installing to intermediate directory..."

# We need absolute path for DESTDIR
ABS_BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
TEMP_INSTALL_DIR="$ABS_BUILD_DIR/temp_install"
mkdir -p "$TEMP_INSTALL_DIR"

# Install kicker subsystem to temp dir
make -C "$BUILD_ROOT/kicker" install DESTDIR="$TEMP_INSTALL_DIR"

# Cherry-picking relevant binaries for package (STRICT LIST)

# Main binaries (kicker core)
cp -a "$TEMP_INSTALL_DIR/opt/trinity/bin/kicker" "$BUILD_DIR/opt/trinity/bin/"

# Core libraries (the engine)
cp -a "$TEMP_INSTALL_DIR/opt/trinity/lib/libtdeinit_kicker.so" "$BUILD_DIR/opt/trinity/lib/"
cp -a "$TEMP_INSTALL_DIR/opt/trinity/lib/libkickermain.so"* "$BUILD_DIR/opt/trinity/lib/"

# The kicker panel applet wrapper
cp -a "$TEMP_INSTALL_DIR/opt/trinity/lib/trinity/kicker.so" "$BUILD_DIR/opt/trinity/lib/trinity/"

# Menu extensions (kickermenu_*) - Required for recent docs, etc.
find "$TEMP_INSTALL_DIR/opt/trinity/lib/trinity" -name "kickermenu_*.so" -exec cp -a {} "$BUILD_DIR/opt/trinity/lib/trinity/" \;

# Install search_empty icon (transparent padding icon for search mode)
mkdir -p "$BUILD_DIR/opt/trinity/share/icons/hicolor/16x16/apps"
cp "$SCRIPT_DIR/data/icons/hi16-app-search_empty.png" "$BUILD_DIR/opt/trinity/share/icons/hicolor/16x16/apps/search_empty.png"

# Cleanup temp install
rm -rf "$TEMP_INSTALL_DIR"

# Apply sstrip/strip for maximum size reduction (User Request)
echo "Stripping binaries (MAXIMUM optimized)..."
find "$BUILD_DIR" -type f -name "*.so*" -o -executable | xargs file | grep "executable\|shared object" | cut -d: -f1 | while read f; do
    if [ -f "$f" ]; then
        if command -v sstrip >/dev/null 2>&1; then
            sstrip "$f" 2>/dev/null || true
        else
            strip --strip-all "$f"
        fi
    fi
done

# Create control file
echo "Creating control file..."
cat <<EOF > "$BUILD_DIR/DEBIAN/control"
Package: $PACKAGE_NAME
Version: $DEB_VERSION
Section: x11
Priority: optional
Architecture: $ARCH
Depends: tqt3-mt, tde-core-trinity, tdeui-trinity
Replaces: tdebase-trinity, libkickermain1-trinity, kicker-trinity
Conflicts: tdebase-trinity (<< 4:14.2.0), libkickermain1-trinity (<< 4:14.2.0)
Maintainer: $MAINTAINER
Description: $DESCRIPTION
 Optimized TDE Kicker with search bar persistence and reset fixes.
 Includes Windows 10 style integration.
 Author: seb3773 (https://github.com/seb3773)
EOF

# Build package
echo "Building package..."
dpkg-deb --build "$BUILD_DIR" "$DEB_NAME"

echo "Success! Package created: $DEB_NAME"
ls -lh "$DEB_NAME"
