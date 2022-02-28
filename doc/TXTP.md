# TXTP format

TXTP is a text file with commands, to improve support for games using audio in certain uncommon or undesirable ways. It's in the form of a mini-playlist or a wrapper with play settings, meant to do post-processing over playable files.

Simply create a file named `(filename).txtp`, and inside write the song name and commands described below. Then open the new file directly and vgmstream should play it.

Common case examples:

**stage01_intro+loop.txtp**
```
stage01_intro.vag
stage01_loop.vag
loop_mode = auto
```

**bgm01_subsong2.txtp**
```
bgm01.fsb #2
```

**sfx01-22khz.txtp**
```
sfx01.wav #h22050
```

**field_channels3+4.txtp**
```
field.bfstm #C3,4
```

**bgm01.flac #I 10.0 .txtp**
```
# (empty)
# this is a "mini-txtp" that sets loop start to 10 seconds
# notice it has the original filename + extension, then commands, then .txtp
```

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

*files* may be anything accepted by the file system (including spaces and symbols), and you can use subdirs. Separators can be `\` or `/`, but stick to `/` for consistency. Commands may be chained and use spaces as needed (also see "*TXTP parsing*" section).
```
sounds/bgm.fsb #s2 #i  #for file inside subdir: play subsong 2 + disable looping
```

You can mix any kind of files (including different formats and codecs), as long as vgmstream plays them separately too. If you have problem getting a TXTP to play try playing file by file first and make a simpler TXTP then add more. There are technical limits in total files (usually hundreds but varies per O.S.) and layered channels though. Also see explanations below for some per-mode limitations too.


### Segments mode
Some games clumsily loop audio by using multiple full file "segments", so you can play separate intro + loop files together as a single track.
 
**Ratchet & Clank (PS2)**: *bgm01.txtp*
```
# define 2 or more segments to play as one
BGM01_BEGIN.VAG
BGM01_LOOPED.VAG

# segments may define loops
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
Another way to set looping is using "loop anchors", that are meant to simplify more complex .txtp (explained later).


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
Mixing sample rates is ok (uses max). Different number of channels is allowed, but you may need to use mixing (explained later) to improve results. 4ch + 2ch will sound ok, but 1ch + 2ch would need some upmixing first.


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

If all layers share loop points they are automatically kept.
```
BGM1a.adx  # loops from 10.0 to 90.0
BGM1b.adx  # loops from 10.0 to 90.0
mode = layers
# resulting file loops from 10.0 to 90.0

# if layers *don't* share loop points this sets a full loop (0..max)
loop_mode = auto
```

Note that the number of channels is the sum of all layers so three 2ch layers play as a 6ch file (you can manually downmix using mixing commands, described later, since vgmstream can't guess if the result should be stereo or 5.1 audio).


### Mixed group mode
You can set "groups" to 'fold' various files into one, as layers or segments, to allow complex mix of segments and layers in the same file. This is a rather advanced setting, so it's explained in detail later (also read other sections first). Most common usage would be:
```
  # 1st segment: file of 2ch
  # group isn't needed here
  bgm1_intro.fsb

  # 2nd segment: layer 2 files ("L") and keep it 2ch
  # equivalent to including a separate .txtp of "mode = layers" with downmixing info (layer-v)
  # note that after the internal bgm1_music/vocal aren't "visible" below
    bgm1_music.fsb
    bgm1_vocal.fsb
  group = -L2  #@layer-v

# final result: groups of 2 segments ("S") above
# like making a .txtp of "mode = segments" with 1st .fsb and 2nd .txtp
# b/c "mode = segments" is the default, this last group can be ommited (it's here for clarity)
group = -S2

# loop last segment
loop_mode = auto
# same thing, points to 2nd segment in "final group", but not that clear with groups
#loop_start_segment = 2
```

```
  # group segment of 3, note that in groups you always need to set number of items
    bgm2_main_a.fsb
    bgm2_main_b.fsb
    bgm2_main_c.fsb
  group = -S3

  # another group segment of 3
    bgm2_vocal_a.fsb
    bgm2_vocal_b.fsb
    bgm2_vocal_c.fsb
  group = -S3

# final result: layer of 2 groups above
group = -L2 #@layer-v

# optional to avoid "segments" default (for debugging, errors if files don't group correctly)
mode = mixed
```

```
# "selectable" (pseudo-random) group, plays 1st internal file only (change to >2 to play 2nd and so on)
  bgm2_intro_a.fsb
  bgm2_intro_b.fsb
group = -R2>1

# main bgm, also sets this point as loop start
bgm3_main.fsb #@loop

# implicitly loop end
bgm3_outro.fsb

# defaults to segments one of the intros with main bgm
```

You mix group and groups just like you would mix .txtp with .txtp, resulting in files any combo of single files, layers and segments. Indentation is optional for clarity.


### Silent files
You can put `?.` in an entry to make a silent (non-existing) file. By default takes channels and sample rate of nearby files, can be combined with regular commands to configure.
```
intro.adx
?.silence #b 3.0  # 3 seconds of silence
loop.adx
```

It also doubles as a quick "silence this file" while keeping the same structure, for complex cases. The `.` can actually be anywhere after `?`, but must exists before commands to function correctly (basically, don't silence extension-less files).
```
  layer1a.adx
 ?layer1b.adx
 group = -L2
 
 ?layer2a.adx
  layer2b.adx
 group = -L2
 
group = -S2
```

Most of the time you can do the same with `#p`/`#P` padding commands or `#@volume 0.0`. This is mainly for complex engines that combine silent entries in twisted ways. You can't silence `group` with `?group` though since they aren't considered "entries".


## TXTP COMMANDS
You can set file commands by adding multiple `#(command)` after the name. `#(space)(anything)` is considered a comment and ignored, as well as any command not understood.

### Subsong selection for bank formats
**`#(number)` or `#s(number)`**: set subsong (number)

**Super Robot Taisen OG Saga: Masou Kishin III - Pride of Justice (Vita)**: *bgm_12.txtp*
```
# select subsong 12
bgm.sxd2#12
# single files loop normally by default (see below to force looping)
```
```
#bgm.sxd2 #s12  #"sN" is alt for subsong
```

### Play segmented subsong ranges as one
**`#(number)~(number)` or `#s(number)~(number)`**: set multiple subsong segments at a time, to avoid so much C&P.

**Prince of Persia Sands of Time**: *song_01.txtp*
```
amb_fx.sb0#254
amb_fx.sb0#122~144
amb_fx.sb0#121

# 3rd segment = subsong 123, not 3rd subsong
loop_start_segment = 3
```
This is just a shorthand, so `song#1~3#h22050` is equivalent to:
```
song#1#h22050
song#2#h22050
song#3#h22050
```
For technical reasons ranges that include hundreds of files may not work.

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

### Play config
*modifiers*
- **`#L`**: play forever (if loops are set set and player supports it)
- **`#i`**: ignore and disable loop points, simply stopping after song's sample count (if file loops)
- **`#e`**: set full looping (end-to-end) but only if file doesn't have loop points (mainly for autogenerated .txtp)
- **`#E`**: force full looping (end-to-end), overriding original loop points
- **`#F`**: don't fade out after N loops but continue playing the song's original end (if file loops)

*processing*
- **`#l(loops)`**: set target number of loops, (if file loops) used to calculate the body part (modified by other values)
- **`#b(time)`**: set target time (even without loops) for the body part (modified by other values)
- **`#f(fade period)`**: set (in seconds) how long the fade out lasts after target number of loops (if file loops)
- **`#d(fade delay)`**: set (in seconds) delay before fade out kicks in (if file loops)
- **`#B(time)`**: same as `#b`, but implies no `#f`/`#d` (for easier exact times).
- **`#p(time-begin)`**: pad song beginning (not between loops)
- **`#P(time-end)`**: pad song song end (not between loops)
- **`#r(time-begin)`**: remove/trim song beginning (not between loops)
- **`#R(time-end)`**: remove/trim song end (not between loops)

*(time)* can be `M:S(.n)` (minutes and seconds), `S.n` (seconds with dot), `0xN` (samples in hex format) or `N` (samples). Beware of 10.0 (ten seconds) vs 10 (ten samples). Fade config always assumes seconds for compatibility.

Based on these values and the player config, vgmstream decides a final song time and tweaks audio output as needed. On simple cases this is pretty straightforward, but when using multiple settings it can get a bit complex. You can even make layered/segments with individual config for extra-twisted cases. How all it all interacts is explained later.

Usage details:
- Settings should mix with player's defaults, but player must support that
- TXTP settings have priority over player's
  - `#L` loops forever even if player is set to play 2 loops
  - `#l` loops N times even if player is set to loop forever
- setting loop forever ignores some options (loops/fades/etc), but are still used to get final time shown in player
- setting target body time overrides loops (`#b` > `#l`)
- setting ignore fade overrides fades (`#F` > `#f`, `#d`)
- setting ignore loops overrides loops (`#i` > `#e` > `#E`)
- files without loop points or disabled loops (`#i`) ignore loops/fade settings (`#L`, `#l`, `#f`, `#d`)
  - loops may be forced first (`#e`, `#E`, `#I n n`) to allow fades/etc
- "body part" depends on number of loops (including non-looped part), or may be a fixed value:
  - a looped file from 0..30s and `#l 2.0` has a body of 30+30s, or could set `#b 60.0` instead
  - a looped file from 0..10..30s (loop start at 10s), and `#l 2.0` has a body of 10+20+20s, or could set `#b 50.0` instead
  - a non-looped file of 30s ignores `#l 2.0`, but can set a body of `#b 40.0` (plays silence after end at 30s)
  - loops can actually be half values: `#l 2.5` means a body of 30+30+15s
  - setting loops to 0 may only play part before loop
  - due to technical limitations a body may only play for so many hours (shouldn't apply to *play forever*)
- using `#F` will truncate loops to nearest integer, so `#F #l 2.7` becomes `#F #l 2.0`
- *modifier*" settings pre-configure how file is played
- *processing* settings are applied when playing

Processing goes like this:
- order: `pad-begin > trim-begin > body > (trim-end) > (fade-delay) > fade-period > pad-end`
- `pad-begin` adds silence before anything else
- `trim-begin` gets audio from `body`, but immediately removes it (subtracts time from body)
- `body` is the main audio decode, possibly including N loops or silence
- `fade-delay` waits after body (decode actually continues so it's an extension of `body`)
- `fade-period` fades-out last decoded part
- `trim-begin` removes audio from `body` (mainly useful when using `#l`
- `pad-end` adds silence after everything else
- final time is: `pad-begin + body - trim-begin - trim-end + fade-delay + fade-period + pad-end`
  - `#R 10.0 #b 60.0` plays for 50s, but it's the same as `#b 50.0`
  - `#d 10.0 #b 60.0` plays for 70s, but it's the same as `#b 70.0`
- big trims may be slow

Those steps are defined separate from "base" decoding (file's actual loops/samples, used to generate the "body") to simplify some parts. For example, if pad/trim were part of the base decode, loop handling becomes less clear.


**God Hand (PS2)**: *boss2_3ningumi_ver6.txtp*
```
boss2_3ningumi_ver6.adx  #l 3                   #default is usually 2.0
```
```
boss2_3ningumi_ver6.adx  #f11.5                 #default is usually 10.0
```
```
boss2_3ningumi_ver6.adx  #d0.5                  #default is usually 0.0
```
```
boss2_3ningumi_ver6.adx  #i                     #this song has a nice stop
```
```
boss2_3ningumi_ver6.adx  #F                     #loop some then hear that stop
```
```
boss2_3ningumi_ver6.adx  #e                     #song has loops, so ignored here
```
```
boss2_3ningumi_ver6.adx  #E                     #force full loops
```
```
boss2_3ningumi_ver6.adx  #L                     #keep on loopin'
```
```
boss2_3ningumi_ver6.adx  #l 3  #F               #combined: 3 loops + ending
```
```
boss2_3ningumi_ver6.adx  #l 1.5 #d1.0 #f5.0     #combined: partial loops + some delay + smaller fade
```
```
#boss2_3ningumi_ver6.adx #l 1.0 #F              #combined: equivalent to #i
```
```
boss2_3ningumi_ver6.adx  #p 1.0 2.0  #f 10.0    #adds 1s to start, fade 10s, add 2s to end
```
```
boss2_3ningumi_ver6.adx  #r 1.0                 #removes 1s from start
```
```
boss2_3ningumi_ver6.adx  #b 100.0s  #f 10.0     #plays for 100s + 10s seconds
```


### Trim file
**`#t(time)`**: trims the file so base sample duration (before applying loops/fades/etc) is `(time)`. If value is negative subtracts `(time)` to duration. 

*(time)* can be `M:S(.n)` (minutes and seconds), `S.n` (seconds with dot), `0xN` (samples in hex format) or `N` (samples). Beware of 10.0 (ten seconds) vs 10 (ten samples).

This changes the internal samples count, and loop end is also adjusted as needed. If value is bigger than max samples it's ignored (use `#l(loops)` and similar play config to alter final play time instead).

Some segments have padding/silence at the end for some reason, that don't allow smooth transitions. You can fix it like this:
```
intro.fsb #t -1.0    #intro segment has 1 second of silence.
main.fsb
```

Similarly other games don't use loop points, but rather repeat/loops the song internally many times:
```
bgm01.vag #t3:20
```

You can combine file trims and begin removes (see above) for weirder combos:

**Cave Story 3D (3DS)**
```
# loop has intro2 (bridge) + body, so we want intro1 + body + intro2 + body ... 
wanpaku_ending_intro.lwav               # intro1
wanpaku_ending_loop.lwav #r 141048      # remove intro2 and leave body (same samples as intro1)
wanpaku_ending_loop.lwav #t 141048      # plays up to intro2

loop_start_segment = 2                  # loops back to body
```

If you need to remove very few samples (like 1) to get smooth transitions it may be a bug in vgmstream, consider reporting.


### Install loops
**`#I(loop start time) [loop end time]`**: force/override looping values. Loop end is optional and defaults to total samples.

*(time)* can be `M:S(.n)` (minutes and seconds), `S.n` (seconds with dot), `0xN` (samples in hex format) or `N` (samples). Beware of 10.0 (ten seconds) vs 10 (ten samples).

Wrong loop values (for example loop end being much larger than file's samples) will be ignored, but there is some leeway when using seconds for the loop end.

**Jewels Ocean (PC)**
```
bgm01.ogg #I32.231              # from ~1421387 samples to end
```
```
bgm01.ogg #I 0:32.231           # equivalent
```
```
bgm01.ogg #I 1421387 4212984    # equivalent, end is 4212984
```
```
bgm01.ogg #I32.231 1_35.533     # equivalent, end over file samples (~4213005) but adjusted to 4212984
```
```
bgm01.ogg #I 1421387 4212985    # ignored, end over file samples
```
```
bgm01.ogg #I32.231 1_37         # ignored, end over file (~4277700) but clearly wrong
```

Use this feature responsibly, though. If you find a format that should loop using internal values that vgmstream doesn't detect correctly, consider reporting the bug for the benefit of all users and other games using the same format, and don't throw out the original loop definitions (as sometimes they may not take into account "encoder delay" and must be carefully adjusted).

Note that a few codecs may not work with arbitrary loop values since they weren't tested with loops. Misaligned loops will cause audible "clicks" at loop point too.


### Loop anchors
**`#a`** (loop start segment), **`#A`** (loop end segment): mark looping parts in segmented layout.

For segmented layout normally you set loop points using `loop_start_segment` and `loop_end_segment`. It's clean in simpler cases but can be a hassle when lots of files exist. To simplify those cases you can set "loop anchors":
```
bgm01.adx
bgm02.adx #a  ##defines loop start
```
```
bgm01.adx
bgm02.adx #a  ##defines loop start
bgm03.adx
bgm04.adx #A  ##defines loop end
bgm05.adx
```
You can also use `#@loop` and `#@loop-end` aliases.

Anchors can be applied to groups too.
```
  bgm01a.adx
  bgm01b.adx
 group -L2
  bgm02a.adx
  bgm02b.adx
 group -L2 #a   ##loops here
group -S2
```
```
  bgm01.adx
  bgm02.adx
group = -L2 #a  ##similar to loop_start_segment=1 or #E
```

This setting also works inside groups, which allows internal loops when using multiple segmented layouts (not possible with `loop_start/end_segment`).
```
  bgm01.adx
  bgm02.adx #a
 group -S2 #l 2.0
  bgm01.adx
  bgm02.adx #a
  bgm03.adx
 group -S2 #l 3.0
group -S2
# could even use R group to select one sub-groups that loops
# (loop_start_segment doesn't make sense for both segments)
```


### Force sample rate
**`#h(sample rate)`**: changes sample rate to selected value, changing play speed.

Needed for a few games set a sample rate value in the header but actually play with other (applying some of pitch or just forcing it), resulting in wrong play speed if not changed. Higher rates will sound faster, and lower rates slower.

**Super Paper Mario (Wii)**
```
btl_koopa1_44k_lp.brstm#h22050  #in hz
```
**Patapon (PSP)**
```
ptp_btl_bgm_voice.sgd#s1#h11050
```

Note that this doesn't resample (change sample rate while keeping play speed).


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
- `N(type)(position)(time-start)+(time-length)`: defines a curve envelope (post-processing fade)
  * `type` can be `{` = fade-in, `}` = fade-out, `(` = crossfade-in, `)` = crossfade-out
    * crossfades are better tuned to use when changing between tracks
  * `(position)` pre-adjusts `(time-start)` to start after certain time (optional)
  * using multiple fades in the same channel will cancel previous fades
    * may only cancel when fade is after previous one
    * `}` then `{` or `{` then `}` makes sense, but `}` then `}` will make funny volume bumps
    * example: `1{0:10+0:5, 1}0:30+0:5` fades-in at 10 seconds, then fades-out at 30 seconds
- `N^(volume-start)~(volume-end)=(shape)@(time-pre)~(time-start)+(time-length)~(time-last)`: defines full fade envelope
  * full definition of a curve envelope to allow precise volume changes over time
    * not necessarily fades, as you could set length 0 for volume "bumps" like `1.0~0.5`
  * `(shape)` can be `{` = fade, `(` = crossfade, other values are reserved for internal testing and may change anytime
  * `(time-start)`+`(time-length)` make `(time-end)`
  * between `(time-pre)` and `(time-start)` song uses `(volume-start)`
  * between `(time-start)` and `(time-end)` song gradually changes `(volume-start)` to `(volume-end)` (depending on `(shape)`)
  * between `(time-end)` and `(time-post)` song uses `(volume-end)`
  * `time-pre/post` may be -1 to set "file start" and "file end", canceled by next fade

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
Proper mixing requires some basic knowledge though, it's further explained later. Order matters and operations are applied sequentially, for extra flexibility at the cost of complexity and user-friendliness, and may result in surprising mixes. Typical mixing operations are provided as *macros* (see below), so try to stick to macros and simple combos, using later examples as a base.

This can be applied to individual layers and segments, but normally you want to use `commands` to apply mixing to the resulting file (see examples). Per-segment mixing should be reserved to specific up/downmixings.

Mixing must be supported by the plugin, otherwise it's ignored (there is a negligible performance penalty per mix operation though, though having *a lot* will add up).


### Macros
**`#@(macro name and parameters)`**: adds a new macro

Manually setting values gets old, so TXTP supports a bunch of simple macros. They automate some of the above commands (analyzing the file), and may be combined, so order still matters.
- `volume N (channels)`: sets volume V to selected channels. N.N = percent or NdB = decibels.
  -  `1.0` or `0dB` = base volume, `2.0` or `6dB` = double volume, `0.5` or `-6dB` = half volume
  - `#v N` also works
- `track (channels)`: makes a file of selected channels (drops others)
- `layer-v (N) (channels)`: for layered files, mixes selected channels to N channels with default volume (for layered vocals). If N is ommited (or 0), automatically sets highest channel count among all layers plus does some extra optimizations for (hopefully) better sounding results. May be applied to global commands or group config.
- `layer-e (N) (channels)`: same, but adjusts volume equally for all layers (for generic downmixing)
- `layer-b (N) (channels)`: same, but adjusts volume focusing on "main" layer (for layered bgm)
- `remix N (channels)`: same, but mixes selected channels to N channels properly adjusting volume (for layered bgm)
- `crosstrack N`: crossfades between Nch tracks after every loop (loop count is adjusted as needed)
- `crosslayer-v/b/e N`: crossfades Nch layers to the main track after every loop (loop count is adjusted as needed)
- `downmix`: downmixes up to 8 channels (7.1, 5.1, etc) to stereo, using standard downmixing formulas.

`channels` can be multiple comma-separated channels or N~M ranges and may be omitted were applicable to mean "all channels" (channel order doesn't matter but it's internally fixed).

Examples: 
```
# plays 2ch layer1 (base melody)
okami-ryoshima_coast.aix#@track 1,2
```
```
# plays 2ch layer1+2 (base melody+percussion)
okami-ryoshima_coast.aix#@layer-b 2 1~4   #1~4 may be skipped
```
```
# uses 2ch layer1 (base melody) in the first loop, adds 2ch layer2 (percussion) to layer1 in the second
okami-ryoshima_coast.aix#@crosslayer-b 2
```
```
# uses 2ch track1 (exploration) in the first loop, changes to 2ch track2 (combat) in the second
ffxiii2-eclipse.scd#@crosstrack 2
```
```
# plays 2ch from 4ch track1 (sneaking)
mgs4-bgm_ee_alert_01.mta2#@layer-e 2 1~4
```
```
# downmix bgm + vocals to stereo
nier_automata-BGM_0_012_04.wem
nier_automata-BGM_0_012_07.wem
mode = layers
commands = #@layer-v 2
```
```
# downmix 4ch bgm + 4ch vocals to 4ch (highest among all layers), automatically
mgr-main-4ch.wem
mgr-vocal-4ch.wem
mode = layers
commands = #@layer-v
```
```
# can be combined with normal mixes too for creative results
# (add channel clone of ch1, then 50% of range)
song#m4u,4+1#@volume 0.5 2~4
```
```
# equivalent to #@layer-e 2 1~4
mgs4-bgm_ee_alert_01.mta2#@track 1~4#@layer-b 2
```
```
# equivalent to #@track 1,2
okami-ryoshima_coast.aix#@layer-b 2 1,2
```
```
# works well enough for pseudo-dynamic tracks
RGG-Ishin-battle01a.hca
RGG-Ishin-battle01b.hca
RGG-Ishin-battle01c.hca
RGG-Ishin-battle01d.hca
mode = layers
commands = #@crosstrack 2
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


## TXTP PARSING
Here is a rough look of how TXTP parses files, so you get a better idea of what's going on if some .txtp fails.

*Filenames* should accept be anything accepted by the file system, and multiple *commands* can be chained:
```
subdir name/bgm bank.fsb#s2#C1,2
subdir name/bgm bank.fsb #s2  #C1,2 #comment
```
All defined files must exist and be parseable by vgmstream, and general config like `mode` must make sense (not `mde = layers` or `mode = laye`). *subdir* may even be relative paths like `../file.adx`, provided your OS supports that.

Commands may add spaces as needed, but try to keep it simple. They *must* start with `#(command)`, as `#(space)(anything)` is a comment. Commands without corresponding file are ignored too (seen as comments), while incorrect commands are ignored and skip to next, though the parser may try to make something usable of them (this may be change anytime without warning):
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

You can also add spaces before files/commands, mainly to improve readability when using more complex features like groups (don't go overboard though):
```
 #segment x2
  song1
  song2
 group = -S2 #E
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

The parser is fairly simplistic and lax, and may be erratic with edge cases or behave unexpectedly due to unforeseen use-cases and bugs. As filenames may contain spaces or # inside, certain name patterns could fool it too. Keep all this in mind this while making .txtp files.


## MINI-TXTP
To simplify TXTP creation, if the .txtp doesn't set a name inside then its filename is used directly, including config. Note that extension must be included (since vgmstream needs a full filename). You can set `commands` inside the .txtp too:
- *bgm.sxd2#12.txtp*: plays subsong 12
- *bgm.sxd2#12.txtp*, inside has `commands = #@volume 0.5`: plays subsong 12 at half volume
- *bgm.sxd2.txtp*, inside has `commands =  #12 #@volume 0.5`: plays subsong 12 at half volume
- *Ryoshima Coast 1 & 2.aix#C1,2.txtp*: channel downmix
- *boss2_3ningumi_ver6.adx#l2#F.txtp*: loop twice then play song end file normally
- etc


## GROUPS
Groups can be used to achieve complex .txtp in the same file, but can be a bit hard to understand. 

### Group definition
`group` can go anywhere in the .txtp, as many times as needed (groups are read and kept in an list that is applied in order at the end). Format is `(position)(type)(count)(repeat)`:
- `position`: file start (optional, default is 1 = first, or set `-` for 'auto from prev N' files)
- `type`: group as `S`=segments, `L`=layers, or `R`=pseudo-random
- `count`: number of files in group (optional, default is all)
- `repeat`: R=repeat group of `count` files until end (optional, default is no repeat)
- `>file`: select file (for pseudo-random groups)

Examples:
- `L`: take all files as layers (equivalent to `mode = layers`)
- `S`: take all files as segments (equivalent to `mode = segments`)
- `-S2`: segment prev 2 files (start is automatically set)
- `-L2`: layer prev 2 files (start is automatically set)
- `3L2`: layer 2 files starting from file 3
- `2L3R`: group every 3 files from position 2 as layers
- `1S1`: segment of one file (mostly useless but allowed as internal file may have play config)
- `1L1`: layer of one file (same)
- `9999L`: absurd values are ignored

### Usage
`position` may be `-` = automatic, meaning "start from position in previous `count` before current". If `repeat` is set it's ignored though (assumes first).
```
  bgm1.adx
  bgm2.adx
 group = -L2  #layer prev 2 (will start from pos.1 = bgm1, makes group of bgm1+2 = pos.1)

  bgm3.adx
  bgm4.adx
 group = -L2  #layer prev 2 (will start from pos.2 = bgm3, makes group of bgm3+4 = pos.2)

group = -S2  #segment prev 2 (will start from pos.1 = bgm1+2, makes group of bgm1+2 + bgm3+4)

# uses "previous" because "next files" often creates ambiguous cases
```

Usually you want automatic positions, but setting them manually may help understanding what's is going on:
```
 # - set introA+B+C as layer (this group becomes position 1, and loopA_2ch position 2)
  introA_2ch.at3  #position 1
  introB_2ch.at3
  introC_2ch.at3
 group = 1L3

 # - set loopA+B+C as layer (this group becomes position 2)
  loopA_2ch.at3   #position 4 > becomes position 2 once prev group is applied
  loopB_2ch.at3
  loopC_2ch.at3
 group = 2L3

# - play both as segments (this step is optional if using mode = segments)
group = S2

# one may mix groups of auto and manual positions too, but results are harder to predict
```

From TXTP's perspective, it starts with N separate files and every command joins some to make a single new "file", so positions are reassigned. End result after all grouping must be a single, final "file" that may contain groups within groups.

That also means you don't need to put groups next to files, if you keep virtual positions in mind. It's pretty flexible so you can express similar things in various ways:
```
introA_2ch.at3
mainA_2ch.at3

introB_2ch.at3
mainB_2ch.at3

introC_2ch.at3
mainC_2ch.at3

# - group intro/main pairs as segments, starting from 1 and repeating for A/B/C
# same as writting "group = -S2" x3
group = S2R

# - play all as layer (can't set loop_start_segment in this case)
# you could also set: group = L and mode = mixed, same thing
mode = layers
```

### Pseudo-random groups
Group `R` is meant to help with games that randomly select a file in a group (like some intro or outro). You can set with `>N` which file will be selected. This way you can quickly edit the TXTP and change the file (you could just comment files too, this is just for convenience in complex cases and testing). You can also set `>-`, meaning "play all", basically turning `R` into `S` (this can be omitted, but it's clearer). Files do need to exist and are parsed before being selected, and it can select groups too.
```
 bgm1.adx
 bgm2.adx
 bgm3.adx
group = -R3>1  #first file, change to >2 for second
```
```
  bgm1a.adx
  bgm1b.adx
 group = -S2
  bgm2a.adx
  bgm2b.adx
 group = -S2
group = -R2>2  #select either group >1 or >2
```

### Other considerations
Internally, `mode = segment/layers` are treated basically as a (default, at the end) group. You can apply commands to the resulting group (rather than the individual files) too. `commands` would be applied to this final group.
```
 mainA_2ch.at3
 mainB_2ch.at3
group = L #h44100
commands = #h48000 #overwrites
```

Segments and layer settings and rules still apply when making groups, so you may need to adjust groups a bit with commands:
```
# this doesn't need to be grouped
intro_2ch.at3

# this is grouped into a single 4ch file, then auto-downmixed to stereo
# (without downmixing may sound a bit strange since channels from mainB wouldn't mix with intro)
mainA_2ch.at3
mainB_2ch.at3
group = -L2 #@layer-v

# finally resulting layers are played as segments (2ch, 2ch)
# (could set a group = S and omit mode here, too)
mode = segments

# if the last group joins all as segments you can use loop_start
loop_start_segment = 3 #refers to final group at position 2
loop_mode = keep
```
Also see loop anchors to handle looping in some cases.


## MIXING
Sometimes games use multiple channels in uncommon ways, for example as layered tracks for dynamic music (like main+vocals), or crossfading a stereo song to another stereo song. In those cases we normally would want a stereo track, but vgmstream can't guess how channels are used (since it's game-dependent). To solve this via TXTP you can set mixing output and volumes manually.

A song file is just data that can contain a (sometimes unlimited) number of channels, that must play in physical speakers. Standard audio formats define how to "map" known channels to speakers:
- `1.0: FC`
- `2.0: FL, FR`
- `2.1: FL, FR, LF`
- `4.0: FL, FR, SL, SR`
- `5.1: FL, FR, FC, LF, SL, SR`
- ... (channels in order, where FL=front left, FC=front center, etc)

If you only have stereo speakers, when playing a 5.1 file your player may silently transform to stereo, as otherwise you would miss some channels. But a game song's channels can be various things: standard mapped, per-format map, per-game, multilayers combined ("downmixed") to a final stereo file, music + all language tracks, etc. So you may need to decide which channels drop or combine and their individual volumes via mixing.

Say you want to mix 4ch to 2ch (ch3 to ch1, ch4 to ch2). Due to how audio signals work, mixing just combines (adds) sounds. So if channels 1/2 are LOUD, and channels 3/4 are LOUD, you get a LOUDER (clipping) channel 1/2. To fix this we set mixing volume, for example: `mix channel 3/4 * 0.707 (-3db/30% lower volume) to channel 1/2`: the resulting stereo file is now more listenable. Those volumes are just for standard audio and may work ok for every game though.

All this means there is no simple, standard way to mix, so you must experiment a bit.


### Mixing examples
TXTP has a few macros that help you handle most simpler cases (like `#C 1 2`, `#@layer-v`), that you should use when possible, but below is a full explanation of manual mixing (macros just automate these options using some standard formulas). 
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
# mix one stereo segment with a mono segment upmixed to stereo
intro-stereo.hca
loop-mono.hca#m2u,2+1
```
```
# this makes mono file from each stereo song
intro-stereo.hca#m2d
loop-stereo.hca#m2d
```
```
# but you normally should do it on the final result as it's more natural
intro-stereo.hca
loop-stereo.hca
commands = #m2d
```
```
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

Performance isn't super-well tuned at the moment, and lots of mixes do add up (shouldn't matter in most simple cases though). Mixing order does affect performance, if you need to optimize:
```
# apply volume then downmix = slower (more channels need volume changes)
song#m0*0.5,1d
# downmix then apply volume = faster (less channels need volume changes)
song#m1d,0*0.5
```


## UNDERSTANDING PLAY CONFIG AND FINAL TIME
When handling a new file, vgmstream reads its loop points and total samples. Based on that and player/TXTP's config it decides actual "final time" that is used to play it. "internal file's samples" and "external play duration" are treated separately, so a non-looping 100s file could be forced to play for 200s (100s of audio then 100s of silence), or a looping 100s file could be set to play 310s (so 3 loops + 10s fade).

For example, with a 100s file that loops from 5..90s, `file.adx #p 5.0  #r 10.0  #l 2.0  #f 10.0  #P 2.0` means:
- pad with 5s of silence first
- decode first loop 0..90s, but trim first 10s seconds, ending up like 10..90s
- decode second loop 5..90 (trim doesn't affect other loops)
- during 3rd loop fade-out audio for 10 seconds
- pad end by 2 seconds
- song ends and player moves to next track
- final time would be: `5s + (90 - 10)s + 85s + 10s + 2s = 182s`

When you use commands to alter samples/looping it actually changes the "internal decoding", so final times are calculated differently So adding `#t -5.0 #I 5.0 85.0` to the above would result in a final time of `5s + (85 - 10)s + 80s + 10s + 2s = 172s`.


### Complex cases
Normally the above is mostly transparent, and vgmstream should "just work" with the player's settings. To understand how some extra-complex cases are handled, here is what it happens when you start having things over things over things.

Internally, a regular file just plays once or keeps looping (if loops are set) for a duration that depends on config (pads/fades/etc, with no settings default duration would be file's samples). Anything past pre-calculated final time would be silence (player should stop at that point, unless "loop forever" is set).

Behavior for layers and segments is a bit more complex. First, a layered/segmented vgmstream is made of multiple "internal vgmstreams", and one "external vgmstream" that manages them. The external takes its values from the internals:
- layered samples: highest base sample count among all internals
- layered loops: inherited if all internals share loop points, not set otherwise
- segmented samples: sum of all internals
- segmented loops: not set (uses `loop segment` settings instead)

Play config (loops/fades/etc) is normally applied to the "base" external vgmstream, and same applies to most TXTP commands (changing some values like sample rate only makes sense in the external part). This allows looping the resulting vgmstream in sync.

However each internal vgmstream can have its own config. This means first layer may be padded and internally looped before going to next one. In that case external vgmstream doesn't inherit loop points, but internals *can* be looped, and sample count is the calculated final duration (so segment of 30s that loops 2 times > external time of 60s). More or less, when using play config on an internal vgmstream it becomes a solid "clip". So when dealing with complex layouts make sure you only put play config in the internal vgmstream if you really need this, otherwise set the external's config.

Since layout/segments can also contain other layouts/segments this works in cascade, and each part can be configured via `groups`, as explained before). Certain combos involving looping (like installing loops in the internal and external vgmstreams) may work unexpectedly.


### Examples
Example with a file of 100s, loop points 10s..95s, no settings.
```
#l 2.0  #f 10.0  / time=190s (default settings for most players)
[0..10..95][10..95][10..20)

#l 2.0  #f 10.0 #d 10.0  / time=200s (same with delay)
[0..10..95][10..95][10..20..30)

#l 2.0  / time=180s (same without fade/delay)
[0..10..95][10..95]

#l 3.0  / time=265s (three loops)
[0..10..95][10..95][10..95]

#i  #l 2.0  #f 10.0  / time=100s (loop times and fades don't apply with loop disabled)
[0..100]

#E  #l 2.0  #f 10.0  / time=210s (ignores original loops and force 0..100)
[0..100][0..100][0..10)

#l 2.5  / time=185s (ends in half loop)
[0..10..95][10..95][10..95][10..53]

#l 2.0  #F  / time=185s (play rest of file after all loops)
[0..10..95][10..95..100s]

#l 2.3  #F #f 10.0  / time=185s (2.3 becomes 2.0 to make sense of it, and fade is removed)
[0..10..95][10..95..100s]

#p 10.0  #l 2.0  / time=190 (pad 10 seconds, then play normally)
(0..10)[0..10..95][10..95]

#r 5.0  #l 2.0  / time=175s (removes first seconds of the output, then loops normally)
[5..10..95][10..95]

#r 5.0  #b 180  / time=175s (same as the above, note that 180 is base body including non-looped part)
[5..10..95][10..95]

#r 25.0  #l 2.0  / time=155s (removes part of loop start, but next loop will play it)
[25..95][10..95]

#p 10.0  #r 25.0  #l 2.0  / time=165s (pad goes before trim, and trim is applied over file's output)
(0..10)[25..95][10..95]

#b 120.0  #i  / time = 120s (force time, no sound after 100s)
[0...100](0..20)

#b 120s  #l 2.0  / time = 120s (force time, stops in the middle of looping)
[0..10..95][10..35]
```

Using *loop forever* is a bit special. Play time is calculated based on config to be shown by the player, but it's ignored internally (vgmstream won't stop looping/playing after that time).
```
#L  #l 1.0  / time=95s (loops forever, but time info is shown using defaults)
[0..10..95][10..95] ... [10..95] ...

#L  #p 10.0  #l 1.0  / time=105s (does padding first)
(0..10)[0..10..95][10..95] ... [10..95] ...

#L  #r 5.0  #l 1.0  / time=90s (does trims first)
[5..95][10..95] ... [10..95] ...

#L  #l 1.0  #f 8.0  / time=103s (song never ends, so fade isn't applied, but is part of shown time)
[0..10..95][10..95] ... [10..95] ...
```

With segments/layers note the difference between this:
```
# play config is set in the external vgmstream
bgm1a.adx
bgm1b.adx
layout = layered
commands = #l 2.0
```
and this:
```
# play config is set in the internal vgmstream, and shouldn't set the external's
# (may change other settings like sample rate, but not others like loops)
bgm1a.adx   #l 2.0
bgm1b.adx   #l 2.0
layout = layered
#commands = ## doesn't inherit looping now
```


It's not possible to do "inside" padding or trims (modifying in the middle of a song), but you can achieve those results by combining trimmed/padded segments (used to simulate unusual Wwise loops). You can do internal loops per part too.
```
#???
[0..10..50][10..95]

#???
[10..95](0..10)[10..95]

#segmented: play 2 clips with padding between
[10..95]                #r 10.0  #b 95.0
(0..10)[10..95]         #r 10.0  #b 95.0 #p 10.0
layout = segmented
loop_mode = auto

#segmented: play 2 clips sequentially and loops them
[0..10..50]             #b 50.0
[10..95]                #r 10.0  #b 95.0
layout = segmented
loop_mode = auto

#layered: plays a single track with "sequential" clips
[0..10..50]             #b 50.0
(0......50)[10..95]     #p 50.0  #r 10.0  #b 95.0
layout = layered

#segmented: loop partially a segment before going next
[0..10]                 #b 10.0
[10..100][0..50]        #E #l 1.5  #r 10.0
layout = segmented
loop_start_segment = 2  #note this plays 10s then loops the *whole* 140s part (new 2nd segment)
```

Since padding has the effect of delaying the start of a part, it can be used for unaligned transitions.
```
# layered: plays a single track with *overlapped* clips (first 10s)
[0..10..50]             #b 50.0
(0...40)[10..95]        #p 40.0  #r 10.0  #b 95.0
layout = layered
```
