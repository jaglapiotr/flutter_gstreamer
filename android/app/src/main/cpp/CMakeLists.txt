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
# toolchain) to the GStreamer SDK's per-arch folder naming. Same mapping
# Android.mk relies on via TARGET_ARCH_ABI.
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(GSTREAMER_ARCH "armv7")
elseif(ANDROID_ABI STREQUAL "arm64-v8a")
    set(GSTREAMER_ARCH "arm64")
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

# get_property(_all_include_dirs DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
# message(STATUS "KANAPKA all include directories: ${_all_include_dirs}")


# ---------------------------------------------------------------------------
# IMPORTANT: unlike ndk-build's gstreamer-1.0.mk (included from Android.mk),
# which statically links the selected GSTREAMER_PLUGINS_* groups into
# libgstreamer_android.so as part of that build, GStreamer does not ship an
# equivalent CMake helper for Android. This CMake build therefore expects
# libgstreamer_android.so to already exist for this ABI under
# $GSTREAMER_ROOT_ANDROID/<arch>/lib/. Build the ndk-build project kept in
# ../jni (Android.mk / Application.mk) once — it produces that .so as a side
# effect — and copy it there, or supply your own prebuilt .so.
# ---------------------------------------------------------------------------
# add_library(gstreamer_android SHARED IMPORTED)
# set_target_properties(gstreamer_android PROPERTIES
#     IMPORTED_LOCATION "${GSTREAMER_ROOT}/lib/libgstreamer_android.so"
# )

add_library(gstreamer_android_texture SHARED
    native-lib.cpp
)

# target_include_directories(gstreamer_android_texture PRIVATE
#     "${GSTREAMER_ROOT}/include/gstreamer-1.0"
#     "${GSTREAMER_ROOT}/include/glib-2.0"
#     "${GSTREAMER_ROOT}/lib/glib-2.0/include"
# )

target_compile_features(gstreamer_android_texture PRIVATE cxx_std_17)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(gstreamer_android_texture
    gstreamer_android
    ${log-lib}
    ${android-lib}
)

# target_include_directories(gstreamer_android_texture PUBLIC 
#     ${ANDROID_ARCH_DIR}/include/gstreamer-1.0
#     ${ANDROID_ARCH_DIR}/include/glib-2.0
#     ${ANDROID_ARCH_DIR}/lib/glib-2.0/include
#     ${ANDROID_ARCH_DIR}/lib/gstreamer-1.0/include
# )

# ---------------------------------------------------------------------------
# Debug: print every include directory actually attached to the target, so
# misconfigured GSTREAMER_ROOT_ANDROID / ANDROID_LIB_ROOT paths are easy to
# spot in the Gradle/CMake configure log.
# ---------------------------------------------------------------------------
get_target_property(_gst_include_dirs gstreamer_android_texture INCLUDE_DIRECTORIES)
message(STATUS "KANAPKA gstreamer_android_texture include directories: ${_gst_include_dirs}")