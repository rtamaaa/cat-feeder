@echo off
chcp 65001 >nul
title Smart Cat Feeder Pro - Web Dashboard Launcher
cd /d "%~dp0"

echo ============================================================
echo   🐱 SMART CAT FEEDER PRO - WEB PORTAL LAUNCHER
echo ============================================================
echo.
echo Membuka dashboard administrator di browser...
echo.

if exist "app\frontend\index.html" (
    start "" "app\frontend\index.html"
) else (
    start "" "http://localhost/smart-cat-feeder/"
)

echo Dashboard berhasil diluncurkan!
timeout /t 3 >nul
exit /b 0
