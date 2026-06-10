@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl /LD /EHsc /Fe:server\Satella.dll /MD /nologo /W3 /D_CRT_SECURE_NO_WARNINGS /GS /Od /std:c++17 /RTC1 ^
    source\Main.cpp ^
    source\Cfg\encrypt.cpp ^
    source\Cfg\Discord\Discord.cpp ^
    source\Cfg\imgui\imgui.cpp ^
    source\Cfg\imgui\imgui_draw.cpp ^
    source\Cfg\imgui\imgui_widgets.cpp ^
    source\Cfg\imgui\imgui_tables.cpp ^
    source\Cfg\imgui\imgui_impl_win32.cpp ^
    source\Cfg\imgui\imgui_impl_dx11.cpp ^
    source\Cfg\imgui\imgui_stdlib.cpp ^
    source\Cfg\imgui\stb_image.c ^
    source\Cfg\minhook\buffer.c ^
    source\Cfg\minhook\hook.c ^
    source\Cfg\minhook\trampoline.c ^
    source\Cfg\minhook\hde\hde64.c ^
    keyauth\ka_bridge.cpp ^
    /I source ^
    /I source\Cfg ^
    /I source\Cfg\imgui ^
    /I source\Cfg\curl ^
    /I source\Cfg\Discord ^
    /I "source\Cfg\Discord\Discord SDK" ^
    /I source\Cfg\minhook ^
    /I source\Imports ^
    /I source\Unity ^
    /I keyauth ^
    /link /LIBPATH:source\Cfg\lib libcurl_a.lib advapi32.lib shell32.lib shlwapi.lib
if errorlevel 1 pause
