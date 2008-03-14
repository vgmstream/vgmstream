vgmstream

This is vgmstream, a library for playing streamed audio from video games.
It is very much under development. There are two end-user bits, a command
line decoder called "test", and a simple Winamp plugin called "in_vgmstream".

--- test ---
Usage: test [-o outfile.wav] [-l loop count]
        [-f fade time] [-i] [-p] [-c] [-m] infile
Options:
        -o outfile.wav: name of output .wav file, default is dump.wav
        -l loop count: loop count, default 2.0
        -f fade time: fade time (seconds), default 10.0
        -i: ignore looping information and play the whole stream once
        -p: output to stdout (for piping into another program)
        -c: loop forever (continuously)
        -m: print metadata only, don't decode
        -x: decode and print adxencd command line to encode as ADX

Typical usage would be:
test -o happy.wav happy.adx
to decode happy.adx to happy.wav.

--- in_vgmstream ---
Drop the in_vgmstream.dll in your Winamp plugins directory. There is no
configuration or seeking yet.

---
Formats supported by this version of vgmstream:
- .adx (CRI ADX ADPCM)
- .brstm (RSTM: GC/Wii DSP ADPCM, 8/16 bit PCM)
- .strm (STRM: NDS IMA ADPCM, 8/16 bit PCM)
- .adp (GC DTK ADPCM)
- .agsc (GC DSP ADPCM)
- .rsf (CCITT G.721 ADPCM)
- .afc (GC AFC ADPCM)
- .ast (GC/Wii AFC ADPCM, 16 bit PCM)
- .hps (GC DSP ADPCM)

Enjoy!
-hcs
