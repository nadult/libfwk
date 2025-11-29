# libfwk
[![License](https://img.shields.io/badge/License-Boost_1.0-lightblue.svg)](https://www.boost.org/LICENSE_1_0.txt)
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

* **freetype2**
	Used under the "FreeType License".  
	[http://www.freetype.org/](http://www.freetype.org/)

* **STB\_image && STB\_dxt && STB\_image\_resize**
    Included in extern/stb_*.h files, used under MIT license.  
    [https://github.com/nothings/stb](https://github.com/nothings/stb)

* **rapidxml**  
	Included in extern/rapidxml/, used under BSL 1.0.  
	[http://rapidxml.sourceforge.net/](http://rapidxml.sourceforge.net/)

* **boost::polygon::voronoi**   
    Included in extern/boost_polygon/, used under BSL 1.0.  
	[http://www.boost.org/](http://www.boost.org/)

* **Volk**   
    Included in extern/volk/, used under MIT license.  
	[https://github.com/zeux/volk](https://github.com/zeux/volk)

* **SDL3**
    Used under ZLIB license.  
	[https://www.libsdl.org/](https://www.libsdl.org/)	

* **Vulkan headers**  
    Used under MIT license.  
	[https://github.com/KhronosGroup/Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers)

* **shaderc**  
    Used under Apache license.  
	[https://github.com/google/shaderc](https://github.com/google/shaderc)

* **zlib**  

* **libogg, libvorbis (optional)**  
	[https://xiph.org/ogg/](https://xiph.org/ogg/)  
	[https://xiph.org/vorbis/](https://xiph.org/vorbis/)

* **OpenAL Soft (optional)**  
	[https://github.com/kcat/openal-soft](https://github.com/kcat/openal-soft)

* **dear imgui (optional)**  
	Included as a git submodule in extern/imgui/, used under MIT license.  
	Menu module & perf::Analyzer will only be compiled if imgui is present.  
	[https://github.com/ocornut/imgui](https://github.com/ocornut/imgui)

* **libdwarf (optional, linux-only)**

## License

Whole library is licensed under [Boost Software license](https://github.com/nadult/libfwk/blob/main/license.txt).

If You found this library useful, please contact the author (nadult (at) fastmail (dot) fm).  
Any kind of feedback is greatly appreciated.
