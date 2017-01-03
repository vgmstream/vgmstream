# vgmstream

## Compilation requirements

**GCC**: you need GCC and MAKE somewhere in path. In Windows this means one of these:
- MinGW-w64 (32bit version): https://sourceforge.net/projects/mingw-w64/
- MSYS2 with the MinGW-w64_shell (32bit) package: https://msys2.github.io/

**MSVC / Visual Studio**: Visual Studio Community 2015 (free) should work:
- Visual Studio: https://www.visualstudio.com/downloads/

**Git**: optional, to generate version numbers:
- Git for Windows: https://git-scm.com/download/win

## Compiling modules

### test.exe / in_vgmstream (Winamp) / xmp-vgmstream (XMPlay)

**With GCC**: use the *./Makefile* in the root folder, see inside for options. For compilation flags check the *Makefile* in each folder.
You need to manually rebuild if you change a *.h* file (use *make clean*).

In Linux you may need to use *Makefile.unix.am* instead, and note that some Linux makefiles aren't up to date.

Windows CMD example for test.exe:
```
set PATH=%PATH%;C:\Git\usr\bin
set PATH=%PATH%;C:\mingw-w64\i686-5.4.0-win32-sjlj-rt_v5-rev0\mingw32\bin

cd vgmstream

mingw32-make.exe mingw_test -f Makefile ^
 VGM_ENABLE_FFMPEG=1 VGM_ENABLE_MAIATRAC3PLUS=0 ^
 SHELL=sh.exe CC=gcc.exe AR=ar.exe STRIP=strip.exe DLLTOOL=dlltool.exe WINDRES=windres.exe
```

**With MSVC**: open *./vgmstream.sln* and compile in Visual Studio.
For XMPlay open *xmp-vgmstream/xmp-vgmstream.sln* instead; FDK-AAC/QAAC/others may be needed (see below).


### foo_input_vgmstream (foobar2000)
Requires MSVC (foobar/SDK only links to MSVC C++ DLLs) and these dependencies:
- foobar2000 SDK, in *(vgmstream)/../foobar/*: http://www.foobar2000.org/SDK
- FDK-AAC, in *(vgmstream)/../fdk-aac/*: https://github.com/kode54/fdk-aac
- QAAC, in *(vgmstream)/../qaac/*: https://github.com/kode54/qaac
- WTL91_5321_Final includes (if needed): http://wtl.sourceforge.net/
FDK-AAC/QAAC can be disabled by removing *VGM_USE_MP4V2* and *VGM_USE_FDKAAC*.

Open *./vgmstream.sln* as a base and add *fb2k/foo_input_vgmstream.vcxproj*, which expects the above, and all projects from those dependencies.

Depending on your VS version you may need to manually do the following:
- Change each project's compiler version from VS2010 to yours
- For foobar add *(vgmstream)/../WTL91_5321_Final/Include* to the compilers's *additional includes*
- For foobar add *(vgmstream)/../foobar/foobar2000/shared/shared.lib* to the linker's *additional dependencies*
VS2013 may not be compatible with the SDK.


## Development

### Structure
vgmstream uses C (C89 when possible), except the foobar2000 plugin (C++).

```
./                   docs, scripts
./ext_includes/      external includes for compiling
./ext_libs/          external libs/DLLs for linking
./fb2k/              foobar2000 plugin
./src/               main vgmstream code and helpers
./src/coding/        format sample decoders
./src/layout/        format data demuxers
./src/meta/          format header parsers
./test/              test.exe CLI
./unix/              Audacious plugin
./winamp/            Winamp plugin
./xmp-vgmstream/     XMPlay plugin
```

### Overview
vgmstream works by parsing a music stream header (*meta/*), reading/demuxing data or looping (*layout/*) and decoding the compressed data into listenable PCM samples (*coding/*).

Very simplified it goes like this:
- player (test.exe, winamp plugin, etc) inits the stream *[main]*
- init tries all parsers (metas) until one works *[init_vgmstream]*
- parser reads header (channels, sample rate, loop points) and set ups a VGMSTREAM struct + layout/coding, if the format is correct *[init_vgmstream_(format-name)]*
- player gets total_samples to play, based on the number of loops and other settings *[get_vgmstream_play_samples]*
- player asks to fill a small sample buffer *[render_vgmstream]*
- layout prepares bytes to read from the stream *[render_vgmstream_(layout)]*
- decoder decodes bytes into PCM samples *[decode_vgmstream_(coding)]*
- player plays those samples, asks to fill sample buffer, repeats until total_samples
- layout moves back to loop_start when loop_end is reached *[vgmstream_do_loop]*

### Adding new formats
For new simple formats, assuming existing layout/coding:
- *src/meta/(format-name).c*: create new format parser that reads all needed info from the stream header and inits VGMSTREAM
- *src/meta/meta.h*: register parser's init
- *src/vgmstream.h*: register new meta
- *src/vgmstream.c*: add parser init to search list, add meta description
- *src/formats.c*: add new extension to the format list
  *fb2k/in_vgmstream.cpp*: add new extension to the file associations list
- *src/Makefile*
  *src/meta/Makefile.unix.am*
  *src/libvgmstream.vcproj/vcxproj/filters*: to compile new (format-name).c parser
- if the format needs an external library don't forget to make it optional with: *#ifdef VGM_USE_X ... #endif*
