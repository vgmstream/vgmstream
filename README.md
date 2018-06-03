# vgmstream

[![AppVeyor build status](https://ci.appveyor.com/api/projects/status/github/kode54/vgmstream?branch=master&svg=true "Build Status")](https://ci.appveyor.com/project/kode54/vgmstream/branch/master/artifacts)


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

Latest development is usually here: https://github.com/kode54/vgmstream/

## Needed extra files (for Windows)
Support for some codecs (Ogg Vorbis, MPEG audio, etc) is done with external
libraries, so you will need to have certain DLL files.

In the case of the foobar2000 component they are all bundled for convenience,
or you can get them here: https://github.com/kode54/vgmstream/tree/master/ext_libs
(bundled here: https://f.losno.co/vgmstream-win32-deps.zip, may not be latest).

Put ```libvorbis.dll```, ```libmpg123-0.dll```, ```libg7221_decode.dll```, ```libg719_decode.dll```,
```avcodec-vgmstream-58.dll```, ```avformat-vgmstream-58.dll```, ```avutil-vgmstream-56.dll```, ```swresample-vgmstream-3.dll```
and ```libatrac9.dll``` somewhere Windows can
find them.

For Winamp/XMPlay/command line this means in the directory with the main .exe,
or in a system directory, or any other directory in the PATH variable.

### test.exe/vgmstream-cli
```
Usage: test.exe [-o outfile.wav] [options] infile
Options:
    -o outfile.wav: name of output .wav file, default is infile.wav
    -l loop count: loop count, default 2.0
    -f fade time: fade time (seconds), default 10.0
    -d fade delay: fade delay (seconds, default 0.0
    -i: ignore looping information and play the whole stream once
    -p: output to stdout (for piping into another program)
    -P: output to stdout even if stdout is a terminal
    -c: loop forever (continuously)
    -m: print metadata only, don't decode
    -x: decode and print adxencd command line to encode as ADX
    -g: decode and print oggenc command line to encode as OGG
    -b: decode and print batch variable commands
    -L: append a smpl chunk and create a looping wav
    -e: force end-to-end looping
    -E: force end-to-end looping even if file has real loop points
    -r outfile2.wav: output a second time after resetting
    -2 N: only output the Nth (first is 0) set of stereo channels
    -F: don't fade after N loops and play the rest of the stream
    -s N: select subtream N, if the format supports multiple streams
```
Typical usage would be: ```test -o happy.wav happy.adx``` to decode ```happy.adx``` to ```happy.wav```.

Please follow the above instructions for installing the other files needed.

### in_vgmstream
Drop the ```in_vgmstream.dll``` in your Winamp plugins directory. Please follow
the above instructions for installing the other files needed.

### xmp-vgmstream
Drop the ```xmp-vgmstream.dll``` in your XMPlay plugins directory. Please follow
the above instructions for installing the other files needed.

Note that there are minor differences compared to in_vgmstream. Since XMPlay
supports Winamp plugins you may also use ```in_vgmstream.dll``` instead.

Because the XMPlay MP3 decoder incorrectly tries to play some vgmstream exts,
you need to manually fix it by going to **options > plugins > input > vgmstream**
and in the "priority filetypes" put: ```ahx,asf,awc,ckd,fsb,genh,msf,p3d,rak,scd,txth,xvag```

### foo_input_vgmstream
Every file should be installed automatically by the .fb2k-component bundle.

### Audacious plugin
Needs to be manually built. Instructions can be found in the BUILD document.

### vgmstream123
Needs to be manually built. Instructions can be found in the BUILD document.

```
Usage: vgmstream123 [options] INFILE ...
```

The program is meant to be a simple stand-alone player, supporting playback
of vgmstream files through libao. Files compressed with gzip/bzip2/xz also
work, as identified by a .gz/.bz2/.xz extension. The file will be decompressed
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
vgmstream aims to support most file formats as-is, but some files require extra
handling.

### Renamed files
A few extensions that vgmstream supports clash with common ones. Since players
like foobar or Winamp don't react well to that, they may be renamed for
vgmstream (mainly to get looping in some cases).
- .aac to .laac
- .ac3 to .lac3
- .aif to .aiffl or .aifcl 
- .asf to .sng (EA formats)
- .mp4 to .lmp4
- .ogg to .logg
- .opus to .lopus
- .stm to .lstm
- .wav to .lwav
Command line tools don't have this restriction and will accept the original
filename.

When extracting from a bigfile sometimes internal files don't have an actual
name+extension. Those should be renamed to its proper/common extension, as the
extractor program may guess wrong (like .wav instead of .at3 or .wem). If
there is no known extension usually the header id is used instead.

### Loop assists
Some formats have companion files with external looping info, and should be
left together.
- .mus (playlist for .acm)
- .pos (loop info for .wav: 32 bit LE loop start sample + loop end sample)
- .sli (loop info for .ogg)
- .sfl (loop info for .ogg)
- .vgmstream + .pos (FFmpeg formats + loop assist)

### Decryption keys
Certain formats have encrypted data, and need a key to decrypt. vgmstream
will try to find the correct key from a list, but it can be provided by
a companion file:
- .adx: .adxkey (derived 6 byte key, in start/mult/add format)
- .ahx: .ahxkey (derived 6 byte key, in start/mult/add format)
- .hca: .hcakey (8 byte decryption key, a 64-bit number)
- .fsb: .fsbkey (decryption key, in hex)

The key file can be ".(ext)key" (for the whole folder), or "(name).(ext)key"
(for a single file). The format is made up to suit vgmstream.

### Artificial/generic headers
In some cases a file only has raw data, while important header info (codec type,
sample rate, channels, etc) is stored in the .exe or other hard to locate places.

Those can be played using an artificial header with info vgmstream needs.

**GENH**: a byte header placed right before the original data, modyfing it.
The resulting file must be (name).genh. Contains static header data.
Programs like VGMToolbox can help to create GENH.
  
**TXTH**: a text header placed in an external file. The TXTH must be named
".txth" or ".(ext).txth" (for the whole folder), or "(name.ext).txth" (for a
single file). Contains dynamic text commands to read data from the original
file, or static values.
  
**TXTP**: a text playlist that works as a single song. Can contain a list of
filenames to play as one (ex. "intro.vag" "loop.vag"), name with subsong index
(ex. bgm.sxd#10), or mask channels to only play some (ex. "song.adx#c1,2").


## Supported codec types
Quick list of codecs vgmstream supports, including many obscure ones that 
are used in few games.

- PCM 16-bit
- PCM 8-bit (signed/unsigned)
- PCM 32-bit float
- u-Law/a-LAW
- CRI ADX (standard, fixed, exponential, encrypted)
- Nintendo DSP ADPCM a.k.a GC ADPCM
- Nintendo DTK ADPCM
- Nintendo AFC ADPCM
- ITU-T G.721
- CD-ROM XA ADPCM
- Sony PSX ADPCM a.k.a VAG (standard, badflags, bmdx, configurable)
- Sony HEVAG
- Electronic Arts EA-XA (stereo, mono, Maxis)
- Electronic Arts EA-XAS
- DVI/IMA ADPCM (stereo/mono + high/low nibble, 3DS, Omikron, SNDS, etc)
- Microsoft MS IMA ADPCM (standard, Xbox, NDS, Radical, Wwise, FSB, etc)
- Microsoft MS ADPCM
- Westwood VBR ADPCM 
- Yamaha AICA ADPCM 
- Procyon Studio ADPCM 
- Level-5 0x555 ADPCM
- Activision EXAKT SASSC DPCM
- lsf ADPCM
- Konami MTAF ADPCM
- Konami MTA2 ADPCM
- Paradigm MC3 ADPCM
- SDX2 2:1 Squareroot-Delta-Exact compression DPCM
- CBD2 2:1 Cuberoot-Delta-Exact compression DPCM
- InterPlay ACM
- VisualArt's NWA
- CRI HCA
- Electronic Arts MicroTalk a.k.a. UTK or UMT
- FMOD FADPCM 4-bit ADPCM
- Xiph Vorbis (Ogg, FSB5, Wwise, OGL, Silicon Knights)
- MPEG MP1/2/3 (standard, AHX, XVAG, FSB, AWC, P3D, etc)
- ITU-T G.722.1 (Polycom Siren 7)
- ITU-T G.722.1 annex C (Polycom Siren 14)
- ITU G.719 annex B (Polycom Siren 22)
- Electronic Arts EALayer3
- Electronic Arts EA-XMA
- Sony ATRAC3, ATRAC3plus
- Sony ATRAC9
- Microsoft XMA1/2
- Microsoft WMA v1, WMA v2, WMAPro
- AAC
- Bink
- AC3/SPDIF
- Xiph Opus (Ogg, Switch)
- Musepack
- FLAC
- Others

Note that vgmstream doesn't (can't) reproduce in-game music 1:1, as internal
resampling, filters, volume, etc, are not replicated. Some codecs are not
fully accurate compared to the games due to minor bugs, but in most cases
it isn't audible. 


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
- Yamaha ADPCM:
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
- artificial/generic headers:
    - .genh (lots)
    - .txth (lots)
- loop assists:
	- .mus (playlist for .acm)
	- .pos (loop info for .wav: 32 bit LE loop start sample + loop end sample)
	- .sli (loop info for .ogg)
	- .sfl (loop info for .ogg)
- other:
	- .adxkey (decryption key for .adx, in start/mult/add format)
	- .ahxkey (decryption key for .ahx, in start/mult/add format)
    - .hcakey (decryption key for .hca, in HCA Decoder format)
    - .fsbkey (decryption key for .fsb, in hex)
	- .vgmstream + .pos (FFmpeg formats + loop assist)

Enjoy! *hcs*
