set -e
make -j8 tests tools
cd test
./streams
./stuff
./math
./enums
./models
./vector
./rollback_test
