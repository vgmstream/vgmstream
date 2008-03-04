vgmstream

This is vgmstream, a library for playing streamed audio from video games.
It is very much under development. The only end-user part right now is the test program, simply called "test" (test.exe for Windows), which decodes a file to a standard .wav output file.

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

Typical usage would be:
test -o happy.wav happy.adx

Formats supported by this version of vgmstream ($Revision$):
- .adx (CRI ADX ADPCM)
- .brstm (RSTM: GC/Wii DSP ADPCM, 8/16 bit PCM)
- .strm (STRM: NDS IMA ADPCM, 8/16 bit PCM)
- .adp (GC DTK ADPCM)
- .agsc (GC ADPCM)
- .rsf (CCITT G.721 ADPCM)
- .afc (GC AFC ADPCM)
- .ast (GC/Wii AFC ADPCM, 16 bit PCM)

Enjoy!
-hcs
