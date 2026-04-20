# Shell for moonchild: requires SDL2 + SDL2_mixer (CMake package configs).
# macOS (Homebrew): brew install cmake sdl2 sdl2_mixer
# Then: cmake -S . -B build && cmake --build build

cmake_minimum_required(VERSION 3.16)
project(moonchild_shell LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(APPLE)
  list(APPEND CMAKE_PREFIX_PATH
    "/opt/homebrew/opt/sdl2"
    "/opt/homebrew/opt/sdl2_mixer"
    "/usr/local/opt/sdl2"
    "/usr/local/opt/sdl2_mixer"
  )
endif()

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

set(FFMPEG_HINTS
  /opt/homebrew
  /usr/local
  /usr
)

find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h
  HINTS ${FFMPEG_HINTS}
  PATH_SUFFIXES include
)
find_library(FFMPEG_AVFORMAT_LIB avformat
  HINTS ${FFMPEG_HINTS}
  PATH_SUFFIXES lib
)
find_library(FFMPEG_AVCODEC_LIB avcodec
  HINTS ${FFMPEG_HINTS}
  PATH_SUFFIXES lib
)
find_library(FFMPEG_AVUTIL_LIB avutil
  HINTS ${FFMPEG_HINTS}
  PATH_SUFFIXES lib
)
find_library(FFMPEG_SWSCALE_LIB swscale
  HINTS ${FFMPEG_HINTS}
  PATH_SUFFIXES lib
)
find_library(FFMPEG_SWRESAMPLE_LIB swresample
  HINTS ${FFMPEG_HINTS}
  PATH_SUFFIXES lib
)

if(NOT FFMPEG_INCLUDE_DIR OR
   NOT FFMPEG_AVFORMAT_LIB OR
   NOT FFMPEG_AVCODEC_LIB OR
   NOT FFMPEG_AVUTIL_LIB OR
   NOT FFMPEG_SWSCALE_LIB OR
   NOT FFMPEG_SWRESAMPLE_LIB)
  message(FATAL_ERROR "FFmpeg development libraries not found. Install ffmpeg dev packages.")
endif()

# All C/C++ sources under src/ (subfolders included). Re-runs CMake when files are added/removed.
file(GLOB_RECURSE MOONCHILD_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)

add_executable(moonchild_shell ${MOONCHILD_SOURCES})

target_include_directories(moonchild_shell PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/moonchild"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/framewrk"
  "${FFMPEG_INCLUDE_DIR}"
)

target_link_libraries(moonchild_shell PRIVATE
  SDL2::SDL2
  SDL2_mixer::SDL2_mixer
  ZLIB::ZLIB
  ${FFMPEG_AVFORMAT_LIB}
  ${FFMPEG_AVCODEC_LIB}
  ${FFMPEG_AVUTIL_LIB}
  ${FFMPEG_SWSCALE_LIB}
  ${FFMPEG_SWRESAMPLE_LIB}
)

# Copy assets next to the executable so SDL_GetBasePath()/relative paths work when running from the build tree.
add_custom_command(
  TARGET moonchild_shell POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:moonchild_shell>/assets"
  COMMENT "Copy assets next to moonchild_shell"
)
