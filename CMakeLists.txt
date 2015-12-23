CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(auracli)
SET(PROJECT_VERSION   0.1.2)
enable_testing()

SET(SRCS main.c)

FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(AURA REQUIRED aura)

SET(CMAKE_C_FLAGS "-Wall -Werror")

INCLUDE_DIRECTORIES(
    ${AURA_INCLUDE_DIRS}
)

ADD_EXECUTABLE(auracli ${SRCS})

TARGET_LINK_LIBRARIES(auracli
    ${AURA_LIBRARIES}
)

add_test(dummy_echo ./auracli -z 90 -t dummy -c echo_u16 678)

INSTALL(TARGETS auracli
        DESTINATION bin)