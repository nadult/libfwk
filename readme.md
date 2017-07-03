# libfwt

## WTF is libfwt?

FWT is a sweet (IMVHO) abbreviation for framework. It is basically a set of classes
and functions which I use in most of my projects. It requires fairly new g++ (7.1) or
clang (4.0).

TODO: write more detailed description.

## Features

Supported platforms: linux, windows (using mingw), browser (using emscripten)

TODO: write me

## Dependencies

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

libfwt requires C++14 support. It also uses some features from C++17:  
- constexpr if  
- simplified static assert  

To compile simply run make. With Makefile.local you can override some basic variables
like LINUX\_CXX, FLAGS or BUILD\_DIR. If you want to cross-compile for mingw32,
simply override MINGW\_PREFIX in Makefile.local with proper prefix.
There is also an option to compile for the web (using emscripten).


