# libfwk
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build status](https://github.com/nadult/libfwk/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/nadult/libfwk/actions)

## WTF is libfwk?

FWK is a sweet (IMVHO) abbreviation for framework. It is basically a set of classes
and functions which I use in most of my projects (mostly game-related). It's trying
to be light, super easy to use and performant where it matters.


### Features

- Vulkan & SDL3 based
- Light-weight and easy to use
- Doesn't use C++ exceptions (has a custom expected<>-based error handling system instead)
- Supported platforms: Ubuntu 22.04+, Windows (requires VS 2022 with LLVM toolset)


## Examples & projects based on libfwk

* Some examples are available in [src/tests/](https://github.com/nadult/libfwk/tree/main/src/tests)
  and in [src/tools](https://github.com/nadult/libfwk/tree/main/src/tools).
* FreeFT [https://github.com/nadult/freeft](https://github.com/nadult/freeft)
* LucidRaster [https://github.com/nadult/lucid](https://github.com/nadult/lucid)

## Building

libfwk can be easily built on Ubuntu and Windows. It leverages CMake for project configuration and
building and it has it's own simple system for managing dependencies similar to PIP or NPM.
This system consists of a single-file python script
([tools/configure.py](https://github.com/nadult/libfwk/blob/main/tools/configure.py)),
which can, based on provided
[dependencies.json](https://github.com/nadult/libfwk/blob/main/dependencies.json)
file build or download pre-built packages of all the required packages. This script can also
be used to simplify project configuration on Windows when building with Visual Studio 2022+ or ninja
paired with clang-cl or MSVC.

libfwk requires a fairly new compiler which supports C++ 20 and statement expressions. That's why,
on Windows, libfwk-based projects should be compiled with clang-cl, and not with MSVC. Clang-cl is
available in Visual Studio's LLVM toolset.

Assuming that you have installed CMake, Python and a proper compiler, you should be able to build
libfwk or libfwk-based project by running the following commands:

```
python [path-to-libfwk]/tools/configure.py download-deps
python [path-to-libfwk]/tools/configure.py
cmake --build build --parallel
```

`download-deps` command will download all the required dependencies in dependencies/ subdirectory.
Those files will be downloaded from github-based package caches, specified in dependencies.json.
If you don't want to use those precompiled packages, then you can instead build those packages
locally with `build-deps` command. It will additionally require installing conan.


### Dependencies

By default all libraries are linked statically. The only exception is, when bulding for Windows in
debug mode, where some libraries are linked dynamically. This avoids problems with linking against
MSVC's debug standard library and improves linking performance.

<table>
<tr><th>Library</th><th>License</th><th>Notes</th></tr>
<tr><td colspan="3"><b>Required:</b></td></tr>
<tr><td><a href="http://www.freetype.org/">freetype2</a></td><td>FreeType License</td><td></td></tr>
<tr><td><a href="https://www.libsdl.org/">SDL3</a></td><td>ZLIB</td><td></td></tr>
<tr><td><a href="https://github.com/KhronosGroup/Vulkan-Headers">Vulkan headers</a></td><td>MIT</td><td></td></tr>
<tr><td><a href="https://github.com/google/shaderc">shaderc</a></td><td>Apache 2.0</td><td></td></tr>
<tr><td><a href="https://zlib.net/">zlib</a></td><td>ZLIB</td><td></td></tr>
<tr><td colspan="3"><b>Required (included in extern/):</b></td></tr>
<tr><td><a href="https://github.com/nothings/stb">STB_image, STB_dxt, STB_image_resize</a></td><td>MIT</td><td>extern/stb_*.h</td></tr>
<tr><td><a href="http://rapidxml.sourceforge.net/">rapidxml</a></td><td>BSL 1.0</td><td>extern/rapidxml/</td></tr>
<tr><td><a href="http://www.boost.org/">boost::polygon::voronoi</a></td><td>BSL 1.0</td><td>extern/boost_polygon/</td></tr>
<tr><td><a href="https://github.com/zeux/volk">Volk</a></td><td>MIT</td><td>extern/volk/</td></tr>
<tr><td colspan="3"><b>Optional:</b></td></tr>
<tr><td><a href="https://xiph.org/ogg/">libogg</a>, <a href="https://xiph.org/vorbis/">libvorbis</a></td><td>BSD 3-clause</td><td></td></tr>
<tr><td><a href="https://github.com/kcat/openal-soft">OpenAL Soft</a></td><td>LGPL 2.0</td><td></td></tr>
<tr><td><a href="https://github.com/AcademySoftwareFoundation/openexr">openexr-core</a></td><td>BSD 3-clause</td><td>Loading & saving EXR images (enabled with FWK_EXR CMake option)</td></tr>
<tr><td><a href="https://github.com/facebook/zstd">zstd</a></td><td>BSD</td><td>Required by openexr-core</td></tr>
<tr><td><a href="https://github.com/ebiggers/libdeflate">libdeflate</a></td><td>MIT</td><td>Required by openexr-core</td></tr>
<tr><td><a href="https://github.com/ocornut/imgui">dear imgui</a></td><td>MIT</td><td>Git submodule in extern/imgui/; Menu module & perf::Analyzer are only compiled if present</td></tr>
<tr><td><a href="https://github.com/davea42/libdwarf-code">libdwarf</a></td><td>LGPL 2.1</td><td>Linux only</td></tr>
</table>

## License

Whole library is licensed under [MIT license](https://github.com/nadult/libfwk/blob/main/license.txt).

If You found this library useful, please contact the author (nadult (at) fastmail (dot) fm).  
Any kind of feedback is greatly appreciated.
