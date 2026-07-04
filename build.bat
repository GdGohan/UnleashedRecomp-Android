@echo off
set "PATH=C:\Windows\system32;C:\Windows;C:\Program Files\Git\cmd;E:\VSInstall\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;E:\VSInstall\VC\Tools\Llvm\x64\bin;E:\VSInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "INCLUDE=E:\VSInstall\VC\Tools\MSVC\14.44.35207\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\winrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\cppwinrt"
set "LIB=E:\VSInstall\VC\Tools\MSVC\14.44.35207\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"
cd /d "E:\UnleashedRecompAndroid"
"E:\VSInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" . --preset x64-Clang-Release
