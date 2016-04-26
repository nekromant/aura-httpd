CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(aurahttpd)
SET(PROJECT_VERSION   0.1.2)
enable_testing()

SET(AHTTPD_TEST_LEAKS no CACHE BOOL "Valgrind for memory leaks during testing")

SET(CORE_SRCS
  vfs.c
  utils.c
  serdes-json.c
  mime.c
  server.c
  fs_control.c
  fs_node.c
  fs_static.c
#  fs_memcache.c
)

FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(AURA REQUIRED aura)
PKG_CHECK_MODULES(LIBEVENT REQUIRED libevent)
PKG_CHECK_MODULES(JSON REQUIRED json-c)

SET(CMAKE_C_FLAGS "${AURA_CFLAGS_OTHER} -Wall -g")

INCLUDE_DIRECTORIES(
    ${AURA_INCLUDE_DIRS}
    ${LIBEVENT_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

message(STATUS "Generarating ${CMAKE_SOURCE_DIR}/.clang_complete")
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
file(WRITE ${CMAKE_SOURCE_DIR}/.clang_complete "")
foreach(dir ${dirs})
  file(APPEND ${CMAKE_SOURCE_DIR}/.clang_complete "-I${dir}\n")
endforeach()
string(REPLACE " " ";" CMAKE_C_FLAGS_SPLIT ${CMAKE_C_FLAGS})
foreach(flag ${CMAKE_C_FLAGS_SPLIT})
  file(APPEND ${CMAKE_SOURCE_DIR}/.clang_complete "${flag}\n")
endforeach()

ADD_LIBRARY(ahttpd STATIC ${CORE_SRCS})
TARGET_LINK_LIBRARIES(ahttpd
    ${AURA_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
    ${JSON_LIBRARIES}
)

file(GLOB UNITS
    "${CMAKE_SOURCE_DIR}/tests/*.c"
)

foreach(file ${UNITS})
  GET_FILENAME_COMPONENT(f ${file} NAME_WE)
  ADD_EXECUTABLE(test-${f} ${file} ${CORE_SRCS})
  TARGET_LINK_LIBRARIES(test-${f} ahttpd)
  ADD_TEST(${f} test-${f})
  if (AHTTPD_TEST_LEAKS)
    ADD_TEST(memcheck-${f} valgrind
      --error-exitcode=1 --read-var-info=yes
      --leak-check=full  --show-leak-kinds=all
      --undef-value-errors=no --xml=yes --xml-file=${f}.xml
    ./test-${f})
  endif()
endforeach(file)


ADD_EXECUTABLE(aurahttpd ${CORE_SRCS} main.c)
TARGET_LINK_LIBRARIES(aurahttpd ahttpd)

INSTALL(TARGETS aurahttpd
        DESTINATION bin)

INSTALL(FILES config.json
        DESTINATION etc/aura-httpd
	RENAME default.conf)
