:: just run this to update our protobufs

@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%
for %%I in ("%SCRIPT_DIR%\..\..\") do set PROTO_PATH=%%~fI
if %PROTO_PATH:~-1%==\ set PROTO_PATH=%PROTO_PATH:~0,-1%

:: set paths
set PROTOC_PATH=%SCRIPT_DIR%\protoc.exe
set PROTO_FILE=%PROTO_PATH%\cc_gcmessages.proto

echo Script location: %SCRIPT_DIR%
echo Proto path: %PROTO_PATH%
echo Proto file: %PROTO_FILE%
echo Protoc path: %PROTOC_PATH%



echo.
echo compiling cc_gcmessages.proto...
"%PROTOC_PATH%" ^
    --proto_path="%PROTO_PATH%" ^
    --cpp_out="%PROTO_PATH%" ^
    "%PROTO_FILE%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo done!
    echo.
)

pause