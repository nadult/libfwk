# libfwk

## WTF is libfwk?

FWK is a sweet (IMVHO) abbreviation for framework. It is basically a set of classes
and functions which I use in most of my projects (mostly game-related). It's trying
to be light, super easy to use and performant where it matters.

TODO: write more detailed description.

## Features

- OpenGL & SDL2 based
- Supported platforms: linux, windows (using MinGW32-W64)
- Doesn't use C++ exceptions (has a much simpler system instead)

TODO: more details

## Examples & projects based on libfwk

* Some examples are available in src/test/ and src/tools.
* FreeFT [https://github.com/nadult/freeft](https://github.com/nadult/freeft)

## Requirements & dependencies

It requires a linux environment with fairly new G++ (7.1?) or Clang (6.0).
Code is written mainly in C++14 with some C++17 and C++20 features used. 

* **SDL2**   
	[https://www.libsdl.org/](https://www.libsdl.org/)

* **rapidxml**  
	included in src/rapidxml*/, licensed under Boost Software License  
	[http://rapidxml.sourceforge.net/](http://rapidxml.sourceforge.net/)

* **libpng**  
	[http://libpng.org](http://libpng.org)

* **freetype2**  
	[http://www.freetype.org/](http://www.freetype.org/)

* **libogg, libvorbis**  
	[https://xiph.org/ogg/](https://xiph.org/ogg/)
	[https://xiph.org/vorbis/](https://xiph.org/vorbis/)

* **OpenAL**  
	[https://www.openal.org/](https://www.openal.org/)


## Compilation

To compile simply run make. With Makefile.local you can override some basic variables
like LINUX\_CXX, FLAGS or BUILD\_DIR. If you want to cross-compile for MinGW32-w64,
simply override MINGW\_PREFIX in Makefile.local with proper prefix.
There is also an option to compile for the web (using emscripten) although it's not
fully supported.

TODO: how to build mingw with mxe (and which version)

## License

Whole library is licensed under Boost Software license (see license.txt).
