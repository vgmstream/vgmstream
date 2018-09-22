# TXTH FORMAT

TXTH is a simple text file that uses text commands to simulate a header for files unsupported by vgmstream, mainly headerless audio.

When an unsupported file is loaded, vgmstream tries to find a TXTH header in the same dir, in this order:
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

# comments start with #, can be inline
# The file is made lines of "key = value" describing a header.
# Spaces are optional: key=value, key=   value, and so on are all ok.
# The parser is fairly simple and may be buggy or unexpected in some cases.
# The order of keys is variable but some things won't work if others aren't defined
# (ex. bytes-to-samples may not work without channels or interleave).

# Common values:
# - (number): constant number in dec/hex.
#   Examples: 44100, 40, 0x40 (dec=64)
# - (offset): format is @(number)[:LE|BE][$1|2|3|4]
#   * @(number): value at offset (required)
#   * :LE|BE: value is little/big endian (optional, defaults to LE)
#   * $1|2|3|4: value has size of 8/16/24/32 bit (optional, defaults to 4)
#   Examples: @0x10:BE$2 (get big endian 16b value at 0x10)
# - {string}: special values for certain keys, described below

# Codec used to encode the data [REQUIRED]
# Accepted codec strings:
# - PSX            PlayStation ADPCM
# - XBOX           Xbox IMA ADPCM
# - NGC_DTK        Nintendo ADP/DTK ADPCM
# - PCM16BE        PCM RAW 16bit big endian
# - PCM16LE        PCM RAW 16bit little endian
# - PCM8           PCM RAW 8bit
# - SDX2           Squareroot-delta-exact 8-bit DPCM (3DO games)
# - DVI_IMA        DVI IMA ADPCM
# - MPEG           MPEG Audio Layer File (MP1/2/3)
# - IMA            IMA ADPCM
# - AICA           Yamaha AICA ADPCM (Dreamcast)
# - MSADPCM        Microsoft ADPCM (Windows)
# - NGC_DSP        Nintendo GameCube ADPCM
# - PCM8_U_int     PCM RAW 8bit unsigned (interleaved)
# - PSX_bf         PlayStation ADPCM with bad flags
# - MS_IMA         Microsoft IMA ADPCM
# - PCM8_U         PCM RAW 8bit unsigned
# - APPLE_IMA4     Apple Quicktime IMA ADPCM
# - ATRAC3         raw ATRAC3
# - ATRAC3PLUS     raw ATRAC3PLUS
# - XMA1           raw XMA1
# - XMA2           raw XMA2
# - FFMPEG         any headered FFmpeg format
codec = (codec string)
 
# Varies with codec:
# - NGC_DSP: 0=normal interleave, 1=byte interleave, 2=no interleave
# - ATRAC3: 0=autodetect joint stereo, 1=force joint stereo, 2=force normal stereo
# - XMA1|XMA2: 0=dual multichannel (2ch xN), 1=single multichannel (1ch xN)
# - XBOX: 0=standard (mono or stereo interleave), 1=force mono interleave mode
# - others: ignored
codec_mode = (number)
 
# Interleave or block size [REQUIRED/OPTIONAL, depends on codec]
# For mono/interleaved codecs it's the amount of data between channels.
# For codecs with variable-sized frames (MSADPCM, MS-IMA, ATRAC3/plus)
# it's the block size (size of a single frame).
# Interleave 0 means "stereo mode" for some codecs (IMA, AICA, etc).
interleave = (number)|(offset)

# Validate that id_value matches value at id_offset [OPTIONAL]
# Can be redefined several times, it's checked whenever a new id_offset is found.
id_value = (number)|(offset)
id_offset = (number)|(offset)
 
# Number of channels [REQUIRED]
channels = (number)|(offset)
 
# Music frequency in hz [REQUIRED]
sample_rate = (number)|(offset)
 
# Data start [OPTIONAL, default to 0]
start_offset = (number)|(offset)
 
# Variable that can be used in sample values [OPTIONAL]
# Defaults to (file_size - start_offset), re-calculated when start_offset is set.
data_size = (number)|(offset)
 
 
# Modifies the meaning of sample fields when set *before* them [OPTIONAL, defaults to samples]
# - samples: exact sample
# - bytes: automatically converts bytes/offset to samples
# - blocks: same as bytes, but value is given in blocks/frames
#   Value is internally converted from blocks to bytes first: bytes = (value * interleave*channels)
# It's possible to re-define values multiple times:
#  * samples_type=bytes ... num_samples=@0x10
#  * samples_type=sample ... loop_end_sample=@0x14
# Sometimes "bytes" values are given for a single channel only. In that case you can temporally set 1 channel
#  * channels=1 ... sample_type=bytes ... num_samples=@0x10 ... channels=2
# Some codecs can't convert bytes-to-samples at the moment: MPEG/FFMPEG
# For XMA1/2 bytes does special parsing, with loop values being bit offsets within data.
sample_type = samples|bytes|blocks
 
# Various sample values [REQUIRED (num_samples) / OPTIONAL (rest)]
num_samples         = (number)|(offset)|data_size
loop_start_sample   = (number)|(offset)
loop_end_sample     = (number)|(offset)|data_size

# For XMA1/2 + sample_type=bytes it means loop subregion, if read after loop values.
# For other codecs its added to loop start/end, if read before loop values
# (rarely a format may have rough loop offset/bytes, then a loop adjust in samples).
loop_adjust = (number)|(offset)

# Force loop, on (>0) or off (0), as loop start/end may be defined but not used [OPTIONAL]
# By default it loops when loop_end_sample is defined
# auto tries to autodetect loop points for PS-ADPCM data, which may include loop flags.
loop_flag = (number)|(offset)|auto

# beginning samples to skip (encoder delay), for codecs that need them (ATRAC3/XMA/etc)
skip_samples = (number)|(offset)


# DSP coefficients [REQUIRED for NGC_DSP]
# Coefs start
coef_offset = (number)|(offset)
# offset separation per channel, usually 0x20
# - Example: channel N coefs are read at coef_offset + coef_spacing * N
coef_spacing = (number)|(offset)
# Format, usually BE; with (offset): 0=LE, >0=BE
coef_endianness = BE|LE|(offset)
# Split/normal coefs [NOT IMPLEMENTED YET]
#coef_mode = (number)|(offset)

```
