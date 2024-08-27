cmake_minimum_required(VERSION 3.18)

set(IGNORE_VAMP_PLUGIN_TESTER OFF CACHE STRING "Disables the tests with vamp plugin tester")
set(PARTIELS_EXE_HINT_PATH "/Applications" CACHE PATH "")

set(CMAKE_XCODE_GENERATE_SCHEME ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Default value for MSVC_RUNTIME_LIBRARY of targets" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE CACHE BOOL "Default value for POSITION_INDEPENDENT_CODE of targets" FORCE)

### Include Vamp (IVE) ###
set(IVE_BUILD_HOST_LIBRARY OFF)
set(IVE_BUILD_PLUGIN_LIBRARY ON)
add_subdirectory(ircam-vamp-extension EXCLUDE_FROM_ALL)

### Include Whisper ###
include(FetchContent)
find_package(Git REQUIRED)

option(BUILD_SHARED_LIBS "ggml: build shared libraries" OFF)
option(GGML_METAL_EMBED_LIBRARY "ggml: embed Metal library" ON)
option(GGML_OPENMP "ggml: use OpenMP" OFF)
set(WHISPER_STATIC ON)
set(BUILD_SHARED_LIBS_DEFAULT OFF)

FetchContent_Declare(
  whisper_cpp
  GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
  GIT_TAG        fe36c909715e6751277ddb020e7892c7670b61d4
  PATCH_COMMAND  ${GIT_EXECUTABLE} reset --hard && ${GIT_EXECUTABLE} clean -fxd && ${GIT_EXECUTABLE} apply ${CMAKE_CURRENT_SOURCE_DIR}/resource/whisper.cpp-fe36c90.patch
  CMAKE_ARGS -DGGML_OPENMP=OFF -DGGML_METAL_EMBED_LIBRARY=ON -DGGML_OPENMP=OFF -DWHISPER_STATIC=ON -DBUILD_SHARED_LIBS_DEFAULT=OFF
  UPDATE_DISCONNECTED TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(whisper_cpp)

if(APPLE)
  target_compile_options(ggml PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(whisper PRIVATE -Wno-shorten-64-to-32)
elseif(UNIX)
  target_compile_options(ggml PRIVATE -Wno-unused-function -Wno-unused-parameter)
  target_compile_options(whisper PRIVATE -Wno-unused-function -Wno-unused-parameter)
endif()

### Project ###
project(WhisperVampPlugin VERSION 1.0.0 LANGUAGES C CXX)

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
set(WVP_MODEL_CPP ${CMAKE_CURRENT_BINARY_DIR}/source/wvp_model.cpp)
set(WVP_MODEL_H ${CMAKE_CURRENT_SOURCE_DIR}/source/wvp_model.h)
if(NOT EXISTS ${WVP_MODEL_CPP})
  if(WIN32)
    execute_process(COMMAND ${whisper_cpp_SOURCE_DIR}/models/download-ggml-model.cmd tiny)
  else()
    execute_process(COMMAND ${whisper_cpp_SOURCE_DIR}/models/download-ggml-model.sh tiny)
  endif()
  set(WVP_MODEL_PATH "${whisper_cpp_SOURCE_DIR}/models/ggml-tiny.bin")
  file(RELATIVE_PATH  WVP_MODEL_HREL "${CMAKE_CURRENT_BINARY_DIR}/source" ${WVP_MODEL_H})
  file(WRITE ${WVP_MODEL_CPP} "#include \"${WVP_MODEL_HREL}\"\n\n")

  if(EXISTS ${WVP_MODEL_PATH})
    message(STATUS "Generating default model ${WVP_MODEL_PATH}")
    file(READ ${WVP_MODEL_PATH} WVP_HEX_DATA HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," WVP_HEX_DATA ${WVP_HEX_DATA})
    file(APPEND ${WVP_MODEL_CPP} "namespace Wvp\n{\n")
    file(APPEND ${WVP_MODEL_CPP} "static const unsigned char binary_data[] =\n{\n")
    file(APPEND ${WVP_MODEL_CPP} "${WVP_HEX_DATA}")
    file(APPEND ${WVP_MODEL_CPP} "};\n}\n\n")
    file(APPEND ${WVP_MODEL_CPP} "const void* Wvp::model = (const void*)Wvp::binary_data;\n")
    file(APPEND ${WVP_MODEL_CPP} "const size_t Wvp::model_size = sizeof(Wvp::binary_data);\n")
  else()
    message(WARNING "Default model ${WVP_MODEL_PATH} invalid")
    file(APPEND ${WVP_MODEL_CPP} "const void* Wvp::model = nullptr;\n")
    file(APPEND ${WVP_MODEL_CPP} "const size_t Wvp::model_size = 0;\n")
  endif()
    file(APPEND ${WVP_MODEL_CPP} "\n")
endif()

file(GLOB WVP_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/source/wvp.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/wvp.h
  ${WVP_MODEL_H}
)
source_group("sources" FILES ${WVP_SOURCES})

### Target ###
add_library(wvp SHARED ${WVP_SOURCES} ${WVP_MODEL_CPP})
ive_prepare_plugin_target(wvp)
target_link_libraries(wvp PRIVATE whisper)
target_compile_definitions(wvp PRIVATE WVP_PLUGIN_VERSION=${PROJECT_VERSION_MAJOR})

add_custom_command(TARGET wvp POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/resource/ircamwhisper.cat "$<IF:$<CONFIG:Debug>,${CMAKE_CURRENT_BINARY_DIR}/Debug/ircamwhisper.cat,${CMAKE_CURRENT_BINARY_DIR}/Release/ircamwhisper.cat>")
set_target_properties(wvp PROPERTIES LIBRARY_OUTPUT_NAME ircamwhisper)
vpp_add_plugin(wvp)

find_program(PARTIELS_EXE "Partiels" HINTS ${PARTIELS_EXE_HINT_PATH} NO_CACHE)
if(PARTIELS_EXE)
  if(NOT IS_DIRECTORY ${PARTIELS_EXE}) 
    cmake_path(GET PARTIELS_EXE PARENT_PATH PARTIELS_EXE_PARENT)
    cmake_path(SET PARTIELS_EXE NORMALIZE "${PARTIELS_EXE_PARENT}/../../")
  endif()
  message(STATUS "Debugging: " ${PARTIELS_EXE})
  set_target_properties(wvp PROPERTIES XCODE_SCHEME_EXECUTABLE ${PARTIELS_EXE})
  set_target_properties(wvp PROPERTIES XCODE_SCHEME_ARGUMENTS "${CMAKE_CURRENT_SOURCE_DIR}/test/whisper.ptldoc")
  set_target_properties(wvp PROPERTIES XCODE_SCHEME_ENVIRONMENT "VAMP_PATH=${CMAKE_CURRENT_BINARY_DIR}/Debug;WHISPERMODELPATH=/Users/guillot/Gitlab/IVP/whisper-vamp-plugin/whisper.cpp/models/ggml-base.bin")
endif()

### Format ###
find_program(CLANG_FORMAT_EXE "clang-format" HINTS "C:/Program Files/LLVM/bin")
if(CLANG_FORMAT_EXE)
  add_custom_target(wvp_check_format ${CLANG_FORMAT_EXE} --Werror --dry-run --verbose -style=file ${WVP_SOURCES})
  add_custom_target(wvp_apply_format ${CLANG_FORMAT_EXE} -i -style=file ${WVP_SOURCES})
else()
  message(STATUS "Clang Format targets cannot be generated because clang-format is not found")
endif()

### Manual ###
set(WVP_MANUAL_DIR ${CMAKE_CURRENT_BINARY_DIR}/Manual)
file(MAKE_DIRECTORY ${WVP_MANUAL_DIR})

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/docs/whisper-manual.md MANUAL_CONTENT)
string(REPLACE "APPVERSION" "${WVP_BUILD_TAG} (${GIT_COMMIT_ID})" MANUAL_CONTENT ${MANUAL_CONTENT})
string(REPLACE "src=\"../resource/" "src=\"${CMAKE_CURRENT_SOURCE_DIR}/resource/" MANUAL_CONTENT ${MANUAL_CONTENT})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/whisper-manual.md ${MANUAL_CONTENT})

file(COPY ${CMAKE_CURRENT_BINARY_DIR}/_deps/whisper_cpp-src/models/download-ggml-model.cmd DESTINATION ${WVP_MANUAL_DIR})
file(COPY ${CMAKE_CURRENT_BINARY_DIR}/_deps/whisper_cpp-src/models/download-ggml-model.sh DESTINATION ${WVP_MANUAL_DIR})
find_program(MDPDF_EXE "mdpdf")
if(MDPDF_EXE)
  add_custom_target(wvp_manual COMMAND ${MDPDF_EXE} ${CMAKE_CURRENT_BINARY_DIR}/whisper-manual.md ${WVP_MANUAL_DIR}/whisper-manual.pdf VERBATIM)
else()
  message(STATUS "Manual target cannot be generated because mdpdf is not found")
endif()

### Publishing ###
set(PROJECT_URL "https://github.com/Ircam-Partiels/whisper-vamp-plugin")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/whisper-release.md "## Downloads\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/whisper-release.md "- [Whisper-Linux.tar.gz](${PROJECT_URL}/releases/download/${WVP_BUILD_TAG}/Whisper-Linux.tar.gz)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/whisper-release.md "- [Whisper-MacOS.pkg](${PROJECT_URL}/releases/download/${WVP_BUILD_TAG}/Whisper-MacOS.pkg)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/whisper-release.md "- [Whisper-Windows.exe](${PROJECT_URL}/releases/download/${WVP_BUILD_TAG}/Whisper-Windows.exe)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/whisper-release.md "- [Whisper-Manual.zip](${PROJECT_URL}/releases/download/${WVP_BUILD_TAG}/Whisper-Manual.zip)\n") 

### Install ###
if(APPLE)
  install(TARGETS wvp RUNTIME LIBRARY DESTINATION "~/Library/Audio/Plug-Ins/Vamp/")
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resource/ircamwhisper.cat DESTINATION "~/Library/Audio/Plug-Ins/Vamp/")
elseif(UNIX)
  install(TARGETS wvp RUNTIME LIBRARY DESTINATION "~/vamp/")
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resource/ircamwhisper.cat DESTINATION "~/vamp/")
elseif(WIN32)
  install(TARGETS wvp RUNTIME DESTINATION "$ENV{PROGRAMFILES}/Vamp Plugins/" PERMISSIONS OWNER_WRITE)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resource/ircamwhisper.cat DESTINATION "$ENV{PROGRAMFILES}/Vamp Plugins/")
endif()

### Testing ###
if(NOT IGNORE_VAMP_PLUGIN_TESTER)
  enable_testing()
  if(APPLE)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester/vamp-plugin-tester)
      file(DOWNLOAD "https://github.com/pierreguillot/vamp-plugin-tester/releases/download/1.1/vamp-plugin-tester-1.1-osx-arm.zip" "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.tar.gz")
      file(ARCHIVE_EXTRACT INPUT "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.tar.gz" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()
  elseif(UNIX)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester/vamp-plugin-tester)
      file(DOWNLOAD "https://github.com/pierreguillot/vamp-plugin-tester/releases/download/1.1/vamp-plugin-tester-1.1-linux64.tar.gz" "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.tar.gz")
      file(ARCHIVE_EXTRACT INPUT "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.tar.gz" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()
  elseif(WIN32)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester/vamp-plugin-tester.exe)
      file(DOWNLOAD "https://github.com/pierreguillot/vamp-plugin-tester/releases/download/1.1/vamp-plugin-tester-1.1a-win64.zip" "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.zip")
      file(ARCHIVE_EXTRACT INPUT "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.zip" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()
  endif()

  add_test(NAME VampPluginTester COMMAND ${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester/vamp-plugin-tester -a)
  set_tests_properties(VampPluginTester PROPERTIES ENVIRONMENT "$<IF:$<CONFIG:Debug>,VAMP_PATH=${CMAKE_CURRENT_BINARY_DIR}/Debug,VAMP_PATH=${CMAKE_CURRENT_BINARY_DIR}/Release>")
endif()
