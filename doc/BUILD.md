# vgmstream build help

## Compilation requirements

**GCC / Make**: In Windows this means one of these two somewhere in PATH:
- MinGW-w64 (32bit version): https://sourceforge.net/projects/mingw-w64/
  - Use this for easier standalone executables
  - Latest online installer with any config should work (for example: gcc-7.2.0, i686, win32, sjlj).
- MSYS2 with the MinGW-w64_shell (32bit) package: https://msys2.github.io/

**MSVC / Visual Studio**: Microsoft's Visual C++ and MSBuild, bundled in either:
- Visual Studio (2015/2017/latest): https://www.visualstudio.com/downloads/
  - Visual Studio Community should work (free, but must register after trial period)
- Visual C++ Build Tools (no IDE): http://landinghub.visualstudio.com/visual-cpp-build-tools

**Git**: optional, to generate version numbers:
- Git for Windows: https://git-scm.com/download/win

## Compiling modules

### CLI (test.exe/vgmstream-cli) / Winamp plugin (in_vgmstream) / XMPlay plugin (xmp-vgmstream)

**With GCC**: use the *./Makefile* in the root folder, see inside for options. For compilation flags check the *Makefile* in each folder.
You may need to manually rebuild if you change a *.h* file (use *make clean*).

In Linux, Makefiles can be used to cross-compile with the MingW headers, but may not be updated to generate native code at the moment. It should be fixable with some effort. Autotools should build it as vgmstream-cli instead (see the Audacious section).

Windows CMD example:
```
prompt $P$G$_$S
set PATH=C:\Program Files (x86)\Git\usr\bin;%PATH%
set PATH=C:\Program Files (x86)\mingw-w64\i686-5.4.0-win32-sjlj-rt_v5-rev0\mingw32\bin;%PATH%

cd vgmstream

mingw32-make.exe vgmstream_cli -f Makefile ^
 VGM_ENABLE_FFMPEG=1 ^
 SHELL=sh.exe CC=gcc.exe AR=ar.exe STRIP=strip.exe DLLTOOL=dlltool.exe WINDRES=windres.exe
```

**With MSVC**: To build in Visual Studio, run *./init-build.bat*, open *./vgmstream_full.sln* and compile. To build from the command line, run *./build.bat*.

The build script will automatically handle obtaining dependencies and making the project changes listed in the foobar2000 section (you may need to get some PowerShell .NET packages).

You can also call MSBuild directly in the command line (see the foobar2000 section for dependencies and examples)

### foobar2000 plugin (foo\_input\_vgmstream)
Requires MSVC (foobar/SDK only links to MSVC C++ DLLs) and these dependencies:
- foobar2000 SDK (2018), in *(vgmstream)/dependencies/foobar/*: http://www.foobar2000.org/SDK
- FDK-AAC, in *(vgmstream)/dependencies/fdk-aac/*: https://github.com/kode54/fdk-aac
- QAAC, in *(vgmstream)/dependencies/qaac/*: https://github.com/kode54/qaac
- WTL (if needed), in *(vgmstream)/dependencies/WTL/*: http://wtl.sourceforge.net/

The following project modifications are required:
- For *foobar2000_ATL_helpers* add *../../../WTL/Include* to the compilers's *additional includes*

FDK-AAC/QAAC can be safely disabled by removing *VGM_USE_MP4V2* and *VGM_USE_FDKAAC* in the compiler/linker options and the project dependencies, as FFmpeg is used instead to support their codecs.

You can also manually use the command line to compile with MSBuild, if you don't want to touch the .vcxproj files, register VS after trial, get PowerShell dependencies for the build script, or only have VC++/MSBuild tools.

Windows CMD example for foobar2000:
```
prompt $P$G$_$S
set PATH=C:\Program Files (x86)\Git\usr\bin;%PATH%
set PATH=C:\Program Files (x86)\MSBuild\14.0\Bin;%PATH%

cd vgmstream

set CL=/I"C:\projects\WTL\Include"
set LINK="C:\projects\foobar\foobar2000\shared\shared.lib"

msbuild fb2k/foo_input_vgmstream.vcxproj ^
 /t:Clean

msbuild fb2k/foo_input_vgmstream.vcxproj ^
 /t:Build ^
 /p:Platform=Win32 ^
 /p:PlatformToolset=v140 ^
 /p:Configuration=Release ^
 /p:DependenciesDir=../..
```

### Audacious plugin
Requires the dev version of Audacious (and dependencies), automake/autoconf, and gcc/make (C++11). It must be compiled and installed into Audacious, where it should appear in the plugin list as "vgmstream".

The plugin needs Audacious 3.5 or higher. New Audacious releases can break plugin compatibility so it may not work with the latest version unless adapted first.

libvorbis and libmpg123 will be used if found, while FFmpeg and other external libraries aren't enabled, thus some formats won't work.

Windows builds aren't supported at the moment (should be possible but there are complex dependency chains).


Terminal example, assuming a Ubuntu-based Linux distribution:
```
# build requirements
sudo apt-get update
sudo apt-get install gcc g++ make
sudo apt-get install autoconf automake libtool
sudo apt-get install git
# vgmstream dependencies
sudo apt-get install libmpg123-dev libvorbis-dev
# Audacious player and dependencies
sudo apt-get install audacious  
sudo apt-get install audacious-dev libglib2.0-dev libgtk2.0-dev libpango1.0-dev

# check Audacious version >= 3.5
pkg-config --modversion audacious

# build
git clone https://github.com/kode54/vgmstream
cd vgmstream

./bootstrap
./configure
make -f Makefile.autotools

# copy to audacious plugins
sudo make -f Makefile.autotools install


# optional post-cleanup
make -f Makefile.autotools clean
find . -name ".deps" -type d -exec rm -r "{}" \;
./unbootstrap
## WARNING, removes *all* untracked files not in .gitignore
git clean -fd
```

# vgmstream123 player
Should be buildable with Autotools, much like the Audacious plugin, though requires libao (libao-dev).

Windows builds are possible with libao.dll and includes, but some features are disabled.
