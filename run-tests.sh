set -e
make -j8 tests tools FAST_BUILD=true
cd tests
./stuff
./math
./enums
./models
./geom
