@echo off
REM Launches all radar_sea_level services in separate windows.
REM Each service can be closed independently with Ctrl-C in its window.

setlocal
set ROOT=%~dp0
set SERVER=%ROOT%server
set GUI=%ROOT%gui
set BUILD=%SERVER%\build\Release

REM ── Ports ────────────────────────────────────────────────────────────────
set RADAR_PORT=8080
set LUT_PORT=9000
set TARGET_PORT=9001

REM ── Paths ────────────────────────────────────────────────────────────────
set TILES=%SERVER%\tiles
set MBTILES=%GUI%\map_tiles\israel.mbtiles
set MAX_RANGE=50000

REM ── Sanity checks ────────────────────────────────────────────────────────
if not exist "%BUILD%\radar_server.exe" (
    echo [run_all] ERROR: %BUILD%\radar_server.exe not found.
    echo           Build the solution in Release ^| x64 first.
    pause
    exit /b 1
)
if not exist "%TILES%" (
    echo [run_all] WARNING: tiles dir not found: %TILES%
)

echo [run_all] Starting services...
echo   radar_server   port %RADAR_PORT%   tiles "%TILES%"   max-range %MAX_RANGE%
echo   lut_server     port %LUT_PORT%     tiles "%TILES%"
echo   target_server  port %TARGET_PORT%
echo   gui            http://127.0.0.1:8050   server http://127.0.0.1:%RADAR_PORT%
echo.

start "radar_server"  cmd /k "cd /d "%SERVER%" && "%BUILD%\radar_server.exe"  --port %RADAR_PORT%  --tiles "%TILES%"  --max-range %MAX_RANGE%"
start "lut_server"    cmd /k "cd /d "%SERVER%" && "%BUILD%\lut_server.exe"    --port %LUT_PORT%    --tiles "%TILES%""
start "target_server" cmd /k "cd /d "%SERVER%" && "%BUILD%\target_server.exe" --port %TARGET_PORT%"

REM Give the radar server a moment to bind its port before the GUI tries to talk to it.
timeout /t 2 /nobreak > nul

start "gui" cmd /k "cd /d "%GUI%" && python app.py --server http://127.0.0.1:%RADAR_PORT% --tiles "%MBTILES%""

echo [run_all] All services launched. Close each window to stop that service.
endlocal
