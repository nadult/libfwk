set -e
#TODO: test/meshes
make -j4 test/streams test/stuff test/math test/enums test/window
cd test
./streams
./stuff
./math
./enums
#./meshes
