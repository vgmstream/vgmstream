# vgmstream lib build help
This document explains how to build various external dependencies used in vgmstream.  Like *vgmstream*, most external libs use C and need to be compiled as such.

The main purpose this doc is to have a reference of what each lib is doing, and to rebuild Windows DLLs. Linux libs are handled automatically using CMake, though you can use these steps too.

See [BUILD](BUILD.md#external-libraries) for a description of each lib first.

## Intro
Guide is mainly geared towards **Windows** DLLs, as a reference for later updates. For **Linux/Mac**, libs are already included when using *CMake*, but you can mostly follow this with minor tweaks (like using default install folders) to create linkable `.so` libs, should you need to.

Unless mentioned, their latest version should be ok to use, though included DLLs may be a bit older. Each lib is compiled using a *recommended version*, but most should work with recent versions (FFMpeg may rarely change the *API* though). Most libs don't provide official pre-compiled *binaries*, or only for certain versions, so we need to compile them ourselves.

### Requirements
Guide assumes you followed the steps above to install Git, GCC (MingW or MSYS2) or Visual Studio, but you'll need GCC most of the time. This guide uses the command line to describe repeatable steps, though for MSVC DLLs you may open `.sln` and compile manually. Both MSVC's and GCC's DLLs should work fine and with comparable performance.

MSVC commands use `msbuild.exe`, which can be called by opening VS's *x86 Native Tools for VS 20xx* console, found on Windows start menu. It can also be added to regular `PATH` for CMD/MSYS2 as well, but since location changes around you may need to download *vswhere* (locator) to find it:
```bat
curl --location https://github.com/Microsoft/vswhere/releases/download/2.6.7/vswhere.exe --output vswhere.exe
REM #path to MSBuild
vswhere.exe -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
REM set PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin
```

On Windows 10, `curl` is included by default but you may need to get it first (or just manually download and unzip files).

### Tools and PATHs
When using Mingw+Git you may add their location to Windows' `PATH` variable, so programs like `gcc.exe` work as-is in Windows CMD without writting the full path. This can be done temporarily by writting in the command line: `set PATH=C:\(path-to-mingw)\mingw32\bin;C:\Git\usr\bin;%PATH%` (for example), or in Windows' system variables panel.

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

When building a DLL/lib compiler sets *exported symbols* (functions). Xiph's *autoconf* may generate DLLs correctly, but don't detect Mingw/Win config properly and export all symbols by default. This is fixed manually, but there may be better ways to handle it (to be researched).

### Shared libs details
Roughly, a `.dll` is a Windows "shared library"; Linux equivalent would be a `.so` file. First, `.c` files are compiled into objects (`.o` in GCC, `.obj` in MSCV), then can be made into a `.dll`. Later, when a program needs that DLL (or rather, it's functions), a compiler can use it as long as some conditions are met.

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
curl --location https://sourceforge.net/projects/mpg123/files/mpg123/1.31.1/mpg123-1.31.1.tar.bz2 --output mpg123-1.31.1.tar.bz2
tar -xf mpg123-1.31.1.tar.bz2
cd mpg123-1.31.1
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
Untested/not possible.


### libg719_decode
Use MSVC and `g719.sln`, or GCC and the Makefile included.

#### Source
```bat
git clone https://github.com/kode54/libg719_decode
git -C libg719_decode checkout da90ad8a676876c6c47889bcea6a753f9bbf7a73
cd libg719_decode
```

#### With GCC
```bat
REM # 32-bit DLL
mkdir C:\vgmstream-dlls\out\g719-32\
make clean shared EXTRA_CFLAGS="-m32" EXTRA_LDFLAGS="-m32 -static-libgcc" OUTPUT_DIR=C:\vgmstream-dlls\out\g719-32\

REM # 64-bit DLL
mkdir C:\vgmstream-dlls\out\g719-64\
make clean shared EXTRA_CFLAGS="-m64" EXTRA_LDFLAGS="-m64 -static-libgcc" OUTPUT_DIR=C:\vgmstream-dlls\out\g719-64\
```

#### With MSVC
```bat
REM # 32-bit DLL
MSBuild.exe g719.sln /p:Platform=Win32 /p:Configuration=Release /p:WindowsTargetPlatformVersion=10.0 /p:PlatformToolset=v142
mkdir C:\vgmstream-dlls\out\g719-32
copy /B .\Release\libg719_decode.dll C:\vgmstream-dlls\out\g719-32\libg719_decode.dll

REM # 64-bit DLL
MSBuild.exe g719.sln /p:Platform=x64 /p:Configuration=Release /p:WindowsTargetPlatformVersion=10.0 /p:PlatformToolset=v142
mkdir C:\vgmstream-dlls\out\g719-64
copy /B .\x64\Release\libg719_decode.dll C:\vgmstream-dlls\out\g719-64\libg719_decode.dll

REM add /t:Clean to the above to clean up compilation
```


### LibAtrac9
Use MSCV and `libatrac9.sln`, or GCC and the Makefile included.

#### Source
```bat
git clone https://github.com/Thealexbarney/LibAtrac9
git -C LibAtrac9 checkout 6a9e00f6c7abd74d037fd210b6670d3cdb313049
cd LibAtrac9/C
```

#### With GCC

**NOTE**: on Windows `mkdir` clashes and needs full path
```bat
REM # 32-bit DLL
make clean shared SFLAGS="-O2 -m32" MKDIR="/Git/usr/bin/mkdir -p" BINDIR=C:\vgmstream-dlls\out\atrac9-32 SHARED_NAME=C:\vgmstream-dlls\out\atrac9-32\libatrac9.dll

REM make clean shared SFLAGS="-O2 -m32" MKDIR="/Git/usr/bin/mkdir -p" BINDIR=C:\vgmstream-dlls\out\atrac9-32 SHARED_FILENAME=libatrac9.dll

REM # 64-bit DLL
make clean shared SFLAGS="-O2 -m64" MKDIR="/Git/usr/bin/mkdir -p" BINDIR=C:\vgmstream-dlls\out\atrac9-64 SHARED_NAME=C:\vgmstream-dlls\out\atrac9-64\libatrac9.dll
```

#### With MSVC
```bat
REM # 32-bit DLL
MSBuild.exe libatrac9.sln /p:Platform=x86 /p:Configuration=Release /p:WindowsTargetPlatformVersion=10.0 /p:PlatformToolset=v142
mkdir C:\vgmstream-dlls\out\atrac9-32
copy /B .\Release\libatrac9.dll C:\vgmstream-dlls\out\atrac9-32\libatrac9.dll

REM # 64-bit DLL
MSBuild.exe libatrac9.sln /p:Platform=x64 /p:Configuration=Release /p:WindowsTargetPlatformVersion=10.0 /p:PlatformToolset=v142
mkdir C:\vgmstream-dlls\out\atrac9-64
copy /B .\x64\Release\libatrac9.dll C:\vgmstream-dlls\out\atrac9-64\libatrac9.dll

REM add /t:Clean to the above to clean up compilation
```
**NOTE**

Some `libatrac9.vcxproj` x64 config may be outdated. In MSBuild +15 (VS +2017) you can force changes by creating a file named `Directory.Build.props` nearby. Also possible to pass this with /p:ForceImportBeforeCppTargets=(file.prop), but only works with full paths. There is no command line support to change CL (MSVC's compile) options other than this.
```
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemDefinitionGroup>
    <ClCompile>
      <ExceptionHandling>Sync</ExceptionHandling>
      <RuntimeLibrary>MultiThreaded</RuntimeLibrary>
      <FloatingPointModel>Fast</FloatingPointModel>
    </ClCompile>
  </ItemDefinitionGroup>
</Project>
```


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
This is used below by FFmpeg (but can be disabled), as a static lib (`.a`/`.lib`) rather than DLL.

If you wonder why use it through FFmpeg instead of directly, all work was already done for FFmpeg's opus so it was faster and easier this way.

#### Source
```bat
curl --location https://archive.mozilla.org/pub/opus/opus-1.3.1.tar.gz --output opus-1.3.1.tar.gz
tar -xf opus-1.3.1.tar.gz
cd opus-1.3.1
```

#### With GCC
**Notes**
- `CFLAGS` on *configure* overwrite defaults
- remove `--prefix` for pkg-config to work properly?

```bat
REM # 32-bit lib
sh ./configure --host=mingw32 --enable-static --disable-shared --disable-doc --disable-extra-programs --prefix=/c/vgmstream-dlls/out/opus-32
make clean install-strip LDFLAGS="-m32 -static-libgcc" AM_CFLAGS="-m32"

REM # 64-bit lib
sh ./configure --host=mingw32 --enable-static --disable-shared --disable-doc --disable-extra-programs --prefix=/c/vgmstream-dlls/out/opus-64
make clean install-strip LDFLAGS="-m64 -static-libgcc" AM_CFLAGS="-m64"
```

#### With MSVC
**Notes**:
- if you have called *configure* first, delete `config.h` on root or you may get odd errors

```bat
del config.h
cd win32/VS2015

REM # 32-bit lib
MSBuild.exe opus.sln /p:Platform=Win32 /p:Configuration=Release /p:WindowsTargetPlatformVersion=10.0 /p:PlatformToolset=v142

REM # 64-bit lib
MSBuild.exe opus.sln /p:Platform=x64 /p:Configuration=Release /p:WindowsTargetPlatformVersion=10.0 /p:PlatformToolset=v142
```

### FFmpeg
vgmstream's FFmpeg builds for **Windows** and static builds for **Linux** remove many unnecessary parts of FFmpeg to trim down its gigantic size, and, on Windows, are also built with the "-vgmstream" suffix to prevent clashing with other plugins. Current options can be seen in `ffmpeg_options.txt`. Shared **Linux** builds usually link to system FFmpeg without issues, while standard FFmpeg DLLs may work (renamed to -vgmstream).

FFmpeg can be compiled with *libopus* (external lib) rather than internal *opus*. This is used because FFmpeg decodes incorrectly Opus files used some in games (mostly low bitrate). In older versions this was audibly wrong, but currently the difference shouldn't be that much, but still not that accurate compared with *libopus* (PCM sample diffs of +5000), so *vgmstream* enables it. Getting *libopus* recognized can be unwieldly, so internal *opus* is a decent enough substitute (remove `--enable-libopus` and change `libopus` to `opus` in `--enable-decoder` from options, and remove `--enable-custom-modes` from *configure*).

GCC and MSVC need `yasm.exe` somewhere in `PATH` to properly compile/optimize: https://yasm.tortall.net (add `--disable-yasm` to *configure* options to disable, may decrease performance).

FFmpeg uses separates DLLs, that depend on each other like this:
- avutil: none (uses bcrypt only in Win7+, could be be patched out)
- swresample: avutil
- avformat: avcodec, avutil
- avcodec: avutil, swresample

Note that *vgmstream* applies various patches in real time to fix several FFmpeg quirks (including infinite loops). Could be done with *git* patches, but not currently since users on Linux may link to system's libs and/or use different versions. Updating FFmpeg version without testing carefully is not recommended.

#### Source
```bat
# clone only current tag's "depth" as otherwise FFmpeg history is pretty big
git clone https://git.ffmpeg.org/ffmpeg.git --depth 1 --branch n5.1.2
cd ffmpeg
```

#### libopus and pkg-config
FFmpeg uses *pkg-config* (a kind of "installed lib manager") to detect pre-compiled *libopus*. On Linux it should detect *libopus* after `make install` with default `--prefix`, or adding opus's `--prefix` path to `PKG_CONFIG_PATH`. On Windows, MSYS2 *probably* works the same.

However when compiling with MSVC it's not clear how to mix Windows-style `.lib` and *pkg-config*, though should be possible (*media-autobuild_suite* project does it?). For now as a temp hack, we can force FFmpeg to skip pkg-config and manually pass lib's location. But if you can get *pkg-config* to work ignore "pkg-config hack" steps (**TODO**: to be researched).

#### With GCC
**Notes**:
- *do not* call `make install` directly without `make` first, doesn't properly execute needed dependencies (won't have version numbers)

```sh
# read current options (removing comments and line breaks); change file path if needed (or manually copy options below)
FFMPEG_OPTIONS=`sed -e '/^#/d' ../vgmstream/ext_libs/ffmpeg_options.txt`
echo $FFMPEG_OPTIONS

# PKG-CONFIG HACK: disables pkg-config in FFmpeg's configure (use only if *configure* throws a pkg-config error)
sed -i -e "s/require_pkg_config libopus/: #require_pkg_config libopus/g" configure

# PKG-CONFIG HACK (untested): pass the following to configure
# --extra-cflags="-I(path to opus's /include)" --extra-ldflags="-L(path to opus's /lib) -lopus -static-libgc"

# 32-bit DLL
sh ./configure $FFMPEG_OPTIONS --target-os=mingw32 --arch=x86 --enable-custom-modes --extra-ldflags="-static-libgcc" --prefix=/c/vgmstream-dlls/out/ffmpeg-32 
make clean
make
make install

# 64-bit DLL
sh ./configure $FFMPEG_OPTIONS --target-os=mingw32 --arch=x86_64 --enable-custom-modes --extra-ldflags="-static-libgcc" --prefix=/c/vgmstream-dlls/out/ffmpeg-64
make clean
make
make install
```

#### With MSVC
Supported but also needs *autotools* with MSYS2, and several hoops:
- have MSVC's `cl.exe`/`link.exe` compiler/linker in `PATH` (`cl.exe` returns info)
  - it's kinda hard to find out so best would be opening VS's *x86 Native Tools for VS 20xx* console, found on Windows start menu
- (libopus only) temp add opus's include and lib paths into MSVC's `INCLUDE` and `LIB` console variables
  - `set INCLUDE=%LIB%;C:\(path to include)`
  - `set LIB=%LIB%;C:\(path to lib)`
- open MSYS2's ming32 console keeping PATH:
  - `C:\msys64\msys2_shell.cmd -mingw32 -use-full-path` (32-bit)
  - `C:\msys64\msys2_shell.cmd -mingw64 -use-full-path` (64-bit)
- a new console should open, you can close *x86 Native Tools Command Prompt* now
- check `which cl` is found (shows VS's path)
- check `which link` is MSVC's and not `/usr/bin/link.exe`
  - temp rename wrong `link.exe` if needed: `mv /usr/bin/link.exe /usr/bin/link.exe.bak`
  - beware as *mingw32* and *ming64* consoles may have different settings/links
- make sure git/gcc/make/autotools/mingw/pkg-config(pkgconf?)/mingw-w64 are installed in MSYS2
- make sure yasm is installed (`pacman -S yasm`)
- (libopus only but kinda optional) install pkg-config
- get *libopus*, compile it with VS (to get a `.lib`) as described before
- get FFmpeg's source and enter it
- call *configure/make/make install* with options described above, changing:
  - `--target-os=mingw32` to `--target-os=win32 --toolchain=msvc`
- if you get "*compiler cannot create executables*" errors make sure that:
  - you have open the *mingw32* (32-bit) or *mingw64* (64-bit) console
  - `which link` is properly set
  - *libopus*'s `opus.lib` path ib `LIB` is correctly set
- if you get pkg-config errors, try hack to disable it (see below)
- if you still get errors, try disabling libopus (remove `--enable-libopus` and change `libopus` to `opus` in `--enable-decoder`)
- if you still get errors, try deleting opusffmpeg dirs and *carefully* redo the steps
  - missing a single step or changing stuff will likely cause issues!
After a while (+5-10min) you should get DLLs. When compiling 64-bit DLLs, open *x64 Native Tools for VS 20xx* console instead, compile *libopus* 64-bit and set `--target-os=win64`.

Reportedly this helper project works (automates all of the above):
- https://github.com/m-ab-s/media-autobuild_suite
`ffmpeg_options.txt` and `media-autobuild_suite.ini` (outdated?) for it can be found in *vgmstream* source.

In theory adding Git+Mingw+yasm in `PATH` inside the VS console would work (temp include: `set PATH=%PATH%;C:\Git\usr\bin;C:\(mingw-path)\mingw32\bin;C:\yasm`), but seems to have issues with existing `awk`.

Extra info:
- http://ffmpeg.org/platform.html
- https://trac.ffmpeg.org/wiki/CompilationGuide/MSVC
- https://github.com/OpenChemistry/tomviz-superbuild/blob/master/projects/win32/BuildFFMPEG.md

##### 32-bit scripts
**x86 Native Tools Command Prompt**
```bat
REM compile libopus first: download, enter win32/VS2015, call MSBuild.exe, etc (detailed above)

set INCLUDE=%INCLUDE%;C:\vgmstream-dlls\sources\opus-1.3.1\include
set LIB=%LIB%;C:\vgmstream-dlls\sources\opus-1.3.1\win32\VS2015\Win32\Release

C:\msys64\msys2_shell.cmd -mingw32 -use-full-path
```

**MSYS2's mingw32 console**
```sh
# download FFmpeg first, etc
cd /c/vgmstream-dlls/sources/ffmpeg

# read current options (removing comments and line breaks); change file path if needed (or manually copy options below)
FFMPEG_OPTIONS=`sed -e '/^#/d' ../vgmstream/ext_libs/ffmpeg_options.txt`
echo $FFMPEG_OPTIONS

# PKG-CONFIG HACK: disables pkg-config in FFmpeg's configure (use only if *configure* throws a pkg-config error)
sed -i -e "s/require_pkg_config libopus/: #require_pkg_config libopus/g" configure

# opus should be on INCLUDE/LIB path now, otherwise try: 
# --extra-cflags="-I."-I(full windows-style path)
# --extra-ldflags="(full windows-style path)\opus.lib"
sh ./configure $FFMPEG_OPTIONS --target-os=win32 --toolchain=msvc --arch=x86 --extra-ldflags="opus.lib" --prefix=/c/vgmstream-dlls/out/ffmpeg-32
make clean
make
make install
``` 

##### 64-bit scripts
**x64 Native Tools Command Prompt**
```bat
REM compile libopus first: download, enter win32/VS2015, call MSBuild.exe, etc (detailed above)

set INCLUDE=%INCLUDE%;C:\vgmstream-dlls\sources\opus-1.3.1\include
set LIB=%LIB%;C:\vgmstream-dlls\sources\opus-1.3.1\win32\VS2015\x64\Release

C:\msys64\msys2_shell.cmd -mingw64 -use-full-path
```

**MSYS2's mingw32 console**
```sh
# download FFmpeg first, etc
cd /c/vgmstream-dlls/sources/ffmpeg

# read current options (removing comments and line breaks); change file path if needed (or manually copy options below)
FFMPEG_OPTIONS=`sed -e '/^#/d' ../vgmstream/ext_libs/ffmpeg_options.txt`
echo $FFMPEG_OPTIONS

# PKG-CONFIG HACK: disables pkg-config in FFmpeg's configure (use only if *configure* throws a pkg-config error)
sed -i -e "s/require_pkg_config libopus/: #require_pkg_config libopus/g" configure

# opus should be on INCLUDE/LIB path now, otherwise try: 
# --extra-cflags="-I."-I(full windows-style path)
# --extra-ldflags="(full windows-style path)\opus.lib"
sh ./configure $FFMPEG_OPTIONS --target-os=win64 --toolchain=msvc --arch=x86_64 --extra-ldflags="opus.lib" --prefix=/c/vgmstream-dlls/out/ffmpeg-64
make clean
make
make install
``` 
