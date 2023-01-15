# vgmstream development help

## Code
vgmstream uses C (C89 when possible), and C++ for the foobar2000 and Audacious plugins.

C should be restricted to features most compilers understand (including not-too-recent versions of VS/GCC/Clang), avoiding some less useful C99 features like variable-length arrays (certain others like // comments are fine).

There are no hard coding rules but for consistency one should follow the style used in most files:
- general C conventions
- 4 spaces instead of tabs
- `\n` breaks (LF, Linux style), instead of `\r\n` (CRLF, Windows style)
- `underscore_and_lowercase_names` instead of `CamelCase`
- `/* C89 comments */` for general comments, `//C99 comments` for special comments (like disabling code but leaving it there for visibility)
- brackets starting in the same line
  - ex. `if (..) { LF ... LF }`
- line length ~100, more is ok for 'noise code' (uninteresting calcs or function defs)
- offsets/sizes in hex, counts/numbers in decimal
- test functions may return 1=ok, 0=ko for simplicity
- `free(ptr)` no need to NULL-check per standard, `close_stuff(ptr)` should follow when possible
- `lowercase_helper_structs_t`, `UPPERCASE_MAIN_STRUCTS`
- spaces in calcs/ifs/etc may be added as desired for clarity
  - ex. `if (simple_check)` or `if ( complex_and_important_stuff(weird + weird) )`
- `goto` are used to abort and reach "fail" sections (typical C cleanup style), beware vars should be defined first
- pointer definitions should keep the `*` together for consistency 
  - ex. `VGMSTREAM* init_x() { ... }` `STREAMFILE* sf = ...`

But other styles may be found, this isn't very important as most files are isolated. When modifying a file or section of the code just try to follow the style set there so code doesn't clash too much.

If you aren't sure you can use this `.clang-format` file as an starting point (put in source root), that IDEs like VS can use to reformat code. This doesn't fully mimic common style though.
```
# see: https://clang.llvm.org/docs/ClangFormatStyleOptions.html
BasedOnStyle: Google

IndentWidth: 4
ColumnLimit: 150

BreakBeforeBraces: Custom
BraceWrapping:
  BeforeElse: true

DerivePointerAlignment: false
PointerAlignment: Left
SpaceBeforeParens: ControlStatements

AccessModifierOffset: -2
AllowShortBlocksOnASingleLine: Never
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: Never
```

### Code quality
There is quite a bit of code that could be improved overall, and parts can feel a bit hacked together and brittle. But given how niche the project is and how few contributors there are, priority is given to adding and improving formats.

For regression testing there is a simple script that compares output of a previous version of vgmstream-cli with current. Some bugs may drastically change output when fixed (for example adjusting loops or decoding) so it could be hard to automate and maintain. There isn't an automated test suite at the moment, so tests are manually done as needed.

Code is checked for leaks from time to time using detection tools, but most of vgmstream formats are quite simple and don't need to manage memory. It's mainly useful for files using external decoders or complex segmented/layered layout combos.
```
# recommended to compile with debug info, for example:
make vgmstream_cli EXTRA_CFLAGS="-g" STRIP=echo

# find leaks
drmemory -- vgmstream-cli -o file.ext
```

Code is reasonably secure: some parts like IO are designed in a way should avoid segfaults, memory allocation is kept to minimum, and buffer handling is often very limited and simple making overflows unlikely. However, parts may cause division-by-zero or even infinite loops on bad data (fixed as known), no fuzz testing is done (some segfaults may remain, specially for complex codecs), and since vgmstream uses some external libraries/codecs there may be issues with old versions (updated at times).

Some of the code can be inefficient or duplicated at places, but it isn't that much of a problem if gives clarity. vgmstream's performance is fast enough (as it mainly deals with playing songs in real time) so that favors clarity over optimization. Performance bottlenecks are mainly:
- I/O: since I/O is buffered it's possible to needlessly trash the buffers when reading previous/next offsets back and forth. It's better to read linearly using big enough data chunks and cache values.
- for loops: since your average audio file contains millions of samples, this means lots of loops. Care should be taken to avoid unnecessary function calls or recalculations per single sample when multiple samples could be processed at once.


## Source structure

```
./                   scripts
./audacious/         Audacious plugin
./cli/               CLI tools
./doc/               docs
./ext_includes/      external includes for compiling
./ext_libs/          external libs/DLLs for linking
./fb2k/              foobar2000 plugin
./src/               main vgmstream code
./src/coding/        format data decoders
./src/layout/        format data demuxers
./src/meta/          format header parsers
./src/util/          helpers
./winamp/            Winamp plugin
./xmplay/            XMPlay plugin
```

## Terminology
Quick list of some audio terms used through vgmstream, applied to code. Mainly meant for the neophyte, hopefully helps new people willing to contribute. vgmstream isn't too complex and with some perseverance one can add a new format (*meta*) easily enough.

- stream: an audio file, or a section inside it, or data 'lane' within, as the name implies. Just a generic term for a data chunk.
  - Streams normally have a header that tells how to play the file, and encoded ('compressed') audio data.
- encoder: program or code that transforms audio samples to encoded data.
- decoder: program or code that transforms encoded data to audio samples.
- encoded data: bunch of bytes (sometimes bits) that decode into one or many samples (for one or many channels) with a decoder.
- audio sample: digital audio unit (single value) to define playable sound. A sound is a wave, and an array of many samples (digital) together make a wave (analog).
  - Each output channel has its own set of samples.
  - Normally `1 sample` actually means `1 sample for every channel` (common standard that makes code logic simpler).
    - If an stereo file has `1000000` samples it actually means `2*1000000` total samples.
- sample rate: number of samples per second (in *hz*). Also called frequency.
  - If a file has a sample rate *44100hz* and lasts *30 seconds* this means `44100 * 30 = 1323000` samples.
  - Since many samples together make a wave, the higher the sample rate the more samples we have, and the better-sounding wave we get.
- frame: smallest part of data that a decoder can transform into samples.
  - A frame can contain samples for one or many channels, depending on the encoder.
- interleave: size of encoded data for one channel. Some encoders only take a single (mono) channel at a time, so to make stereo or more we interlace frames.
  - For example 1 frame L, 1 frame R, 1 frame L, 1 frame R, etc. Or 10 frames L, 10 frames R, etc.
- block: a generic section of data, made of one or many frames for all channels.


## Overview
vgmstream works by parsing a music stream header (*meta/*), preparing/controlling data and sample buffers (*layout/*) and decoding the compressed data into listenable PCM samples (*coding/*).

Very simplified it goes like this:
- player (CLI, plugin, etc) opens a file stream (STREAMFILE) *[plugin's main/decode]*
- init tries all parsers (metas) until one works *[init_vgmstream]*
- parser reads header (channels, sample rate, loop points) and set ups the VGMSTREAM struct, if the format is correct *[init_vgmstream_(format-name)]*
- player finds total_samples to play, based on the number of loops and other settings *[get_vgmstream_play_samples]*
- player asks to fill a small sample buffer *[render_vgmstream]*
- layout prepares samples and offsets to read from the stream *[render_vgmstream_(layout)]*
- decoder reads and decodes bytes into PCM samples *[decode_vgmstream_(coding)]*
- player plays those samples, asks to fill sample buffer again, repeats (until total_samples)
- layout moves offsets back to loop_start when loop_end is reached *[vgmstream_do_loop]*
- player closes the VGMSTREAM once the stream is finished

vgsmtream's main code (located in src) may be considered "libvgmstream", and plugins interface it through vgmstream.h, mainly the part commented as "vgmstream public API". There isn't a clean external API at the moment, this may be improved later.

## Components

### STREAMFILEs
Structs with I/O callbacks that vgmstream uses in place of stdio/FILEs. All I/O must be done through STREAMFILEs as it lets plugins set up their own. This includes reading data or opening other STREAMFILEs (ex. when a header has companion files that need to be parsed, or during setup).

Players should open a base STREAMFILE and pass it to init_vgmstream. Once it's done this STREAMFILE must be closed, as internally vgmstream opens its own copy (using the base one's callbacks).

For optimization purposes vgmstream may open a copy of the FILE per channel, as each has its own I/O buffer, and channel data can be too separate to fit a single buffer.

Custom STREAMFILEs wrapping base STREAMFILEs may be used for complex I/O cases:
- file is a container of another format (`fakename/clamp_streamfile`)
- data needs decryption (`io_streamfile`)
- data must be expanded/reduced on the fly for codecs that are not easy to feed chunked data (`io_streamfile`)
- data is divided in multiple physical files, but must be read as a single (`multifile_streamfile`)

Certain metas combine those streamfiles together with special layouts to support very complex cases, that would require massive changes in vgmstream to support in a cleaner (possible undesirable) way.


### VGMSTREAM
The VGMSTREAM (caps) is the main struct created during init when a file is successfully recognized and parsed. It holds the file's configuration (channels, sample rate, decoder, layout, samples, loop points, etc) and decoder state (STREAMFILEs, offsets per channel, current sample, etc), and is used to interact with the API.

### metas
Metadata (header) parsers that identify and handle formats.

To add a new one:
- *src/meta/(format-name).c*: create new init_vgmstream_(format-name) parser that tests the extension and header id, reads all needed info from the stream header and sets up the VGMSTREAM
- *src/meta/meta.h*: define parser's init
- *src/vgmstream.h*: define meta type in the meta_t list
- *src/vgmstream.c*: add parser init to the init list
- *src/formats.c*: add new extension to the format list, add meta type description
- *src/libvgmstream.vcproj/vcxproj/filters*: add to compile new (format-name).c parser in VS
- if the format needs an external library don't forget to mark optional parts with: *#ifdef VGM_USE_X ... #endif*

Ultimately the meta must alloc the VGMSTREAM, set config and initial state. vgmstream needs the total of number samples to work, so at times must convert from data sizes to samples (doing calculations or using helpers).

It also needs to open and assign to the VGMSTREAM one or several STREAMFILEs (usually reopening the base one, but could be any other file) to do I/O during decode, as well as setting the starting offsets of each channel and other values; this gives metas full flexibility at the cost of some repetition. The STREAMFILE passed to the meta will be discarded and its pointer must not be reused.

The .c file is usually named after the format's main extension or header id, optionally with affixes. Each file should parse one format and/or its variations (regardless of accepted extensions or decoders used) for consistency, but deviations may be found in the codebase. Sometimes a format is already parsed but not accepted due to bugs though.

Different formats may use the same extension but this isn't a problem as long as the header id or some other validation tells them apart, and should be implemented in separate .c files. If the format is headerless and the extension isn't unique enough it probably needs a generic GENH/TXTH header instead of direct support.

If the format supports subsongs it should read the stream index (subsong number) in the passed STREAMFILE, and use it to parse a section of the file. Then it must report the number of subsongs in the VGMSTREAM, to signal this feature is enabled. The index is 1-based (first subsong is 1, 0 is default/first). This makes possible to directly use bank-like formats like .FSB, and while vgmstream could technically support any container (like generic bigfiles or even .zip) it should be restricted to files that actually are audio banks.

### layouts
Layouts control most of the main logic:
- receive external buffer to fill with PCM samples
- detect when looping must be done
- find max number of samples to do next decoder call (usually one frame, less if loop starts/ends)
- call decoder
- do post-process if necessary (move offsets, check stuff, etc)
- repeat until buffer is filled

Available layouts, depending on how codec data is laid out:
- flat: straight data. Decoder should handle channel offsets and other details normally.
- interleave: one data block per channel, mixed in configurable sizes. Once one channel block is fully decoded this layout skips the other channels, so the decoder only handles one at a time.
- blocked: data is divided into blocks, often with a header. Layout detects when a block is done and asks a helper function to fix offsets (skipping the header and pointing to data per channel), depending on the block format.
- segmented: file is divided into consecutive but separate segments, each one is setup as a fully separate VGMSTREAM.
- layered: file is divided into multichannel layers that play at the same time, each one is setup as a fully separate VGMSTREAM.
- others: uncommon cases may need its own custom layout, but may be dealt with using custom IO STREAMFILEs instead.

The layout used mainly depends on the decoder. MP3 data (that may have 1 or 2 channels per frame) uses flat layout, while DSP ADPCM (that only decodes one channel at a time) is interleaved. In case of mono files either could be used as there won't be any actual difference.

Layouts expect the VGMSTREAM to be properly initialized during the meta processing (channel offsets must point to each channel start offset).

### decoders
Decoders take a sample buffer, convert data to PCM samples and fill one or multiple channels at a time, depending on the decoder itself. Usually its data is divided into frames with a number of samples, and should only need to do one frame at a time (when size is fixed/informed; vgmstream gives flexibility to the decoder), but must take into account that the sample buffer may be smaller than the frame samples, and that may start some samples into the frame (this is also done to handle looping in some cases, where decoder state must stop in the middle).

Every call the decoder will need to find out the current frame offset (usually per channel). This is usually done with a base channel offset (from the VGMSTREAM) plus deriving the frame number (thus sub-offset, but only if frames are fixed) through the current sample, or manually updating the channel offsets every frame. This second method is not suitable to use with the interleave layout as it advances the offsets assuming they didn't change (this is a limitation/bug at the moment). Similarly, the blocked layout cannot contain interleaved data, and must use alt decoders with internal interleave (also a current limitation). Thus, some decoders and layouts don't mix.

If the decoder needs to keep state between calls it may use the VGMSTREAM for common values (like ADPCM history), or alloc a custom data struct. In that case the decoder should provide init/free functions so the meta or vgmstream may use. This is seen with decoders implemented using external libraries (*ext_libs*), as seen in *#ifdef VGM_USE_X ... #endif* sections.

Adding a new decoder involves:
- *src/coding/(decoder-name).c*: create `decode_x` function that decodes stream data into the passed sample buffer. If the codec requires custom internals it may need `init/reset/seek/free_x`, or other helper functions.
- *src/coding/coding.h*: define decoder's functions and type
- *src/decode.c: get_vgmstream_samples_per_frame*: define so vgmstream only asks for N samples per decode_x call. May return 0 if variable/unknown/etc (decoder then must handle arbitrary number of samples)
- *src/decode.c: get_vgmstream_frame_size*: define so vgmstream can do certain internal calculations. May return 0 if variable/unknown/etc, but blocked/interleave layouts will need to be used in a certain way.
- *src/decode.c: decode_vgmstream*: call `decode_x`, possibly once per channel if the decoder works with a channel at a time.
- *src/decode.c: add handling in `reset/seek/free_codec` if needed
- *src/formats.c*: add coding type description
- *src/libvgmstream.vcproj/vcxproj/filters*: add to compile new (decoder-name).c parser in VS
- if the codec depends on a external library don't forget to mark parts with: *#ifdef VGM_USE_X ... #endif*

### core
The vgmstream core simply consists of functions gluing the above together and some helpers (ex.- extension list, loop adjust, etc). 

The *Overview* section should give an idea about how it's used.
