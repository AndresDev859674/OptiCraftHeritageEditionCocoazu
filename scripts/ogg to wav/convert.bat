@echo off
setlocal

cd /d "%~dp0"

set "mainFolder=%~dp0resources"
set "ADP=%~dp0..\..\psdevwindows\ps2sdk\bin\adpenc.exe"

if exist "%mainFolder%" (
    goto :OGGtoWAV
) else (
    echo No esta la carpeta "%mainFolder%"
    pause
    exit /b 1
)

:OGGtoWAV
echo.
echo ========================================
echo OGG TO WAV
echo ========================================

for /r "%mainFolder%" %%f in (*.ogg) do (
    echo Convirtiendo "%%f"
    ffmpeg -y -i "%%f" -ac 1 -ar 22050 -c:a pcm_s16le "%%~dpnf.wav"
)

:OGGDelete
echo.
echo ========================================
echo ELIMINANDO OGG
echo ========================================

for /r "%mainFolder%" %%f in (*.ogg) do (
    del /q "%%f"
)


endlocal
exit /b