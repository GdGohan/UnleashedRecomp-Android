@echo off
set "PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0;C:\Program Files\Git\cmd;E:\VSInstall\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;E:\VSInstall\VC\Tools\Llvm\x64\bin;E:\VSInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
cd /d "E:\UnleashedRecompAndroid"
"E:\VSInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build out/build/x64-subst-test --target UnleashedRecompLib
