# TXTH FORMAT

TXTH is a simple text file that uses text commands to simulate a header for files unsupported by vgmstream, mainly headerless audio.

When an unsupported file is loaded (for instance "bgm01.snd"), vgmstream tries to find a TXTH header in the same dir, in this order:
- (filename.ext).txth
- .(ext).txth
- .txth

If found and parsed correctly (the .txth may be rejected if incorrect commands are found) vgmstream will try to play the file as described. Extension must be accepted/added to vgmstream (plugins like foobar2000 only load extensions from a whitelist in formats.c), or one could rename to any supported extension (like .vgmstream), or leave the file extensionless.

You can also use ".(sub).(ext).txth" (if the file is "filename.sub.ext"), to allow mixing slightly different files in the same folder. The "sub" part doesn't need to be an extension, for example:
- 001.1ch.str, 001.1ch.str may use .1ch.txth
- 003.2ch.str, 003.2ch.str may use .2ch.txth
- etc

## Example of a TXTH file
For an unsupported bgm01.vag this would be a simple TXTH for it:
```
id_value = 0x534E4420       #test that file starts with "SND "
id_offset = @0x00:BE        #test is done at offset 0, big endian value
codec = PSX                 #data uses PS-ADPCM
sample_rate = @0x10$2       #get sample rate at offset 0x10, 16 bit value
channels = @0x14            #get number of channels at offset 14
interleave = 0x1000         #fixed value
start_offset = 0x100        #data starts after exactly this value
num_samples = data_size     #find automatically number of samples in the file
loop_flag = auto            #find loop points in PS-ADPCM
```
A text file with the above commands must be saved as ".vag.txth" or ".txth", notice it starts with a "." (dot). On Windows files starting with a dot can be created by appending a dot at the end: ".txth."


## Available commands

The file is made of lines with `key = value` commands describing a header. Commands are all case sensitive and spaces are optional: `key=value`, `key  =   value`, and so on are all ok. Comments start with # and can be inlined.

The parser is fairly simple and may be buggy or unexpected in some cases. The order of keys is variable but some things won't work if others aren't defined (ex. bytes-to-samples may not work without channels or interleave).



```
# COMMON VALUES
# - (number): constant number in dec/hex, unsigned (no +10 or -10).
#   Examples: 44100, 40, 0x40 (decimal=64)
# - (offset): format is @(number)[:LE|BE][$1|2|3|4]
#   * @(number): value at offset (required)
#   * :LE|BE: value is little/big endian (optional, defaults to LE)
#   * $1|2|3|4: value has size of 8/16/24/32 bit (optional, defaults to 4)
#   Examples: @0x10:BE$2 (get big endian 16b value at 0x10)
# - (field): uses current value of a field. Accepted strings:
#   - interleave, interleave_last, channels, sample_rate
#   - start_offset, data_size
#   - num_samples, loop_start_sample, loop_end_sample
#   - subsong_count, subsong_offset
# - (string): other special values for certain keys, described below
# Those may be combined with math operations (+-*/):
#  "field = (number) (op) (offset) (op) (field) (...)"

# CODEC [REQUIRED]
# Sets codec used to encode the data.
# Accepted codec strings:
# - PSX            PlayStation ADPCM
#   * For many PS1/PS2/PS3 games
#   * Interleave is multiple of 0x10, often +0x1000
# - PSX_bf         PlayStation ADPCM with bad flags
#   * Variation with garbage data, for rare PS2 games
# - XBOX           Xbox IMA ADPCM (mono/stereo)
#   * For many XBOX games, and some PC games
#   * Special interleave is multiple of 0x24 (mono) or 0x48 (stereo)
# - DSP|NGC_DSP    Nintendo GameCube ADPCM
#   * For many GC/Wii/3DS games
#   * Interleave is multiple of 0x08, often +0x1000
#   * Must set decoding coefficients (coef_offset/spacing/etc)
# - DTK|NGC_DTK    Nintendo ADP/DTK ADPCM
#   * For rare GC games
# - PCM16LE        PCM 16-bit little endian
#   * For many games (usually on PC)
#   * Interleave is multiple of 0x2
# - PCM16BE        PCM 16-bit big endian
#   * Variation for certain consoles (GC/Wii/PS3/X360/etc)
# - PCM8           PCM 8-bit signed
#   * For some games (usually on PC)
#   * Interleave is multiple of 0x1
# - PCM8_U         PCM 8-bit unsigned
#   * Variation with modified encoding
# - PCM8_U_int     PCM 8-bit unsigned (interleave block)
#   * Variation with modified encoding
# - IMA            IMA ADPCM (mono/stereo)
#   * For some PC games, and rarely consoles
#   * Special interleave is multiple of 0x1, often +0x80
# - DVI_IMA        IMA ADPCM (DVI order)
#   * Variation with modified encoding
# - YAMAHA|AICA    Yamaha ADPCM (mono/stereo)
#   * For some Dreamcast games, and some arcade games
#   * Special interleave is multiple of 0x1
# - APPLE_IMA4     Apple Quicktime IMA ADPCM
#   * For some Mac/iOS games
# - MS_IMA         Microsoft IMA ADPCM
#   * For some PC games
#   * Interleave (frame size) varies, often multiple of 0x100 [required]
# - MSADPCM        Microsoft ADPCM (mono/stereo)
#   * For some PC games
#   * Interleave (frame size) varies, often multiple of 0x100 [required]
# - SDX2           Squareroot-delta-exact 8-bit DPCM
#   * For many 3DO games
# - MPEG           MPEG Audio Layer file (MP1/2/3)
#   * For some games (usually PC/PS3)
#   * May set skip_samples
# - ATRAC3         Sony ATRAC3
#   * For some PS2 and PS3 games
#   * Interleave (frame size) can be 0x60/0x98/0xC0 * channels [required]
#   * Should set skip_samples (more than 1024 but varies)
# - ATRAC3PLUS     Sony ATRAC3plus
#   * For many PSP games and rare PS3 games
#   * Interleave (frame size) can be: [required]
#     Mono: 0x0118|0178|0230|02E8
#     Stereo: 0x0118|0178|0230|02E8|03A8|0460|05D0|0748|0800
#   * Should set skip_samples (more than 2048 but varies)
# - XMA1           Microsoft XMA1
#   * For early X360 games
# - XMA2           Microsoft XMA2
#   * For later X360 games
# - FFMPEG         Any headered FFmpeg format
#   * For uncommon games
#   * May set skip_samples
# - AC3            AC3/SPDIF
#   * For few PS2 games
#   * Should set skip_samples (around 256 but varies)
# - PCFX           PC-FX ADPCM
#   * For many PC-FX games
#   * Interleave is multiple of 0x1, often +0x8000
#   * Sample rate may be ~31468/~15734/~10489/~7867
# - PCM4           PCM 4-bit signed
#   * For early consoles
# - PCM4_U         PCM 4-bit unsigned
#   * Variation with modified encoding
# - OKI16          OKI ADPCM with 16-bit output (not VOX/Dialogic 12-bit)
#   * For few PS2 games (Sweet Legacy, Hooligan)
# - AAC            Advanced Audio Coding (raw without .mp4)
#   * For some 3DS games and many iOS games
#   * Should set skip_samples (around 1024 but varies)
codec = (codec string)

# CODEC VARIATIONS [OPTIONAL, depends on codec]
# Accepted values:
# - NGC_DSP: 0=normal interleave, 1=byte interleave, 2=no interleave
# - ATRAC3: 0=autodetect joint stereo, 1=force joint stereo, 2=force normal stereo
# - XMA1|XMA2: 0=dual multichannel (2ch xN), 1=single multichannel (1ch xN)
# - XBOX: 0=standard (mono or stereo interleave), 1=force mono interleave mode
# - PCFX: 0=standard, 1='buggy encoder' mode, 2/3=same as 0/1 but with double volume
# - PCM4|PCM4_U: 0=low nibble first, 1=high nibble first
# - others: ignored
codec_mode = (value)

# (deprecated) VALUE MODIFIERS [OPTIONAL]
# Changes next read to: "(key) = (number)|(offset)|(field) */+- value_(op)"
# Deprecated, should use inline math instead.
# Set to 0 when done using, as it affects ANY value. Priority is as listed.
value_mul|value_* = (number)|(offset)|(field)
value_div|value_/ = (number)|(offset)|(field)
value_add|value_+ = (number)|(offset)|(field)
value_sub|value_- = (number)|(offset)|(field)

# INTERLEAVE / FRAME SIZE [REQUIRED/OPTIONAL, depends on codec]
# For mono/interleaved codecs it's the amount of data between channels,
# and while optional you'll often need to set it to get proper sound.
# For codecs with custom frame sizes (MSADPCM, MS-IMA, ATRAC3/plus)
# means frame size and it's required.
# Interleave 0 means "stereo mode" for codecs marked as "mono/stereo",
# and setting it will usually force mono-interleaved mode.
# Special values:
# - half_size: sets interleave as data_size / channels
interleave = (number)|(offset)|(field)|half_size

# INTERLEAVE IN THE LAST BLOCK [OPTIONAL]
# In some files with interleaved data the last block is smaller than interleave,
# so interleave must be smaller in the last block. This fixes decoding glitches
# for those files. Note that this doesn't affect files with padding data in the
# last block (as the interleave itself is constant).
# Special values:
# - auto: calculate based on channels, interleave and data_size/start_offset
interleave_last = (number)|auto

# ID VALUES [OPTIONAL]
# Validates that id_value (normally set as constant) matches value at id_offset.
# Can be redefined several times, it's checked whenever a new id_offset is found.
id_value = (number)|(offset)|(field)
id_offset = (number)|(offset)|(field)

# NUMBER OF CHANNELS [REQUIRED]
channels = (number)|(offset)|(field)

# MUSIC FREQUENCY [REQUIRED]
sample_rate = (number)|(offset)|(field)

# DATA START [OPTIONAL, defaults to 0]
start_offset = (number)|(offset)|(field)

# DATA SIZE [OPTIONAL]
# Special variable that can be used in sample values.
# Defaults to (file_size - start_offset), re-calculated when start_offset
# is set. With multiple subsongs or block_size are this it's recalculated as well.
# When padding is set it's also adjusted.
# If data_size is manually set it stays constant and won't be auto changed.
data_size = (number)|(offset)|(field)

# DATA PADDING [OPTIONAL, defaults to 0]
# Some aligned files have some extra padding that is meant to be skipped.
# This adjusts the padding in data_size, manually or auto-calculated.
# Special values (for PS-ADPCM):
# - auto: discards null frames
# - auto-empty: discards null and 'empty' frames (for games with weird padding)
padding_size = (number)|(offset)|(field)|auto|auto-empty

# SAMPLE MEANINGS [OPTIONAL, defaults to samples]
# Modifies the meaning of sample fields when set *before* them.
# Accepted values:
# - samples: exact sample
# - bytes: automatically converts bytes/offset to samples (applies after */+- modifiers)
# - blocks: same as bytes, but value is given in blocks/frames
#   Value is internally converted from blocks to bytes first: bytes = (value * interleave*channels)
# Some codecs can't convert bytes-to-samples at the moment: FFMPEG
# For XMA1/2 bytes does special parsing, with loop values being bit offsets within data.
sample_type = samples|bytes|blocks

# SAMPLE VALUES [REQUIRED (num_samples) / OPTIONAL (rest)]
# Special values:
# - data_size: automatically converts bytes-to-samples
num_samples         = (number)|(offset)|(field)|data_size
loop_start_sample   = (number)|(offset)|(field)
loop_end_sample     = (number)|(offset)|(field)|data_size

# LOOP SETTING [OPTIONAL]
# Force loop, on (>0) or off (0), as loop start/end may be defined but not used.
# Ignores values 0xFFFF/0xFFFFFFFF (-1) as they are often used to disable loops.
# By default it loops when loop_end_sample is defined and less than num_samples.
# Special values:
# - auto: tries to autodetect loop points for PS-ADPCM data using data loop flags.
loop_flag = (number)|(offset)|(field)|auto

# LOOP START/END MODIFIER [OPTIONAL]
# For XMA1/2 + sample_type=bytes it means loop subregion, if read after loop values.
# For other codecs its added to loop start/end, if read before loop values
# (a format may rarely have rough loop offset/bytes, then a loop adjust in samples).
loop_adjust = (number)|(offset)|(field)

# ENCODER DELAY [OPTIONAL]
# Beginning samples to skip, a.k.a. priming samples or encoder delay.
# Only a few codecs use them (ATRAC3/ATRAC3PLUS/XMA/FFMPEG/AC3/AAC), since
# they need to "warm up" with a number of skip_samples.
skip_samples = (number)|(offset)|(field)

# DSP DECODING COEFFICIENTS [REQUIRED for DSP]
# These coefs are a list of 8*2 16-bit values per channel, starting from offset.
coef_offset = (number)|(offset)|(field)
# Offset separation per channel, usually 0x20 (16 values * 2 bytes)
# Channel N coefs are read at coef_offset + coef_spacing * N
coef_spacing = (number)|(offset)|(field)
# Format, usually BE; with (offset): 0=LE, >0=BE
coef_endianness = BE|LE|(offset)|(field)
# Split/normal coefs [NOT IMPLEMENTED YET]
#coef_mode = (number)|(offset)
# Inline coefs, used over those found in coef_offset. Format is a long string
# of bytes (optionally space-separated). It's interpreted like normal coefs
# (byte array), meaning you still need to set coef_spacing and coef_endianness.
# coef_table = 0x1E02DE01 3C0C0EFA ...
coef_table = (string)

# HEADER/BODY SETTINGS [OPTIONAL]
# Changes internal header/body representation to external files.
#
# TXTH commands are done on a "header", and decoding on "body".
# When loading an unsupported file it becomes the "base" file
# that loads the .txth, and is both header and body.
#
# You can alter those, mainly for files that split header and body
# in separate files (load base file and txth sets header on another file).
# It's also possible to load the .txth directly with a set body, as a sort of
# "reverse TXTH" (useful with bigfiles, as you could have one .txth per song).
#
# Allowed values:
# - (filename): open any file, subdirs also work (dir/filename)
# - *.(extension): opens with same name as the "base" file plus another extension
# - null: unloads file and goes back to defaults (body/header = base file).
header_file = (filename)|*.(extension)|null
body_file = (filename)|*.(extension)|null

# SUBSONGS [OPTIONAL]
# Sets the number of subsongs in the file, adjusting reads per subsong N:
# "value = @(offset) + subsong_offset*N". (number) values aren't adjusted
# as they are seen as constants.
# Mainly for bigfiles with consecutive headers per subsong, set subsong_offset
# to 0 when done as it affects any reads.
# The current subsong number is handled externally by plugins or TXTP.
subsong_count = (number)|(offset)|(field)
subsong_offset = (number)|(offset)|(field)

# NAMES [OPTIONAL]
# Sets the name of the stream, most useful when used with subsongs.
# TXTH will read a string at name_offset, with name_size characters.
# name_size defaults to 0, which reads until null-terminator or a
# non-ascii character.
# name_offset can be a (number) value, but being an offset it's also
# adjusted by subsong_offset.
name_offset = (number)|(offset)|(field)
name_size = (number)|(offset)|(field)

# SUBFILES [OPTIONAL]
# Tells TXTH to parse a full file (ex. .ogg) at subfile_offset, with size
# of subfile_size (defaults to file size - subfile_offset if not set).
# Internal subfile extension can be changed to subfile_extension if needed,
# as vgmstream won't accept unknown extensions (for example if your file 
# uses .pogg you may need to set subfile_extension = ogg).
# Setting any of those three will trigger this mode (it's ok to set offset 0).
# Once triggered most fields are ignored, but not all, explained below.
subfile_offset = (number)|(offset)|(field)
subfile_size = (number)|(offset)|(field)
subfile_extension = (string)
```

## Usages

### Temporary values
Most commands are evaluated and calculated immediatedly, every time they are found. 
This is by design, as it can be used to adjust and trick for certain calculations.

It makes TXTHs a bit harder to follow, as they are order dependant, but otherwise it's hard to accomplish some things or others become ambiguous.


For example, normally you are given a data_size in bytes, that can be used to calculate num_samples for all channels.
```
channels = 2
sample_type = bytes
num_samples = @0x10     #calculated from data_size
```

But sometimes this size is for a single channel only (even though the file may be stereo). You can set temporally change the channel number to force a correct calculation.
```
channels = 1            #not the actual number of channels
sample_type = bytes
num_samples = @0x10     #calculated from channel_size
channels = 2            #change once calculations are done
```
You can also use:
```
channels = 2
sample_type = bytes
num_samples = @0x10 * channels  # resulting bytes is transformed to samples
```

Do note when using special values/strings like `data_size` in `num_samples` and `loop_end_samples` they must be alone to trigger.
```
data_size = @0x100
num_samples = data_size * 2 # doesn't tranform bytes-to-samples (do it before? after?)
```
```
data_size = @0x100 * 2
num_samples = data_size     # ok
```
Also beware of order:
```
start_offset = 0x200        # recalculated data_size
num_samples = data_size     # transforms bytes-to-samples
data_size = @0x100          # useless as num_samples is already transformed
```


### Redefining values
Some commands alter the function of all next commands and can be redefined as needed:
```
samples_type = bytes
num_samples = @0x10

samples_type = sample
loop_end_sample = @0x14
```

### External files
When setting external files all commands are done on the "header" file, but with some creativity you can read in multiple files.
```
body_file = bgm01.bdy
header_file = bgm01.hdr
channels = @0x10        #base info in bgm01.hdr
header_file = bgm01.bdy
coef_offset = 0x00      #DSP coefs in bgm01.bdy
```
Note that DSP coefs are special in that aren't read immediately, and will use *last* header_file set.


### Resetting values
Values may need to be reset (to 0 or other sensible value) when done. Subsong example:
```
subsong_count = 5
subsong_offset = 0x20   # there are 5 subsong headers, 0x20 each
channel_count = @0x10   # reads channels at 0x10+0x20*subsong
# 1st subsong: 0x10+0x20*0: 0x10
# 2nd subsong: 0x10+0x20*1: 0x30
# 2nd subsong: 0x10+0x20*2: 0x50
# ...
start_offset = @0x14    # reads offset within data at 0x14+0x20*subsong

subsong_offset = 0      # reset value
sample_rate = 0x04      # sample rate is the same for all subsongs
# Nth subsong ch: 0x04+0x00*N: 0x08
```

### Math
Sometimes header values are in "sectors" or similar concepts (typical in DVD games), and need to be adjusted to a real value.
```
sample_type   = bytes
start_offset  = @0x10 * 0x800    # 0x15 * DVD sector size, for example
```

You can also use certain fields' values:
```
num_samples = @0x10 * channels  # byte-to-samples of channel_size
```
`data_size` is a special value for `num_samples` and `loop_end_sample` and will always convert as bytes-to-samples, though.


Priority is left-to-right. Do add brackets though, they are accounted for and if they are implemented in the future your .txth *will* break with impunity.
```
# normal priority
data_size = @0x10 * 0x800 + 0x800
# also works
data_size = (@0x10 + 1) * 0x800
# same as above but don't do this
# (may become @0x10 + (1 * 0x800) in the future
data_size = @0x10 + 1 * 0x800
# doesn't work at the moment, so reorder as (1 * 0x800) + @0x10
data_size = @0x10 + (1 * 0x800)
# fails, wrong bracket count
data_size = (@0x10 + 1 * 0x800
# fails, wrong bracket count
data_size = )@0x10 + 1 * 0x800
```

If a TXTH needs too many calculations it may be better to implement directly in vgmstream though, consider reporting.


### Modifiers
Remnant of simpler math (priority is fixed to */+-), shouldn't be needed anymore.

```
value_multiply = 0x800
start_offset   = @0x10
value_multiply = 0
```

```
value_add = 1
channels = @0x08
value_add = 0

value_multiply = channels
sample_type = bytes
num_samples = @0x10
value_multiply = 0
```

```
value_add       = 0x10
value_mul       = 0x800
start_offset    = @0x10
```

### Subfiles
Sometimes a file is just a wrapper for another common format. In those cases you can tell TXTH to just play the internal format:
```
subfile_offset = 0x20   # tell TXTH to parse a full file (ex. .ogg) at this offset
subfile_size = @0x10    # defaults to (file size - subfile_offset) if not set
subfile_extension = ogg # may be ommited if subfile extension is the same

# many fields are ignored
codec = PCM16LE
interleave = 0x1000
channels = 2

# a few fields are applied
sample_rate = @0x08
num_samples = @0x10
loop_start_sample = @0x14
loop_end_sample = @0x18
```
Most fields can't be changed after parsing since doesn't make much sense technically, as the parsed subfile should supply them.
