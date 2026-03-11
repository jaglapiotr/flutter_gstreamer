cmake_minimum_required(VERSION 3.13)
project(runner LANGUAGES CXX)

# Define the application target. To change its name, change BINARY_NAME in the
# top-level CMakeLists.txt, not the value here, or `flutter run` will no longer
# work.
#
# Any new source files that you add to the application should be added here.
add_executable(${BINARY_NAME}
  "main.cc"
  "my_application.cc"
  "${FLUTTER_MANAGED_DIR}/generated_plugin_registrant.cc"
)

# Apply the standard set of build settings. This can be removed for applications
# that need different build settings.
apply_standard_settings(${BINARY_NAME})

# Add preprocessor definitions for the application ID.
add_definitions(-DAPPLICATION_ID="${APPLICATION_ID}")

set(CMAKE_C_FLAGS_INIT "-fsanitize=address -fno-omit-frame-pointer -g")
set(CMAKE_CXX_FLAGS_INIT "-fsanitize=address -fno-omit-frame-pointer -g")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fsanitize=address")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fsanitize=address")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands" FORCE)
set(PKG_CONFIG_EXECUTABLE "/home/piotr/Tools/vcpkg/installed/x64-linux/tools/pkgconf/pkgconf")
set(ENV{PKG_CONFIG_PATH} "/home/piotr/Tools/vcpkg/installed/x64-linux/lib/pkgconfig")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/home/piotr/Tools/vcpkg/installed/x64-linux/share/cmake")
set(GSTREAMER_APIS app audio) 

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 
  gstreamer-app-1.0 gstreamer-audio-1.0 gstreamer-video-1.0)
message(STATUS "KANAPKA GStreamer include dirs: ${GSTREAMER_INCLUDE_DIRS}")
message(STATUS "KANAPKA GStreamer libraries: ${GSTREAMER_LIBRARIES}")
# find_package(GStreamer REQUIRED COMPONENTS app audio)
# Add dependency libraries. Add any application-specific dependencies here.
target_link_libraries(${BINARY_NAME} PRIVATE flutter)
target_link_libraries(${BINARY_NAME} PRIVATE PkgConfig::GTK)
# target_link_libraries(${BINARY_NAME} PRIVATE GStreamer::GStreamer GStreamer::app GStreamer::audio)
target_include_directories(${BINARY_NAME} PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(${BINARY_NAME} PRIVATE ${GSTREAMER_LIBRARIES})

target_include_directories(${BINARY_NAME} PRIVATE "${CMAKE_SOURCE_DIR}")
