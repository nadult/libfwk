# libfwk
[![License](https://img.shields.io/badge/License-Boost_1.0-lightblue.svg)](https://www.boost.org/LICENSE_1_0.txt)
[![Build status](https://github.com/nadult/libfwk/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/nadult/libfwk/actions)

## WTF is libfwk?

FWK is a sweet (IMVHO) abbreviation for framework. It is basically a set of classes
and functions which I use in most of my projects (mostly game-related). It's trying
to be light, super easy to use and performant where it matters.

### Features

- Vulkan & SDL3 based
- Easy to use interfaces (trying to be as intuitive as possible)
- Doesn't use C++ exceptions
- Supported platforms: Ubuntu 22.04, Windows (requires VS 2022 with LLVM toolset)

TODO: more details


## Examples & projects based on libfwk

* Some examples are available in src/test/ and src/tools.
* FreeFT [https://github.com/nadult/freeft](https://github.com/nadult/freeft)
* LucidRaster [https://github.com/nadult/lucid](https://github.com/nadult/lucid)

## Building

libfwk is written in C++20, so it requires a fairly new C++ compiler, preferably Clang.
libfwk can be easily compiled in Ubuntu 22.04 and under Windows (github actions are provided).

### Windows

libfwk requires Visual Studio 2022 with LLVM toolset. [tools/install\_deps.py](https://github.com/nadult/libfwk/blob/main/tools/install_deps.py) will download all the necessary dependencies into a specified folder. libfwk project files expect this folder to be 'C:/libraries/x86_64'. You can change that in [windows/shared\_libraries.props](https://github.com/nadult/libfwk/blob/main/windows/shared_libraries.props). To build it, simply open [windows/libfwk.sln](https://github.com/nadult/libfwk/blob/main/windows/libfwk.sln)
in Visual Studio and run build.

### Ubuntu

Several external dependencies have to be installed, before building libfwk. 'Install dependencies' step in test-linux job ([.github/workflows/test.yml](https://github.com/nadult/libfwk/blob/main/.github/workflows/test.yml#L55)) is a good reference of how to install them.  
To compile simply run make. There is a basic description of the options that you can pass to make at the beginning of Makefile & Makefile-shared.

Examples:

    $ make MODE=release-paranoid COMPILER=clang++-17 STATS=true -j12
    $ make MODE=release-paranoid print-stats print-variables
    $ make MODE=debug-nans clean
    $ make clean-all

### Dependencies

#### External dependencies

* **SDL3**  
	[https://www.libsdl.org/](https://www.libsdl.org/)
	
* **Vulkan SDK**  
	[https://www.lunarg.com/vulkan-sdk/](https://www.lunarg.com/vulkan-sdk/)  
	Only shaderc, vulkan headers & loader is actually used  

* **zlib**

* **freetype2**  
	[http://www.freetype.org/](http://www.freetype.org/)

* **libogg, libvorbis**  
	[https://xiph.org/ogg/](https://xiph.org/ogg/)
	[https://xiph.org/vorbis/](https://xiph.org/vorbis/)

* **OpenAL**  
	[https://www.openal.org/](https://www.openal.org/)

* **dear imgui (optional)**  
	Included as a git submodule in extern/imgui/.  
	Menu module & perf::Analyzer will only be compiled if imgui is present.  
	[https://github.com/ocornut/imgui](https://github.com/ocornut/imgui)

* **libdwarf (optional, linux-only)**

#### Included dependencies

* **STB\_image && STB\_dxt && STB\_image\_resize**
    [https://github.com/nothings/stb](https://github.com/nothings/stb)

* **rapidxml**  
	included in extern/rapidxml/, licensed under BSL 1.0.  
	[http://rapidxml.sourceforge.net/](http://rapidxml.sourceforge.net/)

* **boost::polygon::voronoi**   
    included in extern/boost_polygon/, licensed under BSL 1.0.  
	[http://www.boost.org/](http://www.boost.org/)

* **Volk**   
    included in extern/volk/, licensed under MIT license.
	[https://github.com/zeux/volk](https://github.com/zeux/volk)


## License

Whole library is licensed under [Boost Software license](https://github.com/nadult/libfwk/blob/main/license.txt).

If You found this library useful, please contact the author (nadult (at) fastmail (dot) fm).  
Any kind of feedback is greatly appreciated.
