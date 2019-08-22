set -e
make -j8
cd tests
./stuff
./math
./enums
./models
./geom
