@echo off

REM creates or updates version.h
REM params: $1=filename (usually version.h), $2=VARNAME (usually VERSION)

setlocal enableextensions enabledelayedexpansion

cd /d "%~dp0"

for /f %%v in ('git describe --always --tag') do set version=%%v

if not "%version%"=="" set version=!version:^:=_!

if not "%version%"=="" goto :gotversion

if exist "version.mk" goto :getversion

echo Git cannot be found, nor can version.mk. Generating unknown version.

set version=unknown

goto :gotversion

:getversion

for /f "delims== tokens=2" %%v in (version.mk) do set version=%%v

set version=!version:^"=!
set version=!version: =!

:gotversion

set version_out=#define %2 "%version%"
set version_mk=%2 = "%version%"

echo %version_out%> %1_temp

if %version%==unknown goto :skipgenerate

echo # static version string; update manually before and after every release.> "version.mk"
echo %version_mk%>> "version.mk"

:skipgenerate

echo n | comp %1_temp %1 > NUL 2> NUL

if not errorlevel 1 goto exit

copy /y %1_temp %1 > NUL 2> NUL

:exit

del %1_temp
