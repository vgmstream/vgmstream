# vgmstream
This is vgmstream, a library for playing streamed (prerecorded) video game audio.

Some of vgmstream's features:
- Decodes [hundreds of video game music formats and codecs](doc/FORMATS.md), from typical
  game engine files to obscure single-game codecs, aiming for high accuracy and compatibility.
- Support for looped BGM, using file's internal metadata for smooth transitions, with accurate
  sample counts.
- [Subsongs](doc/USAGE.md#subsongs), playing a format's multiple internal songs separately.
- Many types of companion files (data split into multiple files) and custom containers.
- Encryption keys, internal stream names, and other unusual cases found in game audio.
- [TXTH](doc/TXTH.md) function, to add external support for extra formats, including raw audio in
  many forms.
- [TXTP](doc/TXTP.md) function, for real-time and per-file config, like forced looping, removing
  channels, playing certain subsong, or fusing multiple files into a single one.
- Simple [external tagging](doc/USAGE.md#tagging) via .m3u files.
- [Plugins](#getting-vgmstream) are available for various media player software and operating systems.

The main development repository: https://github.com/vgmstream/vgmstream/

Automated builds with the latest changes: https://vgmstream.org
(https://github.com/vgmstream/vgmstream-releases/releases/tag/nightly)

Numbered releases: https://github.com/vgmstream/vgmstream/releases

Help can be found here: https://www.hcs64.com/

More documentation: https://github.com/vgmstream/vgmstream/tree/master/doc

## Getting vgmstream
There are multiple end-user components:
- [vgmstream-cli](doc/USAGE.md#testexevgmstream-cli-command-line-decoder): A command-line decoder.
- [in_vgmstream](doc/USAGE.md#in_vgmstream-winamp-plugin): A Winamp plugin.
- [foo_input_vgmstream](doc/USAGE.md#foo_input_vgmstream-foobar2000-plugin): A foobar2000 component.
- [xmp-vgmstream](doc/USAGE.md#xmp-vgmstream-xmplay-plugin): An XMPlay plugin.
- [vgmstream.so](doc/USAGE.md#audacious-plugin): An Audacious plugin.
- [vgmstream123](doc/USAGE.md#vgmstream123-command-line-player): A command-line player.

The main library (plain *vgmstream*) is the code that handles the internal conversion, while the
above components are what you use to get sound.

### Usage
If you want to convert game audio to `.wav`, get *vgmstream-cli* then drag-and-drop one
or more files to the executable (support may vary per O.S. or distro). This should create
`(file.extension).wav`, if the format is supported. You can also try the online web player
instead. See: https://vgmstream.org

More user-friendly would be installing a player like *foobar2000* (on Windows) or *Audacious*
(on Linux) and the vgmstream plugin. Then you can directly listen your files and set options like
infinite looping, or convert to `.wav` with the player's options (also easier to use if your file
has multiple "subsongs").

See [components](doc/USAGE.md#components) in the *usage guide* for full install instructions and
explanations. The aim is feature parity, but there are a few differences between them due to
missing parts on vgmstream's side or lack of support in the player.

Note that vgmstream cannot *encode* (convert from `.wav` to a game format), it only *decodes*
(plays game audio).

### Windows binaries
Prebuilt binaries:
- https://vgmstream.org (latest)
- https://github.com/vgmstream/vgmstream/releases (infrequent numbered releases)

The foobar2000 component is also available on https://www.foobar2000.org based on current
release.

You may also try the alternative versions (irregularly) built by [bnnm](https://github.com/bnnm):
- https://github.com/bnnm/vgmstream-builds/raw/master/bin/vgmstream-latest-test-u.zip

Or compile from source, see the [build guide](doc/BUILD.md).

### Linux binaries
A prebuilt CLI binary is available. It's statically linked and should work on systems running
Linux kernel v3.2 and above:
- https://vgmstream.org (latest)
- https://github.com/vgmstream/vgmstream/releases (infrequent numbered releases)

Building from source will also give you *vgmstream.so* (Audacious plugin), and *vgmstream123*
(command-line player), which can't be statically linked.

When building it needs several external libraries. For a quick script for Debian and Ubuntu-style
distros run `./make-build-cmake.sh`. The script will need to install dependencies first, so you
may prefer to run steps manually, which the [build guide](doc/BUILD.md) describes in detail.

### macOS binaries
A prebuilt CLI binary is available:
- https://vgmstream.org (latest)
- https://github.com/vgmstream/vgmstream/releases (infrequent numbered releases)

Otherwise follow the [build guide](doc/BUILD.md).


## More info
- [Usage guide](doc/USAGE.md)
- [List of supported audio formats](doc/FORMATS.md)
- [Build guide](doc/BUILD.md)
- [TXTH file format](doc/TXTH.md)
- [TXTP file format](doc/TXTP.md)


Enjoy! *hcs*
