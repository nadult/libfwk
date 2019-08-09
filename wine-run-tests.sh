set -e
make -j8 tests_mingw tools_mingw FAST_BUILD=true
cd tests
wine ./stuff.exe
wine ./math.exe
wine ./enums.exe
wine ./models.exe
wine ./vector.exe
wine ./geom.exe
