CPPFLAGS=-I/opt/homebrew/Cellar/berkeley-db@4/4.8.30/include/
CXXFLAGS=-I/opt/homebrew/Cellar/boost/1.88.0/include
LDFLAGS=-L/opt/homebrew/Cellar/berkeley-db@4/4.8.30/lib/

export CFLAGS="-O3 -mcpu=apple-m1 -mtune=apple-m1 -Wall"
export CPPFLAGS="-I/opt/homebrew/Cellar/openssl@3/3.5.0/include $CPPFLAGS"
export CXX="/opt/homebrew/bin/ccache g++ -std=c++17"
export CXXFLAGS="-std=c++14 -stdlib=libc++ -I/opt/homebrew/Cellar/boost/1.88.0/include $CXXFLAGS"
export LDFLAGS="-L/opt/homebrew/Cellar/openssl@3/3.5.0/lib $LDFLAGS"
export PKG_CONFIG_PATH="/opt/homebrew/Cellar/openssl@3/3.5.0/lib/pkgconfig:$PKG_CONFIG_PATH"

./autogen.sh

./configure --disable-man --without-gui --with-wallet --enable-hardening --without-zmq --enable-cxx --with-unsupported-ssl --disable-tests --without-miniupnpc --with-boost=/opt/homebrew/Cellar/boost/1.88.0 --with-boost-libdir=/opt/homebrew/Cellar/boost/1.88.0/lib