# vgmstream

This is vgmstream, a library for playing streamed (pre-recorded) audio
from video games.

There are multiple end-user bits:
- a command line decoder called "test.exe/vgmstream-cli"
- a Winamp plugin called "in_vgmstream"
- a foobar2000 component called "foo_input_vgmstream"
- an XMPlay plugin called "xmp-vgmstream"
- an Audacious plugin called "libvgmstream"
- a command line player called "vgmstream123"

Help and newest builds can be found here: https://www.hcs64.com/

Latest development is usually here: https://github.com/losnoco/vgmstream/

Latest releases are here: https://github.com/losnoco/vgmstream/releases

Automated builds with the latest changes: https://vgmstream-builds.losno.co/latest/

You can find further info about other details in https://github.com/losnoco/vgmstream/tree/master/doc

## Needed extra files (for Windows)
Support for some codecs (Ogg Vorbis, MPEG audio, etc) is done with external
libraries, so you will need to have certain DLL files.

In the case of the foobar2000 component they are all bundled for convenience,
or you can get them here: https://github.com/losnoco/vgmstream/tree/master/ext_libs
(bundled here: https://f.losno.co/vgmstream-win32-deps.zip, may not be latest).

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

For Winamp/XMPlay/command line this means in the directory with the main .exe,
or in a system directory, or any other directory in the PATH variable.


## Components

### test.exe/vgmstream-cli
*Installation*: unzip the file and follow the above instructions for installing
the other files needed.

Converts playable files to wav. Typical usage would be:
- `test.exe -o happy.wav happy.adx` to decode `happy.adx` to `happy.wav`.

If command-line isn't your thing you can also drag and drop files to the
executable to decode them as `(filename).wav`.

There are multiple options that alter how the file is converted, for example:
- `test.exe -m -o file.wav file.adx`: print info but don't decode
- `test.exe -i -o file.wav file.hca`: convert without looping
- `test.exe -s 2 -F -o file.wav file.fsb`: play 2nd subsong + ending after 2.0 loops
- `test.exe -l 3.0 -f 5.0 -d 3.0 -o file.wav file.wem`: 3 loops, 3s delay, 5s fade

Available commands are printed when run with no flags. Note that you can also
achieve similar results for other plugins using TXTP, described later.

With files multiple subsongs you need to specify manually subsong (by design, to avoid
massive data dumps since some formats have hundred of subsongs), but you could do
some command line tricks:
```
: REM extracts from subsong 5 to 10 in file.fsb
for /L %A in (5,1,10) do test.exe -s %A -o file_%A.wav file.fsb
```

Output filename in `-o` may use multiple wildcards:
- `?s`: sets current subsong (or 0 if format doesn't have subsongs)
- `?0Ns`: same, but left pads subsong with up to `N` zeroes
- `?n`: internal stream name, or input filename if format doesn't have name
- `?f`: input filename

For example `test.exe -s 2 -o ?04s_?n.wav file.fsb` could generate `0002_song1.wav`


### in_vgmstream
*Installation*: drop the `in_vgmstream.dll` in your Winamp plugins directory,
and follow the above instructions for installing the other files needed.

Once installed supported files should be playable.

### xmp-vgmstream
*Installation*: drop the `xmp-vgmstream.dll` in your XMPlay plugins directory,
and follow the above instructions for installing the other files needed.

Note that this has less features compared to in_vgmstream and has no configuration.
Since XMPlay supports Winamp plugins you may also use `in_vgmstream.dll` instead.

Because the XMPlay MP3 decoder incorrectly tries to play some vgmstream extensions,
you need to manually fix it by going to **options > plugins > input > vgmstream**
and in the "priority filetypes" put: `ahx,asf,awc,ckd,fsb,genh,msf,p3d,rak,scd,txth,xvag`

XMPlay cannot support subsongs due to player limitations, try using *TXTP* instead
(explained below).

### foo_input_vgmstream
*Installation*: every file should be installed automatically by the `.fb2k-component`
bundle.

A known quirk is that when loop options or tags change, playlist info won't refresh
automatically. You need to manually refresh it by selecting songs and doing
**shift + right click > Tagging > Reload info from file(s)**.

### Audacious plugin
*Installation*: needs to be manually built. Instructions can be found in the BUILD
document in vgmstream's source code.

### vgmstream123
*Installation*: needs to be manually built. Instructions can be found in the BUILD
document in vgmstream's source code.

Usage: `vgmstream123 [options] INFILE ...`

The program is meant to be a simple stand-alone player, supporting playback
of vgmstream files through libao. Files compressed with gzip/bzip2/xz also
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
https://github.com/losnoco/vgmstream/tree/master/cli/txtp_maker.py
Put in the same dir as test.exe/vgmstream_cli, then to drag-and-drop files with
subsongs to `txtp_maker.py`.

### Renamed files
A few extensions that vgmstream supports clash with common ones. Since players
like foobar or Winamp don't react well to that, they may be renamed to make
them playable through vgmstream.
- `.aac` to `.laac` (tri-Ace games)
- `.ac3` to `.lac3` (standard AC3)
- `.aif` to `.laif` (standard Mac AIF, Asobo AIF, Ogg)
- `.aiff/aifc` to `.laiffl/laifc` (standard Mac AIF)
- `.asf` to `.lasf` (EA games, Argonaut ASF)
- `.bin` to `.lbin` (various)
- `.flac` to `.lflac` (standard FLAC)
- `.mp2` to `.lmp2` (standard MP2)
- `.mp3` to `.lmp3` (standard MP3)
- `.mp4` to `.lmp4` (standard M4A)
- `.mpc` to `.lmpc` (standard MPC)
- `.ogg` to `.logg` (standard OGG)
- `.opus` to `.lopus` (standard OPUS or Switch OPUS)
- `.stm` to `.lstm` (Rockstar STM)
- `.wav` to `.lwav` (standard WAV)
- `.wma` to `.lwma` (standard WMA)
- `.(any)` to `.vgmstream` (FFmpeg formats or TXTH)

Command line tools don't have this restriction and will accept the original
filename.

The main advantage to rename them is that vgmstream may use the file's
internal loop info, or apply subtle fixes, but is also limited in some ways
(like standard/player's tagging). `.vgmstream` is a catch-all extension that
may work as a last resort to make a file playable.

Some plugins have options that allow any extension (common or unknown) to be
played, making renaming unnecessary (may need to adjust plugin priority in
player's options).

Also be aware that some plugins can tell the player they handle some extension,
then not actually play it. This makes the file unplayable as vgmstream doesn't
even get the chance to parse that file, so you may need to disable the offending
plugin or rename the file (for example this may happen with .asf and foobar).

When extracting from a bigfile, sometimes internal files don't have an actual
name+extension. Those should be renamed to its proper/common extension, as the
extractor program may guess wrong (like .wav instead of .at3 or .wem). If
there is no known extension, usually the header id string may be used instead.

Note that vgmstream also accepts certain extension-less files too.

### Demuxed videos
vgmstream also supports audio from videos, but usually must be demuxed (extracted
without modification) first, since vgmstream doesn't attempt to support them.

The easiest way to do this is using VGMToolBox's "Video Demultiplexer" option
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
- `.mus`: playlist for `.acm`
- `.ogg.sli` or `.sli`: loop info for `.ogg`
- `.ogg.sfl` : loop info for `.ogg`
- `.opus.sli`: loop info for `.opus`
- `.pos`: loop info for .wav
- `.vgmstream.pos`: loop info for FFmpeg formats
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
bigger file (body), save a few formats where the smaller file is opened instead
for technical reasons (mainly some bank formats).

Generally companion files are named the same (`bgm.awb`+`bgm.acb`), or internally
point to another file `sfx.sb0`+`STREAM.sb0`. A few formats may have different names
which are hardcoded instead of being listed in the main file (e.g. `.mpf+.mus`).
In these cases, you can use *TXTM* format to specify associated companion files.
See *Artificial files* below for more information.

A special case of the above is "dual file stereo", where 2 similarly named mono
files are fused together to make 1 stereo song.
- `(file)_L.dsp`+`(file)_R.dsp`
- `(file)-l.dsp`+`(file)-l.dsp`
- `(file).L`+`(file).R`
- `(file)_0.dsp`+`(file)_1.dsp`
- `(file)_Left.dsp`+`(file)_Right.dsp`
- `(file).v0`+`(file).v1`
This is only allowed in a few formats (mainly `.dsp` and `.vag`). In those cases
you can open either `L` or `R` and you'll get the same stereo song. If you rename
one of the files the "pair" won't be found, and both will be played as mono.

`.pos` is a small file with 32 bit little endian values: loop start sample and
loop end sample. This is a real format, but is sometimes reused to force loops.
If you want to force looping files consider using *TXTP* instead, as it's much
simpler to make and cleaner: for example create a text file named `bgm01-loop.txtp`
and inside write `bgm01.mp3 #I 10.0 90.0`. Open the `.txtp` to play the `.mp3`
looping from 10 to 90 seconds.

#### OS case sensitiveness
When using OS with case sensitive filesystem (mainly Linux), a known issue with
companion files is that vgmstream generally tries to find them using lowercase
extension.

This means that if the developer used uppercase instead (e.g. `bgm.ABK`+`bgm.AST`)
loading will fail. It's technically complex to fix this, so for the time being
the only option is renaming the companion extension to lowercase.

A particularly nasty variation of that is that some formats load files by full
name (e.g. `STREAM.SS0`), but sometimes the actual filename is in other case
(`Stream.ss0`), and some files could even point to that with another case. You
could try adding *symlinks* in various upper/lower/mixed cases to handle this.
Currently there isn't any way to know what exact name is needed (other than
hex-editting), though only a few formats do this, mainly *Ubisoft* banks.

Regular formats without companion files should work fine in upper/lowercase.

### Decryption keys
Certain formats have encrypted data, and need a key to decrypt. vgmstream
will try to find the correct key from a list, but it can be provided by
a companion file:
- `.adx`: `.adxkey` (keystring, 8 byte keycode, or derived 6 byte start/mult/add key)
- `.ahx`: `.ahxkey` (derived 6 byte start/mult/add key)
- `.hca`: `.hcakey` (8 byte decryption key, a 64-bit number)
  - May be followed by 2 byte AWB scramble key for newer HCA
- `.fsb`: `.fsbkey` (decryption key, in hex)
- `.bnsf`: `.bnsfkey` (decryption key, a string up to 24 chars)

The key file can be `.(ext)key` (for the whole folder), or `(name).(ext)key"
(for a single file). The format is made up to suit vgmstream.

### Artificial files
In some cases a file only has raw data, while important header info (codec type,
sample rate, channels, etc) is stored in the .exe or other hard to locate places.

Those can be played using an artificial header with info vgmstream needs.

**GENH**: a byte header placed right before the original data, modyfing it.
The resulting file must be (name).genh. Contains static header data.
Programs like VGMToolbox can help to create *GENH*.

**TXTH**: a text header placed in an external file. The TXTH must be named
`.txth` or `.(ext).txth` (for the whole folder), or `(name.ext).txth` (for a
single file). Contains dynamic text commands to read data from the original
file, or static values.

*TXTH* is recomended over *GENH* as it's far easier to create and has many
more functions.

For files that already play, sometimes they are used by the game in various
complex and non-standard ways, like playing multiple small songs as a single
one, or using some channels as a section of the song. For those cases we
can create a *TXTP* file.

**TXTP**: text files with player configuration, named `(name).txtp`. Text inside
can contain a list of filenames to play as one (ex. `intro.vag(line)loop.vag`),
list of separate channel files to join as a single multichannel file,
subsong index (ex. `bgm.sxd#10`), per-file configurations like number of
loops, remove unneeded channels, make looping files, and many other features.

**TXTM**: text file named `.txtm` for formats with companion files. It lists
name combos determining which companion files to load for each main file.
It is useful for formats where name combos are hardcoded so vgmstream doesn't
know which companion file(s) to load if its name doesn't match the main file.
Note that companion file order is usually important.

Usage example:
```
# Harry Potter and the Chamber of Secrets (PS2)
entrance.mpf:entrance.mus,entrance_o.mus
willow.mpf:willow.mus,willow_o.mus
```
```
# Metal Gear Solid: Snake Eater 3D (3DS) names for .awb
bgm_2_streamfiles.awb: bgm_2.acb
```

Creation of those files is meant for advanced users, docs can be found in
vgmstream source.

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
can reduce the number of channels to a playable amount. Note that this type
of downmixing is very generic, not meant to be used when converting to other
formats (channels are re-assigned and volumes modified in simplistic ways,
since it can't guess how the file should be properly adjusted).

You can also choose which channels to play using *TXTP*. For example, create
a file named `song.adx#C1,2.txtp` to play only channels 1 and 2 from `song.adx`.
*TXTP* also has command to tweak how files is downmixed.


## Tagging
Some of vgmstream's plugins support simple read-only tagging via external files.

Tags are loaded from a text/M3U-like file named *!tags.m3u* in the song folder.
You don't have to load your songs with that M3U though (but you can, for pre-made
ordering), the file itself just 'looks' like an M3U.

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
Accepted tags depend on the player (foobar: any; winamp: see ATF config, Audacious:
few standard ones), typically *ALBUM/ARTIST/TITLE/DISC/TRACK/COMPOSER/etc*, lower
or uppercase, separated by one or multiple spaces. Repeated tags overwrite previous
(ex.- may define *@COMPOSER* multiple times for "sections"). It only reads up to
current *filename* though, so any *@TAG* below would be ignored.

Playlist title formatting should follow player's config. ASCII or UTF-8 tags work.

*GLOBAL_COMMAND*s currently can be:
- *AUTOTRACK*: sets *%TRACK* tag automatically (1..N as files are encountered
  in the tag file).
- *AUTOALBUM*: sets *%ALBUM* tag automatically using the containing dir as album.
- *EXACTMATCH*: disables matching .txtp with regular files (explained below).

Note that with global tags you don't need to put all files inside. This would be
a perfectly valid *!tags.m3u*:
```
# @ALBUM    Game
# @ARTIST   Various Artists
```

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
- *Winamp*: should refresh tags when file is played again.
- *foobar2000*: needs to force refresh (for reasons outside vgmstream's control)
  - **select songs > shift + right click > Tagging > Reload info from file(s)**.
- *Audacious*: files need to be readded to the playlist

Currently there is no tool to aid in the creation of these tags, but you can create
a base `.m3u` and edit as a text file.

vgmstream's "m3u tagging" is meant to be simple to make and share (just a text
file), easier to support in multiple players (rather than needing a custom plugin),
allow OST-like ordering but also combinable with other `.m3u`, and be flexible enough
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
non-existant files referenced in a `.m3u`, while other players may filter them
first.


## Supported codec types
Quick list of codecs vgmstream supports, including many obscure ones that
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
- DVI/IMA ADPCM (stereo/mono + high/low nibble, 3DS, Omikron, SNDS, etc)
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
- Ocean DSA 4-bit ADPCM
- Circus XPCM ADPCM
- Circus XPCM VQ
- OKI 4-bit ADPCM (16-bit output, 4-shift, PC-FX)
- Ubisoft 4/6-bit ADPCM
- Tiger Game.com ADPCM
- SDX2 2:1 Squareroot-Delta-Exact compression DPCM
- CBD2 2:1 Cuberoot-Delta-Exact compression DPCM
- Activision EXAKT SASSC DPCM
- Xilam DERF DPCM
- InterPlay ACM
- VisualArt's NWA
- Electronic Arts MicroTalk a.k.a. UTK or UMT
- Relic Codec
- CRI HCA
- Xiph Vorbis (Ogg, FSB5, Wwise, OGL, Silicon Knights)
- MPEG MP1/2/3 (standard, AHX, XVAG, FSB, AWC, P3D, etc)
- ITU-T G.722.1 annex C (Polycom Siren 14)
- ITU-T G.719 annex B (Polycom Siren 22)
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
	- .pos (loop info for .wav: 32 bit LE loop start sample + loop end sample)
	- .sli (loop info for .ogg)
	- .sfl (loop info for .ogg)
	- .vgmstream + .vgmstream.pos (FFmpeg formats + loop assist)
- other:
	- .adxkey (decryption key for .adx)
	- .ahxkey (decryption key for .ahx)
    - .hcakey (decryption key for .hca)
    - .fsbkey (decryption key for .fsb)
    - .bnsfkey (decryption key for .bnsf)
    - .txtp (per song segment/layer handler and player configurator)

Enjoy! *hcs*
