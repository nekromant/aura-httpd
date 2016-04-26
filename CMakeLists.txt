CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(aurahttpd)
SET(PROJECT_VERSION   0.1.2)
enable_testing()

SET(SRCS
  vfs.c
  utils.c
  main.c
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


ADD_EXECUTABLE(aurahttpd ${SRCS})

TARGET_LINK_LIBRARIES(aurahttpd
    ${AURA_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
    ${JSON_LIBRARIES}
)

INSTALL(TARGETS aurahttpd
        DESTINATION bin)

INSTALL(FILES config.json
        DESTINATION etc/aura-httpd
	RENAME default.conf)
