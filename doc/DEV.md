# vgmstream development help

This doc explains various possibly-not-too-obvious details about vgmstream for development purposes.


## API
The external API can be found in `libvgmstream.h`. Players and plugins that want to use vgmstream should follow that.

Historically there wasn't an "official" API so plugins used vgmstream's internals directly (which were mostly stable). But since there is proper API now internals may change without warning.


## Code
vgmstream uses C (C90/99 when possible), and C++ for the foobar2000 and Audacious plugins.

C should be restricted to features most compilers understand (including not-too-recent versions of VS/GCC/Clang), avoiding some less useful C99 features like variable-length arrays (others like // comments are fine).

### Conventions
There are no hard coding rules but for consistency one should follow the style used in most files:
- general C conventions
- 4 spaces instead of tabs
- `\n` breaks (LF, Linux style), instead of `\r\n` (CRLF, Windows style)
- `underscore_and_lowercase_names` instead of `CamelCase`
- `/* C89 comments */` for general comments, `//C99 comments` for other comments
- brackets starting in the same line
  - ex. `if (..) { LF ... LF }`
- line length ~100, more is ok for 'noise code' (uninteresting calcs or function defs)
- offsets/sizes in hex, counts/numbers in decimal
- test functions may return 1=ok, 0=ko for simplicity
- `free(ptr)` no need to NULL-check per standard, `close_stuff(ptr)` should follow when possible
- `lowercase_helper_structs_t`, `UPPERCASE_INTERNAL_STRUCTS`
- spaces in calcs/ifs/etc may be added as desired for clarity
  - ex. `if (simple_check)` or `if ( complex_and_important_stuff(weird + weird) )`
  - though generally you should split steps if readibility is impaired
- `goto` are used to abort and reach "fail" sections (typical C cleanup style), beware vars should be defined first
- pointer definitions should keep the `*` together for consistency 
  - ex. `VGMSTREAM* init_x() { ... }` `STREAMFILE* sf = ...`

But other styles may be found, this isn't very important as most files are isolated. When modifying a file or section of the code just try to follow the style set there so code doesn't clash too much.

### Formatting
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

## Code quality
There is quite a bit of code that could be improved overall, and many parts are a bit hacked together and brittle. But given how niche the project is and how few contributors there are, priority is given to adding and improving formats.

For regression testing there is a simple script that compares output of a previous version of vgmstream-cli with current. Some bugs may drastically change output when fixed (for example adjusting loops or decoding) so it could be hard to automate and maintain. There isn't an automated test suite at the moment, so tests are manually done as needed.

Code is checked for leaks from time to time using detection tools, but most of vgmstream formats are quite simple and don't need to manage memory. It's mainly useful for files using external decoders or complex segmented/layered layout combos.
```
# recommended to compile with debug info, for example:
make vgmstream_cli EXTRA_CFLAGS="-g" STRIP=echo

# find leaks
drmemory -- vgmstream-cli -o file.ext
```

### Security
Code is reasonably secure: some parts like IO are designed in a way to avoid segfaults, memory allocation is kept to minimum, and buffer handling is often very limited and simple making overflows unlikely.

However, parts may cause division-by-zero or even infinite loops on bad data (fixed as known), no fuzz testing is done (some segfaults may remain, specially for complex codecs), and since vgmstream uses some external libraries/codecs there may be issues with old versions (updated at times).

### Performance
Some of the code can be inefficient or duplicated at places, but it isn't that much of a problem if gives clarity. vgmstream's performance is fast enough (as it mainly deals with playing songs in real time) so that favors clarity over optimization.

Performance bottlenecks are mainly:
- I/O: since I/O is buffered it's possible to needlessly trash the buffers when reading previous/next offsets back and forth. It's better to read linearly using big enough data chunks and cache values.
- `for` loops: since your average audio file contains millions of samples, this means lots of loops. Care should be taken to avoid unnecessary function calls (relatively expensive) or recalculations per single sample when multiple samples could be processed at once.


## Source structure

```
./                   scripts
./audacious/         Audacious plugin
./cli/               CLI tools
./doc/               docs
./ext_includes/      external includes for compiling
./ext_libs/          external libs/DLLs for linking
./fb2k/              foobar2000 plugin
./src/               initial vgmstream code
./src/base/          core vgmstream features
./src/coding/        format data decoders
./src/coding/lib/    lib-like decoders, somewhat external to vgmstream
./src/layout/        format data demuxers
./src/meta/          format header parsers
./src/util/          helpers
./winamp/            Winamp plugin
./xmplay/            XMPlay plugin
```

## Terminology
Quick list of some audio terms used through vgmstream, applied to code. Mainly meant for the neophyte, hopefully helps new people willing to contribute.

- stream: an audio file, or a section inside it, or data 'lane' within, as the name implies. Just a generic term for a data chunk.
  - Streams normally have a header that tells how to play the file, and encoded ('compressed') audio data.
- encoder: program or code that transforms audio samples to encoded data.
- decoder: program or code that transforms encoded data to audio samples.
- encoded data: bunch of bytes (sometimes bits) that decode into one or many samples (for one or many channels) with a decoder.
- audio sample: digital audio unit (single value) to define playable sound. A "sound" is an oscillating "wave", and an array of many samples (digital) together simulate a wave (analog).
  - Each output channel has its own set of samples.
  - Normally `1 sample` actually means `1 sample for every channel` (common standard that makes code logic simpler).
    - If an stereo file has `1000000` samples it actually means `2 channels * 1000000` total samples.
- sample rate: number of samples per second (in *hz*). Also called frequency.
  - If a file has a sample rate *44100hz* and lasts *30 seconds* this means `44100 * 30 = 1323000` samples.
  - Since many samples together make a wave, the higher the sample rate the more samples we have, and the better-sounding wave we get.
- frame: smallest part of data that a decoder can transform into samples.
  - A frame can contain samples for one or many channels, depending on the encoder.
- interleave: size of encoded data for one channel. Some encoders only take a single (mono) channel at a time, so to make stereo or more we interleave frames.
  - For example 1 frame L, 1 frame R, 1 frame L, 1 frame R, etc. Or 10 frames L, 10 frames R, etc.
- block: a generic section of data, made of one or many frames for all channels.

vgmstream isn't too complex and with some perseverance one can add a new format (*meta*) easily enough.

## Process overview
vgmstream works by parsing audio header metadata (*meta/*), preparing + managing data and sample buffers (*layout/*) and decoding the compressed data into listenable PCM samples (*coding/*).

Very simplified it goes like this:
- player (CLI, plugin, etc) opens a *libstreamfile* (typically a file) *[plugin's main/decode]*
- player inits a *libvgmstream* and asks it to recognize the file
- init tries all parsers (metas) until one works *[vgmstream_init.c]*
- a parser identifies the header (channels, sample rate, loops...) and set ups the VGMSTREAM struct *[init_vgmstream_(format-name)]*
- player reads meta info returned by *libvgmstream* and does some setup
- player asks *libvgmstream* to fill a small sample buffer *[api_x.c]*
- renderer prepares buffers and pre-processes output *[render.c]*
- renderer and calls layouts *[render.c]*
- layout prepares loops, offsets and samples to read from the stream *[render_vgmstream_(layout)]*
- layout calls decoder *[render_vgmstream_(layout)]*
- decoder reads and decodes bytes into PCM samples *[decode.c]*
- layout repeats the process as needed *[render_vgmstream_(layout)]*
- renderer postprocesses the buffer filled by the layout + decoder *[render.c]*
- player plays those samples
- player asks to fill sample buffer again and repeats
- layout moves offsets back to loop_start when loop_end is reached *[decode_do_loop]*
- at some point renderer signals end
- player closes the *libvgmstream* once the stream is finished


## Internal parts

### STREAMFILEs
Structs with I/O callbacks that vgmstream uses in place of stdio/FILEs. All I/O must be done through STREAMFILEs as it lets plugins set up their own I/O. This includes reading data or opening other STREAMFILEs (ex. when a header has companion files that need to be parsed, or during setup).

For optimization purposes vgmstream may open a copy of the STREAMFILE per channel, as each has its own I/O buffer, and channel data can be too separate to fit a single buffer.

#### Custom STREAMFILEs
Sometimes game do certain complex behaviors that are hard to handle as-is.

For these we use custom STREAMFILEs wrapping base STREAMFILEs, used for complex I/O cases:
- file is a container of another full format (`fakename/clamp_streamfile`)
- data needs decryption (`io_streamfile`)
- data must be expanded/reduced on the fly for codecs that are not easy to feed chunked data (`io_streamfile`)
- data is divided in multiple physical files, but must be read as a single (`multifile_streamfile`)
- etc

Some metas combine those streamfiles together with special layouts to support very complex cases, that would require massive changes in vgmstream to support in a cleaner (but possibly undesirable) way.

So while they are bit clunky they allow adding extra cases with minimal effort and chance to break anything.


### VGMSTREAM
The VGMSTREAM (caps) is the main struct created during init when a file is successfully recognized and parsed. It holds the file's configuration (channels, sample rate, decoder, layout, samples, loop points, etc) and decoder state (STREAMFILEs, offsets per channel, current sample, etc).

### metas
Metadata (header) parsers that identify and handle formats. They must test the header id and do validations and read all needed info. Extension is also validated for documentation and heuristic purposes, but isn't strictly necessary (with +500 formats it makes telling apart formats a bit easier).

Formats are accepted when they are ready to be playable. Meaning, there isn't a separate step to "probe" header info and other to prepare to play.

#### Adding new metas
To add a new one:
- *src/meta/(format-name).c*: create new `init_vgmstream_(format-name)` that parses the format
- *src/vgmstream_types.h*: define meta type in the meta_t list
- *src/formats.c*: add new extension to the format list (if needed), add meta type description
- *src/meta/meta.h*: define parser's init
- *src/vgmstream_init.c*: add parser init to the init list
- *src/libvgmstream.vcproj/vcxproj/filters*: add to compile new `(format-name).c` parser in Visual Studio
  - May run `vspf.py` on root to add it automatically

Ultimately a meta must alloc the VGMSTREAM, set config and initial state. vgmstream needs the total of number samples to work, so at times must convert from data sizes to samples (doing calculations or using helpers).

It also needs to open and assign to the VGMSTREAM one or several STREAMFILEs (usually reopening the base one, but could be any other file) to do I/O during decode, as well as setting the starting offsets of each channel and other values; this gives metas full flexibility at the cost of some repetition. The STREAMFILE passed to the meta will be discarded and its pointer must not be reused.

If the format needs an external library don't forget to mark optional parts with: *#ifdef VGM_USE_X ... #endif*

#### meta names
The main `.c` file is usually named after the format's header id or official/common name. Each file should parse one format and its variations (regardless of accepted extensions or decoders used) for consistency, but deviations may be found in the codebase.

Different formats may use the same extension, but this isn't a problem as long as the header id or some other validation tells them apart, and should be implemented in separate `.c` files.

If the format is headerless or very simple to tell it appart (like only having sample rate + channels in the header), *TXTH* is used instead of adding direct support.

Some simple-but-common formats may be allowed for historical reasons, but they are considered "lower priority" and `.txth` files take precedence over them if found.

#### Subsongs
If a format supports subsongs it should read the stream index (subsong number) in the passed STREAMFILE, and use it to parse a section of the file. Then it must report the number of subsongs to the VGMSTREAM, to signal this feature is enabled. The index is 1-based (first subsong is 1, 0 is default/first). 

This makes possible to directly use bank-like formats like .FSB, and while vgmstream could technically support any container (like generic bigfiles or even .zip) it should be restricted to files that actually are audio banks.


### layouts
Layouts control most of the decoder logic:
- receive external buffer to fill with PCM samples
- detect when looping must be done
- find max number of samples to do next decoder call (usually one frame, less if loop starts/ends)
- call decoder
- do post-process if necessary (move offsets, check stuff, etc)
- repeat until buffer is filled

Available layouts, depending on how codec data is laid out:
- *flat*: straight data. Decoder should handle channel offsets and other details normally.
- *interleave*: one data block per channel, mixed in configurable sizes. Once one channel block is fully decoded this layout skips the other channels, so the decoder only handles one at a time.
- *blocked*: data is divided into blocks, often with a header. Layout detects when a block is done and asks a helper function to fix offsets (skipping the header and pointing to data per channel), depending on the block format.
- *segmented*: file is divided into consecutive but separate segments, each one is setup as a fully separate VGMSTREAM.
- *layered*: file is divided into multichannel layers that play at the same time, each one is setup as a fully separate VGMSTREAM.

Adding a new layouts is fairly hard due to the loop control logic and glue code needed, so for complex data formats custom IO STREAMFILEs is used instead.

The layout used mainly depends on the decoder. MP3 data (that may have 1 or 2 channels per frame) uses flat layout, while DSP ADPCM (that only decodes one channel at a time) is interleaved. In case of mono files either could be used as there won't be any actual difference.

Layouts expect the VGMSTREAM to be properly initialized during the meta processing (channel offsets must point to each channel start offset).

Some layouts (segmented/layered) internally use full VGMSTREAMs, that can be set to use play config as well to internally loop as well (see *render*). They simply call *render* like if each VGMSTREAM was a (big) decoder.

### decoders
Decoders take a sample buffer, convert data to PCM samples and fill one or multiple channels at a time, depending on the decoder itself.

Typically data is divided into frames with a number of samples, and should only need to decode one frame at a time (decoder has flexibility on this).

#### Common decoders
Many decoders are designed to take a supplied buffer, first_sample and samples_to_do. This means they can decode less samples than a full frame, or start in the middle of one, mainly for looping purposes (keeping partial decoding state). 

Typically layouts control how many samples are needed per decoder call. Some decoders advance offsets directly, or not and use workarounds and let layouts handle that. This behavior depends on setting `decode_get_frame_size` and is mainly used in interleaved and blocked layouts.

It makes them somewhat hard to understand, though the decoder themselves are fairly simple.

Adding a new decoder involves:
- *src/coding/(decoder-name).c*: create `decode_x` function that decodes stream data into the passed sample buffer.
  - If the codec requires custom internals it may need `init/reset/seek/free_x`, or other helper functions.
- *src/coding/coding.h*: define decoder's functions and type
- *src/decode.c -> decode_get_samples_per_frame*: define so vgmstream only asks for N samples per decode_x call.
 - May return 0 if variable/unknown/etc (decoder then must handle arbitrary number of samples)
- *src/decode.c -> decode_get_frame_size*: define so vgmstream can do certain internal calculations.
  - May return 0 if variable/unknown/etc, but blocked/interleave layouts will need to be used in a certain way.
- *src/decode.c -> decode_vgmstream*: call `decode_x`, possibly once per channel if the decoder works with a channel at a time.
- *src/decode.c*: add handling in `reset/seek/decode_free` if needed
- *src/formats.c*: add coding type description
- *src/libvgmstream.vcproj/vcxproj/filters*: add to compile new (decoder-name).c parser in VS
- if the codec depends on a external library don't forget to mark parts with: *#ifdef VGM_USE_X ... #endif*
  - May run `vspf.py` on root to add it automatically

#### Frame decoders
Some decoders simply decode samples for 1 frame. They have full control on the decoding, such as setting their own sample buffer and sample output format. They are a bit more complex to code but are easier to set up.

They are meant to be used in flat layouts for more complex codecs that decode many samples at once (MP3-like).

Adding a new decoder involves:
- *src/coding/(decoder-name).c*: create `decode_x` and related functions. At tbe bottom define `const codec_info_t hca_decoder = { ... }` with references to those functions.
- *src/coding/codec_info.c*: add decoder's new definition as extern and also in the 
- *src/formats.c*: add coding type description
- *src/libvgmstream.vcproj/vcxproj/filters*: add to compile new (decoder-name).c parser in VS
- if the codec depends on a external library don't forget to mark parts with: *#ifdef VGM_USE_X ... #endif*
  - May run `vspf.py` on root to add it automatically


### renderer
When vgmstream generates samples from a stream we collectively call this process "rendering".

The flow is: base (API/internals) -> render (pre/postprocess) -> layout (data setup) -> decode (output samples).

Unlike a simple decoder, vgmstream can loop and use advanced config like downmixing (enabled externally via API or internally for complex formats), so it behaves a bit like an audio player. The *render* part controls the *player* features.

#### Play config
By default vgmstream renders the stream once and stops. However it can be configured to loop N times, fade, trim part of the audio, etc. These are detected and applied in steps during the *render* process, meaning the output is not 1:1 of what the file has.

This config is done via API, and sometimes internally or via TXTP for audio formats with DAW-like features.

#### Mixing
After decoding, sometimes we change volume, number of channels, etc. These "ops" are applied in order as a mixing chain, and modify the final buffer (see `mixing.c`). Resampling is similar but alters total output samples and may swap buffers as well.

#### Stopping
*render* decides when vgmstream must stops, typically when its *total time* (target samples) is reached.

This *time* depends on vgmstream's config. For example, if a file of 60s is configured to loop twice and fade 10s, it must render 130s. Similarly, if a file is set to play forever it continues rendering forever (never reaching a target playtime).

This means vgmstream needs to know the (approximate) file's total samples to make calculations, and may ignore decoding errors (including end of file) to reach target samples.


### Seeking
vgmstream doesn't properly seek, just render + discard (with some speed-up tricks). This means seeking is fairly slow.

Many codecs also aren't designed to seek, such as having variable-sized frames (unknown data offsets) or seeking would leave the decoder in an inconsistent internal state. Some formats add seeking helpers but aren't standard (handled case by case).

It's possible to improve this but it's low priority due to the amount of work, and since fast seeking isn't that important for game music, given it's usually fairly short.


### base/core
The vgmstream core simply consists of functions gluing the above together and some helpers: accepted extensions list for plugins (vgmstream itself doesn't use this), info printer, seeking, tag reading, etc.

These are used 

