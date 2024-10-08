cmake_minimum_required(VERSION 3.22.1)
project(tjv VERSION 1.0.0 LANGUAGES C)

set(TARGET tjv)
set(CMAKE_C_STANDARD   11)
set(CMAKE_C_STANDARD_REQUIRED true)
set(THREADS_PREFER_PTHREAD_FLAG ON)

if(WIN32)
    # Prefix all shared libraries with 'lib'.
    set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
    # Prefix all static libraries with 'lib'.
    set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(cJSON REQUIRED)
message(STATUS "CJSON_LIBRARIES: ${CJSON_LIBRARIES}")
message(STATUS "CJSON_INCLUDE_DIRS: ${CJSON_INCLUDE_DIRS}")

find_package(TCL 8.6.13 REQUIRED)  # TCL_INCLUDE_PATH TCL_LIBRARY
find_program(TCL_TCLSH
  NAMES
      tclsh86 tclsh8.6
      tclsh90 tclsh9.0
      tclsh
  PATHS
      ${TCL_INCLUDE_PATH}/../bin
      ${TCL_INCLUDE_PATH}/../../bin
  NO_DEFAULT_PATH
)

message(STATUS "TCL_INCLUDE_PATH: ${TCL_INCLUDE_PATH}")
message(STATUS "TCL_LIBRARY: ${TCL_LIBRARY}")
message(STATUS "TCL_TCLSH: ${TCL_TCLSH}")

add_compile_options(-Wall -Wextra -Wpedantic)
add_compile_definitions(TCL_THREADS VERSION=${PROJECT_VERSION})

if ("${ADDRESS_SANITIZER}" STREQUAL "ON")
    add_compile_options(-fPIC -g -fsanitize=undefined -fsanitize=address)
    add_link_options(-fsanitize=undefined -fsanitize=address)
    add_compile_definitions(TCL_MEM_DEBUG=1)
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_compile_definitions(TCL_MEM_DEBUG=1)
    set(CMAKE_C_FLAGS "-g -DDEBUG ${CMAKE_C_FLAGS}")
else()
    # NDEBUG to not generate code for assert
    add_compile_definitions(NDEBUG)
endif ()

#set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CTEST_ARGUMENTS "--verbose")
#set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
enable_testing()
add_test(NAME AllUnitTests
    COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/tests/all.tcl -verbose "body error start"
)
set_tests_properties(AllUnitTests
    PROPERTIES ENVIRONMENT TCLLIBPATH=${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(shell ${CMAKE_COMMAND} -E env TCLLIBPATH=${CMAKE_CURRENT_BINARY_DIR} ${TCL_TCLSH}
    USES_TERMINAL
    DEPENDS ${TARGET})

add_library(tjv SHARED
    src/common.h
    src/library.c
    src/library.h
    src/tjvCompile.c
    src/tjvCompile.h
    src/tjvValidateTcl.c
    src/tjvValidateTcl.h
    src/tjvValidateJson.c
    src/tjvValidateJson.h
    src/tjvMessage.c
    src/tjvMessage.h
)
set_target_properties(tjv PROPERTIES POSITION_INDEPENDENT_CODE ON)

include_directories(${TCL_INCLUDE_PATH} ${CJSON_INCLUDE_DIRS})
target_link_libraries(tjv PRIVATE ${TCL_LIBRARY} ${CJSON_LIBRARIES})
get_filename_component(TCL_LIBRARY_PATH "${TCL_LIBRARY}" PATH)

install(TARGETS ${TARGET}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)

configure_file(pkgIndex.tcl.in pkgIndex.tcl @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${TARGET}${PROJECT_VERSION}
)
