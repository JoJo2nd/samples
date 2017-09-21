cmake_minimum_required(VERSION 2.8.12.2)

project(gfx)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(BUILD_PLATFORM "windows")
  set(PLATFORM_WINDOWS true)
endif()

function(ADD_SAMPLE SAM_NAME)

    file(GLOB SAMPLE_FILES 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${SAM_NAME}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${SAM_NAME}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${SAM_NAME}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx/examples/common/imgui/imgui.cpp"
    )
    add_executable(${SAM_NAME} ${SAMPLE_FILES})

    target_link_libraries(${SAM_NAME} common)
    target_link_libraries(${SAM_NAME} debug bgfxDebug)
    target_link_libraries(${SAM_NAME} debug bxDebug)
    target_link_libraries(${SAM_NAME} debug bimgDebug)
    target_link_libraries(${SAM_NAME} debug bimg_encodeDebug)
    target_link_libraries(${SAM_NAME} debug bimg_decodeDebug)
    target_link_libraries(${SAM_NAME} debug example-commonDebug)
    target_link_libraries(${SAM_NAME} optimized bgfxRelease) 
    target_link_libraries(${SAM_NAME} optimized bxRelease)
    target_link_libraries(${SAM_NAME} optimized bimgRelease)
    target_link_libraries(${SAM_NAME} optimized bimg_encodeRelease)
    target_link_libraries(${SAM_NAME} optimized bimg_decodeRelease)
    target_link_libraries(${SAM_NAME} optimized example-commonRelease)

endfunction()

function(ADD_TOOL TOOL_NAME)

    file(GLOB TOOL_FILES 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${TOOL_NAME}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${TOOL_NAME}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${TOOL_NAME}/*.cpp"
    )
    add_executable(${TOOL_NAME} ${TOOL_FILES})
endfunction()

set( INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bimg/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bx/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx/examples/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx/3rdparty"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common"
)
set( LINK_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx/.build/win64_vs2015/bin"
)

file(GLOB SRC_COMMON_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.cpp"
)

add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_ITERATOR_DEBUG_LEVEL=0)
# BGFX uses the static runtime so link to that
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")

include_directories(${INCLUDE_DIRS})
link_directories(${LINK_DIRS})

add_library(common ${SRC_COMMON_FILES})

ADD_TOOL(mtlc)

ADD_SAMPLE(template)
ADD_SAMPLE(zbin)
ADD_SAMPLE(example)
