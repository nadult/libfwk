# libfwk

## WTF is libfwk?

FWK is a sweet (IMVHO) abbreviation for framework. It is basically a set of classes
and functions which I use in most of my projects (mostly game-related). It's trying
to be light, super easy to use and performant where it matters.

TODO: write more detailed description.

## Features

- OpenGL & SDL2 based
- Supported platforms: linux, windows (using MinGW32-W64), WebGL (through emscripten)
- Doesn't use C++ exceptions (has a much simpler system instead)

TODO: more details

## Examples & projects based on libfwk

* Some examples are available in src/test/ and src/tools.
* FreeFT [https://github.com/nadult/freeft](https://github.com/nadult/freeft)

## Requirements & dependencies

It requires a linux environment with fairly new Clang (>= 8.0) or G++ (>= 9.1).
Code is written in C++20. Clang is used during development so from time to
time libfwk may fail to compile under G++.

* **SDL2**   
	[https://www.libsdl.org/](https://www.libsdl.org/)

* **dear imgui (optional)**  
	Included as a git submodule in extern/imgui/.  
	Menu module & perf::Analyzer will only be compiled if imgui is present.  
	[https://github.com/ocornut/imgui](https://github.com/ocornut/imgui)

* **zlib**

* **freetype2**  
	[http://www.freetype.org/](http://www.freetype.org/)

* **libogg, libvorbis**  
	[https://xiph.org/ogg/](https://xiph.org/ogg/)
	[https://xiph.org/vorbis/](https://xiph.org/vorbis/)

* **OpenAL**  
	[https://www.openal.org/](https://www.openal.org/)

* **STB\_image && STB\_dxt && STB\_image\_resize (included)**
    [https://github.com/nothings/stb](https://github.com/nothings/stb)

* **rapidxml (included)**  
	included in extern/rapidxml/, licensed under BSL 1.0.  
	[http://rapidxml.sourceforge.net/](http://rapidxml.sourceforge.net/)

* **boost::polygon::voronoi (included)**   
    included in extern/boost_polygon/, licensed under BSL 1.0.  
	[http://www.boost.org/](http://www.boost.org/)

* **libdwarf (optional)**
    TODO: info

## Compilation

To compile simply run make. There is a basic description of the options that you can pass to make at the beginning of
Makefile & Makefile-shared. To cross-compile for mingw specify MINGW\_PREFIX and run make with PLATFORM=mingw.

Examples:

    $ make MODE=release-paranoid COMPILER=clang++-8 STATS=true -j12
    $ make MODE=release-paranoid print-stats print-variables
    $ make MODE=debug-nans clean
    $ make clean-all

TODO: how to build mingw with mxe (and which version)  
TODO: how to use Makefile-shared for your own projects

## License

Whole library is licensed under Boost Software license (see license.txt).

If You found this library useful, please contact the author (nadult (at) fastmail (dot) fm).  
Any kind of feedback is greatly appreciated.
