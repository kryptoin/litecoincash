#!/bin/bash
# Remove build artifacts and temporary files

# Log files
find . -type f -name "*.log" -print -delete

# Editor backup files
find . -type f -name "*~" -print -delete

# macOS app bundle
rm -rf Litecoin-Cash-Qt.app 2>/dev/null

# macOS disk image
rm -f Litecoin-Cash-Core.dmg 2>/dev/null

# Stray files from interrupted builds
rm -f Litecoin-Cash-Qt 2>/dev/null
rm -f Litecoin 2>/dev/null
rm -f osx_volname 2>/dev/null
rm -rf Cash-Qt.app 2>/dev/null

# Object files and static libraries
find . -type f \( -name "*.o" -o -name "*.obj" -o -name "*.lo" \) -print -delete
find . -type f \( -name "*.a" -o -name "*.la" \) ! -path "*/secp256k1/*" -print -delete

# LevelDB build output
rm -rf src/leveldb/out-static 2>/dev/null
rm -rf src/leveldb/out-shared 2>/dev/null
rm -f src/leveldb/build_config.mk 2>/dev/null
rm -f src/leveldb/Makefile 2>/dev/null

# Autotools cache
rm -rf autom4te.cache 2>/dev/null

echo "Cleanup complete."