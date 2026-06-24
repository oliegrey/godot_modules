@echo off
setlocal

set GODOT_BIN=C:\Users\Olie\Desktop\godot\bin
set TEMPLATES=C:\Users\Olie\Desktop\saltminergodot\export_templates
set EDITOR_INSTALL=C:\Program Files\Godot

REM ===== Windows editor =====
cd C:\Users\Olie\Desktop\godot
scons platform=windows target=editor profile=custom_editor.py num_jobs=8
if %errorlevel% neq 0 (
    echo [ERROR] Windows editor build failed.
    pause & exit /b %errorlevel%
)
echo Copying Windows editor...
copy /Y "%GODOT_BIN%\godot.windows.editor.x86_64.exe" "%EDITOR_INSTALL%\Godot_v4.4-stable_win64.exe"
if %errorlevel% neq 0 (
    echo [ERROR] Failed to copy editor - try running this script as Administrator.
    pause & exit /b %errorlevel%
)

REM ===== Web export template =====
call C:\Users\Olie\emsdk\emsdk_env.bat
scons platform=web target=template_release editor=no threads=no debug_symbols=no optimize=size lto=none num_jobs=8 production=yes profile=custom_template.py
if %errorlevel% neq 0 (
    echo [ERROR] Web template build failed.
    pause & exit /b %errorlevel%
)
echo Copying web template...
xcopy /Y "%GODOT_BIN%\godot.web.template_release.wasm32.nothreads.*" "%TEMPLATES%\"

REM ===== Linux export template (WSL) =====
wsl bash -lc "cd /mnt/c/Users/Olie/Desktop/godot && scons platform=linuxbsd target=template_release editor=no threads=yes debug_symbols=no optimize=speed lto=full production=yes num_jobs=8 profile=custom_template.py"
if %errorlevel% neq 0 (
    echo [ERROR] Linux template build failed.
    pause & exit /b %errorlevel%
)
echo Copying Linux template...
xcopy /Y "%GODOT_BIN%\godot.linuxbsd.template_release.x86_64" "%TEMPLATES%\"

echo.
echo All builds and copies completed successfully.
pause