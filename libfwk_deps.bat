:: This file downloads conan packages and copies them to libraries directory
:: Package configuration: x86_64 / Release / MD / 16

set LIBRARIES_PATH=C:\libraries
set INCLUDE_PATH=%LIBRARIES_PATH%\x86_64\include
set     LIB_PATH=%LIBRARIES_PATH%\x86_64\lib

set      BZIP2_VER=1.0.8
set      BZIP2_CFG=d16a91eadaaf5829b928b12d2f836ff7680d3df5
set     BROTLI_VER=1.0.9
set     BROTLI_CFG=7e52046413f1e13a5fa19e3d410c5b50df627c3f
set     LIBPNG_VER=1.6.39
set     LIBPNG_CFG=ce650d9f1f1c1c0839cf0694a55c1351ddbed859
set   FREETYPE_VER=2.11.1
set   FREETYPE_CFG=27b2733304cef577b19f699fec3a5bdbefb36d16
set        OGG_VER=1.3.5
set        OGG_CFG=3fb49604f9c2f729b85ba3115852006824e72cab
set     OPENAL_VER=1.21.1
set     OPENAL_CFG=127af201a4cdf8111e2e08540525c245c9b3b99e
set        SDL_VER=2.26.1
set        SDL_CFG=5cda0de2c06faae2c1a6188bb8747aead42fd859
set     VORBIS_VER=1.3.7
set     VORBIS_CFG=becff00909fb3d957c6b4ca60fa64d4e6a32a540
set       ZLIB_VER=1.2.11
set       ZLIB_CFG=3fb49604f9c2f729b85ba3115852006824e72cab
set VK_HEADERS_VER=1.3.236.0
set VK_HEADERS_CFG=5ab84d6acfe1f23c4fae0ab88f26e3a396351ac9
set  VK_LOADER_VER=1.3.236.0
set  VK_LOADER_CFG=595054a9f5d93cebcdd932c4adb8ade54c859ee5
set    SHADERC_VER=2021.1
set    SHADERC_CFG=262692e8840f6e6a13986c13da07f3633baede95

:: TODO: spirv-tools, spirv-headers glslang


conan download         brotli/%BROTLI_VER%@:%BROTLI_CFG%
conan download          bzip2/%BZIP2_VER%@:%BZIP2_CFG%
conan download         libpng/%LIBPNG_VER%@:%LIBPNG_CFG%
conan download       freetype/%FREETYPE_VER%@:%FREETYPE_CFG%
conan download            ogg/%OGG_VER%@:%OGG_CFG%
conan download         openal/%OPENAL_VER%@:%OPENAL_CFG%
conan download            sdl/%SDL_VER%@:%SDL_CFG%
conan download         vorbis/%VORBIS_VER%@:%VORBIS_CFG%
conan download           zlib/%ZLIB_VER%@:%ZLIB_CFG%
::conan download vulkan-headers/%VK_HEADERS_VER%@:%VK_HEADERS_CFG%
::conan download vulkan-loader/%VK_LOADER_VER%@:%VK_LOADER_CFG%
::conan download       shaderc/%SHADERC_VER%@:%SHADERC_CFG%

mkdir %LIBRARIES_PATH%
mkdir %LIBRARIES_PATH%\x86_64
mkdir %INCLUDE_PATH%
mkdir %LIB_PATH%

xcopy %userprofile%\.conan\data\brotli\%BROTLI_VER%\_\_\package\%BROTLI_CFG%\include                 %INCLUDE_PATH% /s /e
xcopy %userprofile%\.conan\data\brotli\%BROTLI_VER%\_\_\package\%BROTLI_CFG%\lib                     %LIB_PATH% /s /e
xcopy %userprofile%\.conan\data\bzip2\%BZIP2_VER%\_\_\package\%BZIP2_CFG%\include                    %INCLUDE_PATH% /s /e
xcopy %userprofile%\.conan\data\bzip2\%BZIP2_VER%\_\_\package\%BZIP2_CFG%\lib                        %LIB_PATH% /s /e
xcopy %userprofile%\.conan\data\libpng\%LIBPNG_VER%\_\_\package\%LIBPNG_CFG%\include                 %INCLUDE_PATH% /s /e
xcopy %userprofile%\.conan\data\libpng\%LIBPNG_VER%\_\_\package\%LIBPNG_CFG%\lib                     %LIB_PATH% /s /e
xcopy %userprofile%\.conan\data\freetype\%FREETYPE_VER%\_\_\package\%FREETYPE_CFG%\include\freetype2 %INCLUDE_PATH% /s /e
xcopy %userprofile%\.conan\data\freetype\%FREETYPE_VER%\_\_\package\%FREETYPE_CFG%\lib               %LIB_PATH% /s /e
xcopy %userprofile%\.conan\data\ogg\%OGG_VER%\_\_\package\%OGG_CFG%\include                          %INCLUDE_PATH% /s /e
xcopy %userprofile%\.conan\data\ogg\%OGG_VER%\_\_\package\%OGG_CFG%\lib                              %LIB_PATH% /s /e
xcopy %userprofile%\.conan\data\openal\%OPENAL_VER%\_\_\package\%OPENAL_CFG%\include                 %INCLUDE_PATH% /s /e
xcopy %userprofile%\.conan\data\openal\%OPENAL_VER%\_\_\package\%OPENAL_CFG%\lib                     %LIB_PATH% /s /e
xcopy %userprofile%\.conan\data\sdl\%SDL_VER%\_\_\package\%SDL_CFG%\include                          %INCLUDE_PATH% /s /e
xcopy %userprofile%\.conan\data\sdl\%SDL_VER%\_\_\package\%SDL_CFG%\lib                              %LIB_PATH% /s /e
xcopy %userprofile%\.conan\data\vorbis\%VORBIS_VER%\_\_\package\%VORBIS_CFG%\include                 %INCLUDE_PATH% /s /e
xcopy %userprofile%\.conan\data\vorbis\%VORBIS_VER%\_\_\package\%VORBIS_CFG%\lib                     %LIB_PATH% /s /e
xcopy %userprofile%\.conan\data\zlib\%ZLIB_VER%\_\_\package\%ZLIB_CFG%\include                       %INCLUDE_PATH% /s /e
xcopy %userprofile%\.conan\data\zlib\%ZLIB_VER%\_\_\package\%ZLIB_CFG%\lib                           %LIB_PATH% /s /e

::xcopy %userprofile%\.conan\data\vulkan-headers\%VK_HEADERS_VER%\_\_\package\%VK_HEADERS_CFG%\include %INCLUDE_PATH% /s /e
::xcopy %userprofile%\.conan\data\vulkan-headers\%VK_HEADERS_VER%\_\_\package\%VK_HEADERS_CFG%\lib     %LIB_PATH% /s /e
::xcopy %userprofile%\.conan\data\vulkan-loader\%VK_LOADER_VER%\_\_\package\%VK_LOADER_CFG%\include    %INCLUDE_PATH% /s /e
::xcopy %userprofile%\.conan\data\vulkan-loader\%VK_LOADER_VER%\_\_\package\%VK_LOADER_CFG%\lib        %LIB_PATH% /s /e
::xcopy %userprofile%\.conan\data\shaderc\%SHADERC_VER%\_\_\package\%SHADERC_CFG%\include              %INCLUDE_PATH% /s /e
::xcopy %userprofile%\.conan\data\shaderc\%SHADERC_VER%\_\_\package\%SHADERC_CFG%\lib                  %LIB_PATH% /s /e
