@echo off
REM Build Doxygen documentation for NetworkGame project
REM Requires Doxygen to be installed and in PATH

echo Building Doxygen documentation...

REM Check if Doxygen is installed
where doxygen >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Doxygen is not installed or not in PATH
    echo Please install Doxygen from https://www.doxygen.nl/download.html
    exit /b 1
)

REM Create docs directory if it doesn't exist
if not exist "docs" mkdir docs

REM Run Doxygen
doxygen Doxyfile

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Documentation built successfully!
    echo Open docs/html/index.html in your browser to view the documentation.
) else (
    echo.
    echo ERROR: Doxygen build failed
    exit /b 1
)

