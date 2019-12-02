set -e
make -j8 PLATFORM=mingw MODE=release
cd tests
wine ./stuff.exe
wine ./math.exe
wine ./enums.exe
wine ./models.exe
wine ./vector.exe
wine ./geom.exe
