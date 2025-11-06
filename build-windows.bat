@echo off
REM Build script for Game Coordinator Server on Windows with MSVC

echo ======================================
echo  Game Coordinator Server - Windows Build
echo ======================================
echo.

REM Check for Visual Studio
where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: MSVC compiler not found!
    echo Please run this from "x64 Native Tools Command Prompt for VS 2022"
    echo Or install Visual Studio Build Tools
    pause
    exit /b 1
)

REM Check if vcpkg is available and integrated
set VCPKG_TOOLCHAIN=""
if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    echo Found vcpkg at C:\vcpkg
    set VCPKG_TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
) else (
    echo Warning: vcpkg not found. Some dependencies may need manual installation.
    echo Run install-deps-windows.ps1 to install dependencies automatically.
)

REM Create build directory
if not exist build-windows mkdir build-windows
cd build-windows

echo.
echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_STANDARD=17 ^
    %VCPKG_TOOLCHAIN%

if %errorlevel% neq 0 (
    echo.
    echo ERROR: CMake configuration failed!
    echo.
    echo Common issues:
    echo   - Missing dependencies: Run install-deps-windows.ps1
    echo   - Missing Steam SDK: Download from partner.steamgames.com
    echo   - Missing MariaDB: Install via vcpkg or download manually
    echo.
    pause
    exit /b 1
)

echo.
echo Building...
cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%

if %errorlevel% neq 0 (
    echo.
    echo ERROR: Build failed!
    echo Check the output above for specific errors.
    echo.
    pause
    exit /b 1
)

echo.
echo ======================================
echo  Build completed successfully!
echo ======================================
echo Binary location: build-windows\gc_server\Release\gc-server_win64.exe
echo.
echo To run the server:
echo   cd build-windows\gc_server\Release
echo   set SteamAppId=730
echo   gc-server_win64.exe
echo.
pause
