#!/bin/bash
# Exit on error
set -e

# --- Environment Setup ---
echo "Setting up build environment..."

# macOS SDK setup
export SDKROOT="/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"

# Use current Homebrew paths (symlinks)
BERKELEY_DB_PREFIX="/opt/homebrew/opt/berkeley-db@4"
# Use boost@1.85 which has dylib files (boost 1.89.0 doesn't have dylibs)
BOOST_PREFIX="/opt/homebrew/Cellar/boost@1.85/1.85.0_3"
OPENSSL_PREFIX="/opt/homebrew/opt/openssl@3"
QT5_PREFIX="/opt/homebrew/opt/qt@5"

# Verify paths exist
for path in "$BERKELEY_DB_PREFIX" "$BOOST_PREFIX" "$OPENSSL_PREFIX" "$QT5_PREFIX"; do
    if [ ! -d "$path" ]; then
        echo "Error: Path does not exist: $path"
        exit 1
    fi
done

# Compilers
export AR="xcrun --sdk macosx ar"
export CC="xcrun --sdk macosx clang"
export CXX="xcrun --sdk macosx clang++"
export CPP="clang -E"

# System flags
SYSTEM_FLAGS="-isysroot $SDKROOT"
SYSTEM_COMPILER_FLAGS=""
if [ "$(uname -m)" = "arm64" ]; then
    SYSTEM_COMPILER_FLAGS="-DLEVELDB_PLATFORM_POSIX_SSE=0"
fi

# Set PKG_CONFIG_PATH first before trying to get Qt flags
export PKG_CONFIG_PATH="$OPENSSL_PREFIX/lib/pkgconfig:$QT5_PREFIX/lib/pkgconfig"

# Add Qt5 to PATH so configure can find it
export PATH="$QT5_PREFIX/bin:$PATH"

# Now get Qt flags (after PKG_CONFIG_PATH is set)
QT_CFLAGS=$(pkg-config --cflags Qt5Core Qt5Gui Qt5Widgets)
QT_LIBS=$(pkg-config --libs Qt5Core Qt5Gui Qt5Widgets)

echo "QT_CFLAGS: $QT_CFLAGS"
echo "QT_LIBS: $QT_LIBS"

# Compiler and Linker Flags (consolidated, no duplicates)
export CFLAGS="$SYSTEM_FLAGS $SYSTEM_COMPILER_FLAGS -O3 -mcpu=apple-m1 -mtune=apple-m1 -Wall -fPIC"
export CXXFLAGS="$SYSTEM_FLAGS $SYSTEM_COMPILER_FLAGS -std=c++17 -stdlib=libc++ -O3 -mcpu=apple-m1 -mtune=apple-m1 -Wall -I$BOOST_PREFIX/include -DNO_SSE -fPIC $QT_CFLAGS"

# IMPORTANT: Remove /opt/homebrew/include to avoid Qt header conflicts
# The flat headers in /opt/homebrew/include cause QStringList definition conflicts
# Qt5-specific headers from pkg-config are sufficient and correct
export CPPFLAGS="$SYSTEM_FLAGS $SYSTEM_COMPILER_FLAGS -I$OPENSSL_PREFIX/include -I$BERKELEY_DB_PREFIX/include -I$BOOST_PREFIX/include -I./src/leveldb/include -I./src/leveldb/helpers/memenv"

# LDFLAGS should NOT include compiler defines (-D flags)
# Include library paths (-L) but let configure add the libraries (-l) via --with-boost
export LDFLAGS="$SYSTEM_FLAGS -L$OPENSSL_PREFIX/lib -lssl -lcrypto -L$BERKELEY_DB_PREFIX/lib -ldb_cxx -L$(pwd)/src/leveldb/out-static -lleveldb -lmemenv -L$BOOST_PREFIX/lib -largon2 $QT_LIBS"

echo "Environment configured."

# --- Build Steps ---
echo "Building LevelDB using its custom build system..."

# Build LevelDB using its custom build system
(
  cd src/leveldb

  # Clean any previous builds
  if [ -f "Makefile" ]; then
    make clean
  fi
  rm -rf out-static out-shared 2>/dev/null || true
  rm -f build_config.mk 2>/dev/null || true

  # Check what build system files exist
  echo "LevelDB build files found:"
  ls -la | grep -E "(Makefile|build_|configure|CMake)" || echo "No standard build files found"

  # Try different build approaches based on what's available
  if [ -f "build_detect_platform" ]; then
    echo "Using build_detect_platform to generate Makefile..."
    # Generate the build configuration (requires output file and directory prefix)
    ./build_detect_platform build_config.mk ./
    echo "PLATFORM_CXXFLAGS+=-fPIC" >> build_config.mk

    # Remove SSE file from SOURCES on ARM64 (Apple M1) since SSE is not supported
    if [ "$(uname -m)" = "arm64" ]; then
      echo "Removing port/port_posix_sse.cc from SOURCES for ARM64..."
      sed -i '' 's/ port\/port_posix_sse\.cc//g' build_config.mk
      # Also remove SSE flags
      sed -i '' 's/PLATFORM_SSEFLAGS=.*/PLATFORM_SSEFLAGS=/' build_config.mk

    fi

    # Create the Makefile if it doesn't exist
    if [ ! -f "Makefile" ]; then
      echo "Generating Makefile..."
      # Some LevelDB versions need this
      cat > Makefile << 'EOF'
include build_config.mk

LIBOBJECTS = $(SOURCES:.cc=.o)

out-static/libleveldb.a: $(LIBOBJECTS)
	mkdir -p out-static
	$(AR) -rs $@ $(LIBOBJECTS)

out-static/libmemenv.a: helpers/memenv/memenv.o
	mkdir -p out-static
	$(AR) -rs $@ helpers/memenv/memenv.o

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(PLATFORM_CXXFLAGS) -c $< -o $@

clean:
	rm -f $(LIBOBJECTS) helpers/memenv/memenv.o
	rm -rf out-static out-shared

.PHONY: clean
EOF
    fi

    # Now build with the generated Makefile
    env AR="$AR" CC="$CC" CXX="$CXX" CFLAGS="$CFLAGS" CXXFLAGS="$CXXFLAGS -I. -Iinclude -fPIC" make out-static/libleveldb.a out-static/libmemenv.a

  elif [ -f "CMakeLists.txt" ]; then
    echo "Using CMake build system..."
    mkdir -p build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DLEVELDB_BUILD_TESTS=OFF \
          -DLEVELDB_BUILD_BENCHMARKS=OFF \
          ..
    make
    # Copy libraries to expected location
    mkdir -p ../out-static
    cp libleveldb.a ../out-static/
    cd ..

  else
    echo "Trying direct compilation..."
    # Fallback: compile directly
    mkdir -p out-static

    # Compile all .cc files
    for file in db/*.cc table/*.cc util/*.cc; do
      if [ -f "$file" ]; then
        echo "Compiling $file"
        $CXX $CXXFLAGS -Iinclude -I. -c "$file" -o "${file%.cc}.o"
      fi
    done

    # Create static library
    ar rcs out-static/libleveldb.a db/*.o table/*.o util/*.o 2>/dev/null || true

    # Build memenv
    if [ -f "helpers/memenv/memenv.cc" ]; then
      $CXX $CXXFLAGS -Iinclude -I. -c helpers/memenv/memenv.cc -o helpers/memenv/memenv.o
      ar rcs out-static/libmemenv.a helpers/memenv/memenv.o
    fi
  fi

  # Verify libraries were built
  if [ -f "out-static/libleveldb.a" ]; then
    echo "✓ libleveldb.a built successfully"
  else
    echo "✗ Failed to build libleveldb.a"
    exit 1
  fi

  if [ -f "out-static/libmemenv.a" ]; then
    echo "✓ libmemenv.a built successfully"
  else
    echo "✗ Failed to build libmemenv.a"
    exit 1
  fi
)

echo "LevelDB built successfully."

echo "Running autogen.sh..."
sh autogen.sh

echo "Running configure..."

# Configure with Qt-specific flags to avoid header conflicts
PKG_CONFIG_PATH="$PKG_CONFIG_PATH" ./configure \
  --enable-qt \
  --disable-tests \
  --disable-qt-tests \
  --with-wallet \
  --enable-hardening \
  --without-zmq \
  --enable-cxx \
  --without-miniupnpc \
  --with-boost="${BOOST_PREFIX}" \
  --with-boost-libdir="${BOOST_PREFIX}/lib" \
  --disable-dependency-tracking \
  CPPFLAGS="$CPPFLAGS" \
  CXXFLAGS="$CXXFLAGS" \
  LDFLAGS="$LDFLAGS"

# Copy config.h if it exists (some configure scripts generate it in src/)
if [ -f "config.h" ]; then
  cp config.h src/bitcoin-config.h
elif [ -f "src/config.h" ]; then
  cp src/config.h src/bitcoin-config.h
fi

echo "Configuring and building secp256k1 submodule..."
(
  cd src/secp256k1
  mkdir -p m4
  cp ../../build-aux/m4/secp256k1.m4 m4/ 2>/dev/null || true
  make clean 2>/dev/null || true
  sh autogen.sh
  env SDKROOT="$SDKROOT" \
  CC="$CC" \
  CFLAGS="$CFLAGS" \
  CXX="$CXX" \
  CXXFLAGS="$CXXFLAGS" \
  LDFLAGS="" \
  CPPFLAGS="" \
  PKG_CONFIG_PATH="$PKG_CONFIG_PATH" \
  ./configure --disable-shared --with-pic --disable-tests --enable-module-recovery
  gmake libsecp256k1.la
)

echo "secp256k1 submodule built."
echo "Building main project..."

# Set Qt tool variables for make since configure doesn't detect them
export UIC="$QT5_PREFIX/bin/uic"
export MOC="$QT5_PREFIX/bin/moc"
export RCC="$QT5_PREFIX/bin/rcc"
export LUPDATE="$QT5_PREFIX/bin/lupdate"
export LRELEASE="$QT5_PREFIX/bin/lrelease"

echo "Qt tools configured:"
echo "UIC: $UIC"
echo "MOC: $MOC"
echo "RCC: $RCC"

# Build with additional flags to handle Qt conflicts
gmake AR="$AR" UIC="$UIC" MOC="$MOC" RCC="$RCC" LUPDATE="$LUPDATE" LRELEASE="$LRELEASE"

echo "Build finished. Running deployment..."
make deploy

echo "Deployment finished."