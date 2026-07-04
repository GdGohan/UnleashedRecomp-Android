@echo off
set "PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0;C:\Program Files\Git\cmd;E:\VSInstall\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;E:\VSInstall\VC\Tools\Llvm\x64\bin;E:\VSInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "INCLUDE=E:\VSInstall\VC\Tools\MSVC\14.44.35207\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\winrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\cppwinrt"
set "LIB=E:\VSInstall\VC\Tools\MSVC\14.44.35207\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"
set "VCPKG_ROOT=E:\UnleashedRecompAndroid\thirdparty\vcpkg"
cd /d "E:\UnleashedRecompAndroid"
"E:\VSInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
  -S . -B out/build/x64-subst-test -G Ninja ^
  -DCMAKE_C_COMPILER=clang-cl.exe -DCMAKE_CXX_COMPILER=clang-cl.exe -DCMAKE_LINKER=lld-link.exe ^
  -DCMAKE_TOOLCHAIN_FILE=thirdparty/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DUNLEASHED_RECOMP_HOST_XENON_RECOMP=E:/UnleashedRecompAndroid/out/build/x64-Clang-HostTools/tools/XenonRecomp/XenonRecomp/XenonRecomp.exe ^
  -DUNLEASHED_RECOMP_HOST_XENOS_RECOMP=E:/UnleashedRecompAndroid/out/build/x64-Clang-HostTools/tools/XenosRecomp/XenosRecomp/XenosRecomp.exe ^
  -DUNLEASHED_RECOMP_HOST_FILE_TO_C=E:/UnleashedRecompAndroid/out/build/x64-Clang-HostTools/tools/file_to_c/file_to_c.exe ^
  -DUNLEASHED_RECOMP_HOST_X_DECOMPRESS=E:/UnleashedRecompAndroid/out/build/x64-Clang-HostTools/tools/x_decompress/x_decompress.exe ^
  -DUNLEASHED_RECOMP_D3D12=OFF
