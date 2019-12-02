set -e
make -j8 MODE=release
cd tests
./stuff
./math
./enums
./models
./geom
