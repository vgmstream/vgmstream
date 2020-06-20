# TXTP FORMAT

TXTP is a text file with commands, to improve support for games using audio in certain uncommon or undesirable ways. It's in the form of a mini-playlist or a wrapper with play settings, meant to do post-processing over playable files.

Simply create a file named `(filename).txtp`, and inside write the commands described below.


## TXTP MODES
TXTP can join and play together multiple songs in various ways by setting a file list and mode:
```
file1
...
fileN

mode = (segments|layers|mixed)  # "segments" is the default if not set
```

You can set commands to alter how files play (described later), and having a single file is ok too:
```
file#12     # set "subsong" command for single file

#mode is ignored here as there is only one file
``` 

*files* may be anything accepted by the file system (including spaces and symbols), and you can use subdirs. Separators can be `\` or `/`, but stick to `/` for consistency. Commands may be chained and use spaces as needed (also see "TXTP parsing" section).
```
sounds/bgm.fsb #s2 #i  #for file inside subdir: play subsong 2 + disable looping
```


### Segments mode
Some games clumsily loop audio by using multiple full file "segments", so you can play separate intro + loop files together as a single track.
 
**Ratchet & Clank (PS2)**: *bgm01.txtp*
```
# define 2 or more segments to play as one
BGM01_BEGIN.VAG
BGM01_LOOPED.VAG

# segments must define loops
loop_start_segment = 2 # 2nd file start
loop_end_segment = 2 # optional, default is last
mode = segments # optional, default is segments
```
You can (should) set looping to the last segment like this:
```
BGM01_BEGIN.VAG
BGM01_LOOPED.VAG

# equivalent to loop_start_segment = 2, loop_end_segment = 2
# (only for multiple segments, to repeat a single file use #E)
loop_mode = auto
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
Mixing sample rates is ok (uses first) but channel number must be equal for all files. You can use mixing (explained later) to join segments of different channels though.


### Layers mode
Some games layer channels or dynamic parts that must play at the same time, for example main melody + vocal track.

**Nier Automata**: *BGM_0_012_song2.txtp*
```
# mix dynamic sections (2ch * 2)
BGM_0_012_04.wem
BGM_0_012_07.wem

mode = layers
```

**Life is Strange**: *BIK_E1_6A_DialEnd.txtp*
```
# bik multichannel isn't autodetectable so must mix manually (1ch * 3)
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#1
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#2
BIK_E1_6A_DialEnd_00000000.audio.multi.bik#3

mode = layers
```
Note that the number of channels is the sum of all layers, so three 2ch layers play as a 6ch file (you can downmix to stereo using mixing commands, described later). If all layers share loop points they are automatically kept.

### Mixed groups
You can set "groups" to 'fold' various files into one, as layers or segments, to allow complex cases:
```
# commands to make two 6ch segments with layered intro + layered loop:

# - set introA+B+C as layer (this group becomes position 1, and loopA_2ch position 2)
introA_2ch.at3  #position 1
introB_2ch.at3
introC_2ch.at3
group = 1L3

# - set loopA+B+C as layer (this group becomes position 2)
loopA_2ch.at3   #position 4
loopB_2ch.at3
loopC_2ch.at3
group = 2L3

# - play both as segments (this step is optional if using mode = segments)
group = S2

# - set loop start loopA+B+C (new position 2, not original position 4)
loop_start_segment = 2

# optional, to avoid "segments" default (for debugging)
mode = mixed
```
From TXTP's perspective, it starts with N separate files and every command joins some files that are treated as a single new file, so positions are reassigned. End result will be a single "file" that may contain groups within groups. It's pretty flexible so you can express similar things in various ways:
```
# commands to make a 6ch with segmented intro + loop:
introA_2ch.at3
mainA_2ch.at3

introB_2ch.at3
mainB_2ch.at3

introC_2ch.at3
mainC_2ch.at3

# - group intro/main pairs as segments, starting from 1 and repeating for A/B/C
group = S2R

# - play all as layer (can't set loop_start_segment in this case)
mode = layers

# you could also set: group = L and mode = mixed, same thing
```

`group` can go anywhere in the .txtp, as many times as needed (groups are read and kept in an list that is applied in order at the end). Format is `(position)(type)(count)(repeat)`:
- `position`: file start (optional, default is 1 = first)
- `type`: group as S=segments or L=layers
- `count`: number of files in group (optional, default is all)
- `repeat`: R=repeat group of `count` files until end (optional, default is no repeat)

Examples:
- `L`: take all files as layers (equivalent to `mode = layers`)
- `S`: take all files as segments (equivalent to `mode = segments`)
- `3L2`: layer 2 files starting from file 3
- `2L3R`: group every 3 files from position 2 as layers
- `1S1`: segment of one file (useless thus ignored)
- `1L1`: layer of one file (same)
- `9999L`: absurd values are ignored

Segments and layer settings and rules still apply, so you can't make segments of files with different total channels. To do it you can use commands to "downmix" the group, as well as giving it some config (explained later):
```
# this doesn't need to be grouped
intro_2ch.at3

# this is grouped into a single 4ch file, then downmixed to stereo
mainA_2ch.at3
mainB_2ch.at3
group = 2L2 #@layer-v 2

# finally resulting layers are played as segments (2ch, 2ch)
# (could set a group = S and ommit mode here, too)
mode = segments

# if the last group joins all as segments you can use loop_start
loop_start_segment = 3 #refers to final group at position 2
loop_mode = keep
```


## TXTP COMMANDS
You can set file commands by adding multiple `#(command)` after the name. `#(space)(anything)` is considered a comment and ignored, as well as any command not understood.

### Subsong selection for bank formats
**`#(number)` or `#s(number)`**: set subsong (number)

**Super Robot Taisen OG Saga: Masou Kishin III - Pride of Justice (Vita)**: *bgm_12.txtp*
```
# select subsong 12
bgm.sxd2#12

#bgm.sxd2#s12 # "sN" is alt for subsong

# single files loop normally by default (see below to force looping)
```

### Play segmented subsong ranges as one
**`#(number)~(number)` or `#s(number)~(number)`**: set multiple subsong segments at a time, to avoid so much C&P.

**Prince of Persia Sands of Time**: *song_01.txtp*
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


### Channel removing/masking for channel subsongs/layers
**`C(number)`** (single) or **`#C(number)~(number)`** (range), **`#c(number)`**: set number of channels to play. You can add multiple comma-separated numbers, or use ` ` space or `-` as separator and combine multiple ranges with single channels too.

If you use **`C(number)`** (uppercase) it will remove non-selected channels. This just a shortcut for macro `#@track` (described later):

If you use **`c(number)`** (lowercase) it doesn't change the final number of channels, just mutes non-selected channels (for backwards compatibility).

**Final Fantasy XIII-2**: *music_Home_01.ps3.txtp*
```
#plays channels 1 and 2 = 1st subsong
music_Home.ps3.scd#C1,2
```
```
#plays channels 3 and 4 = 2nd subsong (muting 1 and 2)
music_Home.ps3.scd#c3 4

#plays 1 to 3
music_Home.ps3.scd#C1~3
```


### Play settings
**`#L`**: play forever (if loops are set and player supports it)
**`#l(loops)`**: set target number of loops (if file loops)
**`#f(fade time)`**: set (in seconds) how long the fade out lasts (if file loops)
**`#d(fade delay)`**: set (in seconds) the delay before fade kicks in after last loop (if file loops)
**`#F`**: don't fade out after N loops but continue playing the song's original end (if file loops)
**`#e`**: set full looping (end-to-end) but only if file doesn't have loop points (mainly for autogenerated .txtp)
**`#E`**: force full looping (end-to-end), overriding original loop points
**`#i`**: ignore and disable loop points, simply stopping after song's sample count (if file loops)

They are equivalent to some `test.exe/vgmstream_cli` options. Settings should mix with or override player's defaults. If player has "play forever" setting loops disables it (while other options don't quite apply), while forcing full loops would allow to play forever, or setting ignore looping would disable it.


**God Hand (PS2)**: *boss2_3ningumi_ver6.txtp*
```
boss2_3ningumi_ver6.adx#l3          #default is usually 2.0
```
```
boss2_3ningumi_ver6.adx#f11.5       #default is usually 10.0
```
```
boss2_3ningumi_ver6.adx#d0.5        #default is usually 0.0
```
```
boss2_3ningumi_ver6.adx#i           #this song has a nice stop
```
```
boss2_3ningumi_ver6.adx#F           #loop some then hear that stop
```
```
boss2_3ningumi_ver6.adx#e           #song has loops, so ignored here
```
```
boss2_3ningumi_ver6.adx#E           #force full loops
```
```
boss2_3ningumi_ver6.adx#L           #keep on loopin'
```
```
boss2_3ningumi_ver6.adx  #l 3 #F    #combined: 3 loops + ending
```
```
boss2_3ningumi_ver6.adx #l1.5#d1#f5 #combined: partial loops + some delay + smaller fade
```
```
# boss2_3ningumi_ver6.adx #l1.0 #F  #combined: equivalent to #i
```


### Time modifications
**`#t(time)`**: trims the file so base duration (before applying loops/fades/etc) is `(time)`. If value is negative substracts `(time)` to duration. Loop end is adjusted when necessary, and ignored if value is bigger than possible (use `#l(loops)` config to extend time instead).

Time values can be `M:S(.n)` (minutes and seconds), `S.n` (seconds with dot), `0xN` (samples in hex format) or `N` (samples). Beware of the subtle difference between 10.0 (ten seconds) and 10 (ten samples). 

Some segments have padding/silence at the end for some reason, that don't allow smooth transitions. You can fix it like this:
```
intro.fsb #t -1.0    #intro segment has 1 second of silence.
main.fsb
```

Similarly other games don't use loop points, but rather repeat/loops the song internally many times:
```
bgm01.vag #t3:20 #i #l1.0   # trim + combine with forced loops for easy fades
```

Note that if you need to remove very few samples (like 1) to get smooth transitions it may be a bug in vgmstream, consider reporting.


### Force sample rate
**`#h(sample rate)`**: changes sample rate to selected value (within some limits).

Needed for a few games set a sample rate value in the header but actually play with other (applying some of pitch or just forcing it).

**Super Paper Mario (Wii)**
```
btl_koopa1_44k_lp.brstm#h22050  #in hz
```
**Patapon (PSP)**
```
ptp_btl_bgm_voice.sgd#s1#h11050
```


### Install loops
**`#I(loop start time) [loop end time]`**: force/override looping values, same as .pos but nicer. Loop end is optional and defaults to total samples.

Time values can be `M:S(.n)` (minutes and seconds), `S.n` (seconds, with dot), `0xN` (samples in hex format) or `N` (samples). Beware of the subtle difference between 10.0 (ten seconds) and 10 (ten samples). Wrong loop values (for example loop end being much larger than file's samples) will be ignored, but there is some leeway when using seconds for the loop end.

**Jewels Ocean (PC)**
```
bgm01.ogg#I32.231             # from ~1421387 samples to end

#bgm01.ogg#I 0:32.231         # equivalent
#bgm01.ogg#I 1421387 4212984  # equivalent, end is 4212984
#bgm01.ogg#I32.231 1_35.533   # equivalent, end over file samples (~4213005) but adjusted to 4212984
#bgm01.ogg#I 1421387 4212985  # ignored, end over file samples
#bgm01.ogg#I32.231 1_37       # ignored, end over file (~4277700) but clearly wrong
```

Use this feature responsibly, though. If you find a format that should loop using internal values that vgmstream doesn't detect correctly, consider reporting the bug for the benefit of all users and other games using the same format, and don't throw out the original loop definitions (as sometimes they may not take into account "encoder delay" and must be carefully adjusted).

Note that a few codecs may not work with arbitrary loop values since they weren't tested with loops. Misaligned loops will cause audible "clicks" at loop point too.


### Channel mixing
**`#m(op),(...),(op)`**: mix channels in various ways by specifying multiple comma-separated sub-commands:

Possible operations:
- `N-M`: swaps M with N
- `N+M*(volume)`: mixes M * volume to N
- `N+M`: mixes M to N
- `N*(volume)`: changes volume of N
- `N=(volume)`: limits volume of N
- `Nu`: upmix (insert) N ('pushing' all following channels forward)
- `Nd`: downmix (remove) N ('pulling' all following channels backward)
- `ND`: downmix (remove) N and all following channels
- `N(type)(position)(time-start)+(time-length)`: defines a fade
  * `type` can be `{` = fade-in, `}` = fade-out, `(` = crossfade-in, `)` = crossfade-out
    * crossfades are better tuned to use when changing between tracks
  * `(position)` pre-adjusts `(time-start)` to start after certain time (optional)
  * using multiple fades in the same channel will cancel previous fades
    * may only cancel when fade is after previous one
    * `}` then `{` or `{` then `}` makes sense, but `}` then `}` will make funny volume bumps
    * example: `1{0:10+0:5, 1}0:30+0:5` fades-in at 10 seconds, then fades-out at 30 seconds
- `N^(volume-start)~(volume-end)=(shape)@(time-pre)~(time-start)+(time-length)~(time-last)`: defines full fade envelope
  * full definition of a fade envelope to allow precise volume changes over time
    * not necessarily fades, as you could set length 0 for volume "bumps" like `1.0~0.5`
  * `(shape)` can be `{` = fade, `(` = crossfade, other values are reserved for internal testing and may change anytime
  * `(time-start)`+`(time-length)` make `(time-end)`
  * between `(time-pre)` and `(time-start)` song uses `(volume-start)`
  * between `(time-start)` and `(time-end)` song gradually changes `(volume-start)` to `(volume-end)` (depending on `(shape)`)
  * between `(time-end)` and `(time-post)` song uses `(volume-end)`
  * `time-pre/post` may be -1 to set "file start" and "file end", cancelled by next fade

Considering:
- `N` and `M` are channels (*current* value after previous operators are applied)
- channel 1 is first
- channel 0 is shorthand for all channels where applicable (`0*V`, `0=V`, `0^...`)
- may use `x` instead of `*` and `_` instead of `:` (for mini-TXTP)
- `(volume)` is a `N.N` decimal value where 1.0 is 100% base volume
  - negative volume inverts the waveform (for weird effects)
- `(position)` can be `N.NL` or `NL` = N.N loops
  - if loop start is 1000 and loop end 5000, `0.0L` = 1000 samples, `1.0L` = 5000 samples, `2.0L` = 9000 samples, etc
- `(time)` can be `N:NN(.n)` (minutes:seconds), `N.N` (seconds) or `N` (samples)
  - represents the file's global play time, so it may be set after N loops
  - beware of `10.0` (ten seconds) vs `10` (ten samples)
  - may not work with huge numbers (like several hours)
- adding trailing channels must be done 1 by 1 at the end (for stereo: `3u,4u,(...)`
- nonsensical values are ignored (like referencing channel 3 in a stereo file)

Main usage would be creating stereo files for games that layer channels.
```
# quad to stereo: all layers must play at the same time
# - mix 75% of channel 3/4 into channel 1/2, then drop channel 3 and 4
song#m1+3*0.75,2+4*.75,3D

# quad to stereo: only channel 3 and 4 should play
# - swap channel 1/2 with 3/4, then drop channel 3/4
song#m1-3,2-4,3D

# also equivalent, but notice the order
# - drop channel 1 then 2 (now 1)
song#m1d,1d
```
Proper mixing requires some basic knowledge though, it's further explained later. Order matters and operations are applied sequentially, for extra flexibility at the cost of complexity and user-friendliness, and may result in surprising mixes. Try to stick to macros and simple combos, using later examples as a base.

This can be applied to individual layers and segments, but normally you want to use `commands` to apply mixing to the resulting file (see examples). Per-segment mixing should be reserved to specific up/downmixings.

Mixing must be supported by the plugin, otherwise it's ignored (there is a negligible performance penalty per mix operation though).


### Macros
**`#@(macro name and parameters)`**: adds a new macro

Manually setting values gets old, so TXTP supports a bunch of simple macros. They automate some of the above commands (analyzing the file), and may be combined, so order still matters.
- `volume N (channels)`: sets volume V to selected channels
- `track (channels)`: makes a file of selected channels
- `layer-v N (channels)`: mixes selected channels to N channels with default volume (for layered vocals)
- `layer-b N (channels)`: same, but adjusts volume depending on layers (for layered bgm)
- `layer-e N (channels)`: same, but adjusts volume equally for all layers (for generic downmixing)
- `remix N (channels)`: same, but mixes selected channels to N channels properly adjusting volume (for layered bgm)
- `crosstrack N`: crossfades between Nch tracks after every loop (loop count is adjusted as needed)
- `crosslayer-v/b/e N`: crossfades Nch layers to the main track after every loop (loop count is adjusted as needed)
- `downmix`: downmixes up to 8 channels (7.1, 5.1, etc) to stereo, using standard downmixing formulas.

`channels` can be multiple comma-separated channels or N~M ranges and may be ommited were applicable to mean "all channels" (channel order doesn't matter but it's internally fixed).

Examples: 
```
# plays 2ch layer1 (base melody)
okami-ryoshima_coast.aix#@track 1,2

# plays 2ch layer1+2 (base melody+percussion)
okami-ryoshima_coast.aix#@layer-b 2 1~4   #1~4 may be skipped

# uses 2ch layer1 (base melody) in the first loop, adds 2ch layer2 (percussion) to layer1 in the second
okami-ryoshima_coast.aix#@crosslayer-b 2

# uses 2ch track1 (exploration) in the first loop, changes to 2ch track2 (combat) in the second
ffxiii2-eclipse.scd#@crosstrack 2

# plays 2ch from 4ch track1 (sneaking)
mgs4-bgm_ee_alert_01.mta2#@layer-e 2 1~4

# downmix bgm + vocals to stereo
nier_automata-BGM_0_012_04.wem
nier_automata-BGM_0_012_07.wem
mode = layers
commands = #@layer-v 2

# can be combined with normal mixes too for creative results
# (add channel clone of ch1, then 50% of range)
song#m4u,4+1#@volume 0.5 2~4

# equivalent to #@layer-e 2 1~4
mgs4-bgm_ee_alert_01.mta2#@track 1~4#@layer-b 2

# equivalent to #@track 1,2
okami-ryoshima_coast.aix#@layer-b 2 1,2
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
commands = #C2,3
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
Here is a rough look of how TXTP parses files, so you get a better idea of what's going on if some command fails.

*Filenames* should accept be anything accepted by the file system, and multiple *commands* can be chained:
```
subdir name/bgm bank.fsb#s2#C1,2
subdir name/bgm bank.fsb #s2  #C1,2 #comment
```

Commands may add spaces as needed, but try to keep it simple and don't go overboard). They *must* start with `#(command)`, as `#(space)(anything)` is a comment. Commands without corresponding file are ignored too (seen as comments), while incorrect commands are ignored and skip to next, though the parser may try to make something usable of them (this may be change anytime without warning):
```
# those are all equivalent
song#s2#C1,2
song    #s2#C1,2   #    comment
song    #s 2  #C1,2# comment
song    #s 2 #C   1   ,  2# comment

#s2  #ignores rogue commands/comments

# seen as incorrect and ignored
song    #s TWO
song    #E enable
song    #E 1
song    #Enable
song    #h -48000
song    #l -2.0

# accepted
song    #E # comment
song    #C1, 2, 3
song    #C  1  2  3

# ignores first and reads second
song    #s TWO#C1,2

# seen as #s1#C1,2
song    #s 1,2 #C1,2

# all seen as #h48000
song    #h48000
song    #h 48000hz
song    #h 48000mhz

# ignored
song    #h hz48000

# ignored as channels don't go that high (may be modified on request)
song    #C32,41

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
commands= #C1,2
```

Repeated commands overwrite previous setting, except comma-separated commands that are additive:
```
# overwrites, equivalent to #s2
song#s1#s2
```
```
# adds, equivalent to #m1-2,3-4,5-6
song#m1-2#m3-4
commands = #m5-6
# also added to song
commands = #l 3.0
```

The parser is fairly simplistic and lax, and may be erratic with edge cases or behave unexpectedly due to unforeseen use-cases and bugs. As filenames may contain spaces or #, certain name patterns could fool it too. Keep in mind this while making .txtp files.


## MINI-TXTP
To simplify TXTP creation, if the .txtp doesn't set a name inside then its filename is used directly, including config. Note that extension must be included (since vgmstream needs a full filename). You can set `commands` inside the .txtp too:
- *bgm.sxd2#12.txtp*: plays subsong 12
- *bgm.sxd2#12.txtp*, , inside has `commands = #@volume 0.5`: plays subsong 12 at half volume
- *bgm.sxd2.txtp*, , inside has `commands =  #12 #@volume 0.5`: plays subsong 12 at half volume
- *Ryoshima Coast 1 & 2.aix#C1,2.txtp*: channel downmix
- *boss2_3ningumi_ver6.adx#l2#F.txtp*: loop twice then play song end file normally
- etc


## MIXING
Sometimes games use multiple channels in uncommon ways, for example as layered tracks for dynamic music (like main+vocals), or crossfading a stereo song to another stereo song. In those cases we normally would want a stereo track, but vgmstream can't guess how channels are used (since it's game-dependant). To solve this via TXTP you can set mixing output and volumes manually.

A song file is just data that can contain a (sometimes unlimited) number of channels, that must play in physical speakers. Standard audio formats define how to "map" known channels to speakers:
- `1.0: FC`
- `2.0: FL, FR`
- `2.1: FL, FR, LF`
- `4.0: FL, FR, SL, SR`
- `5.1: FL, FR, FC, LF, SL, SR`
- ... (channels in order, where FL=front left, FC=front center, etc)

If you only have stereo speakers, when playing a 5.1 file your player may silently transform to stereo, as otherwise you would miss some channels. But a game song's channels can be various things: standard mapped, per-format map, per-game, multilayers combined ("downmixed") to a final stereo file, music then all language tracks, etc. So you need to decide which channels drop or combine and their individual volumes via mixing.

Say you want to mix 4ch to 2ch (ch3 to ch1, ch4 to ch2). Due to how audio signals work, mixing just combines (adds) sounds. So if channels 1/2 are LOUD, and channels 3/4 are LOUD, you get a LOUDER channel 1/2. To fix this we set mixing volume, for example: `mix channel 3/4 * 0.707 (-3db/30% lower volume) to channel 1/2`: the resulting stereo file is now more listenable. Those volumes are just for standard audio and may work ok for every game though.

All this means there is no simple, standard way to mix, so you must experiment a bit.


### MIXING EXAMPLES
For most common usages you can stick with macros but actual mixing is quite flexible:
```
# boost volume of all channels by 30%
song#m0*1.3

# boost but limit volume (highs don't go too high, while lows sound louder)
song#m0*1.3,0=0.9

# downmix 4ch layers to stereo (this may sound too loud)
song#m1+3,2+4,3D

# downmix 4ch layers to stereo with adjusted volume for latter channels (common 4.0 to 2.0 mixdown)
song#m1+3*0.7,2+4*0.7,3D

# downmix 4ch layers to stereo with equal adjusted volume (common layer mixdown)
song#m0*0.7,1*0.7,1+3*0.7,2+4*0.7,3D

# downmix stereo to mono (ignored if file is 1ch)
zelda-cdi.xa#m1d

# upmix mono to stereo by copying ch1 to ch2
zelda-cdi.xa#m2*0.0,2+1

# downmix 5.1 wav (FL FR FC LFE BL BR) to stereo
# (uses common -3db mixing formula: Fx=Fx + FC*0.707 + Rx*0.707)
song#m1+3*0.707,2+3*0.707,1+5*0.707,2+6*0.707,3D

# mask sfx track ch3 in a 6ch file
song#m3*0.0

# add a fake silent channel to a 5ch file (FL FR FC BL BR) and move it to LFE position
# "make ch6, swap BL with LFE (now FL FR FC LFE BR BL), swap BR with BL (now FL FR FC LFE BL BR)
sf5.hca#m6u,4-6,5-6

# mix 50% of channel 3 into 1 and 2, drop 3
song#m1+3*0.5,2+3*0.5,3d

# swap ch1 and 2 then change volume to the resulting swapped channel
song#m1-2,2*0.5

# fade-in ch3+4 percussion track layer into main track, downmix to stereo
# (may be split in multiple lines, no difference)
okami-ryoshima_coast.aix#l2
commands = #m3(1:10+0:05         # loop happens after ~1:10
commands = #m4(1:10+0:05         # ch3/4 are percussion tracks
commands = #m1+3*0.707,2+4*0.707 # ch3/4 always mixed but silent until 1:10
commands = #m3D                  # remove channels after all mixing

# same but fade-out percussion after second loop
okami-ryoshima_coast.aix#l3
commands = #m3(1:10+0:05,3)2:20+0:05
commands = #m4(1:10+0:05,4)2:20+0:05
commands = #m1+3*0.707,2+4*0.707,3D

# crossfade exploration and combat sections after loop
ffxiii-2~eclipse.aix
commands = #m1)1:50+0:10,2)1:50+0:10
commands = #m3(1:50+0:10,4(1:50+0:10
commands = #m1+3,2+4,3D  # won't play at the same time, no volume needed

# ghetto voice removal (invert channel + other channel removes duplicated parts, and vocals are often layered)
song
commands = #m3u,3+1*-1.0,4u,4+2*-1.0
commands = #m1+4,2+3,3d,3d

# crosstrack 4ch file 3 times, going back to first track by creating a fake 3rd track with ch1 and 2:
ffxiii2-eclipse.scd#m5u,6u,5+1,6+2#@crosstrack 2
```

Segment/layer downmixing is allowed but try to keep it simple, some mixes accomplish the same things but are a bit strange.
```
# mix one stereo segment with a mono segment
intro-stereo.hca
loop-mono.hca#m2u

# this makes mono file
intro-stereo.hca#m2u
loop-stereo.hca#m2u

# but you normally should do this instead as it's more natural
intro-stereo.hca
loop-stereo.hca
commands = #m2u

# fading segments work rather unexpectedly
# fades out 1 minute into the _segment_  (could be 2 minutes into the resulting file)
segment1.hca#m0{0:10+10.0
segment2.hca#m0}1:00+10.0
# better use: commands = #m0{0:10+10.0,0}2:00+10.0
# it would work ok it they were layers, but still, better to use commands with the resulting file
```

Combine with groups or extra complex cases:
```
BGM_SUMMON_0001_02-Intro.hca    # 2ch file
BGM_SUMMON_0001_02-Intro2.hca   # 2ch file

BGM_SUMMON_0001_02.hca
BGM_SUMMON_0001_02-Vocal.hca
group = 3L2 #@layer-v 2     # layer Main+Vocal as 4ch then downmix to 2ch

loop_start_segment = 3 #refers to new group at position 3
loop_mode = keep
```

Note how order subtly affects end results:
```
# after silencing channel 1 mixing is meaningless
song#m1*0.0,2+1

# allowed but useless or ignored
song#m1u,1d,1-1,1*1.0,11d,7D

# this creates a new ch1 with 50% of ch2 (actually old ch1), total 3ch
song#m1u,1+2*0.5

# so does this
song#m3u,3+1*0.5,1-3,2-3

# this may not be what you want
# (result is a silent ch1, and ch2 with 50% of ch3)
song#m1+2*0.5,1u

# for a 2ch file 2nd command is ignored, since ch2 is removed after 1st command
song#m1d,2+1*0.5
```
