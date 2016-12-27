@echo off
chcp 65001
REM #-------------------------------------------------------------------------
REM # VGMSTREAM REGRESSION TESTING SCRIPT
REM #
REM # Searches for files in a directory (or optionally subdirs) and compares
REM # the output of two test.exe versions, both wav and stdout, for regression
REM # testing. This creates and deletes temp files, trying to process all
REM # extensions found unless specified (except a few).
REM #
REM # Options: see below.
REM #-------------------------------------------------------------------------
REM #TODO: escape & ! % in file/folder names

setlocal enableDelayedExpansion


REM #options
REM # -vo <exe> -vn <exe>: path to old/new exe
set OP_CMD_OLD=test_old.exe
set OP_CMD_NEW=test.exe
REM # -f <filename>: search wildcard (ex. -f "*.adx")
set OP_SEARCH="*.*"
REM # -r: recursive subfolders
set OP_RECURSIVE=
REM # -nd: don't delete compared files
set OP_NODELETE=
REM # -nc: don't report correct files
set OP_NOCORRECT=


REM # parse options
:set_options
if "%~1"=="" goto end_options
if "%~1"=="-vo" set OP_CMD_OLD=%2
if "%~1"=="-vn" set OP_CMD_NEW=%2
if "%~1"=="-f"  set OP_SEARCH=%2
if "%~1"=="-r"  set OP_RECURSIVE=/s
if "%~1"=="-nd" set OP_NODELETE=true
if "%~1"=="-nc" set OP_NOCORRECT=true
shift
goto set_options
:end_options

REM # output color defs
set C_W=0e
set C_E=0c
set C_O=0f


REM # check exe
set CMD_CHECK=where "%OP_CMD_OLD%" "%OP_CMD_NEW%"
%CMD_CHECK% > nul
if %ERRORLEVEL% NEQ 0 (
    echo Old/new exe not found
    goto error
)
if %OP_SEARCH%=="" (
    echo Search wildcard not specified
    goto error
)

REM # process start
echo VRTS: start @%time%

REM # search for files
set CMD_DIR=dir /a:-d /b %OP_RECURSIVE% %OP_SEARCH%
set CMD_FIND=findstr /i /v "\.exe$ \.dll$ \.zip$ \.7z$ \.rar$ \.bat$ \.sh$ \.txt$ \.lnk$ \.wav$"

REM # process files
for /f "delims=" %%x in ('%CMD_DIR% ^| %CMD_FIND%') do (
    set CMD_FILE=%%x
    call :process_file "!CMD_FILE!"
)

REM # process end (ok)
goto done


REM # test a single file
:process_file outer
    REM # ignore files starting with dot (no filename)
    set CMD_SHORTNAME=%~n1
    if "%CMD_SHORTNAME%" == "" goto continue

    REM # get file
    set CMD_FILE=%1
    set CMD_FILE=%CMD_FILE:"=%
    REM echo VTRS: file %CMD_FILE%
   
    REM # old/new temp output
    set WAV_OLD=%CMD_FILE%.old.wav
    set TXT_OLD=%CMD_FILE%.old.txt
    set CMD_VGM_OLD="%OP_CMD_OLD%" -o "%WAV_OLD%" "%CMD_FILE%"
    %CMD_VGM_OLD% 1> "%TXT_OLD%" 2>&1  & REM || goto error

    set WAV_NEW=%CMD_FILE%.new.wav
    set TXT_NEW=%CMD_FILE%.new.txt
    set CMD_VGM_NEW="%OP_CMD_NEW%" -o "%WAV_NEW%" "%CMD_FILE%"
    %CMD_VGM_NEW% 1> "%TXT_NEW%" 2>&1  & REM || goto error

    REM # ignore if no files are created (unsupported formats)
    if not exist "%WAV_NEW%" (
        if not exist "%WAV_OLD%" (
            REM echo VRTS: nothing created for file %CMD_FILE%
            if exist "%TXT_NEW%"  del /a:a "%TXT_NEW%"
            if exist "%TXT_OLD%"  del /a:a "%TXT_OLD%"
            goto continue
        )
    )

    REM # compare files (doesn't use /b for speedup, somehow)
    set CMP_WAV=fc /a /lb1 "%WAV_OLD%" "%WAV_NEW%"
    set CMP_TXT=fc /a /lb1 "%TXT_OLD%" "%TXT_NEW%"

    %CMP_WAV% 1> nul 2>&1
    set CMP_WAV_ERROR=0
    if %ERRORLEVEL% NEQ 0  set CMP_WAV_ERROR=1
    
    %CMP_TXT% 1> nul 2>&1
    set CMP_TXT_ERROR=0
    if %ERRORLEVEL% NEQ 0  set CMP_TXT_ERROR=1

    REM # print output
    if %CMP_WAV_ERROR% EQU 1 (
        if %CMP_TXT_ERROR% EQU 1  ( 
            call :echo_color %C_E% "%CMD_FILE%" "wav and txt diffs"
        ) else (
            call :echo_color %C_E% "%CMD_FILE%" "wav diffs"
        )
    ) else (
        if %CMP_TXT_ERROR% EQU 1 (
            call :echo_color %C_W% "%CMD_FILE%" "txt diffs"
        ) else (
            if "%OP_NOCORRECT%" == "" (
                call :echo_color %C_O% "%CMD_FILE%" "no diffs"
            )
        )
    )

    REM # delete temp files
    if "%OP_NODELETE%" == "" (
        if exist "%WAV_OLD%"  del /a:a "%WAV_OLD%"
        if exist "%TXT_OLD%"  del /a:a "%TXT_OLD%"
        if exist "%WAV_NEW%"  del /a:a "%WAV_NEW%"
        if exist "%TXT_NEW%"  del /a:a "%TXT_NEW%"
    )

:continue
exit /B
REM :process_file end, continue from last call


REM # hack to get colored output in Windows CMD using findstr + temp file
:echo_color
set TEMP_FILE=%2-result
set TEMP_FILE=%TEMP_FILE:"=%
set TEMP_TEXT=%3
set TEMP_TEXT=%TEMP_TEXT:"=%
echo  %TEMP_TEXT% > "%TEMP_FILE%"
REM # show colored filename + any text in temp file
findstr /v /a:%1 /r "^$" "%TEMP_FILE%" nul
del "%TEMP_FILE%"
exit /B
REM :echo_color end, continue from last call


:done
echo VRTS: done @%time%
goto exit


:error
echo VRTS: error @%time%
goto exit


:exit
