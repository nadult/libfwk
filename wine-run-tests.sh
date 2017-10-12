set -e
make -j8 tests_mingw tools_mingw
cd test
wine ./streams.exe
wine ./stuff.exe
wine ./math.exe
wine ./enums.exe
wine ./models.exe
wine ./vector.exe
wine ./rollback_test.exe
