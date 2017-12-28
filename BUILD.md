# vgmstream

## Compilation requirements

**GCC / Make**: In Windows this means one of these two somewhere in PATH:
- MinGW-w64 (32bit version): https://sourceforge.net/projects/mingw-w64/
  - Use this for easier standalone executables
- MSYS2 with the MinGW-w64_shell (32bit) package: https://msys2.github.io/

**MSVC / Visual Studio**: Microsoft's Visual C++ and MSBuild, bundled in either:
- Visual Studio: https://www.visualstudio.com/downloads/
  - Visual Studio Community should work (free, but must register after trial period)
- Visual C++ Build Tools (no IDE): http://landinghub.visualstudio.com/visual-cpp-build-tools

**Git**: optional, to generate version numbers:
- Git for Windows: https://git-scm.com/download/win

## Compiling modules

### CLI (test.exe/vgmstream-cli) / Winamp plugin (in_vgmstream) / XMPlay plugin (xmp-vgmstream)

**With GCC**: use the *./Makefile* in the root folder, see inside for options. For compilation flags check the *Makefile* in each folder.
You need to manually rebuild if you change a *.h* file (use *make clean*).

In Linux, Makefiles can be used to cross-compile with the MingW headers, but may not be updated to generate native code at the moment. It should be fixable with some effort. Autotools should build it as vgmstream-cli instead (see the Audacious section).

Windows CMD example:
```
prompt $P$G$_$S
set PATH=C:\Program Files (x86)\Git\usr\bin;%PATH%
set PATH=C:\Program Files (x86)\mingw-w64\i686-5.4.0-win32-sjlj-rt_v5-rev0\mingw32\bin;%PATH%

cd vgmstream

mingw32-make.exe mingw_test -f Makefile ^
 VGM_ENABLE_FFMPEG=1 VGM_ENABLE_MAIATRAC3PLUS=0 ^
 SHELL=sh.exe CC=gcc.exe AR=ar.exe STRIP=strip.exe DLLTOOL=dlltool.exe WINDRES=windres.exe
```

**With MSVC**: open *./vgmstream_full.sln* and compile in Visual Studio, or use MSBuild in the command line. See the foobar2000 section for dependencies and CMD examples.

### foobar2000 plugin (foo\_input\_vgmstream)
Requires MSVC (foobar/SDK only links to MSVC C++ DLLs) and these dependencies:
- foobar2000 SDK, in *(vgmstream)/../foobar/*: http://www.foobar2000.org/SDK
- FDK-AAC, in *(vgmstream)/../fdk-aac/*: https://github.com/kode54/fdk-aac
- QAAC, in *(vgmstream)/../qaac/*: https://github.com/kode54/qaac
- WTL (if needed), in *(vgmstream)/../WTL/*: http://wtl.sourceforge.net/

Open *./vgmstream_full.sln* as a base, which expects the above dependencies. Then, depending on your VS version (like VS2015) you may need to manually do the following (in *Debug* and *Release* configurations):
- Change each project's compiler version from VS2010 to your version (right click menu)
- For *foobar2000_ATL_helpers* add *../../../WTL/Include* to the compilers's *additional includes*
- For *foo_input_vgmstream* add *../../WTL/Include* to the compilers's *additional includes*
- For *foo_input_vgmstream* add *../../foobar/foobar2000/shared/shared.lib* to the linker's *additional dependencies*

VS2013/VS2015/VS2017 may not be compatible with the SDK in release mode when some options are enabled due to compiler bugs.
FDK-AAC/QAAC can be safely disabled by removing *VGM_USE_MP4V2* and *VGM_USE_FDKAAC* in the compiler/linker options and the project dependencies, MAIATRAC3 too, as FFmpeg is used instead to support their codecs.


You can also use the command line to compile with MSBuild, if you don't want to touch the .vcxproj files, register VS2015 after trial, or only have VC++/MSBuild tools.

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
 /p:Configuration=Release
```

### Audacious plugin
Requires the dev version of Audacious (and dependencies), automake/autoconf, and gcc/make (C++11). It must be compiled and installed into Audacious, where it should appear in the plugin list as "vgmstream".

The plugin needs Audacious 3.5 or higher. New Audacious releases can break plugin compatibility so it may not work with the latest version unless adapted first.

FFmpeg and other external libraries aren't enabled, thus some formats are not supported. libvorbis and libmpg123 can be disabled with -DVGM_DISABLE_VORBIS and -DVGM_DISABLE_MPEG.

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
make -f Makefile.audacious

# copy to audacious plugins
sudo make -f Makefile.audacious install


# optional post-cleanup
make -f Makefile.audacious clean
find . -name ".deps" -type d -exec rm -r "{}" \;
./unbootstrap
## WARNING, removes *all* untracked files not in .gitignore
git clean -fd
```

# vgmstream123 player
Should be buildable with Autotools, much like the Audicious plugin, though requires libao (libao-dev).

Windows builds aren't supported at the moment (source may need to be adapted for non-POSIX systems).


## Development

### Code
vgmstream uses C (C89 when possible), and C++ for the foobar2000 and Audacious plugins.

C should be restricted to features VS2010 understands. This mainly means declaring variables at the start of a { .. } block (declare+initialize is fine, as long as it doesn't reference variables declared in the same block) and avoiding C99 features like variable-length arrays (but certain others like // comments are fine).

There are no hard coding rules but for consistency should follow general C conventions and the style used in most files: 4 spaces instead of tabs, underscore_and_lowercase_names, brackets starting in the same line (`if (..) { CRLF ... }`), etc. Some of the code may be a bit inefficient or duplicated at places, but it isn't much of a problem if gives clarity.

### Structure

```
./                   docs, scripts
./audacious/         Audacious plugin
./ext_includes/      external includes for compiling
./ext_libs/          external libs/DLLs for linking
./fb2k/              foobar2000 plugin
./src/               main vgmstream code and helpers
./src/coding/        format data decoders
./src/layout/        format data demuxers
./src/meta/          format header parsers
./test/              CLI tools
./winamp/            Winamp plugin
./xmplay/            XMPlay plugin
```

### Overview
vgmstream works by parsing a music stream header (*meta/*), preparing/controlling data and sample buffers (*layout/*) and decoding the compressed data into listenable PCM samples (*coding/*).

Very simplified it goes like this:
- player (test.exe, plugin, etc) opens a file stream (STREAMFILE) *[plugin's main/decode]*
- init tries all parsers (metas) until one works *[init_vgmstream]*
- parser reads header (channels, sample rate, loop points) and set ups the VGMSTREAM struct, if the format is correct *[init_vgmstream_(format-name)]*
- player finds total_samples to play, based on the number of loops and other settings *[get_vgmstream_play_samples]*
- player asks to fill a small sample buffer *[render_vgmstream]*
- layout prepares samples and offsets to read from the stream *[render_vgmstream_(layout)]*
- decoder reads and decodes bytes into PCM samples *[decode_vgmstream_(coding)]*
- player plays those samples, asks to fill sample buffer again, repeats (until total_samples)
- layout moves offsets back to loop_start when loop_end is reached *[vgmstream_do_loop]*
- player closes the VGMSTREAM once the stream is finished

### Components

#### STREAMFILEs
Structs with I/O callbacks that vgmstream uses in place of stdio/FILEs. All I/O must be done through STREAMFILEs as it lets plugins set up their own. This includes reading data or opening other STREAMFILEs (ex. when a header has companion files that need to be parsed, or during setup).

Players should open a base STREAMFILE and pass it to init_vgmstream. Once it's done this STREAMFILE must be closed, as internally vgmstream opens its own copy (using the base one's callbacks).

Custom STREAMFILEs wrapping base STREAMFILEs may be used for complex I/O cases (ex. if data needs decryption, or a file is composed of multiple sub-files).

#### VGMSTREAM
The VGMSTREAM (caps) is the main struct created during init when a file is successfully recognized and parsed. It holds the file's configuration (channels, sample rate, decoder, layout, samples, loop points, etc) and decoder state (STREAMFILEs, offsets per channel, current sample, etc), and is used to interact with the API.

#### metas
Metadata (header) parsers that identify and handle formats.

To add a new one:
- *src/meta/(format-name).c*: create new init_vgmstream_(format-name) parser that tests the extension and header id, reads all needed info from the stream header and sets up the VGMSTREAM
- *src/meta/meta.h*: define parser's init
- *src/vgmstream.h*: define meta type in the meta_t list
- *src/vgmstream.c*: add parser init to the init list
- *src/formats.c*: add new extension to the format list, add meta type description
- *src/libvgmstream.vcproj/vcxproj/filters*: add to compile new (format-name).c parser in VS
- if the format needs an external library don't forget to mark optional parts with: *#ifdef VGM_USE_X ... #endif*

Ultimately the meta must alloc the VGMSTREAM, set config and initial state. vgmstream needs the total number samples to work, so at times must convert from data sizes to samples (doing calculations or using helpers).

It also needs to open and assign to the VGMSTREAM one or several STREAMFILEs (usually reopening the base one, but could be any other file) to do I/O during decode, as well as setting the starting offsets of each channel and other values; this gives metas full flexibility at the cost of some repetition. The STREAMFILE passed to the meta will be discarded and its pointer must not be reused.

The .c file is usually named after the format's main extension or header id, optionally with affixes. Each file should parse one format and/or its variations (regardless of accepted extensions or decoders used) for consistency, but deviations may be found in the codebase. Sometimes a format is already parsed but not accepted due to bugs though.

Different formats may use the same extension but this isn't a problem as long as the header id or some other validation tells them apart, and should be implemented in separate .c files. If the format is headerless and the extension isn't unique enough it probably needs a generic GENH/TXTH header instead of direct support.

If the format supports subsongs it should read the stream index (subsong number) in the passed STREAMFILE, and use it to parse a section of the file. Then it must report the number of subsongs in the VGMSTREAM, to signal this feature is enabled. The index is 1-based (first subsong is 1, 0 is default/first).

#### layouts
Layouts control most of the main logic:
- receive external buffer to fill with PCM samples
- detect when looping must be done
- find max number of samples to do next decoder call (usually one frame, less if loop starts/ends)
- call decoder
- do post-process if necessary (move offsets, check stuff, etc)
- repeat until buffer is filled

Available layouts, depending on how codec data is laid out:
- none/flat: straight data. Decoder should handle channel offsets and other details normally.
- interleave: one data block per channel, mixed in configurable sizes. Once one channel block is fully decoded this layout skips the other channels, so the decoder only handles one at a time.
- blocked: data is divided into blocks, often with a header. Layout detects when a block is done and asks a helper function to fix offsets (skipping the header and pointing to data per channel), depending on the block format.
- others: uncommon cases may need its own custom layout (ex.- multistream/subfiles)

The layout is used mainly depends on the decoder. MP3 data (that may have 1 or 2 channels per frame) uses the flat layout, while DSP ADPCM (that only decodes one channel at a time) is interleaved. In case of mono files either could be used as there won't be any actual difference.

Layouts expect the VGMSTREAM to be properly initialized during the meta processing (channel offsets must point to each channel start offset).

#### decoders
Decoders take a sample buffer, convert data to PCM samples and fill one or multiple channels at a time, depending on the decoder itself. Usually its data is divided into frames with a number of samples, and should only need to do one frame at a time (when size is fixed/informed; vgmstream gives flexibility to the decoder), but must take into account that the sample buffer may be smaller than the frame samples, and that may start some samples into the frame.

Every call the decoder will need to find out the current frame offset (usually per channel). This is usually done with a base channel offset (from the VGMSTREAM) plus deriving the frame number (thus sub-offset, but only if frames are fixed) through the current sample, or manually updating the channel offsets every frame. This second method is not suitable to use with the interleave layout as it advances the offsets assuming they didn't change (this is a limitation/bug at the moment). Similarly, the blocked layout cannot contain interleaved data, and must use alt decoders with internal interleave (also a current limitation). Thus, some decoders and layouts don't mix.
 
If the decoder needs to keep state between calls it may use the VGMSTREAM for common values (like ADPCM history), or alloc a custom data struct. In that case the decoder should provide init/free functions so the meta or vgmstream may use. This is the case with decoders implemented using external libraries (*ext_libs*), as seen in *#ifdef VGM_USE_X ... #endif* sections.

Adding a new decoder involves:
- *src/coding/(decoder-name).c*: create `decode_x` function that decodes stream data into the passed sample buffer. If the codec requires custom internals it may need `init/reset/seek/free_x`, or other helper functions.
- *src/coding/coding.h*: define decoder's functions.
- *src/vgmstream.h*: define new coding type in the list. If the codec requires custom internals, define new `x_codec_data` struct.
- *src/vgmstream.c: reset_vgmstream*: call `reset_x` if needed 
- *src/vgmstream.c: close_vgmstream*: call `free_x` if needed
- *src/vgmstream.c: get_vgmstream_samples_per_frame*: define so vgmstream only asks for N samples per decode_x call. May return 0 if variable/unknown/etc (decoder must handle setting arbitrary number of samples)
- *src/vgmstream.c: get_vgmstream_frame_size*: define so vgmstream can do certain internal calculations. May return 0 if variable/unknown/etc, but blocked/interleave layouts will need to be used in a certain way.
- *src/vgmstream.c: decode_vgmstream*: call `decode_x`, possibly once per channel if the decoder works with a channel at a time.
- *src/vgmstream.c: vgmstream_do_loop*: call `seek_x` if needed
- *src/vgmstream.c: reset_vgmstream*: call `reset_x` if needed
- *src/formats.c*: add coding type description
- *src/libvgmstream.vcproj/vcxproj/filters*: add to compile new (decoder-name).c parser in VS
- *src/vgmstream.c*: add parser init to the init list
- if the codec depends on a external library don't forget to mark parts with: *#ifdef VGM_USE_X ... #endif*

#### core
The vgmstream core simply consists of functions gluing the above together and some helpers (ex.- extension list, loop adjust, etc). 

The *Overview* section should give an idea about how it's used.
