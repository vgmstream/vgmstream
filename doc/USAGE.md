# Usage

## Needed extra files
On Windows support for some codecs (Ogg Vorbis, MPEG audio, etc.) is done with external
libraries, so you will need to put certain DLL files together.

In the case of components like foobar2000 they are all bundled for convenience,
while other components include them but must be installed manually. You can also
get them here: https://github.com/vgmstream/vgmstream/tree/master/ext_libs
or compile them manually, even (see tech docs).

Put the following files somewhere Windows can find them:
- `libvorbis.dll`
- `libmpg123-0.dll`
- `libg719_decode.dll`
- `avcodec-vgmstream-58.dll`
- `avformat-vgmstream-58.dll`
- `avutil-vgmstream-56.dll`
- `swresample-vgmstream-3.dll`
- `libatrac9.dll`
- `libcelt-0061.dll`
- `libcelt-0110.dll`
- `libspeex.dll`

For command line (`test.exe`) and XMPlay this means in the directory with the main `.exe`,
or possibly a directory in the PATH variable.

For Winamp, the above `.dll` also go near main `winamp.exe`, but note that `in_vgmstream.dll`
plugin itself goes in `Plugins`.

On other OSs like Linux/Mac, libs need to be installed before compiling, then should be used
automatically, though not all may enabled at the moment due to build scripts issues.


## Components

### test.exe/vgmstream-cli (command line decoder)
*Windows*: unzip `test.exe` and follow the above instructions for installing needed extra files.
`test.exe` is used for historical reasons, but you can call it `vgmstream-cli.exe`, anyway.

*Others*: build instructions can be found in the [BUILD.md](BUILD.md) document (can be compiled
with CMake/Make/autotools).

Converts playable files to `.wav`. Typical usage would be:
- `test.exe -o happy.wav happy.adx` to decode `happy.adx` to `happy.wav`.

If command-line isn't your thing you can simply drag and drop one or multiple
files to the executable to decode them as `(filename.ext).wav`.

There are multiple options that alter how the file is converted, for example:
- `test.exe -m file.adx`: print info but don't decode
- `test.exe -i -o file_noloop.wav file.hca`: convert without looping
- `test.exe -s 2 -F file.fsb`: write 2nd subsong + ending after 2.0 loops
- `test.exe -l 3.0 -f 5.0 -d 3.0 file.wem`: 3 loops, 3s delay, 5s fade
- `test.exe -o bgm_?f.wav file1.adx file2.adx`: convert multiple files to `bgm_(name).wav`

Available commands are printed when run with no flags. Note that you can also
achieve similar results for other plugins using TXTP, described later.

Output filename in `-o` may use wildcards:
- `?s`: sets current subsong (or 0 if format doesn't have subsongs)
- `?0Ns`: same, but left pads subsong with up to `N` zeroes
- `?n`: internal stream name, or input filename if format doesn't have name
- `?f`: input filename

For example `test.exe -s 2 -o ?04s_?n.wav file.fsb` could generate `0002_song1.wav`.
Default output filename is `?f.wav`, or `?f#?s.wav` if you set subsongs (`-s/-S`).


### in_vgmstream (Winamp plugin)
*Windows*: drop the `in_vgmstream.dll` in your Winamp Plugins directory,
and follow the above instructions for installing needed extra files.

*Others*: may be possible to use through *Wine*

Once installed, supported files should be playable. There is a simple config
menu to tweak some options too. If the *Preferences... > Plug-ins > Input* shows
vgmstream as *"NOT LOADED"* that means extra DLL files aren't in the correct
place.


### foo_input_vgmstream (foobar2000 plugin)
*Windows*: every file should be installed automatically when opening the `.fb2k-component`
bundle

*Others*: may be possible to use through *Wine*

A known quirk is that when loop options or tags change, playlist info won't refresh
automatically. You need to manually refresh it by selecting songs and doing
**shift + right click > Tagging > Reload info from file(s)**.


### xmp-vgmstream (XMPlay plugin)
*Windows*: drop the `xmp-vgmstream.dll` in your XMPlay plugins directory,
and follow the above instructions for installing the other files needed.

*Others*: may be possible to use through *Wine*

Note that this has less features compared to *in_vgmstream* and has no config.
Since XMPlay supports Winamp plugins you may also use `in_vgmstream.dll` instead.

Because the XMPlay MP3 decoder incorrectly tries to play some vgmstream extensions,
you need to manually fix it by going to **options > plugins > input > vgmstream**
and in the "priority filetypes" put: `ahx,asf,awc,ckd,fsb,genh,lwav,msf,p3d,rak,scd,txth,xvag`

XMPlay cannot support vgmstream's type of mixed subsongs due to player limitations
(with neither *xmp-vgmstream* nor *in_vgmstream* plugins), try using *TXTP* instead (explained below).


### Audacious plugin
*Windows*: not possible at the moment.

*Others*: needs to be manually built. Instructions can be found in [BUILD.md](BUILD.md)
document in vgmstream's source code (can be done with CMake or autotools).


### vgmstream123 (command line player)
*Windows/Linux*: needs to be manually built. Instructions can be found in the
*[BUILD.md](BUILD.md)* document. On Windows it needs `libao.dll` and appropriate includes.

Usage: `vgmstream123 [options] INFILE ...`

The program is meant to be a simple stand-alone player, supporting playback of
vgmstream files through libao. On Linux, files compressed with gzip/bzip2/xz also
work, as identified by a `.gz/.bz2/.xz` extension. The file will be decompressed
to a temp dir using the respective utility program (which must be installed
and accessible) and then loaded.

It also supports playlists, and will recognize a special extended-M3U tag
specific to vgmstream of the following form:
```
#EXT-X-VGMSTREAM:LOOPCOUNT=2,FADETIME=10.0,FADEDELAY=0.0,STREAMINDEX=0
```
(Any subset of these four parameters may appear in the line, in any order)

When this "magic comment" appears in the playlist before a vgmstream-compatible
file, the given parameters will be applied to the playback of said file. This makes
it feasible to play vgmstream files directly instead of needing to make "arranged"
WAV/MP3 conversions ahead of time.

The tag syntax follows the conventions established in Apple's HTTP Live Streaming
standard, whose docs discuss extending M3U with arbitrary tags.


## Special cases
vgmstream aims to support most audio formats as-is, but some files require extra
handling.

### Subsongs
Certain container formats have multiple audio files, usually called "subsongs", often
not meant to be extracted (no simple separation from container).

By default vgmstream plays first subsong and reports total subsongs, if the format
is able to contain them. Easiest to use would be the *foobar/winamp/Audacious*
plugins, that are able to "unpack" those subsongs automatically into the playlist.

With CLI tools, you can select a subsong using the `-s` flag followed by a number,
for example: `text.exe -s 5 file.bank` or `vgmstream123 -s 5 file.bank`.

Using *vgmstream-cli* you can convert multiple subsongs at once using the `-S` flag.
**WARNING, MAY TAKE A LOT OF SPACE!** Some files have been observed to contain +20000
subsongs, so don't use this lightly. Remember to set an output name (`-o`) with subsong
wildcards (or leave it alone for the defaults).
- `test.exe -s 1 -S 100 file.bank`: writes from subsong 1 to subsong 100
- `test.exe -s 101 -S 0 file.bank`: writes from subsong 101 to max subsong (automatically changes 0 to max)
- `test.exe -S 0 file.bank`: writes from subsong 1 to max subsong
- `test.exe -s 1 -S 5 -o bgm.wav file.bank`: writes 5 subsongs, but all overwrite the same file = wrong.
- `test.exe -s 1 -S 5 -o bgm_?02s.wav file.bank`: writes 5 subsongs, each named differently = correct.

For other players without support, or to play only a few choice subsongs, you
can create multiple `.txtp` (explained later) to select one, like `bgm.sxd#10.txtp`
(plays subsong 10 in `bgm.sxd`).

You can use this python script to autogenerate one `.txtp` per subsong:
https://github.com/vgmstream/vgmstream/tree/master/cli/tools/txtp_maker.py
Put in the same dir as test.exe/vgmstream_cli, then to drag-and-drop files with
subsongs to `txtp_maker.py` (it has CLI options to control output too).

### Common and unknown extensions
A few extensions that vgmstream supports clash with common ones. Since players
like foobar or Winamp don't react well to that, they may be renamed to these
"designated fake extensions" to make them playable through vgmstream.
- `.aac` to `.laac` (tri-Ace games)
- `.ac3` to `.lac3` (standard AC3)
- `.aif` to `.laif` (standard Mac AIF, Asobo AIF, Ogg)
- `.aiff/aifc` to `.laiff/laifc` (standard Mac AIF)
- `.asf` to `.lasf` (EA games, Argonaut ASF)
- `.bin` to `.lbin` (various formats)
- `.flac` to `.lflac` (standard FLAC)
- `.mp2` to `.lmp2` (standard MP2)
- `.mp3` to `.lmp3` (standard MP3)
- `.mp4` to `.lmp4` (standard M4A)
- `.mpc` to `.lmpc` (standard MPC)
- `.ogg` to `.logg` (standard OGG)
- `.opus` to `.lopus` (standard OPUS or Switch OPUS)
- `.stm` to `.lstm` (Rockstar STM)
- `.wav` to `.lwav` (standard WAV, various formats)
- `.wma` to `.lwma` (standard WMA)
- `.(any)` to `.vgmstream` (FFmpeg formats or TXTH)

Command line tools don't have this restriction and will accept the original
filename.

The main advantage of renaming here is that vgmstream may use the file's internal
loop info, or apply subtle fixes, but is also limited in some ways (like ignoring
standard tags). `.vgmstream` is a catch-all extension that may work as a last resort
to make a file playable.

Some plugins have options that allow "*common extensions*" to be played, making any
renaming unnecessary. You may need to adjust plugin priority in player's options
first. Note that vgmstream also accepts certain extension-less files as-is too.

Similarly, vgmstream has a curated list of known extensions, that plugins may take
into account and ignore unknowns. Through *TXTH* you can make unknown files playable,
but you also need to either rename or set plugin options to allow "*unknown extensions*"
(or, preferably, report this new extension so it can be added to the known list).

It's also possible to make a .txtp file that opens files with those common/unknown
extensions as a way to force them into vgmstream without renaming.

#### Related issues
Also be aware that other plugins (not vgmstream) can tell the player they handle
some extension, then not actually play it. This makes the file unplayable as
vgmstream doesn't even get the chance to parse it, so you may need to disable
the offending plugin or rename the file to the fake extension shown above (for
example this may happen with `.asf` in foobar2000/Winamp, may be fixed in newer
versions).

When extracting from a bigfile, sometimes internal files don't have a proper
extension. Those should be renamed to its correct one when possible, as the
extractor program may guess wrong (like `.wav` instead of `.at3` or `.wem`).
If there is no known extension, usually the header id/magic string may be used instead.

#### Windows 10 folder bugs
Windows 10's *Web Media Extensions* is a pre-installed package seems to read metadata
from files like `.ogg`, `.opus`, `.flac` and so on when opening a folder. However
it tends to noticeably slow down opening folders, also seems to crash and leave files
unusable when reading unsupported formats like Switch Opus (rather than Ogg Opus).

Renaming extensions should prevent those issues, or just uninstall those *Web
Media Extension* better experience anyway.

#### Fallout SFX .ACM
Due to technical limitations, to play Fallout 1 SFX you need to rename them from
`.acm` to `.wavc` (forces mono).

### Demuxed videos
vgmstream also supports audio from videos, but usually must be demuxed (extracted
without modification) first, since vgmstream doesn't attempt to support most of them
(it does support a few video formats as-is though).

The easiest way to do this is using *VGMToolBox*'s "Video Demultiplexer" option
for common game video formats (`.bik`, `.vp6`, `.pss`, `.pam`, `.pmf`, `.usm`, `.xmv`, etc).

For standard videos formats (`.avi`, `.mp4`, `.webm`, `.m2v`, `.ogv`, etc) not supported
by VGMToolBox, FFmpeg binary may work:
- `ffmpeg.exe -i (input file) -vn -acodec copy (output file)`
Output extension may need to be adjusted to some appropriate audio file depending
on the audio codec used. `ffprobe.exe` can list this codec, though the correct audio
extension depends on the video itself (like `.avi` to `.wav/mp2/mp3` or `.ogv` to `.ogg`).

Some games use custom video formats, demuxer scripts in `.bms` format may be found
on the internet.

### Companion files
Some formats have companion files with external info, that should be left together:
- `.mus`: playlist with `.acm`
- `.ogg.sli` or `.sli`: loop info for `.ogg`
- `.ogg.sfl` : loop info for `.ogg`
- `.opus.sli`: loop info for `.opus`
- `.pos`: loop info for .wav
- `.acb`: names for `.awb`
- `.xsb`: names for `.xwb`

Similarly some formats split header+body data in separate files, examples:
- `.abk`+`.ast`
- `.bnm`+`.apm/wav`
- `.ktsl2asbin`+`.ktsl2stbin`
- `.mih`+`.mib`
- `.mpf`+`.mus`
- `.pk`+`.spk`
- `.sb0`+`.sp0` (or other numbers instead of `0`)
- `.sgh`+`.sgd`
- `.snr`+`.sns`
- `.spt`+`.spd`
- `.sts`+`.int`
- `.xwh`+`.xwb`
- `.xps`+`dat`
- `.wav.str`+`.wav`
- `.wav`+`.dcs`
- `.wbh`+`.wbd`

Both are needed to play and must be together. The usual rule is you open the
bigger file (body), save a few formats where the smaller (header) file is opened
instead for technical reasons (mainly some bank formats).

Generally companion files are named the same (`bgm.awb`+`bgm.acb`), or internally
point to another file `sfx.sb0`+`STREAM.sb0`. A few formats may have different names
which are hardcoded instead of being listed in the header file (e.g. `.mpf+.mus`).
In these cases, you can use *TXTM* format to specify associated companion files.
See *Artificial files* below for more information.

#### Dual stereo
A special case of the above is "dual file stereo", where 2 similarly named mono
files are fused together to make 1 stereo song.
- `(file)_L.dsp`+`(file)_R.dsp`
- `(file)-l.dsp`+`(file)-l.dsp`
- `(file).L`+`(file).R`
- `(file)_0.dsp`+`(file)_1.dsp`
- `(file)_Left.dsp`+`(file)_Right.dsp`
- `(file).v0`+`(file).v1`

vgmstream automatically detects these pairs and makes a stereo song from `L` + `R`.
You can open either `L` or `R` and you'll get the same stereo. If you rename one
of the files the "pair" won't be found, and both will be played as mono. This
is only done for a few choice formats (mainly `.dsp` and `.vag`) that commonly
split audio like that, though.

#### OS case sensitiveness
When using OS with case sensitive filesystem (mainly Linux), a known issue with
companion files is that vgmstream generally tries to find them using lowercase
extension.

This means that if the developer used uppercase instead (e.g. `bgm.ABK`+`bgm.AST`)
loading will fail. It's technically complex to fix this, so for the time being
the only option is renaming the companion extension to lowercase.

A particularly nasty variation of that is that some formats load files by full
name (e.g. `STREAM.SS0`), but sometimes the actual filename is in other case
(`Stream.ss0`), and some files could even point to that with yet another case.
You could try adding *symlinks* in various upper/lower/mixed cases to handle this,
though only a few formats do this, mainly *Ubisoft* banks.

Regular formats without companion files should work fine in upper/lowercase. For
`.(ext).txth` files make sure `(ext)` matches case too.

### Decryption keys
Certain formats have encrypted data, and need a key to decrypt. vgmstream
will try to find the correct key from a list, but it can be provided by
a companion file:
- `.adx`: `.adxkey` (keystring, 8-byte keycode, or derived 6 byte start/mult/add key)
- `.ahx`: `.ahxkey` (derived 6-byte start/mult/add key)
- `.hca`: `.hcakey` (8-byte decryption key, a 64-bit number)
  - `.awb`/`.acb` also may use `.hcakey`, and will combine with an internal AWB subkey
  - May set a 8-byte key followed a 2-byte AWB subkey for newer HCA
- `.fsb`: `.fsbkey` (decryption key in hex, usually between 8-32 bytes) 
- `.bnsf`: `.bnsfkey` (decryption key, a string up to 24 chars)

The key file can be `.(ext)key` (for the whole folder), or `(name).(ext)key"
(for a single file). The format is made up to suit vgmstream.

### Artificial files
In some cases a file only has raw data, while important header info (codec type,
sample rate, channels, etc) is stored in the .exe or other hard to locate places.
Or maybe the file plays normally, but has many layers at once that are silenced
dynamically during gameplay, or looping metadata is stored externally.

Cases like those can be supported using an artificial files with info vgmstream
needs.

Creation of these files is meant for advanced users, full docs can be found in
vgmstream source.

#### TXTH
A text header placed in an external file. The TXTH must be named
`.txth` or `.(ext).txth` (for the whole folder), or `(name.ext).txth` (for a
single file). Contains dynamic text commands to read data from the original
file, or static values. This allows vgmstream to play unsupported formats.

Usage example (used when opening an unknown file named `bgm_01.pcm`):

**.pcm.txth**
```
codec = PCM16LE
channels = @0x04        #in the file, at offset 4
sample_rate = 48000     #hardcoded
start_offset = 0x10
num_samples = data_size #auto
```

#### TXTP
Text files with player configuration, named `(name).txtp`.

For files that already play, sometimes games use them in various complex
and non-standard ways, like playing multiple small songs as a single
one, or using some channels as a section of the song. For those cases we
can create a *TXTP* file to customize how vgmstream handles songs.

Text inside `.txtp` can contain a list of filenames to play as one, a list of
single-channel files to join as a single multichannel file, subsong index,
per-file configurations like number of loops, remove unneeded channels,
force looping, and many other features.

Usage examples (open directly, name can be set freely):

**bgm01-full.txtp**
```
# plays 2 files as a single one
bgm01_intro.vag
bgm01_loop.vag
loop_mode = auto
```

**bgm-subsong10.txtp**
```
# plays subsong number 10
bgm.sxd#10
```

**song01-looped.txtp**
```
# force looping an .mp3 from 10 seconds up to file end
song02.mp3 #I 10.0
```

**music01-demux2.txtp**
```
# plays channels 3 and 4 only, removes rest
music01.bfstm #C3,4
```

#### TXTM
A text file named `.txtm` for some formats with companion files. It lists
name combos determining which companion files to load for each main file.

It is needed for formats where name combos are hardcoded, so vgmstream doesn't
know which companion file(s) to load if its name doesn't match the main file.
Note that companion file order is usually important.

Usage example (used when opening files in the left part of the list):
```
# Harry Potter and the Chamber of Secrets (PS2)
entrance.mpf: entrance.mus,entrance_o.mus
willow.mpf: willow.mus,willow_o.mus
```
```
# Metal Gear Solid: Snake Eater 3D (3DS) names for .awb
bgm_2_streamfiles.awb: bgm_2.acb
```
```
# Snack World (Switch) names for .awb (single .acb for all .awb, order matters)
bgm.awb: bgm.acb
bgm_DLC1.awb: bgm.acb
```
In rare cases you need to setup some extra flags
```
event_stream2.awb: event_stream2.acb
event_stream2_dlc1.awb: event_stream2.acb
event_stream2_dlc2.awb: event_stream2.acb
event_stream2_dlc3.awb: event_stream2.acb
# next "flag" allows both effect.acb and even_stream2.acb in the same file
#@reset-pos
effect.awb: effect.acb
effect_dlc2.awb: effect.acb
effect_dlc3.awb: effect.acb
```

#### GENH
A byte header placed right before the original data, modifying it.
The resulting file must be `(name).genh`. Contains static header data.

Programs like VGMToolbox can help to create *GENH*, but consider using *TXTH*
instead, *GENH* is mostly deprecated. *TXTH* is recommended over *GENH* as
it's far easier to create and has many more functions, plus doesn't modify
original data.


### Plugin conflicts
Since vgmstream supports a huge amount of formats it's possibly that some of
them are also supported in other plugins, and this sometimes causes conflicts.
If a file that should isn't playing or looping, first make sure vgmstream is
really opening it (should show "VGMSTREAM" somewhere in the file info), and
try to remove a few other plugins.

foobar's FFmpeg plugin and foo_adpcm are known to cause issues, but in
modern versions (+1.4.x) you can configure plugin priority.

In Audacious, vgmstream is set with slightly higher priority than FFmpeg,
since it steals many formats that you normally want to loop (like `.adx`).
However other plugins may set themselves higher, stealing formats instead.
If current Audacious version doesn't let to change plugin priority you may
need to disable some plugins (requires restart) or set priority on compile
time. Particularly, mpg123 plugin may steal formats that aren't even MP3,
making impossible for vgmstream to play them properly.

### Channel issues
Some games layer a huge number of channels, that are disabled or downmixed
during gameplay. The player may be unable to play those files (for example
foobar can only play up to 8 channels, and Winamp depends on your sound
card). For those files you can set the "downmix" option in vgmstream, that
can reduce the number of channels to a playable amount. 

Note that this type of downmixing is very generic (not meant to be used when
converting to other formats), channels are re-assigned and volumes modified
in simplistic ways, since it can't guess how the file should be properly
adjusted. Most likely it will sound a bit quieter than usual.

You can also choose which channels to play using *TXTP*. For example, create
a file named `song.adx#C1,2.txtp` to play only channels 1 and 2 from `song.adx`.
*TXTP* also has command to set how files are downmixed, like `song.adx #@downmix.txtp`
for standard 5.1/4.0/etc audio to stereo, or manual (per-channel) mixing.

### Average bitrate
Note that vgmstream shows the "file bitrate" (counts all data) as opposed to
"codec bitrate" (counts pure audio-only parts). This means bitrate may be
slightly higher (or much higher, if file is bloated) than what encoder
tools or other players may report.

Calculating 100% correct codec bitrate usually needs manual reading of the whole
file, slowing down opening files and needing extra effort by devs for minimal
benefit, so it's not done.

In some cases it's debatable what the codec bitrate is. Unlike MP3/AAC, 48kbps
of raw Vorbis/Opus is unplayable/unusable unless it's packed into .ogg/wem/etc
with extra data, that does increase final file size (thus bitrate) by some percent.

Also, keep in mind video game audio bitrate isn't always a great indicator of quality.
There are many factors in play like encoder, type of codec, sample rate and so on.
A higher bitrate `.wav` can sound worse than a lower `.ogg` (like mono 22050hz `.wav`
vs stereo 48000hz `.ogg`).

### Containers
Some formats are *audio containers* of other common audio formats. For example
`.acb`/`.awb` may contain standard `.hca` inside. Rather than extracting the
internal "files", it's recommended that you keep data unmodified for preservation
purposes. Sometimes containers have useful data (like loop info or names), that
you may be unknowingly throwing away if you extract internal files.

It's a good practice (and simpler) to just let containers be and play them
directly with vgmstream. Newer `.acb`/`.awb` have extra data needed to decrypt
the `.hca`, so if you are already used to those containers you don't need to
worry about extracted `.hca` not working later. Plus you can use TXTH's "subfile"
function to make unsupported containers playable:
```
# Simple container with an Ogg inside. Maybe values 0x00..0x10 could contain
# loops or other useful info, that other users are able to figure out:
subfile_extension = ogg
subfile_offset = 0x10
```
With unmodified data, you can always extract the internal files later if you
change your mind, but you can't get the (potentially useful) container data back
once extracted.

However, if your file is a *generic container* (like a `.zip`, that could hold
graphics or audio) you may safely extract the internal files without worry.

Note that some formats are *audio banks* rather than *containers* (like `.fsb`),
in that info for playing the audio is part of the bank header, and extracting
internal files as-is isn't really possible.


## Logged errors and unplayable supported files
Some formats should normally play, but somehow don't. In those cases plugins
can print vgmstream's error info to console (for example, `.fsb` with an unknown
codec, `.hca/awb` with missing decryption key, bank has no audio, `.txth` is
malformed, or `.wav` has an incorrectly ripped size).

Console location and format depends on plugin:
- *foobar2000*: found in *View menu > Console*
- *Winamp*: open vgmstream's config (*Preferences... > Plug-ins > vgmstream* + *Configure*
  button) then press "Open Log"
- *Audacious*: start with `audacious -V` from terminal
- CLI utils: printed to stdout directly

Only a few errors are printed ATM but may be helpful for more common cases.

## Tagging
Some of vgmstream's plugins support simple read-only tagging via external files.

Tags are loaded from a text/M3U-like file named *!tags.m3u* in the song folder.
You don't have to load your songs with this M3U though, but you can (for pre-made
order). The format is meant to be both a quick playlist and tags, but the tagfile
itself just 'looks' like an M3U. you can load files manually or using other playlists
and still get tags.

Format is:
```
# ignored comment
# $GLOBAL_COMMAND (extra features)
# @GLOBAL_TAG text (applies all following tracks)

# %LOCAL_TAG text (applies to next track only)
filename1
# %LOCAL_TAG text (applies to next track only)
filename2
```
Accepted tags depend on the player (foobar: any; Winamp: see ATF config, Audacious:
few standard ones), typically *ALBUM/ARTIST/TITLE/DISC/TRACK/COMPOSER/etc*, lower
or uppercase, separated by one or multiple spaces. Repeated tags overwrite previous
(ex.- may define *@COMPOSER* multiple times for "sections"). It only reads up to
current *filename* though, so any *@TAG* below would be ignored.

*GLOBAL_COMMAND*s currently can be:
- *AUTOTRACK*: sets *%TRACK* tag automatically (1..N as files are encountered
  in the tag file).
- *AUTOALBUM*: sets *%ALBUM* tag automatically using the containing dir as album.
- *EXACTMATCH*: disables matching .txtp with regular files (explained below).

Playlist title formatting (how tags are shown) should follow player's config, as
vgmstream simply passes tags to the player. It's better to name the file lowercase
`!tags.m3u` rather than `!Tags.m3u` (Windows accepts both but Linux is case sensitive).

Note that with global tags you don't need to put all files inside. This would be
a perfectly valid *!tags.m3u*:
```
# @ALBUM    Game
# @ARTIST   Various Artists
```

### Compatibility and non-English filenames and tags
For best compatibility save `!tags.m3u` as *"ANSI"* or *"UTF-8" (with BOM)*.

Tags and filenames using extended characters (like Japanese) should work, as long
as `!tags.m3u` is saved as *"UTF-8 with BOM"* (UTF-8 is a way to define non-English
characters, and BOM is a helper "byte-order" mark). Windows' *notepad* creates files
*"with BOM"* when selecting UTF-8 encoding in *save as* dialog, or you may use other
programs like *notepad++.exe* to convert them.

More exactly, vgmstream needs the file saved in *UTF-8* to match tags and filenames
(and ignores *BOM*), while foobar/Winamp won't understand UTF-8 *filenames* unless
`.m3u` is saved *with BOM* (ignoring tags). Whereas if saved in what Windows calls
"Unicode" (UTF-16) neither may work.

Conversely, if your *filenames* only use English/ANSI characters you may ommit *BOM*,
and if your tags are English only you may save the `.m3u` as ANSI. Or if you only use
`!tags.m3u` for tags and not for opening files (for example opening them manually
or with a `playlist.m3u8`) you won't need BOM either.

Other players may not need BOM (or CRLF), but for consistency use them when dealing
with non-ASCII names and tags.

### Tags with spaces
Some players like foobar accept tags with spaces. To use them surround the tag
with both characters.
```
# @GLOBAL TAG WITH SPACES@ text
# ...
# %LOCAL TAG WITH SPACES% text
filename1
```
As a side effect if text has @/% inside you also need them: `# @ALBUMARTIST@ Tom-H@ck`

For interoperability with other plugins, consider using only common tags without spaces.

### ReplayGain
foobar2000/Winamp can apply the following replaygain tags (if ReplayGain is
enabled in preferences):
```
# %replaygain_track_gain N.NN dB
# %replaygain_track_peak N.NNN
# @replaygain_album_gain N.NN dB
# @replaygain_album_peak N.NNN
```

### TXTP matching
To ease *TXTP* config, tags with plain files will match `.txtp` with config, and tags
with `.txtp` config also match plain files:

**!tags.m3u**
```
# @TITLE    Title1
BGM01.adx #P 3.0.txtp
# @TITLE    Title2
BGM02.wav
```
**config.m3u**
```
# matches "Title1" (1:1)
BGM01.adx #P 3.0.txtp
# matches "Title1" (plain file matches config tag)
BGM01.adx
# matches "Title2" (config file matches plain tag)
BGM02.wav #P 3.0.txtp
# doesn't match anything (different config can't match)
BGM01.adx #P 10.0.txtp
```

Since it matches when a tag is found, some cases that depend on order won't work.
You can disable this feature manually then:

**!tags.m3u**
```
# $EXACTMATCH
#
# %TITLE    Title3 (without config)
BGM01.adx
# %TITLE    Title3 (with config)
BGM01.adx #I 1.0 90.0 .txtp
```
**config.m3u**
```
# Would match "Title3 (without config)" without "$EXACTMATCH", as it's found first
# Could use "BGM01.adx.txtp" as first entry in !tags.m3u instead (different configs won't match)
BGM01.adx #I 1.0 90.0 .txtp
```

### Issues
If your player isn't picking tags make sure vgmstream is detecting the song
(as other plugins can steal its extensions, see above), `.m3u` is properly
named and that filenames inside match the song filename. For Winamp you need
to make sure *options > titles > advanced title formatting* checkbox is set and
the format defined.

When tags change behavior varies depending on player:
- *Winamp*: should refresh tags when a different file is played.
- *foobar2000*: needs to force refresh (for reasons outside vgmstream's control)
  - **select songs > shift + right click > Tagging > Reload info from file(s)**.
- *Audacious*: files need to be re-added to the playlist

Currently there is no tool to aid in the creation of these tags, but you can create
a base `.m3u` and edit as a text file. You may try this python script to make the
base file: https://raw.githubusercontent.com/bnnm/vgm-tools/master/py/tags-maker.py

vgmstream's "m3u tagging" is meant to be simple to make and share (just a text
file), easier to support in multiple players (rather than needing a custom plugin),
allow OST-like ordering but also mixable with other `.m3u`, and be flexible enough
to have commands. If you are not satisfied with vgmstream's tagging format,
foobar2000 has other plugins (with write support) that may be of use:
- m-TAGS: http://www.m-tags.org/
- foo_external_tags: https://foobar.hyv.fi/?view=foo_external_tags


## Virtual TXTP files
Some of vgmstream's plugins (and CLI) allow you to use virtual `.txtp` files, that
combined with playlists let you make quick song configs.

Normally you can create a physical .txtp file that points to another file with
config, and `.txtp` have a "mini-txtp" mode that configures files with only the
filename.

Instead of manually creating `.txtp` files you can put non-existing virtual `.txtp`
in a `.m3u` playlist:
```
# playlist that opens subsongs directly without having to create .txtp
# notice the full filename, then #(config), then ".txtp" (spaces are optional)
bank_bgm_full.nub  #s1  .txtp
bank_bgm_full.nub  #s10 .txtp
```

Combine with tagging (see above) for extra fun OST-like config.
```
# @ALBUM    GOD HAND

# play 1 loop, delay and do a longer fade
# %TITLE    Too Hot !!
circus_a_mix_ver2.adx       #l 1.0 #d 5.0 #f 15.0 .txtp

# play 1 loop instead of the default 2 then fade with the song's internal fading
# %TITLE    Yet... Oh see mind
boss2_3ningumi_ver6.adx     #l 1.0  #F .txtp

...
```

You can also use it in CLI for quick access to some txtp-exclusive functions:
```
# force change sample rate to 22050 (don't forget to use " with spaces)
test.exe -o btl_koopa1_44k_lp.wav "btl_koopa1_44k_lp.brstm  #h22050.txtp"
```

Support for this feature is limited by player itself, as foobar and Winamp allow
non-existent files referenced in a `.m3u`, while other players may filter them
first.

You can use this python script to autogenerate one `.txtp` per virtual-txtp:
https://github.com/vgmstream/vgmstream/tree/master/cli/tools/txtp_dumper.py
Drag and drop the `.m3u`, or any text file with .txtp  (it has CLI options
to control output too).
