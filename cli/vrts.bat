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


REM #-------------------------------------------------------------------------
REM #options
REM #-------------------------------------------------------------------------
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
REM # -p: performance test new (decode with new exe and no comparison done)
REM # -P: performance test new (same but also don't write file)
REM # -po: performance test old (decode with new old and no comparison done)
REM # -Po: performance test old (same but also don't write file)
REM # -Pm: performance test new (only parse meta)
REM # -Pmo: performance test old (only parse meta)
set OP_PERFORMANCE=
REM # -fc <exe>: file comparer
REM # Windows's FC is slow, try -fc ffc.exe (https://github.com/bnnm/vgm-tools/tree/master/misc/ffc)
set OP_CMD_FC=fc /a /b


REM # parse options
:set_options
if "%~1"=="" goto end_options
if "%~1"=="-vo" set OP_CMD_OLD=%2
if "%~1"=="-vn" set OP_CMD_NEW=%2
if "%~1"=="-f"  set OP_SEARCH=%2
if "%~1"=="-r"  set OP_RECURSIVE=/s
if "%~1"=="-nd" set OP_NODELETE=true
if "%~1"=="-nc" set OP_NOCORRECT=true
if "%~1"=="-p"  set OP_PERFORMANCE=1
if "%~1"=="-P"  set OP_PERFORMANCE=2
if "%~1"=="-po" set OP_PERFORMANCE=3
if "%~1"=="-Po" set OP_PERFORMANCE=4
if "%~1"=="-Pm"  set OP_PERFORMANCE=5
if "%~1"=="-Pmo" set OP_PERFORMANCE=6
if "%~1"=="-fc" set OP_CMD_FC=%2
shift
goto set_options
:end_options

REM # output color defs
set COLOR_WN=0e
set COLOR_ER=0c
set COLOR_OK=0f

REM # remove command options and possibly "
for /f "tokens=1-1 delims= "    %%A in ("%OP_CMD_OLD%") do set CHECK_OLD=%%A
for /f "tokens=1-1 delims= "    %%A in ("%OP_CMD_NEW%") do set CHECK_NEW=%%A
set CHECK_OLD=%CHECK_OLD:"=%
set CHECK_NEW=%CHECK_NEW:"=%

REM # check exe
set CMD_CHECK=where "%CHECK_OLD%" "%CHECK_NEW%"
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
set TIME_START=%time%
set FILES_OK=0
set FILES_KO=0
echo VRTS: start @%TIME_START%

REM # search for files
set CMD_DIR=dir /a:-d /b %OP_RECURSIVE% %OP_SEARCH%
set CMD_FIND=findstr /i /v "\.exe$ \.dll$ \.zip$ \.7z$ \.rar$ \.bat$ \.sh$ \.txt$ \.lnk$ \.wav$"

REM # process files
for /f "delims=" %%x in ('%CMD_DIR% ^| %CMD_FIND%') do (
    set CMD_FILE=%%x

    REM # must enable/disable after setting CMD as ! & in dir names would become expanded/removed otherwise
    setlocal EnableDelayedExpansion

    if "%OP_PERFORMANCE%" == "" (
        call :process_file "!CMD_FILE!"
    ) else (
        call :performance_file "!CMD_FILE!"
    )

    REM this doesn't seem to properly disable it and will error out after N files
    REM but using endlocal will remove vars and counters won't work
    REM setlocal DisableDelayedExpansion
    endlocal
)

REM # find time elapsed
set TIME_END=%time%
for /F "tokens=1-4 delims=:.," %%a in ("%TIME_START%") do (
   set /A "TIME_START_S=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
for /F "tokens=1-4 delims=:.," %%a in ("%TIME_END%") do (
   set /A "TIME_END_S=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
set /A TIME_ELAPSED_S=(TIME_END_S-TIME_START_S)/100
set /A TIME_ELAPSED_C=(TIME_END_S-TIME_START_S)%%100


REM # process end (ok)
echo VRTS: done @%TIME_END% (%TIME_ELAPSED_S%,%TIME_ELAPSED_C%s)
echo VRTS: ok=%FILES_OK%, ko=%FILES_KO%

goto exit


REM # ########################################################################
REM # test a single file
REM # ########################################################################
:process_file outer
    REM # ignore files starting with dot (no filename)
    set CMD_SHORTNAME=%~n1
    if "%CMD_SHORTNAME%" == "" goto process_file_continue


    REM # get file (not from arg %1)
    set CMD_FILE=!CMD_FILE!
    set CMD_FILE=!CMD_FILE:"=!
    REM echo VTRS: file !CMD_FILE!

    REM # old/new temp output
    set WAV_OLD=!CMD_FILE!.old.wav
    set TXT_OLD=!CMD_FILE!.old.txt
    set CMD_VGM_OLD="%OP_CMD_OLD%" -o "!WAV_OLD!" "!CMD_FILE!"
    !CMD_VGM_OLD! 1> "!TXT_OLD!" 2>&1  & REM || goto error

    set WAV_NEW=!CMD_FILE!.new.wav
    set TXT_NEW=!CMD_FILE!.new.txt
    set CMD_VGM_NEW="%OP_CMD_NEW%" -o "!WAV_NEW!" "!CMD_FILE!"
    !CMD_VGM_NEW! 1> "!TXT_NEW!" 2>&1  & REM || goto error

    REM # ignore if no files are created (unsupported formats)
    if not exist "!WAV_NEW!" (
        if not exist "!WAV_OLD!" (
            REM echo VRTS: nothing created for file !CMD_FILE!
            if exist "!TXT_NEW!"  del /a:a "!TXT_NEW!"
            if exist "!TXT_OLD!"  del /a:a "!TXT_OLD!"
            goto process_file_continue
        )
    )

    REM # compare files (without /b may to be faster for small files?)
    set CMP_WAV=%OP_CMD_FC% "!WAV_OLD!" "!WAV_NEW!"
    set CMP_TXT=%OP_CMD_FC% "!TXT_OLD!" "!TXT_NEW!"

    !CMP_WAV! 1> nul 2>&1
    set CMP_WAV_ERROR=0
    if %ERRORLEVEL% NEQ 0  set CMP_WAV_ERROR=1

    !CMP_TXT! 1> nul 2>&1
    set CMP_TXT_ERROR=0
    if %ERRORLEVEL% NEQ 0  set CMP_TXT_ERROR=1

    REM # print output
    if %CMP_WAV_ERROR% EQU 1 (
        if %CMP_TXT_ERROR% EQU 1  (
            call :echo_color %COLOR_ER% "!CMD_FILE!" "wav and txt diffs"
        ) else (
            call :echo_color %COLOR_ER% "!CMD_FILE!" "wav diffs"
        )
        set /a FILES_KO=FILES_KO + 1
    ) else (
        if %CMP_TXT_ERROR% EQU 1 (
            call :echo_color %COLOR_WN% "!CMD_FILE!" "txt diffs"
        ) else (
            if "%OP_NOCORRECT%" == "" (
                call :echo_color %COLOR_OK% "!CMD_FILE!" "no diffs"
            )
        )
        
        set /a FILES_OK=FILES_OK + 1
    )

    REM # delete temp files
    if "%OP_NODELETE%" == "" (
        if exist "!WAV_OLD!"  del /a:a "!WAV_OLD!"
        if exist "!TXT_OLD!"  del /a:a "!TXT_OLD!"
        if exist "!WAV_NEW!"  del /a:a "!WAV_NEW!"
        if exist "!TXT_NEW!"  del /a:a "!TXT_NEW!"
    )

:process_file_continue
exit /B
REM :process_file end, continue from last call


REM # ########################################################################
REM # decode only (no comparisons done), for performance testing
REM # ########################################################################
:performance_file outer
    REM # ignore files starting with dot (no filename)
    set CMD_SHORTNAME=%~n1
    if "%CMD_SHORTNAME%" == "" goto performance_file_continue

    REM # get file (not from arg %1)
    set CMD_FILE=!CMD_FILE!
    set CMD_FILE=!CMD_FILE:"=!
    REM echo VTRS: file !CMD_FILE!

    set WAV_NEW=!CMD_FILE!.test.wav
    if "%OP_PERFORMANCE%" == "1" (
        set CMD_VGM="%OP_CMD_NEW%" -o "!WAV_NEW!" "!CMD_FILE!"
    )
    if "%OP_PERFORMANCE%" == "2" (
        set CMD_VGM="%OP_CMD_NEW%" -O "!CMD_FILE!"
    )
    if "%OP_PERFORMANCE%" == "3" (
        set CMD_VGM="%OP_CMD_OLD%" -o "!WAV_OLD!" "!CMD_FILE!"
    )
    if "%OP_PERFORMANCE%" == "4" (
        set CMD_VGM="%OP_CMD_OLD%" -O "!CMD_FILE!"
    )
    if "%OP_PERFORMANCE%" == "5" (
        set CMD_VGM="%OP_CMD_NEW%" -m "!CMD_FILE!"
    )
    if "%OP_PERFORMANCE%" == "6" (
        set CMD_VGM="%OP_CMD_OLD%" -m "!CMD_FILE!"
    )

    !CMD_VGM! 1> nul 2>&1  & REM || goto error
    set CMP_WAV_ERROR=0
    if %ERRORLEVEL% NEQ 0  set CMP_WAV_ERROR=1

    call :echo_color %COLOR_OK% "!CMD_FILE!" "done"

    REM # ignore output
    if exist "!WAV_NEW!"  del /a:a "!WAV_NEW!"

:performance_file_continue
exit /B
REM :performance_file end, continue from last call


REM # ########################################################################
REM # hack to get colored output in Windows CMD using findstr + temp file
REM # ########################################################################
:echo_color
REM # don't use %2 but !CMD_FILE!
set TEMP_FILE=!CMD_FILE!-result
set TEMP_FILE=!TEMP_FILE:"=!
set TEMP_TEXT=%3
set TEMP_TEXT=!TEMP_TEXT:"=!
echo  !TEMP_TEXT! > "!TEMP_FILE!"

REM # show colored filename + any text in temp file
findstr /a:%1 /r ".*" "!TEMP_FILE!" nul
del "!TEMP_FILE!"

exit /B
REM :echo_color end, continue from last call


REM # ########################################################################

:error
echo VRTS: error
goto exit

:exit
