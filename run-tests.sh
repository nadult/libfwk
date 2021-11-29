set -e
make -j16 MODE=release
cd tests
./stuff
./math
./enums
./models
./geom
