vgmstream

This is vgmstream, a library for playing streamed audio from video games.
It is very much under development. There are two end-user bits, a command
line decoder called "test", and a simple Winamp plugin called "in_vgmstream".

--- test ---
Usage: test.exe [-o outfile.wav] [-l loop count]
    [-f fade time] [-ipcmxeE] infile
Options:
    -o outfile.wav: name of output .wav file, default is dump.wav
    -l loop count: loop count, default 2.0
    -f fade time: fade time (seconds), default 10.0
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
Drop the in_vgmstream.dll in your Winamp plugins directory. There is no
configuration or seeking yet.

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

Enjoy!
-hcs
