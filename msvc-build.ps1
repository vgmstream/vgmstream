[CmdletBinding()]
Param(
    [Parameter(Position=0, mandatory=$true)]
    [ValidateSet("Init", "Build", "Rebuild", "Clean", "Package", "PackageArtifacts")]
    [string]$Task
)

###############################################################################
# CONFIG
# set these vars to override project defaults
# can also create a mssvc-build.config.ps1 with those
###############################################################################
$config_file = ".\msvc-build.config.ps1"
if((Test-Path $config_file)) { . $config_file }

# - toolsets: "" (default), "v140" (MSVC 2015), "v141" (MSVC 2017), "v141_xp" (XP support), "v142" (MSVC 2019), etc
if (!$toolset) { $toolset = "" }

# - sdks: "" (default), "7.0" (Win7 SDK), "8.1" (Win8 SDK), "10.0" (Win10 SDK), etc
if (!$sdk) { $sdk = "" }

# - platforms: "" (default), "Win32", "x64"
if (!$platform) { $platform = "" }

# print compilation log
#$log = 1

# Debug or Release, usually
if (!$configuration) { $configuration = "Release" }

###############################################################################

$solution = "vgmstream_full.sln"
$dependencies = "dependencies"
$vswhere = "$dependencies/vswhere.exe"
$config = "/p:Configuration=" + $configuration
# not used ATM
$enable_aac = 0

if ($platform) { $platform = "/p:Platform=" + $platform }
if ($toolset) { $toolset = "/p:PlatformToolset=" + $toolset }
if ($sdk) { $sdk = "/p:WindowsTargetPlatformVersion=" + $sdk }

# https://stackoverflow.com/a/41618979/9919772
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# helper
function Unzip
{
    param([string]$zipfile, [string]$outpath)
    Write-Output "Extracting $zipfile"
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zipfile, $outpath)
}

# helper
function Download
{
    param([string]$uri, [string]$outfile)
    Write-Output "Downloading $uri"
    $wc = New-Object net.webclient
    $wc.Downloadfile($uri, $outfile)
}

# download and unzip dependencies
function Init
{
    Add-Type -AssemblyName System.IO.Compression.FileSystem

    Remove-Item -Path "$dependencies" -Recurse -ErrorAction Ignore
    New-Item "$dependencies" -Type directory -Force | out-null

    # vswhere: MSBuild locator
    # may already be in %ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
    # so could test that and skip this step
    Download "https://github.com/Microsoft/vswhere/releases/download/2.6.7/vswhere.exe" "$dependencies\vswhere.exe"

    # foobar: wtl
    Download "https://www.nuget.org/api/v2/package/wtl/10.0.10320" "$dependencies\wtl.zip"
    Unzip "$dependencies\wtl.zip" "$dependencies\wtl_tmp"
    Move-Item "$dependencies\wtl_tmp\lib\native" "$dependencies\wtl"
    Remove-Item -Path "$dependencies\wtl_tmp" -Recurse

    # foobar: sdk anti-hotlink (random link) defeater
    #Download "https://www.foobar2000.org/SDK" "$dependencies\SDK"
    #$key = (Select-String -Path $dependencies\SDK -Pattern "\/([a-f0-9]+)\/SDK-2018-01-11\.zip").matches.groups[1]
    #Remove-Item -Path "$dependencies\SDK"
    #Download "https://www.foobar2000.org/files/$key/SDK-2018-01-11.zip" "$dependencies\foobar.zip"

    # foobar: sdk direct link, but 2019< sdks gone ATM
    #Download "https://www.foobar2000.org/files/SDK-2018-01-11.zip" "$dependencies\foobar.zip"

    # foobar: sdk static mirror
    Download "https://github.com/vgmstream/vgmstream-deps/raw/master/foobar2000/SDK-2023-01-18.zip" "$dependencies\foobar.zip"
    Unzip "$dependencies\foobar.zip" "$dependencies\foobar"

    # foobar: aac (not used ATM)
    if ($enable_aac)
    {
        Download "https://github.com/kode54/fdk-aac/archive/master.zip" "$dependencies\fdk-aac.zip"
        Download "https://github.com/kode54/qaac/archive/master.zip" "$dependencies\qaac.zip"
        Unzip "$dependencies\fdk-aac.zip" "$dependencies\fdk-aac_tmp"
        Unzip "$dependencies\qaac.zip" "$dependencies\qaac_tmp"
        Move-Item "$dependencies\fdk-aac_tmp\fdk-aac-master" "$dependencies\fdk-aac"
        Move-Item "$dependencies\qaac_tmp\qaac-master" "$dependencies\qaac"
        Remove-Item -Path "$dependencies\fdk-aac_tmp" -Recurse
        Remove-Item -Path "$dependencies\qaac_tmp" -Recurse
    }

    # open foobar sdk project and modify WTL path
    # (maybe should just pass include to CL envvar: set CL=/I"(path)\WTL\Include")
    [xml]$proj = Get-Content $dependencies\foobar\foobar2000\helpers\foobar2000_sdk_helpers.vcxproj
    $proj.project.ItemDefinitionGroup | ForEach-Object {
        $_.ClCompile.AdditionalIncludeDirectories += ";../../../wtl/include"
    }
    $proj.Save("$dependencies\foobar\foobar2000\helpers\foobar2000_sdk_helpers.vcxproj")

    [xml]$proj = Get-Content $dependencies\foobar\libPPUI\libPPUI.vcxproj
    $proj.project.ItemDefinitionGroup | ForEach-Object {
        $_.ClCompile.AdditionalIncludeDirectories += ";../../wtl/include"
    }
    $proj.Save("$dependencies\foobar\libPPUI\libPPUI.vcxproj")
}

# main build
function CallMsbuild
{
    param([string]$target)
    if ($target) { $target = "/t:" + $target }

    # download dependencies if needed
    if(!(Test-Path $vswhere)) { Init }


    # autolocate MSBuild path
    $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe

    if(!($msbuild -and $(Test-Path $msbuild))) {
        throw "Unable to find MSBuild. Is Visual Studio installed?"
    }

    # TODO improve (why does every xxxxer make their own scripting engine)
    # main build (pass config separate and not as a single string)
    if (!$log) {
        if ($platform) {
            & $msbuild $solution $config $platform $toolset $sdk $target /m
        }
        else {
            & $msbuild $solution $config /p:Platform=Win32 $toolset $sdk $target /m
            if ($LASTEXITCODE -ne 0) {
                throw "MSBuild failed"
            }

            & $msbuild $solution $config /p:Platform=x64 $toolset $sdk $target /m
        }
    }
    else {
        Remove-Item "msvc-build*.log" -ErrorAction Ignore
        if ($platform) {
            & $msbuild $solution $config $platform $toolset $sdk $target /m >> "msvc-build.log"
        }
        else {
            & $msbuild $solution $config /p:Platform=Win32 $toolset $sdk $target /m >> "msvc-build-32.log"
            if ($LASTEXITCODE -ne 0) {
                throw "MSBuild failed"
            }
            & $msbuild $solution $config /p:Platform=x64 $toolset $sdk $target /m >> "msvc-build-64.log"
        }
    }

    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed"
    }
}

function Build
{
    CallMsbuild "Build"    
}

function Rebuild
{
    CallMsbuild "Rebuild"
}

function Clean
{
    CallMsbuild "Clean"
    # todo fix the above, for now:
    #Remove-Item -Path "$dependencies" -Recurse -ErrorAction Ignore
    Remove-Item -Path "build-msvc" -Recurse -ErrorAction Ignore

    Remove-Item -Path "Debug" -Recurse -ErrorAction Ignore
    Remove-Item -Path "Release" -Recurse -ErrorAction Ignore
    Remove-Item -Path "x64" -Recurse -ErrorAction Ignore

    Remove-Item -Path "bin" -Recurse -ErrorAction Ignore

    Remove-Item "msvc-build*.log" -ErrorAction Ignore
}

$cliFiles32 = @(
    "ext_libs/*.dll",
    "$configuration/vgmstream-cli.exe",
    "$configuration/in_vgmstream.dll",
    "$configuration/xmp-vgmstream.dll",
    "COPYING",
    "README.md"
    "doc/USAGE.md"
)

$cliFiles64 = @(
    "ext_libs/dll-x64/*.dll",
    "x64/$configuration/vgmstream-cli.exe",
    "COPYING",
    "README.md"
    "doc/USAGE.md"
)

$fb2kFiles32 = @(
    "ext_libs/*.dll",
    "$configuration/foo_input_vgmstream.dll",
    "README.md"
    "doc/USAGE.md"
)

$fb2kFiles64 = @(
    "ext_libs/dll-x64/*.dll"
    "x64/$configuration/foo_input_vgmstream.dll"
)

$fb2kFiles_remove = @(
    "bin/foobar2000/xxxx.dll"
)

$cliPdbFiles32 = @(
    "$configuration/vgmstream-cli.pdb",
    "$configuration/in_vgmstream.pdb",
    "$configuration/xmp-vgmstream.pdb"
)

$cliPdbFiles64 = @(
    "x64/$configuration/vgmstream-cli.pdb"
)

$fb2kPdbFiles32 = @(
    "$configuration/foo_input_vgmstream.pdb"
)

$fb2kPdbFiles64 = @(
    "x64/$configuration/foo_input_vgmstream.pdb"
)

# make a valid zip from a dir, since Compress-Archive uses non-standard backslashes
function CompressProperZip {
    param(
        [string]$Path,
        [string]$ZipFile
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem

    if (Test-Path $ZipFile) {
        Remove-Item $ZipFile -Force
    }

    # may use work dir otherwise
    $Path = (Resolve-Path $Path).Path
    if (-not $Path.EndsWith('\')) {
        $Path += '\'
    }

    $file = $null
    $zip = $null
    try {
        $file  = [System.IO.File]::Open($ZipFile, [System.IO.FileMode]::Create)
        $zip = New-Object System.IO.Compression.ZipArchive($file, [System.IO.Compression.ZipArchiveMode]::Create, $false)

        Get-ChildItem -Path $Path -Recurse -File | ForEach-Object {
            $fullPath = $_.FullName

            # load relative paths and normalize to forward slashes
            $relPath = $fullPath.Substring($Path.Length)
            $relPath = $relPath -replace '\\','/'

            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $fullPath, $relPath) | Out-Null
        }
    }
    finally {
        if ($zip) { $zip.Dispose() }
        if ($file)  { $file.Dispose() }
    }
}


function MakePackage
{
    Build

    if (!(Test-Path "$configuration/vgmstream-cli.exe")) {
        Write-Error "Unable to find binaries, check for compilation errors"
        return
    }

    mkdir -Force bin | Out-Null

    Compress-Archive $cliFiles32 bin/vgmstream-win.zip -Force
    Compress-Archive $cliFiles64 bin/vgmstream-win64.zip -Force

    # foobar 32 and 64-bit components go to the same file, in an extra "x64" subdir for the later
    mkdir -Force bin/foobar2000 | Out-Null
    mkdir -Force bin/foobar2000/x64 | Out-Null
    Copy-Item $fb2kFiles32 bin/foobar2000/ -Recurse -Force
    Copy-Item $fb2kFiles64 bin/foobar2000/x64/ -Recurse -Force
    Remove-Item $fb2kFiles_remove -ErrorAction Ignore

    # workaround for a foobar 2.0 64-bit bug: (earlier?) PowerShell creates zip paths with '\' (which seem
    # non-standard), and that confuses foobar when trying to unpack x64/* subdirs in the zip.
    $component_done = $false
    try {
        if (-not $component_done) {
            # makes standard zips and should be available in github actions, but not common in PATH/current dir
            & '7z' a -tzip bin/foo_input_vgmstream.fb2k-component ./bin/foobar2000/*
            $component_done = $true
            #Write-Output "Packaged foobar2000 component with 7z"
        }
    } catch { }

    try {
        if (-not $component_done) {
            # manually make a proper zip using PowerShell arcane stuff
            CompressProperZip -Path "bin\foobar2000" -ZipFile "bin\foo_input_vgmstream.fb2k-component"
            $component_done = $true
            #Write-Output "Packaged foobar2000 component with custom script"
        }
    } catch { }

    # shutil seems to default to Compress-Archive which uses backslashes in paths (could call a custom .py instead)
    #python -c "__import__('shutil').make_archive('bin\foo_input_vgmstream', 'zip', 'bin/foobar2000)"

    try {
        if (-not $component_done) {
            # PowerShell defaults, maybe it will work some day
            Write-Warning "Could not make a proper zip, component may only work with foobar 32-bit"
            Compress-Archive -Path bin/foobar2000/* bin/foo_input_vgmstream.zip -Force
            Move-Item bin/foo_input_vgmstream.zip bin/foo_input_vgmstream.fb2k-component -Force
        }
    } catch { }

    if (-not $component_done) {
        Write-Warning "Could not package foobar component"
    }

    if ($component_done) {
        Remove-Item -Path bin/foobar2000 -Recurse -ErrorAction Ignore
    }
}


# github actions/artifact uploads config, that need a dir with files to make an .zip artifact (don't allow single/pre-zipped files)
function MakePackageArtifacts
{
    MakePackage

    mkdir -Force bin/artifacts/cli-x32
    mkdir -Force bin/artifacts/cli-x64
    mkdir -Force bin/artifacts/foobar2000
    mkdir -Force bin/artifacts/foobar2000/x64
    mkdir -Force bin/artifacts/pdb/x32
    mkdir -Force bin/artifacts/pdb/x64

    Copy-Item $cliFiles32 bin/artifacts/cli-x32/ -Recurse -Force
    Copy-Item $cliFiles64 bin/artifacts/cli-x64/ -Recurse -Force
    Copy-Item $fb2kFiles32 bin/artifacts/foobar2000/ -Recurse -Force
    Copy-Item $fb2kFiles64 bin/artifacts/foobar2000/x64/ -Recurse -Force
    Remove-Item $fb2kFiles_remove -ErrorAction Ignore
    Copy-Item $cliPdbFiles32 bin/artifacts/pdb/x32/ -Recurse -Force
    Copy-Item $fb2kPdbFiles32 bin/artifacts/pdb/x32/ -Recurse -Force
    Copy-Item $cliPdbFiles64 bin/artifacts/pdb/x64/ -Recurse -Force
    Copy-Item $fb2kPdbFiles64 bin/artifacts/pdb/x64/ -Recurse -Force
}


switch ($Task)
{
    "Init" { Init }
    "Build" { Build }
    "Rebuild" { Rebuild }
    "Clean" { Clean }
    "Package" { MakePackage }
    "PackageArtifacts" { MakePackageArtifacts }
}
