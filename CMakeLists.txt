cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0091 NEW)

option(WVP_ENABLE_DEBUG "Enables the debug scheme using the Partiels application" ON)

set(WVP_TESTER_ENABLE ON CACHE STRING "Enables the tests with the vamp plugin tester")
set(WVP_TESTER_TIMEOUT 600 CACHE STRING "The timeout in seconds of the test with the vamp plugin tester (zero for no timeout)")

### Sccache ###
set(WVP_USE_SCCACHE OFF CACHE BOOL "Uses sccache as the compiler launcher")
if(WVP_USE_SCCACHE)
  if(NOT CMAKE_C_COMPILER_LAUNCHER AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(WVP_SCCACHE_PROGRAM sccache)
    if(WVP_SCCACHE_PROGRAM)
      set(CMAKE_C_COMPILER_LAUNCHER "${WVP_SCCACHE_PROGRAM}" CACHE STRING "C compiler launcher" FORCE)
      set(CMAKE_CXX_COMPILER_LAUNCHER "${WVP_SCCACHE_PROGRAM}" CACHE STRING "C++ compiler launcher" FORCE)
      set(WVP_SCCACHE_LAUNCHER_MANAGED ON CACHE INTERNAL "Whether the Whisper Vamp Plugin enabled the compiler launcher with sccache")
      message(VERBOSE "sccache found, compiler launcher enabled")
    else()
      message(VERBOSE "sccache not found, continuing without compiler cache")
    endif()
  endif()
elseif(WVP_SCCACHE_LAUNCHER_MANAGED)
  unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
  unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
  unset(WVP_SCCACHE_LAUNCHER_MANAGED CACHE)
  message(VERBOSE "sccache compiler launcher disabled")
endif()

set(CMAKE_XCODE_GENERATE_SCHEME ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Default value for MSVC_RUNTIME_LIBRARY of targets" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE CACHE BOOL "Default value for POSITION_INDEPENDENT_CODE of targets" FORCE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE CACHE BOOL "Default value for INTERPROCEDURAL_OPTIMIZATION of targets" FORCE)

### Include Vamp (IVE) ###
set(IVE_BUILD_HOST_LIBRARY OFF)
set(IVE_BUILD_PLUGIN_LIBRARY ON)
add_subdirectory(ircam-vamp-extension EXCLUDE_FROM_ALL)

### Include Whisper ###
set(BUILD_SHARED_LIBS OFF CACHE BOOL "ggml: build shared libraries" FORCE)
set(GGML_USE_METAL ON CACHE BOOL "ggml: use Metal" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "ggml: optimize the build for the current system" FORCE)
option(GGML_METAL_EMBED_LIBRARY "ggml: embed Metal library" ON)
option(GGML_LTO "ggml: use LTO" ON)
option(GGML_OPENMP "ggml: use OpenMP" OFF)
option(GGML_CCACHE "ggml: use ccache" OFF)
option(GGML_CPU  "ggml: enable CPU backend" ON)
if(APPLE)
  option(GGML_BLAS "ggml: use Accelerate framework" OFF)
endif()
if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
   set(GGML_NATIVE OFF CACHE BOOL "ggml: optimize the build for the current system" FORCE)
endif()
option(WHISPER_CCACHE "whisper: use ccache if available" OFF)

set(WHISPER_STATIC ON)

add_subdirectory(whisper.cpp EXCLUDE_FROM_ALL)

# Set specific compile options and properties for ggml and llama targets
function(set_ggml_options)
  foreach(target IN LISTS ARGN)
    if(TARGET ${target})
      if(CMAKE_GENERATOR STREQUAL "Xcode")
        target_compile_options(${target} PRIVATE -Wno-shorten-64-to-32 -Wno-implicit-float-conversion -Wno-shadow-field-in-constructor -Wno-missing-noreturn -Wno-unused-parameter)
        set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_GCC_WARN_64_TO_32_BIT_CONVERSION "NO")
        set_target_properties(${target} PROPERTIES STATIC_LIBRARY_OPTIONS "-no_warning_for_no_symbols")
      elseif(UNIX)
        target_compile_options(${target} PRIVATE -Wno-unused-function -Wno-unused-parameter -Wno-c99-extensions)
      elseif(MSVC)
        target_compile_options(${target} PRIVATE /wd4005 /wd4244 /wd4305 /wd4068 /wd4244)
      endif()
    endif()
  endforeach()
endfunction()

set_ggml_options(ggml ggml-metal ggml-cpu ggml-base ggml-blas ggml-metal whisper)

if(MSVC)
  set_target_properties(ggml PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set_target_properties(whisper PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

### Project ###
project(WhisperVampPlugin VERSION 4.0.0 LANGUAGES C CXX)

### Version ###
execute_process(COMMAND git log -1 --format=%h WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_COMMIT_ID OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git status --porcelain WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_HAS_DIFF OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT GIT_HAS_DIFF STREQUAL "")
  set(GIT_COMMIT_ID "${GIT_COMMIT_ID}-head")
endif()

if(NOT WVP_BUILD_TAG OR WVP_BUILD_TAG STREQUAL "")
  set(WVP_BUILD_TAG ${CMAKE_PROJECT_VERSION})
endif()
message(STATUS "Whisper Vamp Plugin: " ${WVP_BUILD_TAG} " (" ${GIT_COMMIT_ID} ")")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/version.txt ${WVP_BUILD_TAG})

### Package ###
set(VPP_NAME "Whisper" CACHE PATH "The name of the project" FORCE)
set(VPP_COMPANY "Ircam" CACHE PATH "The name of the company" FORCE)
set(VPP_URL "www.ircam.fr" CACHE PATH "The URL of the project")
set(VPP_APPID "f19f1fac-b588-4615-80cf-356ec2b917c1" CACHE PATH "The unique ID of the project")
set(VPP_BUILD_TAG "${WVP_BUILD_TAG}" CACHE STRING "The current tag of the project" FORCE)
set(VPP_ABOUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resource/About.txt" CACHE PATH "The about file to add to the distribution" FORCE)
set(VPP_CHANGELOG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resource/ChangeLog.txt" CACHE PATH "The changelog file to add to the distribution" FORCE)
set(VPP_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resource/Ircam-logo-noir-RS.png" CACHE PATH "The icon file to add to the distribution" FORCE)
set(VPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/package CACHE PATH "The directory of the package" FORCE)
include(vamp-plugin-packager/vamp-plugin-packager.cmake)

### Source ###

### Models ###
# The ggml models are not compiled into the plugin: they are downloaded here
# and copied into the Resources folder of the Vamp bundle (see the POST_BUILD command).
set(WVP_MODELS_DIR "${CMAKE_CURRENT_BINARY_DIR}/models")
file(MAKE_DIRECTORY "${WVP_MODELS_DIR}")
foreach(WVP_MODEL_CAPACITY IN ITEMS "base-q5_1" "tiny-q5_1" "small-q5_1" "medium-q5_0" "large-v2-q5_0")
  set(WVP_MODEL_PATH "${WVP_MODELS_DIR}/ggml-${WVP_MODEL_CAPACITY}.bin")
  if(NOT EXISTS ${WVP_MODEL_PATH})
    message(STATUS "Downloading model ggml-${WVP_MODEL_CAPACITY}.bin")
    if(WIN32)
      execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/models/download-ggml-model.cmd ${WVP_MODEL_CAPACITY})
    else()
      execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/models/download-ggml-model.sh ${WVP_MODEL_CAPACITY})
    endif()
    file(COPY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/models/ggml-${WVP_MODEL_CAPACITY}.bin" ${WVP_MODEL_PATH})
    file(REMOVE "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/models/ggml-${WVP_MODEL_CAPACITY}.bin")
  endif()
endforeach()

file(GLOB WVP_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/source/wvp.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/wvp.h
)
source_group("sources" FILES ${WVP_SOURCES})

### Target ###
add_library(wvp MODULE ${WVP_SOURCES})
ive_prepare_plugin_target(wvp)
target_link_libraries(wvp PRIVATE whisper)

if(WVP_TESTER_ENABLE)
  enable_testing()
endif()

vpp_add_plugin(wvp
  OUTPUT_NAME ircamwhisper
  CATFILE ${CMAKE_CURRENT_SOURCE_DIR}/resource/ircamwhisper.cat
  COPYRIGHT "Copyright 2026 Ircam. All rights reserved"
  VERSION ${PROJECT_VERSION_MAJOR}
  INSTALL ON
  DEBUG ${WVP_ENABLE_DEBUG}
  DEBUG_DOCUMENT ${CMAKE_CURRENT_SOURCE_DIR}/test/whisper.ptldoc
  # The test C1 (Normal input) is sufficient to check that the plugin loads (no
  # dependency issues), that the ggml models of the bundle are loaded and that
  # the inference runs, without testing all the configurations that take long.
  TESTER ${WVP_TESTER_ENABLE}
  TESTER_ARGUMENTS -a -t C1
  TESTER_TIMEOUT ${WVP_TESTER_TIMEOUT}
)

add_custom_command(TARGET wvp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${WVP_MODELS_DIR}" "$<TARGET_FILE_DIR:wvp>/../Resources/models"
)

vpp_create_clang_format_targets(wvp ${WVP_SOURCES})

### Manual ###
set(WVP_MANUAL_DIR ${CMAKE_CURRENT_BINARY_DIR}/Manual)
file(MAKE_DIRECTORY ${WVP_MANUAL_DIR})

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/docs/whisper-manual.md MANUAL_CONTENT)
string(REPLACE "APPVERSION" "${WVP_BUILD_TAG} (${GIT_COMMIT_ID})" MANUAL_CONTENT ${MANUAL_CONTENT})
string(REPLACE "src=\"../resource/" "src=\"${CMAKE_CURRENT_SOURCE_DIR}/resource/" MANUAL_CONTENT ${MANUAL_CONTENT})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/whisper-manual.md ${MANUAL_CONTENT})

find_program(MDPDF_EXE "mdpdf")
if(MDPDF_EXE)
  add_custom_target(wvp_manual COMMAND ${MDPDF_EXE} ${CMAKE_CURRENT_BINARY_DIR}/whisper-manual.md ${WVP_MANUAL_DIR}/whisper-manual.pdf VERBATIM)
else()
  message(STATUS "Manual target cannot be generated because mdpdf is not found")
endif()

### Publishing ###
set(PROJECT_URL "https://github.com/Ircam-Partiels/whisper-vamp-plugin")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/whisper-release.md "## Downloads\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/whisper-release.md "- [Whisper-Linux.zip](${PROJECT_URL}/releases/download/${WVP_BUILD_TAG}/Whisper-Linux.zip)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/whisper-release.md "- [Whisper-MacOS.pkg](${PROJECT_URL}/releases/download/${WVP_BUILD_TAG}/Whisper-MacOS.pkg)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/whisper-release.md "- [Whisper-Windows.exe](${PROJECT_URL}/releases/download/${WVP_BUILD_TAG}/Whisper-Windows.exe)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/whisper-release.md "- [Whisper-Manual.zip](${PROJECT_URL}/releases/download/${WVP_BUILD_TAG}/Whisper-Manual.zip)\n") 
