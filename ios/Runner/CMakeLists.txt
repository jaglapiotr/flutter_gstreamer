cmake_minimum_required(VERSION 3.22)
project(gstreamer_ios_texture)

# ---------------------------------------------------------------------------
# Builds SyntheticVideoSource against a prebuilt GStreamer.xcframework. This is
# invoked automatically by Xcode via the "Build GStreamer CMake Library" Run
# Script build phase in Runner.xcodeproj (see project.pbxproj), so it's wired
# into the normal Xcode build - no manual CMake invocation required.
#
# GSTREAMER_ROOT_IOS may point either at GStreamer.xcframework itself, or at
# its parent directory (e.g. $HOME/Tools/gstreamer containing
# GStreamer.xcframework).
# ---------------------------------------------------------------------------
if(NOT DEFINED GSTREAMER_ROOT_IOS)
    message(FATAL_ERROR
        "GSTREAMER_ROOT_IOS must be passed via -DGSTREAMER_ROOT_IOS=...")
endif()

message(STATUS "KANAPKA CMAKE_OSX_ARCHITECTURES = ${CMAKE_OSX_ARCHITECTURES}")
message(STATUS "KANAPKA CMAKE_OSX_SYSROOT = ${CMAKE_OSX_SYSROOT}")

set(GSTREAMER_FRAMEWORK_NAME "GStreamer" CACHE STRING
    "Name of the .framework bundled inside GStreamer.xcframework's slices")

if(GSTREAMER_ROOT_IOS MATCHES "\\.xcframework/?$")
    set(_gst_xcframework_dir "${GSTREAMER_ROOT_IOS}")
else()
    set(_gst_xcframework_dir "${GSTREAMER_ROOT_IOS}/GStreamer.xcframework")
endif()

if(NOT IS_DIRECTORY "${_gst_xcframework_dir}")
    message(FATAL_ERROR
        "KANAPKA Could not find GStreamer.xcframework at '${_gst_xcframework_dir}'. "
        "Set GSTREAMER_ROOT_IOS to the folder containing GStreamer.xcframework, "
        "or directly to the .xcframework itself.")
endif()

# Pick the xcframework slice matching the current SDK: simulator builds have
# "Simulator" in CMAKE_OSX_SYSROOT, device builds don't. xcframework slice
# folders are conventionally named "ios-<arch...>[-simulator]".
string(TOLOWER "${CMAKE_OSX_SYSROOT}" _gst_sysroot_lower)
if(_gst_sysroot_lower MATCHES "simulator")
    set(_gst_want_simulator TRUE)
else()
    set(_gst_want_simulator FALSE)
endif()

file(GLOB _gst_slice_candidates LIST_DIRECTORIES true "${_gst_xcframework_dir}/ios-*")
set(_gst_slice_dir "")
foreach(_candidate ${_gst_slice_candidates})
    if(NOT IS_DIRECTORY "${_candidate}")
        continue()
    endif()
    string(TOLOWER "${_candidate}" _candidate_lower)
    if(_gst_want_simulator AND _candidate_lower MATCHES "simulator")
        set(_gst_slice_dir "${_candidate}")
        break()
    elseif(NOT _gst_want_simulator AND NOT _candidate_lower MATCHES "simulator")
        set(_gst_slice_dir "${_candidate}")
        break()
    endif()
endforeach()

if(_gst_slice_dir STREQUAL "")
    message(FATAL_ERROR
        "KANAPKA No slice under '${_gst_xcframework_dir}' matches "
        "simulator=${_gst_want_simulator}. Candidates found: ${_gst_slice_candidates}")
endif()
message(STATUS "KANAPKA Using GStreamer.xcframework slice: ${_gst_slice_dir}")

# GStreamer's iOS xcframework has shipped both as framework-style (a
# *.framework bundle inside the slice) and library-style (Headers/ + a plain
# lib*.a / lib*.dylib directly in the slice) - detect whichever is actually
# there instead of assuming one.
file(GLOB _gst_framework_candidates LIST_DIRECTORIES true "${_gst_slice_dir}/*.framework")
set(GSTREAMER_FRAMEWORK "")
foreach(_candidate ${_gst_framework_candidates})
    get_filename_component(_candidate_name "${_candidate}" NAME_WE)
    if(_candidate_name STREQUAL "${GSTREAMER_FRAMEWORK_NAME}")
        set(GSTREAMER_FRAMEWORK "${_candidate}")
        break()
    endif()
endforeach()
if(GSTREAMER_FRAMEWORK STREQUAL "" AND _gst_framework_candidates)
    list(GET _gst_framework_candidates 0 GSTREAMER_FRAMEWORK)
    get_filename_component(GSTREAMER_FRAMEWORK_NAME "${GSTREAMER_FRAMEWORK}" NAME_WE)
endif()

set(GSTREAMER_LINK_STYLE "")
set(GSTREAMER_HEADERS_DIR "")
set(GSTREAMER_LIBRARY_FILE "")
set(GSTREAMER_LIBRARY_NAME "")

if(NOT GSTREAMER_FRAMEWORK STREQUAL "")
    set(GSTREAMER_LINK_STYLE "FRAMEWORK")
    set(GSTREAMER_HEADERS_DIR "${GSTREAMER_FRAMEWORK}/Headers")
    message(STATUS "KANAPKA Resolved GStreamer framework bundle: ${GSTREAMER_FRAMEWORK}")
elseif(IS_DIRECTORY "${_gst_slice_dir}/Headers")
    file(GLOB _gst_lib_candidates "${_gst_slice_dir}/*.a" "${_gst_slice_dir}/*.dylib")
    if(_gst_lib_candidates)
        list(GET _gst_lib_candidates 0 GSTREAMER_LIBRARY_FILE)
        set(GSTREAMER_LINK_STYLE "LIBRARY")
        set(GSTREAMER_HEADERS_DIR "${_gst_slice_dir}/Headers")
        get_filename_component(GSTREAMER_LIBRARY_NAME "${GSTREAMER_LIBRARY_FILE}" NAME_WE)
        string(REGEX REPLACE "^lib" "" GSTREAMER_LIBRARY_NAME "${GSTREAMER_LIBRARY_NAME}")
        message(STATUS "KANAPKA Resolved GStreamer static library: ${GSTREAMER_LIBRARY_FILE} (link as -l${GSTREAMER_LIBRARY_NAME})")
    endif()
endif()

if(GSTREAMER_LINK_STYLE STREQUAL "")
    file(GLOB _gst_slice_contents LIST_DIRECTORIES true "${_gst_slice_dir}/*")
    message(FATAL_ERROR
        "KANAPKA Could not recognize the layout of slice '${_gst_slice_dir}' "
        "(expected either a *.framework bundle, or a Headers/ dir next to a "
        "lib*.a / lib*.dylib) - actual contents: ${_gst_slice_contents}")
endif()

if(NOT IS_DIRECTORY "${GSTREAMER_HEADERS_DIR}")
    file(GLOB _gst_headers_parent_contents LIST_DIRECTORIES true "${_gst_slice_dir}/*")
    message(FATAL_ERROR
        "KANAPKA Resolved headers dir does not exist: '${GSTREAMER_HEADERS_DIR}' "
        "- slice contents: ${_gst_headers_parent_contents}")
endif()

# Expose the resolved headers + library/framework at FIXED, predictable names
# under the build dir (Xcode sees $(BUILT_PRODUCTS_DIR)/gstreamer_ios_texture_build
# via HEADER_SEARCH_PATHS/LIBRARY_SEARCH_PATHS/FRAMEWORK_SEARCH_PATHS in
# project.pbxproj), so project.pbxproj doesn't need to hardcode per-slice
# folder names or care which style the SDK ships as - CMake re-resolves the
# right slice/style every build (simulator vs device don't collide since each
# gets its own BUILT_PRODUCTS_DIR).
file(REMOVE "${CMAKE_BINARY_DIR}/gstreamer_headers")
file(CREATE_LINK "${GSTREAMER_HEADERS_DIR}" "${CMAKE_BINARY_DIR}/gstreamer_headers" SYMBOLIC)

if(GSTREAMER_LINK_STYLE STREQUAL "FRAMEWORK")
    file(REMOVE "${CMAKE_BINARY_DIR}/${GSTREAMER_FRAMEWORK_NAME}.framework")
    file(CREATE_LINK "${GSTREAMER_FRAMEWORK}" "${CMAKE_BINARY_DIR}/${GSTREAMER_FRAMEWORK_NAME}.framework" SYMBOLIC)
else()
    get_filename_component(_gst_lib_filename "${GSTREAMER_LIBRARY_FILE}" NAME)
    file(REMOVE "${CMAKE_BINARY_DIR}/${_gst_lib_filename}")
    file(CREATE_LINK "${GSTREAMER_LIBRARY_FILE}" "${CMAKE_BINARY_DIR}/${_gst_lib_filename}" SYMBOLIC)
endif()

add_library(gstreamer_ios_texture STATIC
    synthetic_video_source.cpp
)

target_compile_features(gstreamer_ios_texture PRIVATE cxx_std_17)

target_include_directories(gstreamer_ios_texture PRIVATE
    "${CMAKE_BINARY_DIR}/gstreamer_headers"
)

# Standard Apple linking: framework-style needs -F<search path> + -framework,
# library-style needs -L<search path> + -l<name> (works for both since the
# resolved lib/framework is symlinked into CMAKE_BINARY_DIR above).
if(GSTREAMER_LINK_STYLE STREQUAL "FRAMEWORK")
    target_link_options(gstreamer_ios_texture PUBLIC
        "-F${CMAKE_BINARY_DIR}"
        "-framework" "${GSTREAMER_FRAMEWORK_NAME}"
    )
else()
    target_link_options(gstreamer_ios_texture PUBLIC
        "-L${CMAKE_BINARY_DIR}"
        "-l${GSTREAMER_LIBRARY_NAME}"
    )
endif()


# NOTE: GStreamer's static iOS build typically also needs a handful of Apple
# system frameworks/libs (AudioToolbox, AVFoundation, CoreMedia, CoreVideo,
# VideoToolbox, Security, libiconv, libresolv, libz, libbz2, ...) to satisfy
# symbols pulled in by its statically-registered plugins. Add those to
# OTHER_LDFLAGS in project.pbxproj (Runner target) as you hit undefined-symbol
# errors for them - the exact set depends on your SDK build/version.

# ---------------------------------------------------------------------------
# Debug: print what actually ended up attached to the target, so a wrong
# GSTREAMER_ROOT_IOS / xcframework layout is easy to spot in the build log.
# ---------------------------------------------------------------------------
get_target_property(_gst_include_dirs gstreamer_ios_texture INCLUDE_DIRECTORIES)
message(STATUS "KANAPKA gstreamer_ios_texture include directories: ${_gst_include_dirs}")

message(STATUS "KANAPKA gstreamer_ios_texture link style: ${GSTREAMER_LINK_STYLE}")
message(STATUS "KANAPKA gstreamer_ios_texture headers symlink: ${CMAKE_BINARY_DIR}/gstreamer_headers -> ${GSTREAMER_HEADERS_DIR}")
if(GSTREAMER_LINK_STYLE STREQUAL "FRAMEWORK")
    message(STATUS "KANAPKA gstreamer_ios_texture framework symlink: ${CMAKE_BINARY_DIR}/${GSTREAMER_FRAMEWORK_NAME}.framework -> ${GSTREAMER_FRAMEWORK}")
else()
    message(STATUS "KANAPKA gstreamer_ios_texture library symlink: ${CMAKE_BINARY_DIR}/${_gst_lib_filename} -> ${GSTREAMER_LIBRARY_FILE}")
endif()
