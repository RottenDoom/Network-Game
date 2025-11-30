@echo off
setlocal enabledelayedexpansion

REM === Path to your build output ===
REM Change this if server.exe / client.exe are somewhere else.
set BUILD_DIR=%~dp0build\build-gcc

REM Validate executables exist
if not exist "%BUILD_DIR%\server.exe" (
    echo [ERROR] server.exe not found in %BUILD_DIR%
    pause
    exit /b 1
)

if not exist "%BUILD_DIR%\client.exe" (
    echo [ERROR] client.exe not found in %BUILD_DIR%
    pause
    exit /b 1
)

echo Starting SERVER in new window...
start "SERVER" cmd /k "%BUILD_DIR%\server.exe"

echo Waiting for server to initialize...
timeout /t 1 >nul

echo Starting CLIENT 1...
start "CLIENT 1" cmd /k "%BUILD_DIR%\client.exe"

echo Starting CLIENT 2...
start "CLIENT 2" cmd /k "%BUILD_DIR%\client.exe"

echo === All processes launched ===
echo Server + 2 clients running in separate terminals.
pause
