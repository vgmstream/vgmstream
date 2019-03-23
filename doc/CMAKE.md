# CMake Build Instructions

## Build requirements

**CMake**: Needs v3.5 or later
- https://cmake.org/download/

**Git**: optional, to generate version numbers:
- Git for Windows: https://git-scm.com/download/win

If building for Windows, you need one of the following:

**GCC / Make**: In Windows this means one of these two somewhere in PATH:
- MinGW-w64 (32bit version): https://sourceforge.net/projects/mingw-w64/
  - Use this for easier standalone executables
  - Latest online installer with any config should work (for example: gcc-7.2.0, i686, win32, sjlj).
- MSYS2 with the MinGW-w64_shell (32bit) package: https://msys2.github.io/

**MSVC / Visual Studio**: Microsoft's Visual C++ and MSBuild, bundled in either:
- Visual Studio (2015/2017/latest): https://www.visualstudio.com/downloads/
  - Visual Studio Community should work (free, but must register after trial period)
- Visual C++ Build Tools (no IDE): http://landinghub.visualstudio.com/visual-cpp-build-tools

If building the CLI for *nix-based OSes, vgmstream123 also needs the following:

- **LibAO**

If building for *nix-based OSes, the following libraries are optional:

- **libmpg123**
- **libvorbis** (really libvorbisfile, though)
- **FFmpeg**

## Build directions

It is recommended to do out-of-source builds as opposed to in-source builds. Out-of-source builds have been tested to work, while in-source builds have not been tested at all.

***NOTE:*** The CMake scripts attempt to collect all the source files are configuration time. If you are following vgmstream development through git or adding your own source files, you **MUST** re-run CMake manually to regenerate the files. Failure to do so can result in either missing functionality or compile errors.

First you will need to run CMake to generate the build setup. You can use either the CMake GUI or run CMake from the command line.

### CMake GUI

If you have access to the CMake GUI, you can use that to create your build setup. Select where the source code is (that should be the directory just above this one) and where you wish to build to.

You may have to add some entries before configuring will succeed. See the [CMake Cache Entries](#cmake-cache-entries) section for details on the entries.

Once the entries are in place, you can click on Generate (or Configure then Generate). This should ask you what build system you wish to use. As long as there are no errors, you should see the following at the bottom of the window:

```
Configuring done
Generating done
```

Before that you'll see what options are enabled and disabled, what is going to be built and where they will be installed to.

If you decided to build for a project-based GUI, you can click on Open Project to open that. (NOTE: Only Visual Studio has been tested as a project-based GUI.) If you decided to build for a command line build system, you can open up the command line for the build directory and run your build system.

### CMake command line

If you don't have access to the CMake GUI or would prefer to only use the command line, you can run CMake from there. Navigate to the directory you wish to build to and run the following:

```
cmake -G "<generator>" <options> <path to source code>
```

Replace `<generator>` with the CMake generator you wish to use as your build system. Make note of the quotes, and use `cmake -h` to get a list of generators for your system.

You may have to add some entries before configuring will success. See the [CMake Cache Entries](#cmake-cache-entries) section for details on the entries.

Place your entries in the `<options>` section of the above command, with each option in the form of `-D<optionname>:<type>=<value>`. Replace `<path to source code>` with the path where the source code is (that should be the directory just above this one).

Once you have run the command, as long as there are no errors, you should see the following at the bottom of the window:

```
Configuring done
Generating done
```

Before that you'll see what options are enabled and disabled, what is going to be built and where they will be installed to.

You can now run the build system you chose as your generator above, whether that be a command line build system like Unix Make or a project-based GUI like Eclipse.

### CMake Cache Entries

The following are the various options and cache entries that you can choose to use when building vgmstream.

If not using a project-based GUI, then you will also need to set what build type you want. This can be set with the `CMAKE_BUILD_TYPE` option and takes one of the following values:

- **Debug**
- **Release**
- **RelWithDebInfo**: Like Release but with debugging information included
- **MinSizeRel**: Like Release but aims for minimum size

#### Library Options

All of these options are of type BOOL and can be set to either `ON` or `OFF`. Most of the details on these libraries can be found in the [External Libraries section of BUILD.md](BUILD.md#external-libraries).

- **USE_FDKAAC**: Chooses if you wish to use FDK-AAC/QAAC for support of MP4 AAC. Note that this requires `QAAC_PATH` and `FDK_AAC_PATH` to also be given if the option is `ON`. The default for is `ON`. See the foobar2000 plugin section of [BUILD.md](BUILD.md) for more information on this.
- **USE_MPEG**: Chooses if you wish to use libmpg123 for support of MP1/MP2/MP3. The default is `ON`.
- **USE_VORBIS**: Chooses if you wish to use libvorbis for support of Vorbis. The default is `ON`.
- **USE_FFMPEG**: Chooses if you wish to use FFmpeg for support of many codecs. The default is `ON`.
- **USE_MAIATRAC3PLUS**: Chooses if you wish to use MAIATRAC3+ for support of ATRAC3+. The default is `OFF`. It is not recommended to enable.

The following options are currently only available for Windows:

- **USE_G7221**: Chooses if you wish to use libg7221_decode for support of ITU-T G.722.1 annex C. The default is `ON`.
- **USE_G719**: Chooses if you wish to use libg719_decode for support ITU-T G.719. The default is `ON`.
- **USE_ATRAC9**: Chooses if you wish to use LibAtrac9 for support of ATRAC9. The default is `ON`.
- **USE_CELT**: Chooses if you wish to use libcelt for support of FSB CELT versions 0.6.1 and 0.11.0. The default is `ON`.

#### Build Options

All of these options are of type BOOL and can be set to either `ON` or `OFF`.

- **BUILD_CLI**: Chooses if you wish to build the vgmstream CLI program (as well as vgmstream123 on *nix-based OSes). The default is `ON`.

The following options are only available for Windows:

- **BUILD_FB2K**: Chooses if you wish to build the foobar2000 component. Note that this requires `FB2K_SDK_PATH` and `WTL_INCLUDE_PATH` to also be given if this option is `ON`. The default for is `ON`.
- **BUILD_WINAMP**: Chooses if you wish to build the Winamp plugin. The default is `ON`.
- **BUILD_XMPLAY**: Chooses if you wish to build the XMPlay plugin. The default is `ON`.

The following option is only available for *nix-based OSes:

- **BUILD_AUDACIOUS**: Chooses if you wish to build the Audacious plugin. The default is `ON`.

#### Paths

All of these paths are of type PATH.

If FDK-AAC/QAAC support is enabled, the following paths are required (with more details in the foobar2000 plugin section of [BUILD.md](BUILD.md)):

- **QAAC_PATH**: The path to the QAAC library. It can be obtained at https://github.com/kode54/qaac
- **FDK_AAC_PATH**: The path to the FDK-AAC library. It can be obtained at https://github.com/kode54/fdk-aac

If MAIATRAC3+ support is enabled, the following path is required:

- **MAIATRAC3PLUS_PATH**: The path to the MAIATRAC3+ library. It is not recommended to use.

The CLI/vgmstream123 programs are normally installed to `CMAKE_INSTALL_PREFIX`, changing this will change where those are installed.

If building the foobar2000 component, the following paths are required:

- **FB2K_SDK_PATH**: The path to the foobar2000 SDK.
- **WTL_INCLUDE_PATH**: The path to the include directory of the WTL.
- **FB2K_COMPONENT_INSTALL_PREFIX**: The path to the foobar2000 component installation directory. If you want to install the plugin for all users, you could set this to the `components` directory of the foobar2000 installation. Otherwise you'll want to set this to the `user-components` directory of your foobar2000 folder within your profile, the location of which depends on your current OS. The component as well as the required DLLs will be installed in this directory.

If building the Winamp plugin, the following path is required:

- **WINAMP_INSTALL_PREFIX**: The path to the Winamp installation. The required DLLs will be installed next to `winamp.exe` and the plugin itself will be installed to the `Plugins` directory.

If building the XMPlay plugin, the following path is required:

- **XMPLAY_INSTALL_PREFIX**: The path to the XMPlay installation. The required DLLs and the plugin will be installed in this directory.

If building the Audacious plugin, no path needs to be given, it will be found by CMake.

## Installation

After the above build has been done, the programs and plugins can be installed with CMake as well. For project-based GUIs, running the `INSTALL` target will install the files. For command line build systems, use the `install` target.
