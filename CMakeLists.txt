CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(aurahttpd)
SET(PROJECT_VERSION   0.1.2)
enable_testing()
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

SET(AHTTPD_TESTSUITE no CACHE BOOL "Run ahttpd test suite")
SET(AHTTPD_TEST_LEAKS no CACHE BOOL "Valgrind for memory leaks during testing")
SET(AHTTPD_COVERALLS no CACHE BOOL "Generate coveralls data")


SET(CORE_SRCS
  vfs.c
  utils.c
  serdes-json.c
  mime.c
  server.c
  call_resource.c
  fs_control.c
  fs_node.c
  fs_static.c
#  fs_memcache.c
)

FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(AURA REQUIRED aura)
PKG_CHECK_MODULES(DAEMON REQUIRED libdaemon)
PKG_CHECK_MODULES(LIBEVENT REQUIRED libevent)
PKG_CHECK_MODULES(JSON REQUIRED json-c)

SET(CMAKE_C_FLAGS "${AURA_CFLAGS_OTHER} -Wall -g")

INCLUDE_DIRECTORIES(
    ${AURA_INCLUDE_DIRS}
    ${LIBEVENT_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIRS}
    ${DAEMON_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

ADD_LIBRARY(ahttpd STATIC ${CORE_SRCS})
TARGET_LINK_LIBRARIES(ahttpd
    ${AURA_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
    ${JSON_LIBRARIES}
    ${DAEMON_LIBRARIES}
    -lpthread
)


if (AHTTPD_COVERALLS)
    include(Coveralls)
    coveralls_turn_on_coverage()
    coveralls_setup(
       "${CORE_SRCS}" # The source files.
       ON                 # If we should upload.
       )
endif()


message(STATUS "Generarating ${CMAKE_SOURCE_DIR}/.clang_complete")
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
file(WRITE ${CMAKE_SOURCE_DIR}/.clang_complete "")
foreach(dir ${dirs})
  file(APPEND ${CMAKE_SOURCE_DIR}/.clang_complete "-I${dir}\n")
endforeach()
string(REPLACE " " ";" CMAKE_C_FLAGS_SPLIT ${CMAKE_C_FLAGS})
foreach(flag ${CMAKE_C_FLAGS_SPLIT})
  if (NOT "${flag}" MATCHES "-Wall")
      if (NOT "${flag}" MATCHES "-g")
        file(APPEND ${CMAKE_SOURCE_DIR}/.clang_complete "${flag}\n")
      endif()
  endif()
endforeach()

file(WRITE ${CMAKE_SOURCE_DIR}/.core_sources "")
foreach(f ${CORE_SRCS} main.c)
  file(APPEND ${CMAKE_SOURCE_DIR}/.core_sources "${f} ")
endforeach()

file(GLOB UNITS
    "${CMAKE_SOURCE_DIR}/tests/*.c"
)

foreach(file ${UNITS})
  GET_FILENAME_COMPONENT(f ${file} NAME_WE)
  ADD_EXECUTABLE(test-${f} ${file})
  TARGET_LINK_LIBRARIES(test-${f} ahttpd)
  ADD_TEST(${f} test-${f} "../config-test.json")
  if (AHTTPD_TEST_LEAKS)
    ADD_TEST(memcheck-${f} valgrind
      --error-exitcode=1 --read-var-info=yes
      --leak-check=full  --show-leak-kinds=all
      --undef-value-errors=no --xml=yes --xml-file=${f}.xml
    ./test-${f} "../config-test.json")
  endif()
endforeach(file)

file(GLOB TEST_SCRIPTS
    "${CMAKE_SOURCE_DIR}/test-scripts/*.sh"
    "${CMAKE_SOURCE_DIR}/test-scripts/*.js"
)

if (AHTTPD_TESTSUITE)
  foreach(file ${TEST_SCRIPTS})
    GET_FILENAME_COMPONENT(f ${file} NAME_WE)
    ADD_TEST(script-${f} ahttpd-test-wrapper "../config-test.json" ${file})
    if (AHTTPD_TEST_LEAKS)
      ADD_TEST(memcheck-${f} valgrind
        --error-exitcode=1 --read-var-info=yes
        --leak-check=full  --show-leak-kinds=all
        --undef-value-errors=no --xml=yes --xml-file=${f}.xml
      ./ahttpd-test-wrapper "../config-test.json" ${file})
    endif()
  endforeach(file)
endif()

ADD_EXECUTABLE(aurahttpd main.c)
TARGET_LINK_LIBRARIES(aurahttpd ahttpd)

ADD_EXECUTABLE(ahttpd-test-wrapper test-wrapper.c)
TARGET_LINK_LIBRARIES(ahttpd-test-wrapper ahttpd)

INSTALL(TARGETS aurahttpd
        DESTINATION bin)

INSTALL(FILES config.json
        DESTINATION etc/aura-httpd
	RENAME default.conf)
