cmake_policy(SET CMP0077 NEW)

set(MSDF_ATLAS_BUILD_STANDALONE OFF)
set(MSDF_ATLAS_USE_SKIA OFF)
set(MSDF_ATLAS_NO_ARTERY_FONT ON)
set(MSDFGEN_DISABLE_PNG ON)

set(SDL2MIXER_DEPS_SHARED OFF)
set(SDL2MIXER_VENDORED ON)
set(SDL2MIXER_FLAC OFF)
set(SDL2MIXER_MOD OFF)
set(SDL2MIXER_MP3 OFF)
set(SDL2MIXER_MIDI OFF)
set(SDL2MIXER_OPUS OFF)
set(SDL2MIXER_VORBIS "VORBISFILE")
set(SDL2MIXER_WAVPACK OFF)

if (CMAKE_SYSTEM_NAME MATCHES "Linux" OR ANDROID)
    set(SDL_VULKAN_ENABLED ON CACHE BOOL "")
endif()

# Vulkan validation on Android is opt-in only: flip this to ON *and* put
# libVkLayer_khronos_validation.so back into the APK's jniLibs when debugging.
# It must stay OFF for normal builds — the layer (plus forced sync validation in
# plume_vulkan.cpp) runs on every API call and costs a large amount of CPU time.
# NOTE: this is a CACHE variable; after changing it, reconfigure the existing
# build dir with -DVULKAN_VALIDATION_LAYER_ENABLED=<ON|OFF> or the old value sticks.
if (ANDROID)
    set(VULKAN_VALIDATION_LAYER_ENABLED OFF CACHE BOOL "")
endif()

if (WIN32)
    set(D3D12_AGILITY_SDK_ENABLED ON CACHE BOOL "")
endif()

add_subdirectory("${UNLEASHED_RECOMP_THIRDPARTY_ROOT}/msdf-atlas-gen")

# The GUI installer wizard (and its native file picker dependency) is not built for
# Android; game files are pushed manually instead. See ui/installer_wizard.cpp guards.
if (NOT ANDROID)
    add_subdirectory("${UNLEASHED_RECOMP_THIRDPARTY_ROOT}/nativefiledialog-extended")
endif()

add_subdirectory("${UNLEASHED_RECOMP_THIRDPARTY_ROOT}/o1heap")
add_subdirectory("${UNLEASHED_RECOMP_THIRDPARTY_ROOT}/SDL")
add_subdirectory("${UNLEASHED_RECOMP_THIRDPARTY_ROOT}/SDL_mixer")
add_subdirectory("${UNLEASHED_RECOMP_THIRDPARTY_ROOT}/plume")

# Experiment: allows loading a custom Vulkan driver (Mesa Turnip) instead of the vendor
# proprietary blob, to work around a driver-side shader compiler bug seen with this
# engine's buffer_device_address-based bindless resource access. See vulkan_driver_android.cpp.
if (ANDROID)
    add_subdirectory("${UNLEASHED_RECOMP_THIRDPARTY_ROOT}/libadrenotools")
endif()

if (APPLE)
    add_subdirectory("${UNLEASHED_RECOMP_THIRDPARTY_ROOT}/MoltenVK")
endif()
