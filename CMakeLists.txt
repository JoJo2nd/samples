cmake_minimum_required(VERSION 2.8.12.2)

project(gfx)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(FLATBUFFERC_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/data/builder/flatc")

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(BUILD_PLATFORM "windows")
  set(PLATFORM_WINDOWS true)
endif()

function(FLATBUFFER_GENERATE_BINDINGS SRCS DEST_FOLDER FBS_INCLUDES)    
  set(LFBS_INCLUDES)
  set(FBS_SRCS "${SRCS}")
  foreach(FIL ${FBS_SRCS})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_WE ${FIL} NAME_WE)

    set(LFBS_INCLUDES "${LFBS_INCLUDES}" "${DEST_FOLDER}/${FIL_WE}_generated.h")
    set_source_files_properties(${FIL} PROPERTIES HEADER_FILE_ONLY TRUE)

    add_custom_command(
      OUTPUT "${DEST_FOLDER}/${FIL_WE}_generated.h"
      COMMAND ${FLATBUFFERC_EXECUTABLE}
      ARGS -o ${DEST_FOLDER} --cpp ${ABS_FIL}
      DEPENDS ${ABS_FIL}
      COMMENT "flatc ${FIL}"
      VERBATIM )
  endforeach()

  set_source_files_properties(${LFBS_INCLUDES} PROPERTIES GENERATED TRUE)
  set(FBS_INCLUDES ${LFBS_INCLUDES} PARENT_SCOPE)
endfunction()

function(ADD_SAMPLE SAM_NAME)

    file(GLOB SAMPLE_FILES 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${SAM_NAME}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${SAM_NAME}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${SAM_NAME}/*.cpp"
    )
    add_executable(${SAM_NAME} ${SAMPLE_FILES})

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

function(ADD_SAMPLE_FBS SAM_NAME FBS_FOLDER)

    file(GLOB SAMPLE_FILES 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${SAM_NAME}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${SAM_NAME}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${SAM_NAME}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/${FBS_FOLDER}/*.fbs"
    )

    file(GLOB SAMPLE_FBS_FILES 
        "${CMAKE_CURRENT_SOURCE_DIR}/${FBS_FOLDER}/*.fbs"
    )

    FLATBUFFER_GENERATE_BINDINGS("${SAMPLE_FBS_FILES}" "${CMAKE_CURRENT_SOURCE_DIR}/src/${SAM_NAME}/generated" FBS_INCLUDES)
    add_executable(${SAM_NAME} ${SAMPLE_FILES} ${FBS_INCLUDES})

    target_link_libraries(${SAM_NAME} debug bgfxDebug)
    target_link_libraries(${SAM_NAME} debug bxDebug)
    target_link_libraries(${SAM_NAME} debug bimgDebug)
    target_link_libraries(${SAM_NAME} debug example-commonDebug)
    target_link_libraries(${SAM_NAME} optimized bgfxRelease) 
    target_link_libraries(${SAM_NAME} optimized bxRelease)
    target_link_libraries(${SAM_NAME} optimized bimgRelease)
    target_link_libraries(${SAM_NAME} optimized example-commonRelease)

endfunction()

set( INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bimg/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bx/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx/examples/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/flatbuffers/include"
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

ADD_SAMPLE(template)
ADD_SAMPLE(zbin)
