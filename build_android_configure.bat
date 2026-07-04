@echo off
set "PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0;C:\Program Files\Git\cmd;E:\VSInstall\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;E:\VSInstall\VC\Tools\Llvm\x64\bin;E:\VSInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "ANDROID_NDK_HOME=E:\AndroidSDK\ndk\29.0.14206865"
set "VCPKG_ROOT=E:\UnleashedRecompAndroid\thirdparty\vcpkg"
cd /d "E:\UnleashedRecompAndroid"
rmdir /s /q out\build\android-arm64 2>nul
"E:\VSInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
  -S . -B out/build/android-arm64 -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE=thirdparty/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=arm64-android ^
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=E:/AndroidSDK/ndk/29.0.14206865/build/cmake/android.toolchain.cmake ^
  -DANDROID_ABI=arm64-v8a ^
  -DANDROID_PLATFORM=android-29 ^
  -DCMAKE_SYSTEM_VERSION=29 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DUNLEASHED_RECOMP_ANDROID=ON ^
  -DUNLEASHED_RECOMP_D3D12=OFF ^
  -DUNLEASHED_RECOMP_HOST_XENON_RECOMP=E:/UnleashedRecompAndroid/out/build/x64-Clang-HostTools/tools/XenonRecomp/XenonRecomp/XenonRecomp.exe ^
  -DUNLEASHED_RECOMP_HOST_XENOS_RECOMP=E:/UnleashedRecompAndroid/out/build/x64-Clang-HostTools/tools/XenosRecomp/XenosRecomp/XenosRecomp.exe ^
  -DUNLEASHED_RECOMP_HOST_FILE_TO_C=E:/UnleashedRecompAndroid/out/build/x64-Clang-HostTools/tools/file_to_c/file_to_c.exe ^
  -DUNLEASHED_RECOMP_HOST_X_DECOMPRESS=E:/UnleashedRecompAndroid/out/build/x64-Clang-HostTools/tools/x_decompress/x_decompress.exe
