@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl /Fe:"C:\Users\User\Downloads\loader\MediaCreationTool.exe" launcher\Launcher.cpp /link winhttp.lib shell32.lib user32.lib
if errorlevel 1 pause
