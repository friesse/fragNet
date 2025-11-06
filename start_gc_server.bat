@echo off
REM ###############################################################################
REM Game Coordinator Server Startup Script (Windows)
REM ###############################################################################

echo =====================================
echo Starting Game Coordinator Server
echo =====================================
echo.

REM Configuration - Edit these values or set environment variables before running
if not defined GC_BIND_IP set GC_BIND_IP=0.0.0.0
if not defined GC_PORT set GC_PORT=27016

REM Get script directory
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

REM Check if build directory exists
if not exist "build" (
    echo ERROR: Build directory not found!
    echo Please build the project first:
    echo   mkdir build ^&^& cd build
    echo   cmake -G "Visual Studio 17 2022" -A Win32 ..
    echo   cmake --build . --config Release
    pause
    exit /b 1
)

REM Find the gc_server executable
set GC_SERVER=
if exist "build\gc_server\Release\gc_server.exe" set GC_SERVER=build\gc_server\Release\gc_server.exe
if exist "build\gc_server\Debug\gc_server.exe" set GC_SERVER=build\gc_server\Debug\gc_server.exe
if exist "build\Release\gc_server.exe" set GC_SERVER=build\Release\gc_server.exe
if exist "build\Debug\gc_server.exe" set GC_SERVER=build\Debug\gc_server.exe
if exist "gc_server.exe" set GC_SERVER=gc_server.exe

if "%GC_SERVER%"=="" (
    echo ERROR: gc_server.exe not found!
    echo Checked locations:
    echo   - build\gc_server\Release\gc_server.exe
    echo   - build\gc_server\Debug\gc_server.exe
    echo   - build\Release\gc_server.exe
    echo   - build\Debug\gc_server.exe
    echo   - gc_server.exe
    pause
    exit /b 1
)

echo Found executable: %GC_SERVER%
echo.

REM Display configuration
echo Configuration:
echo   Bind IP:   %GC_BIND_IP%
echo   Port:      %GC_PORT%
echo.

REM Check if port is in use (requires netstat)
echo Checking if port %GC_PORT% is available...
netstat -ano | findstr ":%GC_PORT% " >nul 2>&1
if %errorlevel% equ 0 (
    echo WARNING: Port %GC_PORT% appears to be in use!
    echo Processes using this port:
    netstat -ano | findstr ":%GC_PORT% "
    echo.
    set /p CONTINUE="Continue anyway? (Y/N): "
    if /i not "%CONTINUE%"=="Y" exit /b 1
)

REM Check if Steam SDK exists
if exist "steamworks\sdk" (
    echo [OK] Steam SDK found
) else (
    echo [WARN] Steam SDK not found at expected location
)

REM Set Steam App ID
set SteamAppId=730
echo [OK] Set SteamAppId=730 (CS:GO)
echo.

REM Create logs directory
if not exist "logs" mkdir logs

REM Generate log filename with timestamp
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set datetime=%%I
set LOG_FILE=logs\gc_server_%datetime:~0,8%_%datetime:~8,6%.log

echo Starting server...
echo Logs will be written to: %LOG_FILE%
echo.
echo Press Ctrl+C to stop the server
echo.

REM Start the server with output to both terminal and log file
"%GC_SERVER%" 2>&1 | tee "%LOG_FILE%"

if %errorlevel% equ 0 (
    echo.
    echo Server exited normally
) else (
    echo.
    echo ERROR: Server exited with error code: %errorlevel%
)

pause
