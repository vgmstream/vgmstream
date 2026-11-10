# vgmstream
This is vgmstream, a library for playing streamed (prerecorded) video game audio.

Some of vgmstream's features:
- Decodes [hundreds of video game music formats and codecs](doc/FORMATS.md), from typical
  game engine files to obscure single-game codecs.
- Support for looped BGM, using file's internal metadata for smooth transitions, with accurate
  sample counts.
- [Subsongs](doc/USAGE.md#subsongs), playing a format's multiple internal songs separately.
- Many types of companion files (data split into multiple files) and custom containers.
- Encryption keys, internal stream names, and other unusual cases found in game audio.
- [TXTH](doc/TXTH.md) function, to add external support for unsupported formats, including raw audio in
  many forms.
- [TXTP](doc/TXTP.md) function, for real-time and per-file config, like forced looping, removing
  channels, playing a single subsong, or fusing multiple files into one.
- Simple [external tagging](doc/USAGE.md#tagging) via .m3u files.
- [Plugins](#getting-vgmstream) are available for various media player software and operating systems.

Resources:
- Main repository: https://github.com/vgmstream/vgmstream/
- Automated builds: https://vgmstream.org
- Numbered releases: https://github.com/vgmstream/vgmstream/releases
- Additional help: https://www.hcs64.com/ (and [discord](https://discord.gg/7ddT4pP))


## Getting vgmstream
See [automated builds](https://vgmstream.org) and [releases](https://github.com/vgmstream/vgmstream/releases).

There are multiple end-user components:
- [vgmstream-cli](doc/USAGE.md#vgmstream-cli-command-line-decoder): A command-line decoder.
- [in_vgmstream](doc/USAGE.md#in_vgmstream-winamp-plugin): A Winamp plugin.
- [foo_input_vgmstream](doc/USAGE.md#foo_input_vgmstream-foobar2000-plugin): A foobar2000 component.
- [xmp-vgmstream](doc/USAGE.md#xmp-vgmstream-xmplay-plugin): An XMPlay plugin.
- [vgmstream.so](doc/USAGE.md#audacious-plugin): An Audacious plugin.
- [vgmstream123](doc/USAGE.md#vgmstream123-command-line-player): A command-line player.
- Other plugins and components may exist as external projects

The main library (plain *vgmstream*) is the code that handles audio conversion, while the
above components are what you use to get sound.

### Usage
To convert game audio to `.wav`, get *vgmstream-cli* then drag-and-drop files to it (support
may vary per O.S.). This should create `(file.extension).wav`, if the format is supported.

You can also try the online web player instead. See: https://vgmstream.org

Alternatively, install an audio player like *foobar2000* (on Windows) or *Audacious* (on Linux)
and the *vgmstream* plugin, and play files directly. They can unpack "subsongs" to the playlist,
set options like infinite looping, or convert many `.wav` at once with the player's interface.

See [components](doc/USAGE.md#components) in the *usage guide* for further explanations. The aim
is feature parity, but there are some differences between them.

Note that vgmstream cannot *encode* (convert from `.wav` to a game format), it only *decodes*
(plays game audio).

### Windows binaries
Prebuilt binaries:
- https://vgmstream.org (latest)
- https://github.com/vgmstream/vgmstream/releases (infrequent numbered releases)

The foobar2000 component is also available on https://www.foobar2000.org (current release).

Or compile from source, see the [build guide](doc/BUILD.md).

### Linux binaries
Prebuilt CLI binary, statically linked for systems running Linux kernel v3.2 and above:
- https://vgmstream.org (latest)
- https://github.com/vgmstream/vgmstream/releases (infrequent numbered releases)

Building from source will also give you *vgmstream.so* (Audacious plugin), and *vgmstream123*
(command-line player), which can't be statically linked.

When building it needs several external libraries. For a quick script for Debian and Ubuntu-style
distros run `./make-build-cmake.sh`. The script will need to install dependencies first, so you
may prefer to run steps manually, which the [build guide](doc/BUILD.md) describes in detail.

### macOS binaries
Prebuilt CLI binary:
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
