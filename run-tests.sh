set -e
make -j8 tests tools
cd tests
./streams
./stuff
./math
./enums
./models
./vector
./rollback_test
