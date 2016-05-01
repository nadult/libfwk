set -e
make -j8 test/streams test/stuff test/math test/enums test/window test/models tools/model_convert tools/model_viewer
make -j8 test/stuff.exe test/window.exe tools/model_convert.exe tools/model_viewer.exe
cd test
./streams
./stuff
./math
./enums
./models
