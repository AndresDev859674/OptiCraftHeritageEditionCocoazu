@echo off
setlocal

cd /d "%~dp0"

set "mainFolder=%~dp0resources"
set "ADP=%~dp0..\..\psdevwindows\ps2sdk\bin\adpenc.exe"

if exist "%mainFolder%" (
    goto :OGGtoPCM
) else (
    echo No esta la carpeta "%mainFolder%"
    pause
    exit /b 1
)

:OGGtoPCM
echo.
echo ========================================
echo OGG TO PCM
echo ========================================

for /r "%mainFolder%" %%f in (*.ogg) do (
    echo Convirtiendo "%%f"
    ffmpeg -y -i "%%f" -ac 2 -ar 22050 -c:a pcm_s16le -f s16le "%%~dpnf.pcm"
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