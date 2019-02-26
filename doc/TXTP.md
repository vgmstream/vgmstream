# TXTP FORMAT

TXTP is a text file with commands, to improve support for games using audio in certain uncommon or undesirable ways. It's in the form of a mini-playlist or a wrapper with play settings, meant to do post-processing over playable files.

Simply create a file named `(filename).txtp`, and inside write the commands described below.


## TXTP MODES
TXTP can join and play together multiple songs in various ways by setting a file list and mode:
```
file1
...
fileN

mode = (mode)  # "segments" is the default if not set
```
You can set commands to alter how files play (described later). Having a single file is ok too.


### Segments mode
Some games clumsily loop audio by using multiple full file "segments", so you can play separate intro + loop files together as a single track. Channel number must be equal, mixing sample rates is ok (uses first).
 
__Ratchet & Clank (PS2)__: _bgm01.txtp_
```
# define 2 or more segments to play as one
BGM01_BEGIN.VAG
BGM01_LOOPED.VAG

# segments must define loops
loop_start_segment = 2 # 2nd file start
loop_end_segment = 2 # optional, default is last
```

If your loop segment has proper loops you want to keep, you can use:
```
BGM_SUMMON_0001_01-Intro.hca
BGM_SUMMON_0001_01-Intro2.hca
BGM_SUMMON_0001_01.hca

loop_start_segment = 3
loop_mode = keep  # loops in 3rd file's loop_start to 3rd file's loop_end
```
```
bgm_intro.adx
bgm_main.adx
bgm_main2.adx

loop_start_segment = 2
loop_end_segment = 3
loop_mode = keep    # loops in 2nd file's loop_start to 3rd file's loop_end
```

### Layers mode
Some games layer channels or dynamic parts that must play at the same time, for example main melody + vocal track.

__Nier Automata__: _BGM_0_012_song2.txtp_
```
# mix dynamic sections (2ch * 2)
BGM_0_012_04.wem
BGM_0_012_07.wem

mode = layers
```

__Life is Strange__: _BIK_E1_6A_DialEnd.txtp_
```
# bik multichannel isn't autodetectable so must mix manually (1ch * 3)
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#1
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#2
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#3

mode = layers
```
Note that the number of channels is the sum of all layers, so three 2ch layers play as a 6ch file. If all layers share loop points they are automatically kept.


## TXTP COMMANDS
You can set file commands by adding multiple `#(command)` after the name. `# (anything)` is considered a comment and ignored, as well as any command not understood.

### Subsong selection for bank formats
**`#(number)` or `#s(number)`**: set subsong (number)

__Super Robot Taisen OG Saga - Masou Kishin III - Pride of Justice (Vita)__: _bgm_12.txtp_
```
# select subsong 12
bigfiles/bgm.sxd2#12 #relative paths are ok too for TXTP

#bigfiles/bgm.sxd2#s12 # "sN" is alt for subsong

# single files loop normally by default
# if loop segment is defined it forces a full loop (0..num_samples)
#loop_start_segment = 1
```

### Play segmented subsong ranges as one
**`#m(number)~(number)` or `#ms(number)~(number)`**: set multiple subsong segments at a time, to avoid so much C&P

__Prince of Persia Sands of Time__: _song_01.txtp_
```
amb_fx.sb0#254
amb_fx.sb0#122~144
amb_fx.sb0#121

#3rd segment = subsong 123, not 3rd subsong
loop_start_segment = 3
```
This is just a shorthand, so `song#1~3#h22050` is equivalent to:
```
song#1#h22050
song#2#h22050
song#3#h22050
```


### Channel mask for channel subsongs/layers
**`#c(number)`** (single) or **`#c(number)~(number)`** (range): set number of channels to play. You can add multiple comma-separated numbers, or use ` ` space or `-` as separator and combine multiple ranges with single channels too.

__Final Fantasy XIII-2__: _music_Home_01.ps3.txtp_
```
#plays channels 1 and 2 = 1st subsong
music_Home.ps3.scd#c1,2
```

```
#plays channels 3 and 4 = 2nd subsong
music_Home.ps3.scd#c3 4

#plays 1 to 3
music_Home.ps3.scd#c1~3
```
Doesn't change the final number of channels though, just mutes non-selected channels.


### Custom play settings
**`#l(loops)`**, **`#f(fade)`**, **`#d(fade-delay)`**, **`#i(ignore loop)`**, **`#F(ignore fade)`**, **`#E(end-to-end loop)`**

Those setting should override player's defaults if set (except "loop forever"). They are equivalent to some test.exe options.

__God Hand (PS2)__: _boss2_3ningumi_ver6.txtp_ (each line is a separate TXTP)
```
# set number of loops
boss2_3ningumi_ver6.adx#l3

# set fade time (in seconds)
boss2_3ningumi_ver6.adx#f10.5

# set fade delay (in seconds)
boss2_3ningumi_ver6.adx#d0.5

# ignore and disable loops
boss2_3ningumi_ver6.adx#i

# don't fade out and instead play the song ending after
boss2_3ningumi_ver6.adx#F  # this song has a nice stop

# force full loops from end-to-end
boss2_3ningumi_ver6.adx#E

# settings can be combined
boss2_3ningumi_ver6.adx#l2#F  # 2 loops + ending

# settings can be combined
boss2_3ningumi_ver6.adx#l1.5#d1#f5

# boss2_3ningumi_ver6.adx#l1.0#F  # this is equivalent to #i
```


### Force sample rate
**`#h(sample rate)`**: for a few games that set a sample rate value in the header but actually play with other (applying some of pitch or just forcing it).

__Super Paper Mario (Wii)__
```
btl_koopa1_44k_lp.brstm#h22050  #in hz
```
__Patapon (PSP)__
```
ptp_btl_bgm_voice.sgd#s1#h11050
```


## OTHER FEATURES

### Default commands
You can set defaults that apply to the *resulting* file. This has subtle differences vs per-file config:
```
BGM01_BEGIN.VAG
BGM01_LOOPED.VAG

# force looping from begin to end of the whole thing
commands = #E
```
```
# mix 2ch * 2
BGM_0_012_04.wem
BGM_0_012_07.wem
mode = layers

# plays R of BGM_0_012_04 and L of BGM_0_012_07
commands = #c2,3
```

As it applies at the end, some options with ambiguous or technically hard to handle meanings may be ignored:
```
bgm.sxd2
bgm.sxd2

# ignored (resulting file has no subsongs, or should apply to all?)
commands = #s12
```

### Force plugin extensions
vgmstream supports a few common extensions that confuse plugins, like .wav/ogg/aac/opus/etc, so for them those extensions are disabled and are expected to be renamed to .lwav/logg/laac/lopus/etc. TXTP can make plugins play those disabled extensions, since it calls files directly by filename.

Combined with TXTH, this can also be used for extensions that aren't normally accepted by vgmstream.


### TXTP combos
TXTP may even reference other TXTP, or files that require TXTH, for extra complex cases. Each file defined in TXTP is internally parsed like it was a completely separate file, so there is a bunch of valid ways to mix them.


### TXTP parsing
*Filenames* may be anything accepted by the file system, including spaces and symbols, and multiple *commands* can be chained:
```
bgm bank#s2#c1,2
```

You may add spaces as needed (but try to keep it simple and don't go overboard), though commands *must* start with `#(command)` (`#(space)(anything)` is a comment). Commands without corresponding file are ignored too (seen as comments too), while incorrect commands are ignored and skip to next, though the parser may try to make something usable of them (this may be change anytime without warning):
```
# those are all equivalent
song#s2#c1,2
song    #s2#c1,2   #    comment
song    #s 2  #c1,2# comment
song    #s 2 #c   1   ,  2# comment

#s2  #ignores rogue commands/comments

# seen as incorrect and ignored
song    #s TWO
song    #E enable
song    #E 1
song    #Enable
song    #h -48000

# accepted
song    #E # comment
song    #c1, 2, 3
song    #c  1  2  3

# ignores first and reads second
song    #s TWO#c1,2

# seen as #s1#c1,2
song    #s 1,2 #c1,2

# all seen as #h48000
song    #h48000
song    #h 48000hz
song    #h 48000mhz

# ignored
song    #h hz48000

# ignored as channels don't go that high (may be modified on request)
song    #c32,41

# swaps 1 with 2
song    #m1-2
song    #m 1 - 2

# swaps 1 with "-2", ignored
song    #m1 -2
```

*Values* found after *=* allow spaces as well:
```
song#s2
loop_start_segment   =    1  #s2# #commands here are ignored

song
commands=#s2   # commands here are allowed
commands= #c1,2
```

Repeated commands overwrite previous setting, except comma-separated commands that are additive:
```
# overwrites, equivalent to #s2
song#s1#s2

# adds, equivalent to #m1-2,3-4,5-6
song#m1-2#m3-4
commands = #m5-6
```

The parser is fairly simplistic and lax, and may be erratic with edge cases or behave unexpectedly due to unforeseen use-cases and bugs. As filenames may contain spaces or #, certain name patterns could fool it too. Keep in mind this while making .txtp files.


## MINI-TXTP
To simplify TXTP creation, if the .txtp is empty (0 bytes) its filename is used directly as a command. Note that extension is also included (since vgmstream needs a full filename).
- _bgm.sxd2#12.txtp_: plays subsong 12
- _Ryoshima Coast 1 & 2.aix#c1,2.txtp_: channel mask
- _boss2_3ningumi_ver6.adx#l2#F.txtp_: loop twice then play song end file normally
- etc
