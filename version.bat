@echo off

REM creates or updates version.h
REM params (optional): $1=filename (usually [relpath]/version.h), $2=VARNAME (usually VGMSTREAM_VERSION)

setlocal enableextensions enabledelayedexpansion

REM defaults
set VERSION_DEFAULT=unknown
set VERSION_FILE=%1
set VERSION_NAME=%2
if "%~1" == "" set VERSION_FILE=version.h
if "%~2" == "" set VERSION_NAME=VGMSTREAM_VERSION

if not "%version%"=="" set version=!version:^:=_!

cd /d "%~dp0"


REM try dynamic version from git
for /f %%v in ('git describe --always') do set version=%%v

if not "%version%"=="" set version=!version:^:=_!
if not "%version%"=="" goto :got_version
if exist "version.mk" goto :mk_version

echo Git version cannot be found, nor can version.mk. Generating unknown version.
set version=%VERSION_DEFAULT%
goto :got_version


REM try static version from .mk file
:mk_version
for /f "delims== tokens=2" %%v in (version.mk) do set version=%%v
set version=!version:^"=!
set version=!version: =!


REM version found, create version.h and update .mk
:got_version
set version_out=#define %VERSION_NAME% "%version%"
set version_mk=%VERSION_NAME% = "%version%"

echo %version_out%> %VERSION_FILE%_temp

if %version%==%VERSION_DEFAULT% goto :skip_generate
echo # static version string; update manually before and after every release.> "version.mk"
echo %version_mk%>> "version.mk"


:skip_generate
echo n | comp %VERSION_FILE%_temp %VERSION_FILE% > NUL 2> NUL

if not errorlevel 1 goto exit

copy /y %VERSION_FILE%_temp %VERSION_FILE% > NUL 2> NUL

:exit

del %VERSION_FILE%_temp
