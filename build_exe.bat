@echo off
chcp 65001 >nul
title Building Smart Cat Feeder Pro Executable
cd /d "%~dp0"
echo =========================================================
echo    Building Smart Cat Feeder Pro Windows Standalone (.exe)
echo =========================================================
echo.

python -m PyInstaller --noconfirm --onedir --windowed --name "SmartCatFeederPro" --collect-all kivy app\frontend\main.py

echo.
if %ERRORLEVEL% EQU 0 (
    echo =========================================================
    echo    BUILD SUKSES! Aplikasi .exe tersedia di:
    echo    dist\SmartCatFeederPro\SmartCatFeederPro.exe
    echo =========================================================
) else (
    echo Build gagal dengan kode error %ERRORLEVEL%
)
pause
