# Install dependencies for Windows build using vcpkg
# Run this script from PowerShell as Administrator

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Game Coordinator Server - Windows Deps" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if vcpkg is installed
$vcpkgPath = "C:\vcpkg"
if (-Not (Test-Path $vcpkgPath)) {
    Write-Host "[1/5] Installing vcpkg package manager..." -ForegroundColor Yellow
    git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
    C:\vcpkg\bootstrap-vcpkg.bat
    C:\vcpkg\vcpkg integrate install
} else {
    Write-Host "[1/5] vcpkg already installed at $vcpkgPath" -ForegroundColor Green
}

# Install MariaDB connector (not MySQL)
Write-Host ""
Write-Host "[2/5] Installing MariaDB Connector/C..." -ForegroundColor Yellow
C:\vcpkg\vcpkg install libmariadb:x64-windows

# Install Crypto++ library
Write-Host ""
Write-Host "[3/5] Installing Crypto++ (cryptopp)..." -ForegroundColor Yellow
C:\vcpkg\vcpkg install cryptopp:x64-windows

# Install Protobuf (optional - CMake fetches it, but vcpkg is faster)
Write-Host ""
Write-Host "[4/5] Installing Protobuf..." -ForegroundColor Yellow
C:\vcpkg\vcpkg install protobuf:x64-windows

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host " Dependencies installed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Installed packages:" -ForegroundColor White
Write-Host "  - MariaDB Connector/C (libmariadb)" -ForegroundColor Gray
Write-Host "  - Crypto++ (cryptopp)" -ForegroundColor Gray
Write-Host "  - Protobuf" -ForegroundColor Gray
Write-Host ""
Write-Host "[5/5] MANUAL STEP REQUIRED:" -ForegroundColor Yellow
Write-Host "  Download Steam SDK from: https://partner.steamgames.com/" -ForegroundColor Cyan
Write-Host "  Extract to: $PSScriptRoot\steamworks\sdk\" -ForegroundColor Cyan
Write-Host ""
Write-Host "To build, run: build-windows.bat" -ForegroundColor White
Write-Host ""
