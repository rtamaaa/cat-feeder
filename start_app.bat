@echo off
chcp 65001 >nul
title Smart Cat Feeder Pro - App Launcher
cd /d "%~dp0"

if exist "dist\SmartCatFeederPro\SmartCatFeederPro.exe" (
    echo Menjalankan Smart Cat Feeder Pro (.exe)...
    start "" "dist\SmartCatFeederPro\SmartCatFeederPro.exe"
    exit /b 0
)

echo Menjalankan Smart Cat Feeder Pro (Python)...
python app\frontend\main.py
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Application stopped with error code %ERRORLEVEL%
    pause
)

