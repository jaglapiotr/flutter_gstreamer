cmake_minimum_required(VERSION 3.22)
project(gstreamer_ios_texture)

# ---------------------------------------------------------------------------
# Placeholder mirroring android/app/src/main/cpp/CMakeLists.txt. Xcode does not
# invoke CMake directly for an iOS app target, so this file is NOT currently
# wired into the Runner.xcodeproj build - it exists so the GStreamer
# include/link setup can be filled in and later hooked up (e.g. via a
# standalone CMake-built static lib, or ported into Xcode build settings)
# without having to rediscover the required paths from scratch.
#
# GSTREAMER_ROOT_IOS should point at the unpacked GStreamer iOS SDK, e.g. the
# gstreamer-1.0-ios-<arch> framework/sdk bundle.
# ---------------------------------------------------------------------------
if(NOT DEFINED GSTREAMER_ROOT_IOS)
    message(FATAL_ERROR
        "GSTREAMER_ROOT_IOS must be passed via -DGSTREAMER_ROOT_IOS=...")
endif()

# TODO: map CMAKE_OSX_ARCHITECTURES / CMAKE_OSX_SYSROOT to the GStreamer iOS
# SDK's per-arch/per-platform folder naming (device vs simulator, arm64/x86_64).
set(GSTREAMER_ROOT "${GSTREAMER_ROOT_IOS}")
message(STATUS "KANAPKA CMAKE_OSX_ARCHITECTURES = ${CMAKE_OSX_ARCHITECTURES}")
message(STATUS "KANAPKA CMAKE_OSX_SYSROOT = ${CMAKE_OSX_SYSROOT}")

# TODO: fill in once GSTREAMER_ROOT_IOS layout is known.
# include_directories(
#     ${GSTREAMER_ROOT}/include/gstreamer-1.0
#     ${GSTREAMER_ROOT}/include/glib-2.0
#     ${GSTREAMER_ROOT}/lib/glib-2.0/include
# )
# link_directories(${GSTREAMER_ROOT}/lib)

add_library(gstreamer_ios_texture STATIC
    synthetic_video_source.cpp
)

target_compile_features(gstreamer_ios_texture PRIVATE cxx_std_17)


target_link_libraries(gstreamer_ios_texture
    # TODO: link the GStreamer static/dynamic libs from GSTREAMER_ROOT_IOS here
    # (e.g. gstreamer-1.0, glib-2.0, gobject-2.0, gst-app, plus the required
    # system frameworks/tbds per the GStreamer iOS SDK docs).
    "$ENV{HOME}/Tools/gstreamer/GStreamer.xcframework"
)

# ---------------------------------------------------------------------------
# Debug: print what actually ended up attached to the target, so a wrong
# GSTREAMER_ROOT_IOS / xcframework path is easy to spot in the configure log.
# ---------------------------------------------------------------------------
get_target_property(_gst_include_dirs gstreamer_ios_texture INCLUDE_DIRECTORIES)
message(STATUS "KANAPKA gstreamer_ios_texture include directories: ${_gst_include_dirs}")

get_target_property(_gst_link_libs gstreamer_ios_texture LINK_LIBRARIES)
message(STATUS "KANAPKA gstreamer_ios_texture link libraries: ${_gst_link_libs}")

if(EXISTS "$ENV{HOME}/Tools/gstreamer/GStreamer.xcframework")
    message(STATUS "KANAPKA GStreamer.xcframework found at $ENV{HOME}/Tools/gstreamer/GStreamer.xcframework")
else()
    message(WARNING "KANAPKA GStreamer.xcframework NOT found at $ENV{HOME}/Tools/gstreamer/GStreamer.xcframework")
endif()
