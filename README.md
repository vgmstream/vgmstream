# vgmstream
This is vgmstream, a library for playing streamed (pre-recorded) audio from
video games.

Some of vgmstream's features:
- hundreds of video game music formats and codecs, from typical game engine files to 
  obscure single-game codecs, aiming for high accuracy and compatibility.
- support for looped BGM, using file's internal metadata for smooth transitions,
  with accurate sample counts
- subsongs, playing a format's multiple internal songs separately
- encryption keys, audio split in multiple files, internal stream names, and many
  other unusual cases found in game audio
- TXTH function, to support extra formats (including raw audio in many forms)
- TXTP function, for real-time and per-file config (like forced looping, removing
  channels, playing certain subsong, or fusing together multiple files as a single one)
- simple external tagging via .m3u files
- plugins available for various common players and O.S.

Latest development is here: https://github.com/vgmstream/vgmstream/

Automated builds with the latest changes: https://vgmstream.org/downloads

Help can be found here: https://www.hcs64.com/

More technical docs: https://github.com/vgmstream/vgmstream/tree/master/doc


## Usage
There are multiple end-user bits:
- a command line decoder called *test.exe/vgmstream-cli*
- a Winamp plugin called *in_vgmstream*
- a foobar2000 component called *foo_input_vgmstream*
- an XMPlay plugin called *xmp-vgmstream*
- an Audacious plugin called *libvgmstream*
- a command line player called *vgmstream123*

Main lib (plain *vgmstream*) is the code that handles internal conversion, while the
above components are what you use to actually get sound. See *components* below for
explanations about each one.

### Files
On Windows, you should get `vgmstream-win.zip` (bundle of various components) or
`foo_input_vgmstream.fb2k-component` (installable foobar2000 plugin) from the
pre-built binaries: https://vgmstream.org/downloads

If the above link fails you may find alt, recent-ish versions here:
https://github.com/bnnm/vgmstream-builds/raw/master/bin/vgmstream-latest-test-u.zip
You may compile them from source as well.

For Linux and other O.S., you need to build vgmstream manually (see *vgmstream/doc/BUILD.md*
in source).

### Needed extra files (for Windows)
On Windows support for some codecs (Ogg Vorbis, MPEG audio, etc.) is done with external
libraries, so you will need to have certain DLL files.

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

*Others*: build instructions can be found in doc/BUILD.md document in vgmstream's source
code (can be compiled with CMake/Make/autotools).

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
Default output filename is `?f.wav`, or `?f#?s.wav` if you set subsongs (`-s/S`).

For files containing multiple subsongs, you can write them all using some flags.
**WARNING, MAY TAKE A LOT OF SPACE!** Some files have been observed to contain +20000
subsongs, so don't use this lightly. Remember to set an output name (`-o`) with subsong
wildcards (or leave it alone for the defaults).
- `test.exe -s 1 -S 100 file.bank`: writes from subsong 1 to subsong 100
- `test.exe -s 101 -S 0 file.bank`: writes from subsong 101 to max subsong (automatically changes 0 to max)
- `test.exe -S 0 file.bank`: writes from subsong 1 to max subsong
- `test.exe -s 1 -S 5 -o bgm.wav file.bank`: writes 5 subsongs, but all overwrite the same file = wrong.
- `test.exe -s 1 -S 5 -o bgm_?02s.wav file.bank`: writes 5 subsongs, each named differently = correct.


### in_vgmstream (Winamp plugin)
*Windows*: drop the `in_vgmstream.dll` in your Winamp Plugins directory,
and follow the above instructions for installing needed extra files.

*Others*: may be possible to use through *Wine*

Once installed, supported files should be playable. There is a simple config
menu to tweak some options too. If the *Preferences... > Plug-ins > Input* shows
vgmstream as *"NOT LOADED"* that means extra DLL files aren't in the correct
place.


### xmp-vgmstream (XMPlay plugin)
*Windows*: drop the `xmp-vgmstream.dll` in your XMPlay plugins directory,
and follow the above instructions for installing the other files needed.

*Others*: may be possible to use through *Wine*

Note that this has less features compared to *in_vgmstream* and has no config.
Since XMPlay supports Winamp plugins you may also use `in_vgmstream.dll` instead.

Because the XMPlay MP3 decoder incorrectly tries to play some vgmstream extensions,
you need to manually fix it by going to **options > plugins > input > vgmstream**
and in the "priority filetypes" put: `ahx,asf,awc,ckd,fsb,genh,lwav,msf,p3d,rak,scd,txth,xvag`

XMPlay cannot support subsongs due to player limitations (with any plugin), try
using *TXTP* instead (explained below).


### foo_input_vgmstream (foobar2000 plugin)
*Windows*: every file should be installed automatically when opening the `.fb2k-component`
bundle

*Others*: may be possible to use through *Wine*

A known quirk is that when loop options or tags change, playlist info won't refresh
automatically. You need to manually refresh it by selecting songs and doing
**shift + right click > Tagging > Reload info from file(s)**.


### Audacious plugin
*Windows*: not possible at the moment.

*Others*: needs to be manually built. Instructions can be found in doc/BUILD.md
document in vgmstream's source code (can be done with CMake or autotools).


### vgmstream123 (command line player)
*Windows/Linux*: needs to be manually built. Instructions can be found in the
*doc/BUILD.md* document in vgmstream's source code. On Windows it needs `libao.dll`
and appropriate includes.

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
not meant to be extracted (no simple separation). Some plugins are able to "unpack"
those files automatically into the playlist. For others without support, you can create
multiple .txtp (explained below) to select one of the subsongs (like `bgm.sxd#10.txtp`).

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
- `.aiff/aifc` to `.laiffl/laifc` (standard Mac AIF)
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
vgmstream doesn't even get the chance to parse that file, so you may need to
disable the offending plugin or rename the file (for example this may happen with
`.asf` in foobar2000/Winamp, may be fixed in newer versions).

When extracting from a bigfile, sometimes internal files don't have a proper
extension. Those should be renamed to its correct one when possible, as the
extractor program may guess wrong (like `.wav` instead of `.at3` or `.wem`).
If there is no known extension, usually the header id/magic string may be used instead.

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

Regular formats without companion files should work fine in upper/lowercase.

### Decryption keys
Certain formats have encrypted data, and need a key to decrypt. vgmstream
will try to find the correct key from a list, but it can be provided by
a companion file:
- `.adx`: `.adxkey` (keystring, 8 byte keycode, or derived 6 byte start/mult/add key)
- `.ahx`: `.ahxkey` (derived 6 byte start/mult/add key)
- `.hca`: `.hcakey` (8 byte decryption key, a 64-bit number)
  - May be followed by 2 byte AWB scramble key for newer HCA
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

#### GENH
A byte header placed right before the original data, modifying it.
The resulting file must be `(name).genh`. Contains static header data.

Programs like VGMToolbox can help to create *GENH*, but consider using *TXTH*
instead, *GENH* is mostly deprecated.

#### TXTH
A text header placed in an external file. The TXTH must be named
`.txth` or `.(ext).txth` (for the whole folder), or `(name.ext).txth` (for a
single file). Contains dynamic text commands to read data from the original
file, or static values. This allows vgmstream to play unsupported formats.

*TXTH* is recommended over *GENH* as it's far easier to create and has many
more functions, plus doesn't modify original data.

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
*TXTP* also has command to set how files are downmixed.

### Logged errors and unplayable supported files
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
Some of vgmstream's plugins allow you to use virtual `.txtp` files, that combined
with playlists let you make quick song configs.

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


## Supported codec types
Quick list of most codecs vgmstream supports, including many obscure ones that
are used in few games.

- PCM 16-bit
- PCM 8-bit (signed, unsigned)
- PCM 4-bit (signed, unsigned)
- PCM 32-bit float
- u-Law/a-LAW
- CRI ADX (standard, fixed, exponential, encrypted)
- Nintendo DSP ADPCM a.k.a GC ADPCM
- Nintendo DTK ADPCM
- Nintendo AFC ADPCM
- ITU-T G.721
- CD-ROM XA ADPCM
- Sony PSX ADPCM a.k.a VAG (standard, badflags, configurable, extended)
- Sony HEVAG
- Electronic Arts EA-XA (stereo, mono, Maxis)
- Electronic Arts EA-XAS (v0, v1)
- DVI/IMA ADPCM (stereo/mono + high/low nibble, 3DS, Quantic Dream, SNDS, etc)
- Microsoft MS IMA ADPCM (standard, Xbox, NDS, Radical, Wwise, FSB, WV6, etc)
- Microsoft MS ADPCM (standard, Cricket Audio)
- Westwood VBR ADPCM
- Yamaha ADPCM (AICA, Aska)
- Procyon Studio ADPCM
- Level-5 0x555 ADPCM
- lsf ADPCM
- Konami MTAF ADPCM
- Konami MTA2 ADPCM
- Paradigm MC3 ADPCM
- FMOD FADPCM 4-bit ADPCM
- Konami XMD 4-bit ADPCM
- Platinum 4-bit ADPCM
- Argonaut ASF 4-bit ADPCM
- Tantalus 4-bit ADPCM
- Ocean DSA 4-bit ADPCM
- Circus XPCM ADPCM
- Circus XPCM VQ
- OKI 4-bit ADPCM (16-bit output, 4-shift, PC-FX)
- Ubisoft 4/6-bit ADPCM
- Tiger Game.com ADPCM
- LucasArts iMUSE VBR ADPCM
- CompressWave (CWav) Huffman ADPCM
- SDX2 2:1 Squareroot-Delta-Exact compression DPCM
- CBD2 2:1 Cuberoot-Delta-Exact compression DPCM
- Activision EXAKT SASSC DPCM
- Xilam DERF DPCM
- InterPlay ACM
- VisualArt's NWA
- Electronic Arts MicroTalk a.k.a. UTK or UMT
- Relic Codec
- CRI HCA
- tri-Ace PS2 Codec
- Xiph Vorbis (Ogg, FSB5, Wwise, OGL, Silicon Knights)
- MPEG MP1/2/3 (standard, AHX, XVAG, FSB, AWC, P3D, EA, etc)
- ITU-T G.722.1 annex C a.k.a. Polycom Siren 14 (Namco)
- ITU-T G.719 annex B a.k.a. Polycom Siren 22
- Electronic Arts EASpeex
- Electronic Arts EALayer3
- Electronic Arts EA-XMA
- Sony ATRAC3, ATRAC3plus
- Sony ATRAC9
- Microsoft XMA1/2
- Microsoft WMA v1, WMA v2, WMAPro
- AAC
- Bink
- AC3/SPDIF
- Xiph Opus (Ogg, Switch, EA, UE4, Exient)
- Xiph CELT (FSB)
- Musepack
- FLAC
- Others

Sometimes standard codecs come in non-standard layouts that aren't normally
supported by other players (like multiple `.ogg` or `.mp3` files chunked and
interleaved together in custom ways).

Some codecs are not fully correct compared to the games due to minor bugs, but
in most cases it isn't audible, and general accuracy is high, with emphasis in
proper support of encoder delay, accurate sample counts and seeking that other
plugins may lack.

Note that vgmstream doesn't (can't) reproduce in-game music 1:1, as internal
resampling, filters, volume, etc, are not replicated.


## Supported file types
As manakoAT likes to say, the extension doesn't really mean anything, but it's
the most obvious way to identify files.

This list is not complete and many other files are supported.

- PS2/PSX ADPCM:
	- .ads/.ss2
	- .ass
	- .ast
	- .bg00
	- .bmdx
	- .ccc
	- .cnk
	- .dxh
	- .enth
	- .fag
	- .filp
	- .gcm
	- .gms
	- .hgc1
	- .ikm
	- .ild
	- .ivb
	- .joe
	- .kces
	- .khv
	- .leg
	- .mcg
	- .mib, .mi4 (w/ or w/o .mih)
	- .mic
	- .mihb (merged mih+mib)
	- .msa
	- .msvp
	- .musc
	- .npsf
	- .pnb
	- .psh
	- .rkv
	- .rnd
	- .rstm
	- .rws
	- .rxw
	- .snd
	- .sfs
	- .sl3
	- .smpl (w/ bad flags)
	- .ster
	- .str+.sth
	- .str (MGAV blocked)
	- .sts
	- .svag
	- .svs
	- .tec (w/ bad flags)
	- .tk5 (w/ bad flags)
	- .vas
	- .vag
	- .vgs (w/ bad flags)
	- .vig
	- .vpk
	- .vs
	- .vsf
	- .wp2
	- .xa2
	- .xa30
	- .xwb+xwh
- GC/Wii/3DS DSP ADPCM:
	- .aaap
	- .agsc
	- .asr
	- .bns
	- .bo2
	- .capdsp
	- .cfn
	- .ddsp
	- .dsp
		- standard, optional dual file stereo
		- RS03
		- Cstr
		- _lr.dsp
		- MPDS
	- .gca
	- .gcm
	- .gsp+.gsp
	- .hps
	- .idsp
	- .ish+.isd
	- .lps
	- .mca
	- .mpdsp
	- .mss
	- .mus (not quite right)
	- .ndp
	- .pdt
	- .sdt
	- .smp
	- .sns
	- .spt+.spd
	- .ssm
	- .stm/.dsp
	- .str
	- .str+.sth
	- .sts
	- .swd
	- .thp, .dsp
	- .tydsp
	- .vjdsp
	- .waa, .wac, .wad, .wam
	- .was
	- .wsd
	- .wsi
	- .ydsp
	- .ymf
	- .zwdsp
- PCM:
	- .aiff (8 bit, 16 bit)
	- .asd (16 bit)
	- .baka (16 bit)
	- .bh2pcm (16 bit)
	- .dmsg (16 bit)
	- .gcsw (16 bit)
	- .gcw (16 bit)
	- .his (8 bit)
	- .int (16 bit)
	- .pcm (8 bit, 16 bit)
	- .kraw (16 bit)
	- .raw (16 bit)
	- .rwx (16 bit)
	- .sap (16 bit)
	- .snd (16 bit)
	- .sps (16 bit)
	- .str (16 bit)
	- .xss (16 bit)
	- .voi (16 bit)
	- .wb (16 bit)
	- .zsd (8 bit)
- Xbox IMA ADPCM:
	- .matx
	- .wavm
	- .wvs
	- .xmu
	- .xvas
	- .xwav
- Yamaha AICA ADPCM:
	- .adpcm
	- .dcs+.dcsw
	- .str
	- .spsd
- IMA ADPCM:
	- .bar (IMA ADPCM)
	- .pcm/dvi (DVI IMA ADPCM)
	- .hwas (IMA ADPCM)
	- .dvi/idvi (DVI IMA ADPCM)
	- .ivaud (IMA ADPCM)
	- .myspd (IMA ADPCM)
	- .strm (IMA ADPCM)
- multi:
	- .aifc (SDX2 DPCM, DVI IMA ADPCM)
	- .asf/as4 (8/16 bit PCM, DVI IMA ADPCM)
	- .ast (GC AFC ADPCM, 16 bit PCM)
	- .aud (IMA ADPCM, WS DPCM)
	- .aus (PSX ADPCM, Xbox IMA ADPCM)
	- .brstm (GC DSP ADPCM, 8/16 bit PCM)
	- .emff (PSX APDCM, GC DSP ADPCM)
	- .fsb/wii (PSX ADPCM, GC DSP ADPCM, Xbox IMA ADPCM, MPEG audio, FSB Vorbis, MS XMA)
	- .msf (PCM, PSX ADPCM, ATRAC3, MP3)
	- .musx (PSX ADPCM, Xbox IMA ADPCM, DAT4 IMA ADPCM)
	- .nwa (16 bit PCM, NWA DPCM)
	- .p3d (Radical ADPCM, Radical MP3, XMA2)
	- .psw (PSX ADPCM, GC DSP ADPCM)
	- .rwar, .rwav (GC DSP ADPCM, 8/16 bit PCM)
	- .rws (PSX ADPCM, XBOX IMA ADPCM, GC DSP ADPCM, 16 bit PCM)
	- .rwsd (GC DSP ADPCM, 8/16 bit PCM)
	- .rsd (PSX ADPCM, 16 bit PCM, GC DSP ADPCM, Xbox IMA ADPCM, Radical ADPCM)
	- .rrds (NDS IMA ADPCM)
	- .sad (GC DSP ADPCM, NDS IMA ADPCM, Procyon Studios NDS ADPCM)
	- .sgd/sgb+sgh/sgx (PSX ADPCM, ATRAC3plus, AC3)
	- .seg (Xbox IMA ADPCM, PS2 ADPCM)
	- .sng/asf/str/eam/aud (8/16 bit PCM, EA-XA ADPCM, PSX ADPCM, GC DSP ADPCM, XBOX IMA ADPCM, MPEG audio, EALayer3)
	- .strm (NDS IMA ADPCM, 8/16 bit PCM)
	- .sb0..7 (Ubi IMA ADPCM, GC DSP ADPCM, PSX ADPCM, Xbox IMA ADPCM, ATRAC3)
	- .swav (NDS IMA ADPCM, 8/16 bit PCM)
	- .xwb (PCM, Xbox IMA ADPCM, MS ADPCM, XMA, XWMA, ATRAC3)
	- .xwb+xwh (PCM, PSX ADPCM, ATRAC3)
	- .wav/lwav (unsigned 8 bit PCM, 16 bit PCM, GC DSP ADPCM, MS IMA ADPCM, XBOX IMA ADPCM)
	- .wem [lwav/logg/xma] (PCM, Wwise Vorbis, Wwise IMA ADPCM, XMA, XWMA, GC DSP ADPCM, Wwise Opus)
- etc:
	- .2dx9 (MS ADPCM)
	- .aax (CRI ADX ADPCM)
	- .acm (InterPlay ACM)
	- .adp (GC DTK ADPCM)
	- .adx (CRI ADX ADPCM)
	- .afc (GC AFC ADPCM)
	- .ahx (MPEG-2 Layer II)
	- .aix (CRI ADX ADPCM)
	- .at3 (Sony ATRAC3 / ATRAC3plus)
	- .aud (Silicon Knights Vorbis)
	- .baf (PSX configurable ADPCM)
	- .bgw (PSX configurable ADPCM)
	- .bnsf (G.722.1)
	- .caf (Apple IMA4 ADPCM, others)
	- .dec/de2 (MS ADPCM)
	- .hca (CRI High Compression Audio)
	- .pcm/kcey (DVI IMA ADPCM)
	- .lsf (LSF ADPCM)
	- .mc3 (Paradigm MC3 ADPCM)
	- .mp4/lmp4 (AAC)
	- .msf (PCM, PSX ADPCM, ATRAC3, MP3)
	- .mtaf (Konami ADPCM)
	- .mta2 (Konami XAS-like ADPCM)
	- .mwv (Level-5 0x555 ADPCM)
	- .ogg/logg (Ogg Vorbis)
	- .ogl (Shin'en Vorbis)
	- .rsf (CCITT G.721 ADPCM)
	- .sab (Worms 4 soundpacks)
	- .s14/sss (G.722.1)
	- .sc (Activision EXAKT SASSC DPCM)
	- .scd (MS ADPCM, MPEG Audio, 16 bit PCM)
	- .sd9 (MS ADPCM)
	- .smp (MS ADPCM)
	- .spw (PSX configurable ADPCM)
	- .stm/lstm [amts/ps2stm/stma] (16 bit PCM, DVI IMA ADPCM, GC DSP ADPCM)
	- .str (SDX2 DPCM)
	- .stx (GC AFC ADPCM)
	- .ulw (u-Law PCM)
	- .um3 (Ogg Vorbis)
	- .xa (CD-ROM XA audio)
	- .xma (MS XMA/XMA2)
	- .sb0/sb1/sb2/sb3/sb4/sb5/sb6/sb7 (many)
	- .sm0/sm1/sm2/sm3/sm4/sm5/sm6/sm7 (many)
	- .bao/pk (many)
- artificial/generic headers:
    - .genh (lots)
    - .txth (lots)
- loop assists:
	- .mus (playlist for .acm)
	- .pos (loop info for .wav)
	- .sli (loop info for .ogg)
	- .sfl (loop info for .ogg)
- other:
	- .adxkey (decryption key for .adx)
	- .ahxkey (decryption key for .ahx)
    - .hcakey (decryption key for .hca)
    - .fsbkey (decryption key for .fsb)
    - .bnsfkey (decryption key for .bnsf)
    - .txtp (per song segment/layer handler and player configuration)

Enjoy! *hcs*
