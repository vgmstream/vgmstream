vgmstream

This is vgmstream, a library for playing streamed audio from video games.
It is very much under development. There are two end-user bits: a command
line decoder called "test.exe", and a Winamp plugin called "in_vgmstream".

*********** IMPORTANT!! ***********
--- needed files (for Windows)  ---
Since Ogg Vorbis and MPEG audio are now supported, you will need to have
libvorbis.dll and libmpg123-0.dll.
You can get these from http://hcs64.com/files/vgmstream_external_dlls.zip

Put libvorbis.dll and libmpg123-0.dll somewhere Windows can find them.
For in_vgmstream this means in the directory with winamp.exe, or in a
system directory. For test.exe this means in the same directory as test.exe,
or in a system directory.

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
    -e: force end-to-end looping
    -E: force end-to-end looping even if file has real loop points
    -r outfile2.wav: output a second time after resetting

Typical usage would be:
test -o happy.wav happy.adx
to decode happy.adx to happy.wav.

--- in_vgmstream ---
Drop the in_vgmstream.dll in your Winamp plugins directory. Please follow
the above instructions for installing the other files needed.

--- File types supported by this version of vgmstream ---

As manakoAT likes to say, the extension doesn't really mean anything, but it's
the most obvious way to identify files.

PS2/PSX ADPCM:
- .ads/.ss2
- .ass
- .bg00
- .bmdx
- .ccc
- .cnk
- .dxh
- .enth
- .fag
- .filp
- .gms
- .hgc1
- .ikm
- .ild
- .ivb
- .joe
- .kces
- .leg
- .mcg
- .mib, .mi4 (w/ or w/o .mih)
- .mic
- .mihb (merged mih+mib)
- .msvp
- .musc
- .musx
- .npsf
- .pnb
- .psh
- .rkv
- .rnd
- .rstm
- .rws
- .rxw
- .snd
- .seg
- .sfs
- .sl3
- .str+.sth
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

GC/Wii DSP ADPCM:
- .agsc
- .amts
- .asr
- .bns
- .cfn
- .dsp
  - standard, with dual file stereo
  - RS03
  - Cstr
  - _lr.dsp
- .gca
- .gcm
- .gsp+.gsp
- .hps
- .idsp
- .ish+.isd
- .lps
- .mpdsp
- .mss
- .mus (not quite right)
- .ndp
- .pdt
- .sdt
- .smp
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
- .gcsw (16 bit)
- .gcw (16 bit)
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
- .dcs+.wav
- .str
- .spsd

IMA ADPCM:
- .dvi (DVI IMA ADPCM)
- .hwas (IMA ADPCM)
- .idvi (DVI IMA ADPCM)
- .ivaud (IMA ADPCM)
- .stma (DVI IMA ADPCM)
- .strm (IMA ADPCM)

multi:
- .aifc (SDX2 DPCM, DVI IMA ADPCM)
- .asf, .as4 (8/16 bit PCM, EACS IMA ADPCM)
- .ast (GC AFC ADPCM, 16 bit PCM)
- .aud (IMA ADPCM, WS DPCM)
- .aus (PSX ADPCM, Xbox IMA ADPCM)
- .brstm (GC DSP ADPCM, 8/16 bit PCM)
- .emff (PSX APDCM, GC DSP ADPCM)
- .fsb, .wii (PSX ADPCM, GC DSP ADPCM, Xbox IMA ADPCM)
- .genh (lots)
- .nwa (16 bit PCM, NWA DPCM)
- .psw (PSX ADPCM, GC DSP ADPCM)
- .rwar, .rwav (GC DSP ADPCM, 8/16 bit PCM)
- .rwsd (GC DSP ADPCM, 8/16 bit PCM)
- .rsd (PSX ADPCM, 16 bit PCM, GC DSP ADPCM, Xbox IMA ADPCM)
- .rrds (NDS IMA ADPCM)
- .sad (GC DSP ADPCM, NDS IMA ADPCM, Procyon Studios NDS ADPCM)
- .sng, .asf, .str, .eam (EA/XA ADPCM or PSX ADPCM)
- .strm (NDS IMA ADPCM, 8/16 bit PCM)
- .ss7 (EACS IMA ADPCM, IMA ADPCM)
- .swav (NDS IMA ADPCM, 8/16 bit PCM)
- .xwb (16 bit PCM, Xbox IMA ADPCM)
- .wav, .lwav (unsigned 8 bit PCM, 16 bit PCM, GC DSP ADPCM, MS IMA ADPCM)

etc:
- .2dx (MS ADPCM)
- .aax (CRI ADX ADPCM)
- .acm (InterPlay ACM)
- .adp (GC DTK ADPCM)
- .adx (CRI ADX ADPCM)
- .afc (GC AFC ADPCM)
- .ahx (MPEG-2 Layer II)
- .aix (CRI ADX ADPCM)
- .caf (Apple IMA4 ADPCM)
- .bgw (FFXI PS-like ADPCM)
- .de2 (MS ADPCM)
- .kcey (EACS IMA ADPCM)
- .mwv (Level-5 0x555 ADPCM)
- .ogg, .logg (Ogg Vorbis)
- .rsf (CCITT G.721 ADPCM)
- .sab (Worms 4 soundpacks)
- .sc (Activision EXAKT SASSC DPCM)
- .sd9 (MS ADPCM)
- .spw (FFXI PS-like ADPCM)
- .str (SDX2 DPCM)
- .stx (GC AFC ADPCM)
- .um3 (Ogg Vorbis)
- .xa (CD-ROM XA audio)

loop assists:
- .mus (playlist for .acm)
- .pos (loop info for .wav)
- .sli (loop info for .ogg)
- .sfl (loop info for .ogg)

Enjoy!
-hcs
