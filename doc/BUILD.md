# vgmstream build help
This document explains how to build each of vgmstream's components and libraries.


## Compilation requirements
vgmstream can be compiled using one of several build scripts that are available in this repository. Components are detailed below, but if you are new to development you probably want one of these:
- **Windows**: [simple scripts](#simple-scripts-builds) + [Visual Studio](#microsofts-visual-c-msvc--visual-studio--msbuild-compiler)
- **Linux**: [CMake](#cmake-builds) + [GCC](#gcc--make-compiler)
- **macOS**: [CMake](#cmake-builds) + [Clang](#clang-compiler)
- **Web**: [CMake](#cmake-builds) + [Emscripten](#emscripten-compiler)

Because each module has different quirks one can't use a single tool for everything. You should be able to build most using a standard *compiler* (GCC/MSVC/Clang) using common *build systems* (scripts/CMake/autotools) in any typical *OS* (Windows/Linux/macOS).

64-bit support should work but hasn't been throughly tested (may have subtle decoding bugs in some codecs), since most used components are plugins for 32-bit players. Windows libraries for extra codecs are included for 32-bit only at the moment.

Though it's rather flexible (like using Windows with GCC and autotools), some combos may be a bit more complex to get working depending on your system and other factors.


## Quick guide

### Linux
- Use these commands to install dependencies and compile with CMake + make
```sh
sudo apt-get update
# base deps
sudo apt-get install -y gcc g++ make cmake build-essential git
# optional: for extra formats (can be ommited to build with static libs)
sudo apt-get install -y libmpg123-dev libvorbis-dev libspeex-dev
sudo apt-get install -y libavformat-dev libavcodec-dev libavutil-dev libswresample-dev
sudo apt-get install -y yasm libopus-dev
# optional: for vgmstream 123 and audacious
sudo apt-get install -y libao-dev audacious-dev

git clone https://github.com/vgmstream/vgmstream
cd vgmstream

mkdir -p build
cd build
cmake ..
make
```
- Output files are located in `./build` and subdirs
- May use `./make-build-cmake.sh` instead (same thing)
  - autotools and simple makefiles also work (less libs)

### macOS
- *Should* work with CMake + make, like the above example
  - Replace `apt-get intall` with manual installation of those deps
  - Or with *Homebrew*: `brew install cmake mpg123 libvorbis ffmpeg libao`
- May try https://formulae.brew.sh/formula/vgmstream instead (not part of this project)

### Windows
- Install Visual Studio: https://www.visualstudio.com/downloads/ (for C/C++, with "MFC support" and "ATL support)
- Make a file called `msvc-build.config.ps1` in vgmstream's root, with your installed toolset and SDK:
```ps1
# - toolsets: "" (default), "v140" (VS 2015), "v141" (VS 2017), "v141_xp" (XP support), "v142" (VS 2019), etc
# - sdks: "" (default), "7.0" (Win7 SDK), "8.1" (Win8 SDK), "10.0" (Win10 SDK), etc
$toolset = "142"
$sdk = "10.0"
```
- Execute file `msvc-build-package.bat` to compile
- Output files are located in `./bin`


## Full guide
This guide is mainly geared towards beginner devs, introducing concepts in steps. Many things may be obvious to experienced devs, so feel free to skim or skip sections.

### GCC / Make (compiler)
Common C compiler, most development is done with this.

On **Windows** you need one of these somewhere in PATH:
- MinGW-w64 (32bit version): https://sourceforge.net/projects/mingw-w64/
  - Use this for easier standalone executables
  - [Latest online MinGW installer](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/installer/mingw-w64-install.exe/download) with any config should work (for example: gcc-8.1.0, i686, win32, sjlj).
  - Or download and unzip the [portable MinGW package](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/8.1.0/threads-win32/sjlj/i686-8.1.0-release-win32-sjlj-rt_v6-rev0.7z/download)
- MSYS2 with the MinGW-w64_shell (32bit) package: https://msys2.github.io/
  - Resulting binaries may depend on `msys*.dll`.

On **Linux** it should be included by default in the distribution, or can be easily installed using the distro's package manager (for example `sudo apt-get install gcc g++ make`).

On **macOS** may be installed with a package manager like *Homebrew*, but using *Clang* is probably easier.

Any versions that are not too ancient should work, since vgmstream uses standard C. GCC usually comes with *Make*, a program that can be used to build vgmstream.

### Microsoft's Visual C++ (MSVC) / Visual Studio / MSBuild (compiler)
Alt C compiler (**Windows** only), auto-generated builds for Windows use this. Bundled in:
- Visual Studio (2015/2017/2019/latest): https://www.visualstudio.com/downloads/

Visual Studio Community (free) should work, but you may need to register after a trial period. Even after trial you can still use *MSBuild*, command-line tool that actually does all the building, calling the *MSVC* compiler (Visual Studio itself is just an IDE for development and not actually needed).

Instead of the full (usually huge) Visual Studio, you can also get "Build Tools for Visual Studio", variation that only installs *MSBuild* and necessary files without the IDE. Usually found in the above link, under "Tools for Visual Studio" (or google as MS's links tend to move around).

When installing check the "Desktop development with C++" group, and optionally select "MFC support" and "ATL support" sub-options to build foobar2000 plugin (you can modify that or re-install IDE later, by running installed "Visual Studio Installer"). You could include MSVC v141 (2017) compatibility too just in case, since it's mainly tested with that.

Older versions of MSVC (2010 and earlier) have limited C support and may not work with latest commits, while reportedly beta/new versions aren't always very stable. Also, only projects (`.vcxproj`) for VS2015+ are included (CMake may be able to generate older `.vcproj` if you really need them). Some very odd issues affecting MSVC only have been found and fixed before. Keep in mind all of this if you run into problems.

### Clang (compiler)
Alt C compiler, reportedly works fine on **macOS** and may used as a replacement of GCC without issues.
- https://releases.llvm.org/download.html

Should be usable on **Linux** and possibly **Windows** with CMake. For default Makefiles may need to set compiler vars appropriately (`CC=clang`, `AR=llvm-ar` and so on).

### Emscripten (compiler)
C compiler that generates *WebAssembly* (custom Javascript), to build vgmstream's components with in-browser support.

First, follow the *Emscripten* installation instructions:
- https://emscripten.org/docs/getting_started/downloads.html
- https://emscripten.org/docs/compiling/Building-Projects.html#building-projects

Though basically:
```sh
git clone https://github.com/emscripten-core/emsdk
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

Then you should be able to build it on **Linux** (**Windows** should be possible too, but has some issues at the moment), for example with CMake:
```sh
git clone https://github.com/vgmstream/vgmstream
cd vgmstream

mkdir -p embuild
cd embuild
emcmake cmake ..
make
```
You can compile faster using `make -j 5` instead of the last `make` command (replace `5` with the number of cores your CPU has plus one), but please note that, with multiple jobs, in case any issues occur the output will become useless.

The output files `vgmstream-cli.wasm` and `vgmstream-cli.js` will be located in the `embuild/cli` directory.

Or with the base makefiles (the output may need to be renamed to .js):
```sh
git clone https://github.com/vgmstream/vgmstream
cd vgmstream
make vgmstream-cli CC=emcc AR=emar strip=echo
```

Load `vgmstream-cli.js` in a web page, you will be able to call the `callMain()` function from the browser developer console. Parameters to vgmstream can be passed in an array: `callMain(["-i", "input_file.pcm"])`. Files can be accessed through Emscripten [File System API](https://emscripten.org/docs/api_reference/Filesystem-API.html) (`FS`).

For a fully-featured player see:
- https://github.com/KatieFrogs/vgmstream-web

### Simple scripts (builds)
Default build scripts are included in the source that can compile vgmstream, though limited in some ways.

**For MSVC**: there is a default Visual Studio `.sln` file that should be up to date (run `./msvc-build-init.bat` first, or see the foobar section to get extra dependencies manually, then open). A PowerShell script also automates compilation (on **Windows 7** may need recent .NET framework and PowerShell versions), simply run `./msvc-build.bat`.

First, you may need to either open the `.sln` and change project compiler (*PlatformToolset*) and SDK (*WindowsTargetPlatformVersion*) to your installed version, or edit `msvc-build.ps1` and set the variables near *CONFIG*. To avoid modifying files, you can also create a file named `msvc-build.config.ps1` with:
```ps1
# - toolsets: "" (default), "v140" (VS 2015), "v141" (VS 2017), "v141_xp" (XP support), "v142" (VS 2019), etc
# - sdks: "" (default), "7.0" (Win7 SDK), "8.1" (Win8 SDK), "10.0" (Win10 SDK), etc
$toolset = "142"
$sdk = "10.0"
```
It's also possible to call MSBuild and pass those values from the CMD, see foobar section for an example.

Once finished resulting binaries are in the *./Release* folder. Remember you need to copy extra `.dll` to run them (see [USAGE.md](USAGE.md)).

**For GCC/CLang**: there are basic Makefiles that work like usual with *make* (like `make vgmstream_cli EXTRA_CFLAGS="-DVGM_DEBUG_OUTPUT`). Artifacts are usually in their subdir (*./cli*, *./winamp*, etc).

On **Windows** this compiles with extra libs enabled by default.

On **Linux** there is no fancy autodetection (try CMake or autotools for that), so you need to make sure libs are in your system and pass flags to enable them manually (install/compile libs then `make vgmstream_cli VGM_VORBIS=1 ...`). Check or run `make-build.sh` for a basic example that builds CLI and vgmstream123 with most libs enabled for a Ubuntu-style distro (if you get errors on your system just tweak or comment out offending parts).

**Linux** example:
```sh
sudo apt-get install -y git

git clone https://github.com/vgmstream/vgmstream
cd vgmstream
# in case they weren't set
chmod +x version-get.sh version-make.sh make-build.sh

# warning: installs stuff, check all "apt install"
./make-build.sh
```

### CMake (builds)
A tool used to generate common build files (for *make*, *VS/MSBuild*, etc), that in turn can be used to compile vgmstream's modules instead of using the existing scripts and files. Needs v3.6 or later:
- https://cmake.org/download/

On **Windows** you can use *cmake-gui*, that should be mostly self-explanatory. You need to set the *source dir*, *build dir*, *config options*, then hit *Configure* to set save options and build type (for example *Visual Studio* project files), then *Generate* to actually create files. If you want to change options, hit *Configure* and *Generate* again.

On **Linux**, the CMake script can automatically download and build the source code for dependencies that it requires. It is also capable of creating a statically linked binary for distribution purposes. See `./make-build-cmake.sh` (basically install desired deps then `mkdir -p build && cd build`, `cmake ..`, `make`).

You can compile faster using `make -j 5` instead of the last `make` command (replace `5` with the number of cores your CPU has plus one), but please note that, with multiple jobs, in case any issues occur the output will become useless.

The output files are `build/cli/vgmstream-cli` (CLI decoder), `build/cli/vgmstream123` (CLI player), and `build/audacious/vgmstream.so` (Audacious plugin).

For more information and options see the full guide in the [CMAKE.md](CMAKE.md) file.

Note that doing in-source builds of CMake (`cmake .` in vgmstream's root dir) is not recommended, as that may clobber default build files (try `cmake -S . -B build` or building some `./build` subfolder).

### autotools (builds)
Autogenerated *make* scripts, used by some modules (mainly Audacious for **Linux**, and external libs).

For **Windows** you must include GCC, and Linux's sh tool in some form in PATH. Simplest would be installing *MinGW-w64* for `gcc.exe` (and related tools), and *Git* for `sh.exe`, and making PATH point their bin dir. 
- ex. `C:\mingw\i686-8.1.0-release-win32-sjlj-rt_v6-rev0\mingw32\bin` and `C:\Git\usr\bin`
- Both must be installed/copied in a dir without spaces (with spaces autoconf seemingly works but creates buggy files)
- If you don't have Git, try compiled GNU tools for Windows (http://gnuwin32.sourceforge.net/packages.html)

A trick on **Windows** is that you can temporary alter PATH variable in `.bat` scripts (PATH is used to call programs in Windows without having to write full path to .exe)
```bat
set PATH=%PATH%;C:\mingw\i686-8.1.0-release-win32-sjlj-rt_v6-rev0\mingw32\bin
set PATH=%PATH%;C:\Git\usr\bin
gcc.exe (...)
```

For **Linux**, GCC/make/autotools should be included already, or install with a package manager, also depends on *Make*.
```sh
sudo apt-get install gcc g++ make autoconf automake libtool
```

Typical usage involves creating Makefiles with `bootstrap` and `configure`, `make` (with the correct makefile) to compile, and `make install` to copy results. This varies slightly depending on module/lib (explained later).
```sh
./bootstrap
./configure
make -f Makefile.autotools
sudo make -f Makefile.autotools install
```

External libs using autotools can be compiled on **Windows** too:
```bat
sh.exe ./bootstrap
sh.exe ./configure
mingw32-make.exe -f Makefile.autotools
mingw32-make.exe -f Makefile.autotools install
```
Also for older libs, call `sh.exe ./configure` with either  `--build=mingw32`, `--host=mingw32` or `--target-os=mingw32` (varies) for older configure. You may also need to use this command so that `.dll` files are correctly generated:
```bat
mingw32-make.exe -f Makefile.autotools LDFLAGS="-no-undefined -static-libgcc" MAKE=mingw32-make.exe
```

### Git (extras)
Code version control for development. Optional, used to auto-generate version numbers:
- https://git-scm.com/download

Remember Git can only be used if you clone the vgmstream repo (not with source downloaded in `.zip`).

On **Windows**, Git also comes with typical Linux utils (in the usr\bin dir), that can help when compiling some extra components.

### Extra libs (extras)
Optional codecs. See *External libraries* for full info.

On **Windows** most libs are pre-compiled and included to simplify building (since they can be quite involved to compile).

On **Linux** you usually need dev packages of each (for example `libao-dev` for vgmstream123, `libvorbis-dev` for Vorbis, and so on) and they should be picked by CMake/autotool scripts.

With no extra libs (or only some) enabled vgmstream works fine, but some advanced formats/codecs won't play. See *External libraries* for info about those extra codecs.


## Compiling modules

### CLI (test.exe/vgmstream-cli) / Winamp plugin (in_vgmstream) / XMPlay plugin (xmp-vgmstream)

**With GCC/Clang**: there are various ways to build it, each with some differences; you probably want CMake described below.

Simplest way is using the *./Makefile* in the root folder, see inside for options. For compilation flags check the *Makefile* in each folder. You may need to manually rebuild if you change a *.h* file (`make clean`). On **Windows** this will build with external libs enabled, but **Linux** can't at the moment.

Also, on **Linux** you can't build *in_vgmstream* and *xmp-vgmstream* (given they are Windows DLLs...). Makefiles have been used in the past to cross-compile from Linux with MingW headers though, but can't generate native Win code at the moment (should be fixable with some effort).

*Autotools* should build and install it as `vgmstream-cli`, this is explained in detail in the Audacious section. It enables (some) extra codecs. Some Linux distributions like **Arch Linux** include pre-patched vgmstream with most libraries, you may want that instead (not part of this project):
- https://aur.archlinux.org/packages/vgmstream-git/

If you use **macOS or Linux**, there is a *Homebrew* script that may automate the process (uses CMake, also not part of this project):
- https://formulae.brew.sh/formula/vgmstream

You may try CMake instead as it may be simpler and handle libs better. See the build steps in the [Cmake section](#cmake-builds). Some older distros may not work though (CMake version needs to recognize FILTER command). You may also need to install resulting artifacts manually. Check the *CMAKE.md* doc for some extra info too.

**Windows** CMD .bat example (with some debugging on):
```bat
prompt $P$G$_$S
set PATH=C:\Program Files (x86)\Git\usr\bin;%PATH%
set PATH=C:\Program Files (x86)\mingw-w64\i686-5.4.0-win32-sjlj-rt_v5-rev0\mingw32\bin;%PATH%

cd vgmstream

mingw32-make.exe vgmstream_cli -f Makefile ^
 EXTRA_CFLAGS="-DVGM_DEBUG_OUTPUT -g -Wimplicit-function-declaration" ^
 SHELL=sh.exe CC=gcc.exe AR=ar.exe STRIP=strip.exe DLLTOOL=dlltool.exe WINDRES=windres.exe ^
 STRIP=echo ^
 1> ../vgmstream-stdout.txt 2> ../vgmstream-stderr.txt
```

**With MSVC**: To build in Visual Studio, run `./msvc-build-init.bat`, open `vgmstream_full.sln` and compile. To build from the command line, just run `./msvc-build.bat`.

The build script will automatically handle obtaining dependencies and making the project changes listed in the foobar2000 section (you may need to install some PowerShell .NET packages). You could also call MSBuild directly in the command line (see the foobar2000 section for dependencies and examples).

If you get build errors, remember you need to adjust compiler/SDK in the `.sln`. See *Simple scripts* above or CMD example in the foobar section.

CMake can also be used instead to create project files (no particular benefit).

#### Notes
While the official name for the CLI tool is `vgmstream-cli`, on **Windows**, `test.exe` is used instead for historical reasons. If you want to reuse it for your project, it's probably better to rename it to `vgmstream-cli.exe`.


### foobar2000 plugin (foo\_input\_vgmstream)
Requires MSVC (foobar/SDK only links to MSVC C++ DLLs). To build in Visual Studio, run `./msvc-build-init.bat`, open `vgmstream_full.sln` and compile. To build from the command line, just run `./msvc-build.bat`.

foobar has multiple dependencies. Build script downloads them automatically, but here they are:
- foobar2000 SDK (2018), in *(vgmstream)/dependencies/foobar/*: http://www.foobar2000.org/SDK
- WTL (if needed), in *(vgmstream)/dependencies/WTL/*: http://wtl.sourceforge.net/
- (optional/disabled) FDK-AAC, in *(vgmstream)/dependencies/fdk-aac/*: https://github.com/kode54/fdk-aac
- (optional/disabled) QAAC, in *(vgmstream)/dependencies/qaac/*: https://github.com/kode54/qaac
- may need to install ATL and MFC libraries if not included by default (can be added from the Visual Studio installer)

The following project modifications are required:
- For *foobar2000_ATL_helpers* add *../../../WTL/Include* to the compilers's *additional includes*

FDK-AAC/QAAC can be enabled adding *VGM_USE_MP4V2* and *VGM_USE_FDKAAC* in the compiler/linker options and the project dependencies, otherwise FFmpeg is used instead to support .mp4. FDK-AAC Support is limited so FFmpeg is recommended.

In theory any foobar SDK should work, but there may be issues when using versions past *2018-02-05*. For those you need to change *RuntimeLibrary* from *MultiThreadedDebug* and *MultiThreaded* to *MultiThreadedDebugDLL* and *MultiThreadedDLL* (to match newer SDK settings). Mirror in case official site is down: https://github.com/vgmstream/vgmstream-deps/raw/master/foobar2000/SDK-2018-02-05.zip

You can also manually use the command line to compile with MSBuild, if you don't want to touch the `.vcxproj` files, register VS after trial, get PowerShell dependencies for the build script, or only have VC++/MSBuild tools.

**Windows** CMD example for foobar2000 (manual build):
```bat
prompt $P$G$_$S

REM MSVC ~2015
REM set PATH=%PATH%;C:\Program Files (x86)\MSBuild\14.0\Bin;%PATH%
REM Latest(?) MSVC
set PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin

cd vgmstream

set CL=/I"C:\projects\WTL\Include"
set LINK="C:\projects\foobar\foobar2000\shared\shared.lib"

msbuild fb2k/foo_input_vgmstream.vcxproj ^
 /t:Clean

REM depending on your installed Visual Studio build tools you may need to change:
REM - PlatformToolset: v140=MSVC 2015, v141=MSVC 2017, v141_xp=same with XP support, v142=MSVC 2019, etc
REM - WindowsTargetPlatformVersion: 7.0=Win7 SDK, 8.1=Win8 SDK, 10.0=Win10 SDK, etc

msbuild fb2k/foo_input_vgmstream.vcxproj ^
 /t:Build ^
 /p:Platform=Win32 ^
 /p:PlatformToolset=v142 ^
 /p:WindowsTargetPlatformVersion=10.0 ^
 /p:Configuration=Release ^
 /p:DependenciesDir=../..
```

### Audacious plugin
Requires the dev version of Audacious (and dependencies), autotools (automake/autoconf) or CMake, and gcc/make (C++11). It must be compiled and installed into Audacious, where it should appear in the plugin list as "vgmstream".

The plugin needs Audacious 3.5 or higher. New Audacious releases can break plugin compatibility so it may not work with the latest version unless adapted first.

CMake should handle all correctly, while when using autotools, libvorbis/libmpg123/libspeex will be used if found, while FFmpeg and other external libraries aren't enabled at the moment, thus some formats won't work (build scripts need to be fixed).

**Windows** builds aren't supported at the moment (should be possible but there are complex dependency chains).

If you get errors during the build phase, we probably forgot some `#ifdef` needed for Audacious, please [notify us](https://github.com/vgmstream/vgmstream/issues) if that happens.

Take note of other plugins stealing extensions (see [USAGE.md](USAGE.md#common-and-unknown-extensions)). To change Audacious's default priority for vgmstream you can make with CFLAG `AUDACIOUS_VGMSTREAM_PRIORITY n` (where `N` is a number where 10=lowest)


You can try building with CMake. See the build steps in the [Cmake section](#cmake-builds). Some older distros may not work though (CMake version needs to recognize FILTER command), and may need to install resulting artifacts manually (check `./audacious` dir).

Instead of CMake you can use autotools. Terminal example, assuming a **Ubuntu-based Linux** distribution:
```sh
# build setup

# default requirements
sudo apt-get update
sudo apt-get install gcc g++ make git
sudo apt-get install autoconf automake libtool
# vgmstream dependencies
sudo apt-get install libmpg123-dev libvorbis-dev libspeex-dev
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev libswresample-dev
# Audacious player and dependencies
sudo apt-get install audacious
sudo apt-get install audacious-dev libglib2.0-dev libgtk2.0-dev libpango1.0-dev
# vgmstream123 dependencies (optional)
sudo apt-get install libao-dev

# check Audacious version >= 3.5
pkg-config --modversion audacious


# base vgmstream build
git clone https://github.com/vgmstream/vgmstream
cd vgmstream

# main vgmstream build (if you get errors here please report)
./bootstrap
./configure
make -f Makefile.autotools

# copy to audacious plugins (note that this will also install "libvgmstream",
# vgmstream-cli and vgmstream123, so they can be invoked from the terminal)
sudo make -f Makefile.autotools install

# update global libvgmstream.so.0 refs
sudo ldconfig

# start audacious in verbose mode to check if it was installed correctly
audacious -V

# if all goes well no "ERROR (something) referencing libvgmstream should show 
# in the terminal log, then go to menu services > plugins > input tab and check
# vgmstream is there (you can start audacious normally next time)
```
```sh
# uninstall if needed
sudo make -f Makefile.autotools uninstall

# optional post-cleanup
make -f Makefile.autotools clean
find . -name ".deps" -type d -exec rm -r "{}" \;
./unbootstrap
## WARNING, removes *all* untracked files not in .gitignore
git clean -fd
```
To update vgmstream it's probably easiest to remove the `vgmstream` folder and start again from *base vgmstream build* step, since updates often require a full rebuild anyway, or call `git clean -fd` or maybe `git reset --hard`.

### vgmstream123 player
Should be buildable with Autotools/CMake by following the same steps as listen in the Audacious section (requires *libao-dev*).

**Windows** builds are possible with `libao.dll` and `libao` includes (found elsewhere) through the `Makefile`, but some features are disabled.

*libao* is licensed under the GPL v2 or later.


## Shared lib
Currently there isn't an official way to make vgmstream a shared lib (`.so`/`.dll`), but it can be achieved with some effort.

For example with CMake (outputs in `build/src/libvgmstream.so`):
```sh
mkdir -p build
cd build
cmake ..
make libvgmstream_shared
```

Or with the basic makefiles:
```sh
# build all of the intermediates with relocatable code
# *note*: quick hack with performance penalty, needs better dependency rules
make vgmstream_cli EXTRA_CFLAGS=-fPIC

# build the actual shared library
make -C src libvgmstream.so
```

May also need to take `vgmstream.h`, `streamfile.h` and `plugins.h`, and trim them somewhat to use as includes for the `.so`.

For MSVC, you could add `__declspec(dllexport)` to exported functions in the "public" API of the above `.h`, and set `<ConfigurationType>DynamicLibrary</ConfigurationType>` in `libvgmstream.vcxproj`, plus add a `<Link>` under `<ClCompile>` to those libs (copy from `vgmstream_cli.vcxproj`).

For integration and "API" usage, easiest would be checking how `vgmstream_cli.c` works.

A cleaner API/.h and build methods is planned for the future (low priority though).


## External libraries
Support for some codecs is done with external libs, instead of copying their code in vgmstream. There are various reasons for this:
- each lib may have complex or conflicting ways to compile that aren't simple to replicate
- their sources can be quite big and undesirable to include in full
- libs usually only compile with either GCC or MSVC, while vgmstream supports both compilers, so linking to the generated binary (compatible) is much easier
- not all licenses used by libs may allow to copy their code
- simplifies maintenance and updating

They are compiled in their own sources, and the resulting binary is linked by vgmstream using a few of their symbols.

Currently repo contains pre-compiled external libraries for **Windows** (32-bit Windows DLLs), while other systems link to system libraries. Ideally vgmstream could use libs compiled as static code (thus eliminating the need of DLLs), but involves a bunch of changes.

Below is a quick explanation of each library and how to compile binaries from them (for **Windows**). Unless mentioned, their latest version should be ok to use, though included DLLs may be a bit older.

MSVC needs a .lib helper to link .dll files, but libs below usually only create .dll (and maybe .def). Instead, those .lib are automatically generated during build step in `ext_libs.vcxproj` from .dll+.def, using lib.exe tool.


### libvorbis
Adds support for Vorbis, inside Ogg as `.ogg` (plain or encrypted) or custom variations like `.wem`, `.fsb`, `.ogl`, etc.
- Source: http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.6.zip
- DLL: `libvorbis.dll`
- lib: `-lvorbis -lvorbisfile`
- licensed under the 3-clause BSD license.

Should be buildable with MSVC (in /win32 dir are .sln files) or autotools (use `autogen.sh`).


### mpg123
Adds support for MPEG (MP1/MP2/MP3), used in formats that may have custom MPEG like `.ahx`, `.msf`, `.xvag`, `.scd`, etc.
- Source: https://sourceforge.net/projects/mpg123/files/mpg123/1.25.10/
- Builds: http://www.mpg123.de/download/win32/1.25.10/
- DLL: `libmpg123-0.dll`
- lib: `-lmpg123`
- licensed under the LGPL v2.1

Must use autotools (sh configure, make, make install), though some scripts simplify the process: `makedll.sh`, `windows-builds.sh`.


### libg719_decode
Adds support for ITU-T G.719 (standardization of Polycom Siren 22), used in a few Namco `.bnsf` games.
- Source: https://github.com/kode54/libg719_decode
- DLL: `libg719_decode.dll`
- lib: ---
- unknown license (possibly invalid and Polycom's)

Use MSVC (use `g719.sln`). It can be built with GCC too, for example, using [the CMake script from this repository](../ext_libs/libg719_decode/CMakeLists.txt).


### FFmpeg
Adds support for multiple codecs: ATRAC3 (`.at3`), ATRAC3plus (`.at3`), XMA1/2 (`.xma`), WMA v1 (`.wma`), WMA v2 (`.wma`), WMAPro (`.xwma`), AAC (`.mp4`, `.aac`), Bink (`.bik`), AC3/SPDIF (`.ac3`), Opus (`.opus`), Musepack (`.mpc`), FLAC (`.flac`), etc.
- Source: https://github.com/FFmpeg/FFmpeg/
- DLLs: `avcodec-vgmstream-58.dll`, `avformat-vgmstream-58.dll`, `avutil-vgmstream-56.dll`, `swresample-vgmstream-3.dll`
- lib: `-lavcodec -lavformat -lavutil -lswresample`
- primarily licensed under the LGPL v2.1 or later, with portions licensed under the GPL v2

vgmstream's FFmpeg builds for **Windows** and static builds for **Linux** remove many unnecessary parts of FFmpeg to trim down its gigantic size, and, on Windows, are also built with the "vgmstream-" prefix to avoid clashing with other plugins. Current options can be seen in `ffmpeg_options.txt`. Shared **Linux** builds usually link to system FFmpeg without issues.

Note that the options above use *libopus*, but you can use FFmpeg's *Opus* by removing `--enable-libopus` and changing `--enable-decoder`'s `libopus` to `opus`. libopus is preferable since FFmpeg's Opus decoding is buggy in some files.

For GCC simply use [autotools](#autotools-builds), passing to `./configure` the above options.

For MSCV it can be done through a helper: https://github.com/jb-alvarado/media-autobuild_suite

Both may need yasm somewhere in PATH to properly compile: https://yasm.tortall.net


### LibAtrac9
Adds support for ATRAC9, used in `.at9` and other formats for the PS4 and Vita.
- Source: https://github.com/Thealexbarney/LibAtrac9
- DLL: `libatrac9.dll`
- lib: `-latrac9` / `-l:libatrac9.a`
- licensed under the MIT license

Use MSCV and `libatrac9.sln`, or GCC and the Makefile included.


### libcelt
Adds support for FSB CELT versions 0.6.1 and 0.11.0, used in a handful of older `.fsb`.
- Source (0.6.1): http://downloads.us.xiph.org/releases/celt/celt-0.6.1.tar.gz
- Source (0.11.0): http://downloads.xiph.org/releases/celt/celt-0.11.0.tar.gz
- DLL: `libcelt-0061.dll`, `libcelt-0110.dll`
- lib: `-lcelt-0061` `-lcelt-0110` / `-l:libcelt-0110.a` `-l:libcelt-0061.a`
- licensed under the MIT license

FSB uses two incompatible, older libcelt versions. Both libraries export the same symbols so normally can't coexist together. To get them working we need to make sure symbols are renamed first. This may be solved in various ways:
- using dynamic loading (LoadLibrary) but for portability it isn't an option
- It may be possible to link+rename using .def files
- Linux/Mingw's objcopy to (supposedly) rename DLL symbols
- Use GCC's preprocessor to rename functions on compile
- Rename functions in the source code directly.

To compile we'll use autotools with GCC preprocessor renaming:
- in the celt-0.6.1 dir:
  ```bat
  # creates Makefiles with Automake
  sh.exe ./configure --build=mingw32 --prefix=/c/celt0.6.1/bin/  --exec-prefix=/c/celt-0.6.1/bin/

  # LDFLAGS are needed to create the .dll (Automake whining)
  # CFLAGS rename a few CELT functions (we don't import the rest so they won't clash)
  mingw32-make.exe clean
  mingw32-make.exe LDFLAGS="-no-undefined" AM_CFLAGS="-Dcelt_decode=celt_0061_decode -Dcelt_decoder_create=celt_0061_decoder_create -Dcelt_decoder_destroy=celt_0061_decoder_destroy -Dcelt_mode_create=celt_0061_mode_create -Dcelt_mode_destroy=celt_0061_mode_destroy -Dcelt_mode_info=celt_0061_mode_info"
  ```
- in the celt-0.11.0 dir:
  ```bat
  # creates Makefiles with Automake
  sh.exe ./configure --build=mingw32 --prefix=/c/celt-0.11.0/bin/  --exec-prefix=/c/celt-0.11.0/bin/

  # LDFLAGS are needed to create the .dll (Automake whining)
  # CFLAGS rename a few CELT functions (notice one is different vs 0.6.1), CUSTOM_MODES is also a must.
  mingw32-make.exe clean
  mingw32-make.exe LDFLAGS="-no-undefined" AM_CFLAGS="-DCUSTOM_MODES=1 -Dcelt_decode=celt_0110_decode -Dcelt_decoder_create_custom=celt_0110_decoder_create_custom -Dcelt_decoder_destroy=celt_0110_decoder_destroy -Dcelt_mode_create=celt_0110_mode_create -Dcelt_mode_destroy=celt_0110_mode_destroy -Dcelt_mode_info=celt_0110_mode_info"
  ```
- take the .dlls from ./bin/bin, and rename libcelt.dll to libcelt-0061.dll and libcelt-0110.dll respectively.
- you need to create a .def file for those DLL with the renamed simbol names above
- finally the includes. libcelt gives "celt.h" "celt_types.h" "celt_header.h", but since we renamed a few functions we have a simpler custom .h with minimal renamed symbols.

For **Linux**, you can use CMake that similarly patch celt libs automatically.

You can also get them from the official git (https://gitlab.xiph.org/xiph/celt) call `./autogen.sh` first, then pass call configure/make with renames (see `./make-build.sh`).

Instead of passing `-DCUSTOM_MODES=1` to `make` you can pass `--enable-custom-codes` to *./configure*. There is also `--disable-oggtests`, `--disable-static/shared` and typical config. Note that if *./configure* finds Ogg in your system it'll try to build encoder/decoder test `tools` (that depend on libogg). There is no official way disable that or compile `libcelt` only, but you can force it by calling `make SUBDIRS=libcelt DIST_SUBDIRS=libcelt`, in case you have dependency issues.

### libspeex
Adds support for Speex (inside custom containers), used in a few *EA* formats (`.sns`, `.sps`) for voices.
- Source: http://downloads.us.xiph.org/releases/speex/speex-1.2.0.tar.gz
- DLL: `libspeex.dll`
- lib: `-lspeex`
- licensed under the Xiph.Org variant of the BSD license.
  https://www.xiph.org/licenses/bsd/speex/

Should be buildable with MSVC (in /win32 dir are .sln files, but not up to date and may need to convert .vcproj to vcxproj) or autotools (use `autogen.sh`, or script below).

You can also find a release on Github (https://github.com/xiph/speex/releases/tag/Speex-1.2.0). It has newer timestamps and some different helper files vs Xiph's release, but actual lib should be the same. Notably, Github's release *needs* `autogen.sh` that calls `autoreconf` to generate a base `configure` script, while Xiph's pre-includes `configure`. Since getting autoreconf working on **Windows** can be quite involved, Xiph's release is recommended on that platform.

**Windows** CMD example:
```bat
set PATH=%PATH%;C:\mingw\i686-8.1.0-release-win32-sjlj-rt_v6-rev0\mingw32\bin
set PATH=%PATH%;C:\Git\usr\bin

sh ./configure --host=mingw32 --prefix=/c/celt-0.11.0/bin/  --exec-prefix=/c/celt-0.11.0/bin/
mingw32-make.exe LDFLAGS="-no-undefined -static-libgcc" MAKE=mingw32-make.exe
mingw32-make.exe MAKE=mingw32-make.exe install
```
If all goes well, use generated .DLL in ./bin/bin (may need to rename to libspeex.dll) and ./win32/libspeex.def, and speex folder with .h in bin/include.


### maiatrac3plus
This lib was used as an alternate for ATRAC3PLUS decoding. Now this is handled by FFmpeg, though some code remains for now.

It was a straight-up decompilation from Sony's libs (presumably those found in SoundForge), without any clean-up or actual reverse engineering, thus legally and morally dubious.

It doesn't do encoder delay properly, but on the other hand decoding is 100% accurate unlike FFmpeg (probably inaudible though).

So, don't use it unless you have a very good reason.
