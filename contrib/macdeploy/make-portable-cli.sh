#!/bin/bash
# make-portable-cli.sh
# Creates portable command-line binaries by bundling required dylibs
# Usage: ./contrib/macdeploy/make-portable-cli.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SRC_DIR="$PROJECT_ROOT/src"
DIST_DIR="$PROJECT_ROOT/dist-cli"

echo "=== Creating Portable CLI Tools ==="
echo "Project root: $PROJECT_ROOT"

# Clean and create dist directory
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/bin"
mkdir -p "$DIST_DIR/lib"

# Binaries to make portable
BINARIES=(
    "litecoincashd"
    "litecoincash-cli"
    "litecoincash-tx"
)

# Function to get dependencies and copy them
copy_dependencies() {
    local binary="$1"
    local lib_dir="$2"

    echo "Processing dependencies for $binary..."

    # Get all dylib dependencies
    otool -L "$binary" | tail -n +2 | while read -r line; do
        # Extract the path
        dylib_path=$(echo "$line" | awk '{print $1}')

        # Skip system libraries and already processed
        if [[ "$dylib_path" == /System/* ]] || \
           [[ "$dylib_path" == /usr/lib/* ]] || \
           [[ "$dylib_path" == @* ]]; then
            continue
        fi

        # Get just the filename
        dylib_name=$(basename "$dylib_path")

        # Skip if already copied
        if [[ -f "$lib_dir/$dylib_name" ]]; then
            continue
        fi

        # Handle framework paths (e.g., Qt frameworks)
        if [[ "$dylib_path" == *".framework"* ]]; then
            # Extract framework binary
            if [[ -f "$dylib_path" ]]; then
                echo "  Copying framework: $dylib_name"
                cp "$dylib_path" "$lib_dir/$dylib_name"
            fi
        else
            # Regular dylib
            if [[ -f "$dylib_path" ]]; then
                echo "  Copying: $dylib_name"
                cp "$dylib_path" "$lib_dir/$dylib_name"
            fi
        fi
    done
}

# Function to fix install names
fix_install_names() {
    local binary="$1"
    local lib_dir="$2"
    local rel_lib_path="$3"

    echo "Fixing install names for $(basename $binary)..."

    # Get all dylib dependencies
    otool -L "$binary" | tail -n +2 | while read -r line; do
        dylib_path=$(echo "$line" | awk '{print $1}')

        # Skip system libraries and already fixed
        if [[ "$dylib_path" == /System/* ]] || \
           [[ "$dylib_path" == /usr/lib/* ]] || \
           [[ "$dylib_path" == @* ]]; then
            continue
        fi

        # Get just the filename
        dylib_name=$(basename "$dylib_path")

        # Handle framework paths
        if [[ "$dylib_path" == *".framework"* ]]; then
            # Extract the actual binary name from framework path
            framework_binary=$(echo "$dylib_path" | sed 's|.*/\([^/]*\.framework\)/.*|\1|' | sed 's/.framework//')
            new_path="@executable_path/$rel_lib_path/$framework_binary"
        else
            new_path="@executable_path/$rel_lib_path/$dylib_name"
        fi

        # Change the reference
        install_name_tool -change "$dylib_path" "$new_path" "$binary" 2>/dev/null || true
    done
}

# Copy binaries
echo ""
echo "=== Copying binaries ==="
for bin_name in "${BINARIES[@]}"; do
    if [[ -f "$SRC_DIR/$bin_name" ]]; then
        echo "Copying $bin_name..."
        cp "$SRC_DIR/$bin_name" "$DIST_DIR/bin/"
        chmod +x "$DIST_DIR/bin/$bin_name"
    else
        echo "Warning: $bin_name not found"
    fi
done

# Copy dependencies for each binary
echo ""
echo "=== Copying dependencies ==="
for bin_name in "${BINARIES[@]}"; do
    if [[ -f "$DIST_DIR/bin/$bin_name" ]]; then
        copy_dependencies "$DIST_DIR/bin/$bin_name" "$DIST_DIR/lib"
    fi
done

# Now process the libs themselves (they may have dependencies on each other)
echo ""
echo "=== Copying nested dependencies ==="
for lib in "$DIST_DIR/lib"/*.dylib; do
    if [[ -f "$lib" ]]; then
        copy_dependencies "$lib" "$DIST_DIR/lib"
    fi
done

# Fix install names for binaries
echo ""
echo "=== Fixing install names for binaries ==="
for bin_name in "${BINARIES[@]}"; do
    if [[ -f "$DIST_DIR/bin/$bin_name" ]]; then
        fix_install_names "$DIST_DIR/bin/$bin_name" "$DIST_DIR/lib" "../lib"
    fi
done

# Fix install names for libraries
echo ""
echo "=== Fixing install names for libraries ==="
for lib in "$DIST_DIR/lib"/*.dylib; do
    if [[ -f "$lib" ]]; then
        # Fix the library ID
        lib_name=$(basename "$lib")
        install_name_tool -id "@executable_path/../lib/$lib_name" "$lib" 2>/dev/null || true

        # Fix references to other libs
        fix_install_names "$lib" "$DIST_DIR/lib" "."
    fi
done

# Verify
echo ""
echo "=== Verifying portability ==="
for bin_name in "${BINARIES[@]}"; do
    if [[ -f "$DIST_DIR/bin/$bin_name" ]]; then
        echo ""
        echo "$bin_name dependencies:"
        otool -L "$DIST_DIR/bin/$bin_name" | grep -v "^$DIST_DIR" | head -10
    fi
done

# Create a simple run script
cat > "$DIST_DIR/README.txt" << 'EOF'
Litecoin Cash Command Line Tools
================================

This folder contains portable command-line tools for Litecoin Cash.

Contents:
  bin/litecoincashd    - Full node daemon
  bin/litecoincash-cli - RPC client for interacting with the daemon
  bin/litecoincash-tx  - Transaction utility
  lib/                 - Required libraries

Usage:
  ./bin/litecoincashd -daemon     # Start the daemon
  ./bin/litecoincash-cli help     # Get RPC help
  ./bin/litecoincash-cli stop     # Stop the daemon

Note: You may need to add the bin folder to your PATH or use full paths.
EOF

echo ""
echo "=== Done ==="
echo "Portable CLI tools created in: $DIST_DIR"
echo ""
ls -la "$DIST_DIR/bin/"
echo ""
echo "Libraries bundled:"
ls -la "$DIST_DIR/lib/" | head -20
