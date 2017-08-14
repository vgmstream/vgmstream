vgmstream

This is vgmstream, a library for playing streamed audio from video games.
There are multiple end-user bits:
- a command line decoder called "test.exe"
- a Winamp plugin called "in_vgmstream"
- a foobar2000 component called "foo_input_vgmstream"
- an XMPlay plugin called "xmp-vgmstream"
- an Audacious plugin called "libvgmstream"

*********** IMPORTANT!! ***********
--- needed files (for Windows)  ---
Since Ogg Vorbis, MPEG audio, and other formats are now supported, you will
need to have certain DLL files.
In the case of the foobar2000 component they are all bundled for convenience,
or you can get them from here: https://github.com/kode54/vgmstream
(also here: https://f.losno.co/vgmstream-win32-deps.zip, may not be latest).

Put libvorbis.dll, libmpg123-0.dll, libg7221_decode.dll, libg719_decode.dll,
at3plusdecoder.dll, avcodec-vgmstream-57.dll, avformat-vgmstream-57.dll, and
avutil-vgmstream-55.dll somewhere Windows can find them.
For Winamp/XMPlay/test.exe this means in the directory with the .exe, or in a
system directory, or any other directory in the PATH variable.

--- test.exe ---
Usage: ./test [-o outfile.wav] [-l loop count]
    [-f fade time] [-d fade delay] [-ipcmxeE] infile
Options:
    -o outfile.wav: name of output .wav file, default is dump.wav
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

Typical usage would be:
test -o happy.wav happy.adx
to decode happy.adx to happy.wav.

--- in_vgmstream ---
Drop the in_vgmstream.dll in your Winamp plugins directory. Please follow
the above instructions for installing the other files needed.

--- xmp-vgmstream ---
Drop the xmp-vgmstream.dll in your XMPlay plugins directory. Please follow
the above instructions for installing the other files needed.

Note that there are minor differences compared to in_vgmstream. Since XMPlay
supports Winamp plugins you may also use in_vgmstream.dll instead.

Because the XMPlay MP3 decoder incorrectly tries to play some vgmstream exts,
you need to manually fix it by going to options > plugins > input > vgmstream
and in the "priority filetypes" put: ckd,fsb,genh,msf,p3d,rak,scd,xvag

--- foo_input_vgmstream ---
Every should be installed automatically by the .fb2k-component bundle.

--- Audacious plugin ---
Needs to be manually built. Instructions can be found in the source files.

--- File types supported by this version of vgmstream ---

As manakoAT likes to say, the extension doesn't really mean anything, but it's
the most obvious way to identify files.

This list is not complete and many other files are supported.

PS2/PSX ADPCM:
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

GC/Wii/3DS DSP ADPCM:
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

PCM:
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

Xbox IMA ADPCM:
- .matx
- .wavm
- .wvs
- .xmu
- .xvas
- .xwav

Yamaha ADPCM:
- .adpcm
- .dcs+.dcsw
- .str
- .spsd

IMA ADPCM:
- .bar (IMA ADPCM)
- .dvi (DVI IMA ADPCM)
- .hwas (IMA ADPCM)
- .idvi (DVI IMA ADPCM)
- .ivaud (IMA ADPCM)
- .myspd (IMA ADPCM)
- .strm (IMA ADPCM)

multi:
- .aifc (SDX2 DPCM, DVI IMA ADPCM)
- .asf/as4 (8/16 bit PCM, EACS IMA ADPCM)
- .ast (GC AFC ADPCM, 16 bit PCM)
- .aud (IMA ADPCM, WS DPCM)
- .aus (PSX ADPCM, Xbox IMA ADPCM)
- .brstm (GC DSP ADPCM, 8/16 bit PCM)
- .emff (PSX APDCM, GC DSP ADPCM)
- .fsb/wii (PSX ADPCM, GC DSP ADPCM, Xbox IMA ADPCM, MPEG audio, FSB Vorbis, 
  MS XMA)
- .genh (lots)
- .txth (lots)
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
- .sng/asf/str/eam/aud (8/16 bit PCM, EA-XA ADPCM, PSX ADPCM, GC DSP ADPCM,
   XBOX IMA ADPCM, MPEG audio, EALayer3)
- .strm (NDS IMA ADPCM, 8/16 bit PCM)
- .ss7 (EACS IMA ADPCM, IMA ADPCM)
- .swav (NDS IMA ADPCM, 8/16 bit PCM)
- .xwb (PCM, Xbox IMA ADPCM, MS ADPCM, XMA, XWMA, ATRAC3)
- .xwb+xwh (PCM, PSX ADPCM, ATRAC3)
- .wav/lwav (unsigned 8 bit PCM, 16 bit PCM, GC DSP ADPCM, MS IMA ADPCM,
   XBOX IMA ADPCM)
- .wem [lwav/logg/xma] (PCM, Wwise Vorbis, Wwise IMA ADPCM, XMA, XWMA,
   GC DSP ADPCM)

etc:
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
- .de2 (MS ADPCM)
- .hca (CRI High Compression Audio)
- .kcey (EACS IMA ADPCM)
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

loop assists:
- .mus (playlist for .acm)
- .pos (loop info for .wav: 32 bit LE loop start sample + loop end sample)
- .sli (loop info for .ogg)
- .sfl (loop info for .ogg)

other:
- .adxkey (decryption key for .adx, in start/mult/add format)
- .ahxkey (decryption key for .ahx, in start/mult/add format)
- .hcakey (decryption key for .hca, in HCA Decoder format)
- .vgmstream + .pos (FFmpeg formats + loop assist)

Enjoy!
-hcs
