@ECHO OFF
GOTO WORK
REM GOTO COMMAND ABOVE IS TO SKIP PARSING THIS COMMENT SECTION.
REM 
REM WARNING: DO NOT DELETE THE "EXTRA" NEW LINES AT THE END OF THE FILE.
REM (THEY PREVENT THE SCRIPT APPENDING A NEW LINE TO THE OUTPUT.)
REM 
REM Searches for the string "VERSION" in the version.mk
REM file in the script's directory, %~dp0 is a reference to
REM the script's absolute path on disk, then if found,
REM echos "#define VERSION " and the third "token" in the found 
REM string to a file called VERSION.H in the same directory.
REM "Tokens" are the result of taking the string input and breaking 
REM it apart at each delimiter character. The default delimiter
REM is a space character. So as long as the version string is
REM something like the following: 
REM VERSION <ANYTHING HERE> <WHAT IS EXTRACTED>
REM the result from this script will be whatever is at
REM <WHAT IS EXTRACTED>'s place in the file.
REM Ex. 
REM Line: VERSION = "r2313"
REM Result of the script: "r2313"
:WORK
FOR /F "tokens=3 delims= " %%G IN ('FINDSTR "VERSION" %~dp0version.mk') DO ECHO #define VERSION %%G > %~dp0VERSION.H
