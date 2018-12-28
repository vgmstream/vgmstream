# TXTH FORMAT

TXTH is a simple text file that uses text commands to simulate a header for files unsupported by vgmstream, mainly headerless audio.

When an unsupported file is loaded (for instance "bgm01.snd"), vgmstream tries to find a TXTH header in the same dir, in this order:
- (filename.ext).txth
- .(ext).txth
- .txth

If found and parsed correctly (the TXTH may be rejected if incorrect commands are found) vgmstream will try to play the file as described. Extension must be accepted/added to vgmstream (plugins like foobar2000 only load extensions from a whitelist in formats.c), or one could rename to any supported extension (like .vgmstream), or leave the file extensionless.


## Example of a TXTH file
For an unsupported bgm01.vag this would be a simple TXTH for it:
```
id_value = 0x534E4420       #test that file starts with "SND "
id_offset = @0x00:BE        #test is done at offset 0, big endian value
codec = PSX
sample_rate = @0x10$2       #get sample rate at offset 0x10, 16 bit value
channels = @0x14            #get number of channels at offset 14
interleave = 0x1000         #fixed value
start_offset = 0x100
num_samples = data_size     #find automatically number of samples in the file
loop_flag = auto
```
A text file with the above commands must be saved as ".vag.txth" or ".txth", notice it starts with a "." (dot). On Windows files starting with a dot can be created by appending a dot at the end: ".txth."


## Available commands

```
######################################################

# The file is made of lines like "key = value" commands describing a header.
# Comments start with #, can be inlined. keys and commands are all case sensitive.
# Spaces are optional: key=value, key  =   value, and so on are all ok.
# The parser is fairly simple and may be buggy or unexpected in some cases.
# The order of keys is variable but some things won't work if others aren't defined
# (ex. bytes-to-samples may not work without channels or interleave).

# Common values:
# - (number): constant number in dec/hex, unsigned (no +10 or -10).
#   Examples: 44100, 40, 0x40 (decimal=64)
# - (offset): format is @(number)[:LE|BE][$1|2|3|4]
#   * @(number): value at offset (required)
#   * :LE|BE: value is little/big endian (optional, defaults to LE)
#   * $1|2|3|4: value has size of 8/16/24/32 bit (optional, defaults to 4)
#   Examples: @0x10:BE$2 (get big endian 16b value at 0x10)
# - (field): uses current value of a field. Accepted strings:
#   - interleave, channels, sample_rate
#   - start_offset, data_size
#   - num_samples, loop_start_sample, loop_end_sample
#   - subsong_count, subsong_offset
# - {string}: other special values for certain keys, described below

# Codec used to encode the data [REQUIRED]
# Accepted codec strings:
# - PSX            PlayStation ADPCM
# - XBOX           Xbox IMA ADPCM
# - NGC_DTK|DTK    Nintendo ADP/DTK ADPCM
# - PCM16BE        PCM 16-bit big endian
# - PCM16LE        PCM 16-bit little endian
# - PCM8           PCM 8-bit
# - SDX2           Squareroot-delta-exact 8-bit DPCM (3DO games)
# - DVI_IMA        DVI IMA ADPCM
# - MPEG           MPEG Audio Layer file (MP1/2/3)
# - IMA            IMA ADPCM
# - AICA           Yamaha AICA ADPCM (Dreamcast)
# - MSADPCM        Microsoft ADPCM (Windows)
# - NGC_DSP|DSP    Nintendo GameCube ADPCM
# - PCM8_U_int     PCM RAW 8bit unsigned (interleaved)
# - PSX_bf         PlayStation ADPCM with bad flags
# - MS_IMA         Microsoft IMA ADPCM
# - PCM8_U         PCM 8-bit unsigned
# - APPLE_IMA4     Apple Quicktime IMA ADPCM
# - ATRAC3         Sony ATRAC3
# - ATRAC3PLUS     Sony ATRAC3plus
# - XMA1           Microsoft XMA1
# - XMA2           Microsoft XMA2
# - FFMPEG         Any headered FFmpeg format
# - AC3            AC3/SPDIF
# - PCFX           PC-FX ADPCM
codec = (codec string)

# Codec variations [OPTIONAL, depends on codec]
# - NGC_DSP: 0=normal interleave, 1=byte interleave, 2=no interleave
# - ATRAC3: 0=autodetect joint stereo, 1=force joint stereo, 2=force normal stereo
# - XMA1|XMA2: 0=dual multichannel (2ch xN), 1=single multichannel (1ch xN)
# - XBOX: 0=standard (mono or stereo interleave), 1=force mono interleave mode
# - PCFX: 0=standard, 1='buggy encoder' mode, 2/3=same as 0/1 but with double volume
# - others: ignored
codec_mode = (number)

# Modifies next values [OPTIONAL]
# Values will be "(key) = (number)|(offset)|(field) */+- value_(op)"
# Useful when a size or such needs adjustments (like given in 0x800 sectors).
# Set to 0 when done using, as it affects ANY value. Priority is as listed.
value_mul|value_* = (number)|(offset)|(field)
value_div|value_/ = (number)|(offset)|(field)
value_add|value_+ = (number)|(offset)|(field)
value_sub|value_- = (number)|(offset)|(field)

# Interleave or block size [REQUIRED/OPTIONAL, depends on codec]
# - half_size: sets interleave as data_size / channels
# For mono/interleaved codecs it's the amount of data between channels,
# and for codecs with variable-sized frames (MSADPCM, MS-IMA, ATRAC3/plus)
# means block size (size of a single frame).
# Interleave 0 means "stereo mode" for some codecs (IMA, AICA, etc).
interleave = (number)|(offset)|(field)|half_size

# Validate that id_value matches value at id_offset [OPTIONAL]
# Can be redefined several times, it's checked whenever a new id_offset is found.
id_value = (number)|(offset)|(field)
id_offset = (number)|(offset)|(field)

# Number of channels [REQUIRED]
channels = (number)|(offset)|(field)

# Music frequency in hz [REQUIRED]
sample_rate = (number)|(offset)|(field)

# Data start [OPTIONAL, default to 0]
start_offset = (number)|(offset)|(field)

# Variable that can be used in sample values [OPTIONAL]
# Defaults to (file_size - start_offset), re-calculated when start_offset
# is set (won't recalculate if data_size is set then start_offset changes).
data_size = (number)|(offset)|(field)

# Modifies the meaning of sample fields when set *before* them [OPTIONAL, defaults to samples]
# - samples: exact sample
# - bytes: automatically converts bytes/offset to samples (applies after */+- modifiers)
# - blocks: same as bytes, but value is given in blocks/frames
#   Value is internally converted from blocks to bytes first: bytes = (value * interleave*channels)
# Some codecs can't convert bytes-to-samples at the moment: MPEG/FFMPEG
# For XMA1/2 bytes does special parsing, with loop values being bit offsets within data.
sample_type = samples|bytes|blocks
 
# Various sample values [REQUIRED (num_samples) / OPTIONAL (rest)]
# - data_size: automatically converts bytes-to-samples
num_samples         = (number)|(offset)|(field)|data_size
loop_start_sample   = (number)|(offset)|(field)
loop_end_sample     = (number)|(offset)|(field)|data_size

# Force loop, on (>0) or off (0), as loop start/end may be defined but not used [OPTIONAL]
# - auto: tries to autodetect loop points for PS-ADPCM data, which may include loop flags.
# Ignores values 0xFFFF/0xFFFFFFFF (-1) as they are often used to disable loops.
# By default it loops when loop_end_sample is defined and less than num_samples.
loop_flag = (number)|(offset)|(field)|auto

# Loop start/end modifier [OPTIONAL]
# For XMA1/2 + sample_type=bytes it means loop subregion, if read after loop values.
# For other codecs its added to loop start/end, if read before loop values
# (a format may rarely have rough loop offset/bytes, then a loop adjust in samples).
loop_adjust = (number)|(offset)|(field)

# Beginning samples to skip (encoder delay) [OPTIONAL]
# Only some codecs use them (ATRAC3/ATRAC3PLUS/XMA/FFMPEG/AC3)
skip_samples = (number)|(offset)|(field)


# DSP decoding coefficients [REQUIRED for NGC_DSP]
# These coefs are a list of 8*2 16-bit values per channel, starting from offset.
coef_offset = (number)|(offset)|(field)
# Offset separation per channel, usually 0x20 (16 values * 2 bytes)
# Channel N coefs are read at coef_offset + coef_spacing * N
coef_spacing = (number)|(offset)|(field)
# Format, usually BE; with (offset): 0=LE, >0=BE
coef_endianness = BE|LE|(offset)|(field)
# Split/normal coefs [NOT IMPLEMENTED YET]
#coef_mode = (number)|(offset)


# Change header/body to external files [OPTIONAL]
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

# Subsongs [OPTIONAL]
# Sets the number of subsongs in the file, adjusting reads per subsong N:
# "value = @(offset) + subsong_offset*N". (number) values aren't adjusted
# as they are seen as constants.
# Mainly for bigfiles with consecutive headers per subsong, set subsong_offset
# to 0 when done as it affects any reads.
# The current subsong number is handled externally by plugins or TXTP.
subsong_count = (number)|(offset)|(field)
subsong_offset = (number)|(offset)|(field)

# Names [OPTIONAL]
# Sets the name of the stream, most useful when used with subsongs.
# TXTH will read a string at name_offset, with name_size characters.
# name_size defaults to 0, which reads until null-terminator or a
# non-ascii character.
# name_offset can be a (number) value, but being an offset it's also
# adjusted by subsong_offset.
name_offset = (number)|(offset)|(field)
name_size = (number)|(offset)|(field)
```

## Usages

### Temporary values
Most commands are evaluated and calculated immediatedly, every time they are found. This is by design, as it can be used to adjust and trick for certain calculations.

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
This can be done with value modifiers too (see below).

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

### Modifiers
Sometimes header values are in "sectors" or similar concepts (typical in DVD games), and need to be adjusted to a real value.
```
value_multiply = 0x800    # offsets are in DVD sector size
start_offset   = @0x10    # 0x15*0x800, for example
value_multiply = 0        # next values don't need to be multiplied
start_offset   = @0x14
```

You can also use certain fields' values:
```
value_add = 1
channels = @0x08                # may be 1 + 1 = 2
value_add = 0

value_multiply = channels       # now set to 2
sample_type = bytes
num_samples = @0x10             # channel_size * channels
value_multiply = 0
```
num_samples and loop_end_sample will always convert "data_size" field as bytes-to-samples though.

Priority is fixed to */+-:
```
value_add       = 0x10
value_mul       = 0x800
start_offset    = @0x10         # (0x15*0x800) + 0x10 = 0xA810
```

But with some creativity you can do fairly involved stuff:
```
value_add       = 0x10
start_offset    = @0x10         # (0x15+0x10) = 0x25
value_add       = 0

value_mul       = 0x800
start_offset    = start_offset  # (0x25*0x800) = 0x12800
value_mul       = 0
```

If a TXTH needs too many complex calculations it may be better to implement directly in vgmstream though.
