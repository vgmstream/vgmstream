# vgmstream lib build help
This document explains how to build various external dependencies used in vgmstream.

The main purpose is to have a reference of what each lib is doing, and to rebuild Windows DLLs. Linux libs are handled automatically using CMake.

See [BUILD](BUILD.md#external-libraries) for a description of each lib.

## Intro
Guide is mainly geared towards **Windows** DLLs, as a reference for later updates. For **Linux/Mac**, libs are already included when using *CMake*, but you can mostly follow this with minor tweaks to create linkable `.so` libs.

Each lib is compiled using a *recommended version*, but most should work with recent versions (FFMpeg may rarely change the *API* though). Most libs don't provide official pre-compiled *binaries*, so we need to compile them ourselves.

### Requirements
Guide assumes you followed the steps in [BUILD](BUILD.md) to install Git, GCC (MingW or MSYS2) or Visual Studio, but you'll need GCC most of the time. Some projects allow using MSVC or GCC; both should work fine and with comparable performance.

MSVC commands use `msbuild.exe`, which can be called by opening VS's **x86 Native Tools for VS 20xx** console, found on Windows start menu. It can also be added to regular `PATH` for CMD/MSYS2 as well, but since location changes with versions you may need to download *vswhere* (locator) to find it:
```bat
curl --location https://github.com/Microsoft/vswhere/releases/download/2.6.7/vswhere.exe --output vswhere.exe
REM #path to MSBuild
vswhere.exe -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
REM set PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin
```

### Tools and PATHs
When using Mingw+Git you may add their location to Windows' `PATH` variable, so programs like `gcc.exe` work as-is in Windows CMD without writing the full path. This can be done temporarily by writing in the command line: `set PATH=C:\(path-to-mingw)\mingw32\bin;C:\Git\usr\bin;%PATH%` (for example), or in Windows' system variables panel.

For MSYS2 commands should work (after installing relevant programs) by opening the `msys2/mingw32.exe` console.

**Important** though is that some libs (like mpg123) only work properly when GCC/MSYS2 paths goes *before* Windows default path, since some program names conflict otherwise.

### standalone MinGW tweaks
MinGW has `mingw32-make.exe` but most scripts expect plain `make`. Generally you can pass a `MAKE=mingw32-make` variable with the full name, but may be easier to clone/rename it to `make.exe`. 

Also for autotool-based buils, it's best to put files (and dev tools) in folders paths *without spaces* as some scripts still get tripped by that. For the sake of the steps below you for (for example) run commands in `C:\vgmstream-dlls\sources`.

### autotools
Several libs use *autotools* (a collection of scripts that guess system's config), that typically works by doing:
```sh
# creates a custom Makefile(s) based on current system
sh ./configure [params]

# default, compiles code using generated Makefile(s) (resulting .dll/files in some internal folder)
make [params]

# copies resulting files to some pre-defined dir, and compiles if plain make wasn't called first
# optionally, "make install-strip" does the same, but "strips" binaries (good)
make install
make install-strip

# cleans compiled code (important when changing ./configure options)
make clean
```

While mainly geared towards *Linux* using GCC-like compilers, works on *Windows* as long as typical *Linux* utils and compilers can be found `PATH`. Not all *configure* are created equal so there is some fiddling around depending on lib.

Usually, *autoconf* DLLs are generated with *debug symbols* by default. This can be fixed by calling's GCC `strip (dll)`, generally done automatically or when using `make install-strip`.

On Linux `install` is used to actually *install* libs on system dirs (so `--prefix` is rarely used), while on Windows is just to copy DLLs, `.h` (used in C code), linker libs (not needed) and other stuff to final dir.

You can call multiple *targets* in a single line `make clean install-strip` is the same as `make clean` and `make install` (which in turn calls plain `make` / default). That's the theory, but at some libs don't properly handle this.

### autotools config
*autotools* are **very** fragile and picky, beware when changing stuff. Check other flags by calling `sh ./configure --help`, but changing some of the steps will likely cause odd issues. *autotools are not consistent between libs*.

Common *configure*/Makefile params:
- `--build=...`: current compilation environment (autodetected, but may fail in outdated libs)
- `--host=...`: target system (same as build by default), can be forced for cross-compilation.
- `--target=...`: target binary (same as host by default). Not really needed.
  - how much `build/host/target` matter depends on lib, included always by default.
- `--target-os/--arch/--etc`: depending on script, to ease config
- `--disable-static --enable-shared`: only needed config as appropriate (varies)
- `--prefix=/c/vgmstream-dlls/(varies)/`: *make install* dir (where libs are copied), to simplify we'll use a fixed one
  - defaults to `/c/Git/usr/local` (standalone) or MSYS2's `/usr/local` folders if not set
  - if you don't call *make install* DLLs will be still there, inside `.libs` subdirs usually (will need to `strip`)
  - most projects (except FFmpeg only?) need a full path (`/path/...`) and won't accept a relative one (`./subdir/...`)
- `MAKE=mingw32-make`: may be passed for mingw32 to work properly (otherwise rename `mingw32-make.exe` to `make.exe`)
- `CFLAGS` / `AM_CFLAGS` / `LDFLAGS` / `AM_LDFLAGS`: extra compiler/linker flags

Compiler/linker flags are very important yet have big gotchas:
- pass `-m32`/`-64` to the compiler for 32/64-bit output
  - this may be autodetected and set in some environments
- pass `-static-libgcc` to the linker to remove Mingw-w64 DLL dependencies (not needed in 64-bit DLLs?)
- `CFLAGS` / `LDFLAGS` on *configure* will usually (**not always**) *add* to the default ones
- `CFLAGS` on `make` will usually *overwrite* default ones (such as `-O2` optimizations)
- `AM_CFLAGS` on `make` should work together with `CFLAGS`, but actually aren't always passed to all `.c`
- `LDFLAGS` on `make` may *overwrite* default ones, but often aren't set
- `AM_LDFLAGS` on `make` should work together with `LDFLAGS`, but some libs don't support them, or *libtool* (hellspawn script that internally generates final `.dll`) sometimes only reads `LDFLAGS`
However, *those flags aren't consistent between libs*, meaning in one using *configure* + `CFLAGS` adds to existing CFLAGS, other overwrites them. So, scripts below may look inconsistent, but they certain flags for a reason.

### Xiph's releases and exports
Sometimes we use "official releases" sources rather than using Git's sources. Both should be the same, but releases have pre-generated *./configure*, while Git needs to call `autogen.sh` that calls `autoreconf` that generates a base `configure` script. Since getting `autoreconf` working on **Windows** without MSYS2 requires extra steps (not described), Xiph's releases are recommended.

When building a DLL/lib, compiler sets *exported symbols* (functions). Xiph's *autoconf* may generate DLLs correctly, but don't detect Mingw/Win config properly and export all symbols by default. This is fixed manually, but there may be better ways to handle it (to be researched).

### Shared libs details
Roughly, a `.dll` is a Windows "shared library"; Linux's equivalent would be a `.so` file. First, `.c` files are compiled into objects (`.o` in GCC, `.obj` in MSCV), then can be made into a `.dll`. Later, when a program needs that DLL (or rather, it's functions), a compiler can use it as long as some conditions are met.

DLL must *export symbols* (functions), which on a Windows's DLL is done with:
- adding `__declspec(dllexport)` to a function (usually done with `#define EXPORT ...` and similar ways)
- using a `.def` module definition file
- if neither of the above is used, GCC exports every function by default (not great)

Then, to *link* (refer to) a DLL compiler usually needs helper files (`.dll.a` in GCC, `.lib` in MSVC). DLL's are copied to vgmstream's source, while helper files are created on compile time from `.dll`+`.def` (see *ext_libs/Makefile* for GCC and `ext_libs.vcxproj` for MSVC).

DLLs also *links* to standard C lib (MingW: `msvcrt.dll`, MSVC: `msvcrt(version).dll`). On Windows there are multiple versions of this *runtime*, but DLLs may include (part of) it with certain compiler/linker flags. This means there are subtle differences between compiler's generated DLLs, but for libs (that only do limited stuff) they don't matter much.

### Static libs details
*vgmstream* uses external DLLs to support extra codecs, but it's clunkier and less user-friendly needing a bunch of extra DLLs around. Ideally *vgmstream* could use *static libs* instead (eliminating the need of DLLs), but it's complex and not done at the moment.

To make static libs, all objects (`.o`/`.obj`) are integrated to an archive (`.a` in GCC, `.lib` in MSVC) then this can use used by compiler. However, unlike DLLs, mixing static libs from one compiler with another is harder due to compiler dependencies that aren't a problem with DLLs. For example, by default Mingw's static libs may depend on `libmingwex.a` and would need that lib if used with MSVC.

One could have static libs for each compiler, but not all projects can be compiled with MSVC or GCC, also being a lot of extra work. Incidentally, C++ DLLs/libs can't be easily shared between MSVC and GCC (unless carefully prepared to be so), unlike plain C libs that are mostly compatible.

### 32 and 64-bit
Maybe obvious, but programs and DLLs can be compiled as 32-bit or 64-bit, but you can't mix 64-bit programs and 32-bit DLLs (on **Windows** mixing DLLs will usually result on error `0xc00007b`). Compilers, being programs, can be 32 or 64-bit as well.

Both 32/64-bit GCC can compile 32-bit or 64-bit without issues, using the `-m32`/`-m64` flags (because GCC is able to create win32/win64/arm32/arm64/etc executables regardless of itself being 32/64-bit), while MSVC probably does as well, but just in case install the 64-bit version that handles both and decides what to generate based on info from `.vcxproj` files.


## Compiling external libs

### mpg123
Must use *autotools*, though some scripts may simplify the process (`makedll.sh`, `windows-builds.sh x86/x86-64`).

#### Source
```bat
curl --location https://sourceforge.net/projects/mpg123/files/mpg123/1.33.7/mpg123-1.33.7.tar.bz2 --output mpg123-1.33.7.tar.bz2
tar -xf mpg123-1.33.7.tar.bz2
cd mpg123-1.33.7
```

#### With GCC
*Notes*:
- if `make` ends with a libtool message of "syntax error near token", make sure GCC/MSYS2/Linux utils go *first* in `PATH` (`set PATH=C:\(...);%PATH%`).
- `make install-strip` throws an error and fails to copy `.h` but seems to properly strip DLLs (regular `install` is used to copy `.h`).

```bat
REM # 32-bit DLL
sh ./configure --host=mingw32 --disable-static --enable-shared --with-cpu=x86 --prefix=/c/vgmstream-dlls/out/mpg123-32 CFLAGS="-m32" LDFLAGS="-static-libgcc"
make clean install install-strip

REM # 64-bit DLL
sh ./configure --host=mingw64 --disable-static --enable-shared --with-cpu=x86-64 --prefix=/c/vgmstream-dlls/out/mpg123-64 CFLAGS="-m64" LDFLAGS="-static-libgcc"
make clean install install-strip
```

#### With MSVC
Untested/not possible?


### libvorbis/libogg
Should be buildable with *autotools* (Git releases need to use `autogen.sh` first) or MSVC (projects in `./win32/`, may not be up to date). *CMake* may work as well.

Methods below create 3 DLL: `libogg.dll`, `libvorbis.dll` and `libvorbisfile.dll` (also `libvorbisenc.dll`, unneeded), plus static libs (`.a`). However Vorbis/Ogg DLL support in vgmstream was originally added using a combined DLL from *RareWares* (https://www.rarewares.org/ogg-libraries.php) simply called `libvorbis.dll`, so separate DLLs can't be used at the moment and we'll need to fix that.

**TODO**: should restrict exported symbols (unsure how libvorbis does it, here it's manually done on last step)

#### Sources
``` bat
curl --location http://downloads.xiph.org/releases/ogg/libogg-1.3.5.zip --output libogg-1.3.5.zip
tar -xf libogg-1.3.5.zip
cd libogg-1.3.5
```
```
curl --location http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.zip --output libvorbis-1.3.7.zip
tar -xf libvorbis-1.3.7.zip
cd libvorbis-1.3.7
```

#### With GCC
*Notes*:
- `-ffast-math` isn't enabled by default but seems safe and commonly done elsewhere

**libogg**
```bat
REM # 32-bit DLL + static lib
sh ./configure --build=mingw32 --enable-static --enable-shared --prefix=/c/vgmstream-dlls/out/ogg-32 CFLAGS="-m32" LDFLAGS="-static-libgcc"
make clean install-strip

REM # 64-bit DLL + static lib
sh ./configure --build=mingw64 --enable-static --enable-shared --prefix=/c/vgmstream-dlls/out/ogg-64 CFLAGS="-m64" LDFLAGS="-static-libgcc"
make clean install-strip
```

**libvorbis/libvorbisfile**
```bat
REM # 32-bit DLL + static lib
sh ./configure --host=mingw32 --enable-static --enable-shared --disable-docs --prefix=/c/vgmstream-dlls/out/vorbis-32 --with-ogg=/c/vgmstream-dlls/out/ogg-32 CFLAGS="-m32 -ffast-math" LDFLAGS="-static-libgcc"
make clean install-strip

sh ./configure --host=mingw64 --enable-static --enable-shared --disable-docs --prefix=/c/vgmstream-dlls/out/vorbis-64 --with-ogg=/c/vgmstream-dlls/out/ogg-64 CFLAGS="-m64 -ffast-math" LDFLAGS="-static-libgcc"
make clean install-strip
```

**libvorbix mix**
There no official (autoconf) way to bundle all 3 libs above into one DLL, as we need, but we can do it manually (ugly but what can you do).

Commands, based on how generated makefile creates dlls (-Wl are 'linker' commands):
- `-shared`: create a .so/.dll
- `(file).def`: limits exported DLL symbols/functions (no dllexports so gcc can't tell otherwise)
- `-s`: strips DLL (removes symbols)
- `-o (name)`: output
- `-Wl,--whole-archive (multiple .a) -Wl,--no-whole-archive`: adds all `.o` inside those `.a` (archives)
  - otherwise could use `ar xo (archive.s)` to unzip and manually pass all `.o` inside to gcc
- `-Wl,--enable-auto-image-base`: for DLLs, improves loading?
- `-Wl,--out-implib=(name).dll.a`: creates a helper link .lib, though we don't really need it
- `-Wl,--output-def=(name).def`: can be included to create a final export file (to compare)

This needs `libvorbis.def` from *vgmstream/ext_libs*, and doesn't include `libvorbisenc` code.
```bat
cd C:\vgmstream-dlls\out

REM # 32-bit DLL
gcc -m32 -shared -s libvorbis.def -o vorbis-32/libvorbis.dll -Wl,--whole-archive ogg-32/lib/libogg.a vorbis-32/lib/libvorbisfile.a vorbis-32/lib/libvorbis.a -Wl,--no-whole-archive -Wl,--enable-auto-image-base

REM # 64-bit DLL
gcc -m64 -shared -s libvorbis.def -o vorbis-64/libvorbis.dll -Wl,--whole-archive ogg-64/lib/libogg.a vorbis-64/lib/libvorbisfile.a vorbis-64/lib/libvorbis.a -Wl,--no-whole-archive -Wl,--enable-auto-image-base
```

#### With MSVC/CMake
Untested.


### libcelt
FSB uses two incompatible, older libcelt versions. Both libraries export the same symbols so normally can't coexist together. To get them working we need to make sure symbols are renamed first. This may be solved in various ways:
- using dynamic loading (LoadLibrary) but for portability it isn't an option
- may be possible to link+rename using .def files, but those are mainly used on **Windows**
- Linux/Mingw's objcopy to (supposedly) rename DLL symbols
- Use GCC's preprocessor to rename functions on compile
- Directly rename functions in source code

We'll use autotools with GCC preprocessor renaming. On **Windows** steps are described below; on **Linux** you can use CMake that patch celt libs automatically (or follow the steps removing Windows-only config).

#### Source

**celt-0.6.1**
```bat
curl --location http://downloads.us.xiph.org/releases/celt/celt-0.6.1.tar.gz --output celt-0.6.1.tar.gz
tar -xf celt-0.6.1.tar.gz 
cd celt-0.6.1
```

**celt-0.11.0**
```bat
curl --location http://downloads.us.xiph.org/releases/celt/celt-0.11.0.tar.gz --output celt-0.11.0.tar.gz
tar -xf celt-0.11.0.tar.gz 
cd celt-0.11.0
```

#### With GCC
*Notes*:
- on **Windows** exports need to be fixed (undefines `CELT_BUILD` and defines `WIN32` to allow `dllexport`)
  - uses `sed` (Linux text replacer) to change `#define`
  - despite the name, `CELT_BUILD` seems to be used only to detect shared builds
- on **Windows** *celt-0.6.1*'s *libtool* somehow removes `-static-libgcc` from `LDFLAGS`, meaning DLLs depend on Mingw's libs
  - partially fixed using `-Wc,-static-libgcc`, but will cause various errors
  - uses `sed` (Linux text replacer) to insert `-static-libgcc` into *libtool* calls
- detects build environment incorrectly (outdated scripts)
  - uses `mingw32` even on 64-bit due to older *configure*
- `CFLAGS` on *configure* overwrite defaults, also no `AM_LDFLAGS`
- in theory passing `--enable-custom-modes` in *configure* would be equivalent to `-DCUSTOM_MODES=1` in 0.11.0 but doesn't seem to work?
- `SUBDIRS="libcelt" DIST_SUBDIRS="libcelt"` forces Makefile to compile libcelt only and ignore tests (no other way to disable)
  - otherwise (buggy?) *configure* may detect *libogg* + compiles extra stuff (not an issue on Windows but for completeness)
  - `--disable-oggtest`: oggtest are test utils, not related to *libogg* detection
- when compiling GCC may complain about missing `ec_log`, but it seems correctly defined in `entcode.c`and included in the lib and not a cause of issues.
  - `-no-undefined` is necessary for this or the DLL won't be created
- uses preprocessor renaming (encoder functions aren't needed but for completion)

**TODO**: would be better to rename DLL output (since it's part of the DLL and makes .def simpler) but who knows what exact command is used

**celt-0.6.1**
```bat
REM # 32-bit DLL
sh ./configure --build=mingw32 --disable-static --enable-shared --disable-oggtest --prefix=/c/vgmstream-dlls/out/celt0061-32
sed -i -e "s/#define CELT_BUILD.*/#undef CELT_BUILD/g" config.h
sed -i -e "s/compiler_flags -o/compiler_flags -static-libgcc -o/g" libtool
make clean
make SUBDIRS="libcelt" DIST_SUBDIRS="libcelt" LDFLAGS="-m32 -static-libgcc -no-undefined" AM_CFLAGS="-m32 -DWIN32 -Dcelt_decode=celt_decode_0061 -Dcelt_decoder_create=celt_decoder_create_0061 -Dcelt_decoder_destroy=celt_decoder_destroy_0061 -Dcelt_mode_create=celt_mode_create_0061 -Dcelt_mode_destroy=celt_mode_destroy_0061 -Dcelt_mode_info=celt_mode_info_0061 -Dcelt_decode_float=celt_decode_float_0061 -Dcelt_decoder_ctl=celt_decoder_ctl_0061 -Dcelt_encode=celt_encode_0061 -Dcelt_encode_float=celt_encode_float_0061 -Dcelt_encoder_create=celt_encoder_create_0061 -Dcelt_encoder_ctl=celt_encoder_ctl_0061 -Dcelt_encoder_destroy=celt_encoder_destroy_0061 -Dcelt_header_from_packet=celt_header_from_packet_0061 -Dcelt_header_init=celt_header_init_0061 -Dcelt_header_to_packet=celt_header_to_packet_0061"
make install-strip SUBDIRS="libcelt" DIST_SUBDIRS="libcelt"

REM # 64-bit DLL
sh ./configure --build=mingw32 --disable-static --enable-shared --disable-oggtest --prefix=/c/vgmstream-dlls/out/celt0061-64
sed -i -e "s/#define CELT_BUILD.*/#undef CELT_BUILD/g" config.h
sed -i -e "s/compiler_flags -o/compiler_flags -static-libgcc -o/g" libtool
make clean
make SUBDIRS="libcelt" DIST_SUBDIRS="libcelt" LDFLAGS="-m64 -static-libgcc -no-undefined" AM_CFLAGS="-m64 -DWIN32 -Dcelt_decode=celt_decode_0061 -Dcelt_decoder_create=celt_decoder_create_0061 -Dcelt_decoder_destroy=celt_decoder_destroy_0061 -Dcelt_mode_create=celt_mode_create_0061 -Dcelt_mode_destroy=celt_mode_destroy_0061 -Dcelt_mode_info=celt_mode_info_0061 -Dcelt_decode_float=celt_decode_float_0061 -Dcelt_decoder_ctl=celt_decoder_ctl_0061 -Dcelt_encode=celt_encode_0061 -Dcelt_encode_float=celt_encode_float_0061 -Dcelt_encoder_create=celt_encoder_create_0061 -Dcelt_encoder_ctl=celt_encoder_ctl_0061 -Dcelt_encoder_destroy=celt_encoder_destroy_0061 -Dcelt_header_from_packet=celt_header_from_packet_0061 -Dcelt_header_init=celt_header_init_0061 -Dcelt_header_to_packet=celt_header_to_packet_0061"
make install-strip SUBDIRS="libcelt" DIST_SUBDIRS="libcelt"
```

**celt-0.11.0**
```bat
REM # 32-bit DLL
sh ./configure --build=mingw32 --disable-static --enable-shared --disable-oggtest --prefix=/c/vgmstream-dlls/out/celt0110-32
sed -i -e "s/#define CELT_BUILD.*/#undef CELT_BUILD/g" config.h
make clean
make SUBDIRS="libcelt" DIST_SUBDIRS="libcelt" LDFLAGS="-m32 -static-libgcc -no-undefined" AM_CFLAGS="-m32 -DWIN32 -DCUSTOM_MODES=1 -Dcelt_decode=celt_decode_0110 -Dcelt_decoder_create_custom=celt_decoder_create_custom_0110 -Dcelt_decoder_destroy=celt_decoder_destroy_0110 -Dcelt_mode_create=celt_mode_create_0110 -Dcelt_mode_destroy=celt_mode_destroy_0110 -Dcelt_mode_info=celt_mode_info_0110 -Dcelt_decode_float=celt_decode_float_0110 -Dcelt_decoder_create=celt_decoder_create_0110 -Dcelt_decoder_ctl=celt_decoder_ctl_0110 -Dcelt_decoder_get_size=celt_decoder_get_size_0110 -Dcelt_decoder_get_size_custom=celt_decoder_get_size_custom_0110 -Dcelt_decoder_init=celt_decoder_init_0110 -Dcelt_decoder_init_custom=celt_decoder_init_custom_0110 -Dcelt_encode=celt_encode_0110 -Dcelt_encode_float=celt_encode_float_0110 -Dcelt_encoder_create=celt_encoder_create_0110 -Dcelt_encoder_create_custom=celt_encoder_create_custom_0110 -Dcelt_encoder_ctl=celt_encoder_ctl_0110 -Dcelt_encoder_destroy=celt_encoder_destroy_0110 -Dcelt_encoder_get_size=celt_encoder_get_size_0110 -Dcelt_encoder_get_size_custom=celt_encoder_get_size_custom_0110 -Dcelt_encoder_init=celt_encoder_init_0110 -Dcelt_encoder_init_custom=celt_encoder_init_custom_0110 -Dcelt_header_from_packet=celt_header_from_packet_0110 -Dcelt_header_init=celt_header_init_0110 -Dcelt_header_to_packet=celt_header_to_packet_0110 -Dcelt_strerror=celt_strerror_0110"
make install-strip SUBDIRS="libcelt" DIST_SUBDIRS="libcelt"

REM # 64-bit DLL
sh ./configure --build=mingw32 --disable-static --enable-shared --disable-oggtest --prefix=/c/vgmstream-dlls/out/celt0110-64
sed -i -e "s/#define CELT_BUILD.*/#undef CELT_BUILD/g" config.h
make clean
make SUBDIRS="libcelt" DIST_SUBDIRS="libcelt" LDFLAGS="-m64 -static-libgcc -no-undefined" AM_CFLAGS="-m64 -DWIN32 -DCUSTOM_MODES=1 -Dcelt_decode=celt_decode_0110 -Dcelt_decoder_create_custom=celt_decoder_create_custom_0110 -Dcelt_decoder_destroy=celt_decoder_destroy_0110 -Dcelt_mode_create=celt_mode_create_0110 -Dcelt_mode_destroy=celt_mode_destroy_0110 -Dcelt_mode_info=celt_mode_info_0110 -Dcelt_decode_float=celt_decode_float_0110 -Dcelt_decoder_create=celt_decoder_create_0110 -Dcelt_decoder_ctl=celt_decoder_ctl_0110 -Dcelt_decoder_get_size=celt_decoder_get_size_0110 -Dcelt_decoder_get_size_custom=celt_decoder_get_size_custom_0110 -Dcelt_decoder_init=celt_decoder_init_0110 -Dcelt_decoder_init_custom=celt_decoder_init_custom_0110 -Dcelt_encode=celt_encode_0110 -Dcelt_encode_float=celt_encode_float_0110 -Dcelt_encoder_create=celt_encoder_create_0110 -Dcelt_encoder_create_custom=celt_encoder_create_custom_0110 -Dcelt_encoder_ctl=celt_encoder_ctl_0110 -Dcelt_encoder_destroy=celt_encoder_destroy_0110 -Dcelt_encoder_get_size=celt_encoder_get_size_0110 -Dcelt_encoder_get_size_custom=celt_encoder_get_size_custom_0110 -Dcelt_encoder_init=celt_encoder_init_0110 -Dcelt_encoder_init_custom=celt_encoder_init_custom_0110 -Dcelt_header_from_packet=celt_header_from_packet_0110 -Dcelt_header_init=celt_header_init_0110 -Dcelt_header_to_packet=celt_header_to_packet_0110 -Dcelt_strerror=celt_strerror_0110"
make install-strip
```

Resulting DLLs need to be renamed to `libcelt-0061.dll` and `libcelt-0110.dll`, and may need to create a `.def` file with `gendef (name).dll` (in theory this is done by passing `-Wl,--output-def=libcelt.def` to `AM_FLAGS` but seems to fail). vgmstream also needs `celt.h`, `celt_types.h`, `celt_header.h` with renamed functions, but a custom `.h` with minimal symbols is already included in source.

#### With MSVC
Untested/not possible.


### libspeex
Should be buildable with *autotools* (Git releases need to use `autogen.sh` first) or MSVC (projects in `./win32/`, may not be up to date).

#### Source
```bat
curl --location http://downloads.us.xiph.org/releases/speex/speex-1.2.1.tar.gz --output speex-1.2.1.tar.gz
tar -xf speex-1.2.1.tar.gz
cd speex-1.2.1
```

#### With GCC
*Notes*:
- on **Windows** exports need to be fixed (swaps Linux exports with Windows' `dllexport`)
- `CFLAGS` on *configure* overwrite defaults
```bat
REM # 32-bit DLL
sh ./configure --host=mingw32 --disable-static --enable-shared --prefix=/c/vgmstream-dlls/out/speex-32
sed -i -e "s/#define EXPORT .*/#define EXPORT __declspec(dllexport)/g" config.h
make clean install-strip LDFLAGS="-m32 -static-libgcc" AM_CFLAGS="-m32"

REM # 64-bit DLL
sh ./configure --host=mingw64 --disable-static --enable-shared --prefix=/c/vgmstream-dlls/out/speex-64
sed -i -e "s/#define EXPORT .*/#define EXPORT __declspec(dllexport)/g" config.h
make clean install-strip LDFLAGS="-m64 -static-libgcc" AM_CFLAGS="-m64"
```

#### With MSVC
Untested/outdated.


### libopus
Used below by FFmpeg (but can be disabled), as a static lib (`.a`/`.lib`) rather than DLL.

#### Source
```bat
curl --location https://downloads.xiph.org/releases/opus/opus-1.6.1.tar.gz --output opus-1.6.1.tar.gz
tar -xf opus-1.6.1.tar.gz
cd opus-1.6.1
```

#### With GCC
```bat
REM # 32-bit lib
sh ./configure --host=mingw32 --enable-static --disable-shared --disable-doc --disable-extra-programs --prefix=/c/vgmstream-dlls/out/opus-32
make clean install-strip LDFLAGS="-m32 -static-libgcc" AM_CFLAGS="-m32"

REM # 64-bit lib
sh ./configure --host=mingw32 --enable-static --disable-shared --disable-doc --disable-extra-programs --prefix=/c/vgmstream-dlls/out/opus-64
make clean install-strip LDFLAGS="-m64 -static-libgcc" AM_CFLAGS="-m64"
```

#### With MSVC + CMake
```bat
REM # 32-bit .lib
cmake -S . -B build-vs-x86 -G "Visual Studio 16 2019" -A Win32 -DOPUS_STATIC_RUNTIME=ON -DCMAKE_INSTALL_PREFIX="C:\vgmstream-dlls\out\opus-vs-32"
cmake --build build-vs-x86 --config Release
cmake --install build-vs-x86 --config Release

REM # 64-bit .lib
cmake -S . -B build-vs-x64 -G "Visual Studio 16 2019" -A x64 -DOPUS_STATIC_RUNTIME=ON -DCMAKE_INSTALL_PREFIX="C:\vgmstream-dlls\out\opus-vs-64"
cmake --build build-vs-x64 --config Release
cmake --install build-vs-x64 --config Release
```

### FFmpeg
vgmstream's FFmpeg builds for **Windows** and static builds for **Linux** remove many unnecessary parts of FFmpeg to trim down its gigantic size, and, on Windows, are also built with the "-vgmstream" suffix to prevent clashing with other plugins. 

Those options are passed to the *configure* script before compilation. Current options can be seen in `vgmstream/ext_libs/extra/ffmpeg_options.txt`, but should work with default options as well.

Should work with any recent version of FFmpeg. When updating FFmpeg versions careful testing is recommended. *vgmstream* fixes a few of FFmpeg's quirks during playback; some may need to be removed depending on version.

FFmpeg uses separates libs/DLLs, that depend on each other like this:
- avutil: none
- swresample: avutil
- avformat: avcodec, avutil
- avcodec: avutil, swresample

#### Extra notes

##### Linked headers
If you are using your own FFmpeg libs (not pre-compiled for vgmstream), make sure you compile with the FFmpeg `.h` headers provided by your lib and not those in `ext_includes`. vgmstream **will crash** if the FFmpeg version has changed (see `LIB*_VERSION_MAJOR` and possibly `LIB*_VERSION_MINOR`), even if it compiles correctly.

##### libopus vs FFmpeg's opus
FFmpeg is compiled with *libopus* (external lib) rather than internal *opus*.

We use *libopus* because FFmpeg decodes incorrectly certain games (mostly low bitrate). In older versions this was audibly wrong, though currently the differences shouldn't be too noticeable. Getting *libopus* recognized can be unwieldly, so internal *opus* is a decent enough substitute.

To disable it, go to `ffmpeg_options.txt` and remove `--enable-libopus` plus change `libopus` to `opus` in `--enable-decoder`. Also, when calling *configure* remove `--enable-custom-modes` (a *libopus*-only option).

If you wonder why use it through FFmpeg instead of directly, all work was already done for FFmpeg's opus so it easier this way.

##### libopus and pkg-config
FFmpeg uses *pkg-config* (a kind of "installed lib manager") to detect pre-compiled *libopus*. On Linux it should detect *libopus* after `make install` with default `--prefix`, or adding opus's `--prefix` path to `PKG_CONFIG_PATH`. On Windows, MSYS2 *probably* works the same.

However when compiling with MSVC it's not clear how to mix Windows-style `.lib` and *pkg-config*, though should be possible (*media-autobuild_suite* project does it?). For now as a temp hack, we can force FFmpeg to skip pkg-config and manually pass lib's location. But if you can get *pkg-config* to work ignore "pkg-config hack" steps (**TODO**: to be researched).

##### nasm
FFmpeg needs `nasm` somewhere in `PATH` for assembly optimizations.

To disable it, add `--disable-x86asm` to the ffmpeg *configure* options (older versions used `yasm` / `--disable-yasm`).


#### Source and builds
```sh
# clone only current tag's "depth" as otherwise FFmpeg history is pretty big
git clone https://git.ffmpeg.org/ffmpeg.git --depth 1 --branch n9.0.2
cd ffmpeg
```

FFmpeg needs *autotools* and GCC or MSVC installed, in a *MSYS2* environment (preferably) or manually from *mingw32*.


#### With GCC
**Notes**
- there may be issues with atomics/threads in some cases.

```sh
# read current options, removing comments and line breaks (copy from vgmstream's source)
FFMPEG_OPTIONS=`sed -e '/^#/d' ../ffmpeg_options.txt`

# make Opus known to pkconfig
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/c/vgmstream-dlls/out/opus-32/lib/pkgconfig"

# 32-bit DLL: prepare options + compile + copy
sh ./configure $FFMPEG_OPTIONS --target-os=mingw32 --arch=x86 --extra-ldflags="-static-libgcc -static" --prefix=/c/vgmstream-dlls/out/ffmpeg-32 
make clean
make
make install
```

```sh
# read current options, removing comments and line breaks (copy from vgmstream's source)
FFMPEG_OPTIONS=`sed -e '/^#/d' ../ffmpeg_options.txt`

# make Opus known to pkconfig
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/c/vgmstream-dlls/out/opus-64/lib/pkgconfig"

# 64-bit DLL: prepare options + compile + copy
sh ./configure $FFMPEG_OPTIONS --target-os=mingw32 --arch=x86_64 --extra-ldflags="-static-libgcc -static" --prefix=/c/vgmstream-dlls/out/ffmpeg-64
make clean
make
make install
```

#### With MSVC + MSYS2 (32-bit)
First install MSYS2 and Visual Studio. Then compile *libopus* with MSVC to get a `.lib` (see above).

Open **x86 Native Tools Command Prompt for VS 20xx** console, found on Windows' start menu, and run:
```bat
C:\msys64\msys2_shell.cmd -mingw32 -use-full-path
```

A new *MSYS2* console should open. You can close the *Command Prompt* now. Some checks (first time only):
```sh
# Both must show MSVC's path, exported by *VS Native Tools Command Prompt*
# If link is /usr/bin/link.exe, try temp-renaming it: mv /usr/bin/link.exe /usr/bin/link.exe.bak
which cl
which link

# Install all needed dependencies
pacman -S --needed git gcc make autotools pkg-config nasm
#pacman -S --needed mingw-w64
```

Finally enter the `ffmpeg` dir and build the DLL:
```sh
# read current options, removing comments and line breaks (copy from vgmstream's source)
FFMPEG_OPTIONS=`sed -e '/^#/d' ../ffmpeg_options.txt`

# make MSVC Opus known to pkconfig
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/c/vgmstream-dlls/out/opus-vs-32/lib/pkgconfig"

# 32-bit DLL: prepare options + compile + copy
sh ./configure $FFMPEG_OPTIONS --target-os=win32 --toolchain=msvc --arch=x86 --prefix=/c/vgmstream-dlls/out/ffmpeg-vs-32
make clean
make
make install
```
After a while (+5-10min) you should get the DLLs in the 'prefix' dir, in `bin/*.dll`. You need the `include/*.h` and `lib/.def` too.

If you get errors, try deleting the opus/ffmpeg dirs and *carefully* redo the steps. For example, it may throw errors about *libopus*' MSVCRT if it's not compiled with `-DOPUS_STATIC_RUNTIME=ON`.

 (in `ffmpeg_options.txt` remove `--enable-libopus` and change `libopus` to `opus` in `--enable-decoder`).


#### With MSVC + MSYS2 (64-bit)
The steps are the same, but using the **x64 Native Tools Command Prompt for VS 20xx** console and x64 variables:
```bat
C:\msys64\msys2_shell.cmd -mingw64 -use-full-path
```

Repeat the compile steps but change *pkconfig* and `sh configure` to write 64-bit files:
```sh
# read current options, removing comments and line breaks (copy from vgmstream's source)
FFMPEG_OPTIONS=`sed -e '/^#/d' ../ffmpeg_options.txt`

# make MSVC Opus known to pkconfig
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/c/vgmstream-dlls/out/opus-vs-64/lib/pkgconfig"

# 64-bit DLL: prepare options + compile + copy
sh ./configure $FFMPEG_OPTIONS --target-os=win64 --toolchain=msvc --arch=x86_64 --prefix=/c/vgmstream-dlls/out/ffmpeg-vs-64
make clean
make
make install
```

#### Extra info
- http://ffmpeg.org/platform.html
- https://trac.ffmpeg.org/wiki/CompilationGuide/MSVC
- https://github.com/m-ab-s/media-autobuild_suite (reportedly automates the above steps)
