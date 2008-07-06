vgmstream

This is vgmstream, a library for playing streamed audio from video games.
It is very much under development. There are two end-user bits: a command
line decoder called "test", and a simple Winamp plugin called "in_vgmstream".

--- needed files (for Windows)  ---
Since Ogg Vorbis and MPEG audio are now supported, you will need to have
libvorbis.dll and libmpg123-0.dll.
I suggest getting libvorbis.dll here:
http://www.rarewares.org/files/ogg/libvorbis1.2.0.zip
and the companion Intel math dll:
http://www.rarewares.org/files/libmmd9.1.zip
And libmpg123-0.dll from this archive:
http://www.mpg123.de/download/win32/mpg123-1.4.3-x86.zip

Put libvorbis.dll, libmmd.dll, and libmpg123-0.dll somewhere Windows can find
them. For in_vgmstream this means in the directory with winamp.exe, or in a
system directory. For test.exe this means in the same directory as test.exe,
or in a system directory.

--- test.exe ---
Usage: test.exe [-o outfile.wav] [-l loop count]
    [-f fade time] [-d fade delay] [-ipPcmxeE] infile
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
    -e: force end-to-end looping
    -E: force end-to-end looping even if file has real loop points

Typical usage would be:
test -o happy.wav happy.adx
to decode happy.adx to happy.wav.

--- in_vgmstream ---
Drop the in_vgmstream.dll in your Winamp plugins directory.

---
File types supported by this version of vgmstream:
- .adx (CRI ADX ADPCM)
- .brstm (RSTM: GC/Wii DSP ADPCM, 8/16 bit PCM)
- .strm (STRM: NDS IMA ADPCM, 8/16 bit PCM)
- .adp (GC DTK ADPCM)
- .agsc (GC DSP ADPCM)
- .rsf (CCITT G.721 ADPCM)
- .afc (GC AFC ADPCM)
- .ast (GC/Wii AFC ADPCM, 16 bit PCM)
- .hps (GC DSP ADPCM)
- .dsp (GC DSP ADPCM)
  - standard, with dual file stereo
  - RS03
  - Cstr
  - .stm
  - _lr.dsp
- .gcsw (16 bit PCM)
- .ads/.ss2 (PSX ADPCM)
- .npsf (PSX ADPCM)
- .rwsd (Wii DSP ADPCM, 8/16 bit PCM)
- .xa (CD-ROM XA audio)
- .rxw (PSX ADPCM)
- .int (16 bit PCM)
- .stm/.dsp (GC DSP ADPCM)
- .sts (PSX ADPCM)
- .svag (PSX ADPCM)
- .mib, .mi4 (w/ or w/o .mih) (PSX ADPCM)
- .mpdsp (GC DSP ADPCM)
- .mic (PSX ADPCM)
- .mss (GC DSP ADPCM)
- .gcm (GC DSP ADPCM)
- .raw (16 bit PCM)
- .vag (PSX ADPCM)
- .gms (PSX ADPCM)
- .str+.sth (PSX ADPCM)
- .ild (PSX APDCM)
- .pnb (PSX ADPCM)
- .wavm (XBOX IMA ADPCM)
- .xwav (XBOX IMA ADPCM)
- .wp2 (PSX ADPCM)
- .str (GC DSP ADPCM)
- .sng, .asf, .str, .eam (EA/XA ADPCM or PSX ADPCM)
- .cfn (GC DSP ADPCM)
- .vpk (PSX ADPCM)
- .genh (PSX ADPCM, XBOX IMA ADPCM, GC DTK ADPCM, 8/16 bit PCM, SDX2, DVI)
- .ogg, .logg (Ogg Vorbis)
- .sad (GC DSP ADPCM)
- .bmdx (PSX ADPCM)
- .wsi (Wii DSP ADPCM)
- .aifc (SDX2 DPCM, DVI IMA ADPCM)
- .aiff (8/16 bit PCM)
- .str (SDX2 DPCM)
- .aud (IMA ADPCM, WS DPCM)
- .ahx (MPEG-2 Layer II)
- .ivb (PS2 ADPCM)

Enjoy!
-hcs
