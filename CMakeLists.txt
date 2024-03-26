cmake_minimum_required(VERSION 3.27)
project(fast_net)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

set(PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)

file(GLOB_RECURSE SOURCE_FILES "${PROJECT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE HEADER_FILES "${PROJECT_SOURCE_DIR}/*.h")
list(REMOVE_ITEM SOURCE_FILES "${PROJECT_SOURCE_DIR}/server.cpp")
list(REMOVE_ITEM SOURCE_FILES "${PROJECT_SOURCE_DIR}/client.cpp")

add_executable(server "${PROJECT_SOURCE_DIR}/server.cpp" ${SOURCE_FILES} ${HEADER_FILES})
add_executable(client "${PROJECT_SOURCE_DIR}/client.cpp" ${SOURCE_FILES} ${HEADER_FILES})

add_custom_target(
        format
        COMMAND find ${CMAKE_SOURCE_DIR} -type f \( -iname "*.h" -o -iname "*.cpp" \) -exec clang-format -i {} +
)
