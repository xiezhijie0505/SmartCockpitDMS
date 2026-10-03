@echo off
rem ===========================================================================
rem Deploy SeetaFace6 liveness-detection DLLs to the build output directory.
rem Invoked automatically by QMAKE_POST_LINK in Face_Recognition_Attendance_System.pro
rem Usage: deploy_seetaface.bat <output_dir>
rem Source: %~dp0\3rdparty\seetaface\bin\*.dll
rem ===========================================================================
setlocal
set "DEST=%~1"
if "%DEST%"=="" (
    echo [deploy_seetaface] ERROR: missing output dir argument
    exit /b 1
)
if not exist "%DEST%" (
    echo [deploy_seetaface] ERROR: output dir not exist: %DEST%
    exit /b 1
)
copy /y "%~dp0\3rdparty\seetaface\bin\*.dll" "%DEST%\" >nul
if errorlevel 1 (
    echo [deploy_seetaface] ERROR: copy failed
    exit /b 1
)
echo [deploy_seetaface] SeetaFace6 DLLs deployed to %DEST%
exit /b 0
