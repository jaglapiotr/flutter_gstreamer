cmake_minimum_required(VERSION 3.18)
project(gstreamer_android_texture)

# ---------------------------------------------------------------------------
# GStreamer Android SDK location. Passed in by Gradle as
# -DGSTREAMER_ROOT_ANDROID=... (see build.gradle.kts), mirroring the
# GSTREAMER_ROOT_ANDROID argument used by the ndk-build (Android.mk) variant.
# ---------------------------------------------------------------------------
if(NOT DEFINED GSTREAMER_ROOT_ANDROID)
    message(FATAL_ERROR
        "GSTREAMER_ROOT_ANDROID must be passed via -DGSTREAMER_ROOT_ANDROID=...")
endif()

# Map the NDK's ANDROID_ABI (set automatically by the Android CMake
# toolchain) to the GStreamer SDK's per-arch folder naming. The prebuilt
# libraries/headers live under folders named after the Android ABI itself
# (e.g. $GSTREAMER_ROOT_ANDROID/arm64-v8a/lib), so this is an identity
# mapping.
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(GSTREAMER_ARCH "armeabi-v7a")
elseif(ANDROID_ABI STREQUAL "arm64-v8a")
    set(GSTREAMER_ARCH "arm64-v8a")
elseif(ANDROID_ABI STREQUAL "x86")
    set(GSTREAMER_ARCH "x86")
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(GSTREAMER_ARCH "x86_64")
else()
    message(FATAL_ERROR "Unsupported ABI for GStreamer: ${ANDROID_ABI}")
endif()

set(GSTREAMER_ROOT "${GSTREAMER_ROOT_ANDROID}/${GSTREAMER_ARCH}")

set(ANDROID_LIB_ROOT "C:/Tools/Android_environment")
set(ANDROID_ARCH_DIR ${ANDROID_LIB_ROOT}/android_env/${CMAKE_ANDROID_ARCH_ABI})
include_directories(
    ${ANDROID_ARCH_DIR}/include
    ${ANDROID_ARCH_DIR}/include/gstreamer-1.0
    ${ANDROID_ARCH_DIR}/include/glib-2.0
    ${ANDROID_ARCH_DIR}/lib/glib-2.0/include
    ${ANDROID_ARCH_DIR}/lib/gstreamer-1.0/include
)
link_directories(${ANDROID_ARCH_DIR}/lib)
add_library(gstreamer_android_texture SHARED
    native-lib.cpp
)

target_compile_features(gstreamer_android_texture PRIVATE cxx_std_17)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(gstreamer_android_texture
    gstreamer_android
    ${log-lib}
    ${android-lib}
)