@echo off
call "D:\MS\VS\80686\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
set SRCDIR=J:\APP\OS\x86\PlexsDOS\xorriso-1.5.8
set OUTDIR=J:\APP\OS\x86\PlexsDOS\xorriso-1.5.8\build\x64\Debug
CL.exe /c /I"%SRCDIR%" /I"%SRCDIR%\libisofs" /I"%SRCDIR%\libisofs\filters" /I"%SRCDIR%\include" /nologo /W3 /std:c11 /D _DEBUG /D _LIB /D _MBCS /permissive- /TC /FI"win-compat.h" /Fo"%OUTDIR%/libisofs/" /FC "%SRCDIR%\libisofs\aaip-os-dummy.c"
echo exit code: %ERRORLEVEL%
