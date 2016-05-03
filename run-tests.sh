set -e
make -j8 test/streams test/stuff test/math test/enums test/window test/models tools/model_convert tools/model_viewer
cd test
./streams
./stuff
./math
./enums
./models
