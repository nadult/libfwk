set -e
make -j16 MODE=release MERGE_MODULES=1
cd tests
./stuff
./math
./models
./geom
