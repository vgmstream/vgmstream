[CmdletBinding()]
Param(
    [Parameter(Position=0, mandatory=$true)]
    [ValidateSet("Init", "Build", "Package")]
    [string]$Task
)

# https://stackoverflow.com/a/41618979/9919772
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$solution = "vgmstream_full.sln"
$vswhere = "dependencies/vswhere.exe"
$config = "/p:Configuration=Release"
$onAppveyor = ($env:APPVEYOR -eq "true")
$appveyorLoggerPath = "C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"

$fb2kFiles = @(
    "ext_libs/*.dll",
    "ext_libs/*.dll.asc",
    "Release/foo_input_vgmstream.dll",
    "README.md"
)

$cliFiles = @(
    "ext_libs/*.dll",
    "Release/in_vgmstream.dll",
    "Release/test.exe",
    "Release/xmp-vgmstream.dll",
    "COPYING",
    "README.md"
)

$fb2kPdbFiles = @(
    "Release/foo_input_vgmstream.pdb"
)

$cliPdbFiles = @(
    "Release/in_vgmstream.pdb",
    "Release/test.pdb",
    "Release/xmp-vgmstream.pdb"
)

function Unzip
{
    param([string]$zipfile, [string]$outpath)
    Write-Output "Extracting $zipfile"
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zipfile, $outpath)
}

function Download
{
    param([string]$uri, [string]$outfile)
    Write-Output "Downloading $uri"
    $wc = New-Object net.webclient
    $wc.Downloadfile($uri, $outfile)
}

function Init
{
    Add-Type -AssemblyName System.IO.Compression.FileSystem

    Remove-Item -Path "dependencies" -Recurse -ErrorAction Ignore
    New-Item dependencies -Type directory -Force | out-null

    Download "https://github.com/kode54/fdk-aac/archive/master.zip" "dependencies\fdk-aac.zip"
    Download "https://github.com/kode54/qaac/archive/master.zip" "dependencies\qaac.zip"
    Download "https://www.nuget.org/api/v2/package/wtl/9.1.1" "dependencies\wtl.zip"
    Download "https://github.com/Microsoft/vswhere/releases/download/2.6.7/vswhere.exe" "dependencies\vswhere.exe"

    Download "https://www.foobar2000.org/SDK" "dependencies\SDK"
    $key = (Select-String -Path dependencies\SDK -Pattern "\/([a-f0-9]+)\/SDK-2018-01-11\.zip").matches.groups[1]
    Remove-Item -Path "dependencies\SDK"
    Download "https://www.foobar2000.org/files/$key/SDK-2018-01-11.zip" "dependencies\foobar.zip"

    Unzip "dependencies\fdk-aac.zip" "dependencies\fdk-aac_tmp"
    Unzip "dependencies\qaac.zip" "dependencies\qaac_tmp"
    Unzip "dependencies\wtl.zip" "dependencies\wtl_tmp"
    Unzip "dependencies\foobar.zip" "dependencies\foobar"

    Move-Item "dependencies\fdk-aac_tmp\fdk-aac-master" "dependencies\fdk-aac"
    Move-Item "dependencies\qaac_tmp\qaac-master" "dependencies\qaac"
    Move-Item "dependencies\wtl_tmp\lib\native" "dependencies\wtl"

    Remove-Item -Path "dependencies\fdk-aac_tmp" -Recurse
    Remove-Item -Path "dependencies\qaac_tmp" -Recurse
    Remove-Item -Path "dependencies\wtl_tmp" -Recurse

    [xml]$proj = Get-Content dependencies\foobar\foobar2000\ATLHelpers\foobar2000_ATL_helpers.vcxproj
    $proj.project.ItemDefinitionGroup | ForEach-Object {
        $includes = $proj.CreateElement("AdditionalIncludeDirectories", $proj.project.NamespaceURI)
        $includes.InnerText = "../../../wtl/include"
        $_.ClCompile.AppendChild($includes)
    }
    $proj.Save("dependencies\foobar\foobar2000\ATLHelpers\foobar2000_ATL_helpers.vcxproj")
}

function Package
{
    Compress-Archive $cliFiles Release/test.zip -Force
    Compress-Archive $fb2kFiles Release/foo_input_vgmstream.zip -Force
    Move-Item Release/foo_input_vgmstream.zip Release/foo_input_vgmstream.fb2k-component -Force
    Compress-Archive $cliPdbFiles Release/test.pdb.zip -Force
    Compress-Archive $fb2kPdbFiles Release/foo_input_vgmstream.pdb.zip -Force
}

function Build
{
    $commit = & git describe --always
    if($onAppveyor) {
        if($env:APPVEYOR_PULL_REQUEST_NUMBER) {
            $prCommits = & git rev-list "$env:APPVEYOR_REPO_BRANCH.." --count
            $prNum = ".PR$env:APPVEYOR_PULL_REQUEST_NUMBER.$prCommits"
        }
        if($env:APPVEYOR_REPO_BRANCH -ne "master") { $branch = ".$env:APPVEYOR_REPO_BRANCH" }
        $version = "$commit$prNum$branch"
        Update-AppveyorBuild -Version $version -errorAction SilentlyContinue
    }

    if(!(Test-Path $vswhere)) { Init }

    $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe

    if(!($msbuild -and $(Test-Path $msbuild))) {
        Write-Error "Unable to find MSBuild. Is Visual Studio installed?"
    }

    $logger = ""
    if(Test-Path $appveyorLoggerPath) {
        $logger = "/logger:$appveyorLoggerPath"
    }

    & $msbuild $solution $config $logger /m

    Package
}

switch ($Task)
{
    "Init" { Init }
    "Build" { Build }
    "Package" { Package }
}
