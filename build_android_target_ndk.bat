@echo off
REM Incremental Android build that also survives a CMake reconfigure.
REM Unlike build_android_target.bat, this sets ANDROID_NDK_HOME/VCPKG_ROOT so that
REM when CMakeLists.txt changes (forcing vcpkg to re-detect the arm64-android
REM compiler) the NDK is still found. Otherwise vcpkg falls back to a default
REM ndk-bundle path and the reconfigure fails.
set "PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0;C:\Program Files\Git\cmd;E:\VSInstall\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;E:\VSInstall\VC\Tools\Llvm\x64\bin;E:\VSInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "ANDROID_NDK_HOME=E:\AndroidSDK\ndk\29.0.14206865"
set "ANDROID_NDK_ROOT=E:\AndroidSDK\ndk\29.0.14206865"
set "VCPKG_ROOT=E:\UnleashedRecompAndroid\thirdparty\vcpkg"
cd /d "E:\UnleashedRecompAndroid"
"E:\VSInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build out/build/android-arm64 --target UnleashedRecomp
