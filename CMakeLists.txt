cmake_minimum_required(VERSION 3.27)
project(fast_net)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# spdlog
find_package(spdlog REQUIRED)

# liburing
find_package(PkgConfig REQUIRED)
pkg_search_module(LIBURING REQUIRED liburing)

set(PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)

file(GLOB_RECURSE SOURCE_FILES "${PROJECT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE SOURCE_FILES "${PROJECT_SOURCE_DIR}/**/*.cpp")
file(GLOB_RECURSE HEADER_FILES "${PROJECT_SOURCE_DIR}/*.h")
file(GLOB_RECURSE HEADER_FILES "${PROJECT_SOURCE_DIR}/**/*.h")
file(GLOB_RECURSE HEADER_FILES "${PROJECT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE HEADER_FILES "${PROJECT_SOURCE_DIR}/**/*.hpp")
list(REMOVE_ITEM SOURCE_FILES "${PROJECT_SOURCE_DIR}/server.cpp")
list(REMOVE_ITEM SOURCE_FILES "${PROJECT_SOURCE_DIR}/server_iou.cpp")
list(REMOVE_ITEM SOURCE_FILES "${PROJECT_SOURCE_DIR}/client.cpp")

add_executable(server "${PROJECT_SOURCE_DIR}/server.cpp" ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(server PRIVATE spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)

add_executable(client "${PROJECT_SOURCE_DIR}/client.cpp" ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(client PRIVATE spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)

add_executable(server_iou "${PROJECT_SOURCE_DIR}/server_iou.cpp" ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(server_iou PRIVATE ${LIBURING_INCLUDE_DIRS})
target_link_libraries(server_iou PRIVATE spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32> ${LIBURING_LIBRARIES})

add_custom_target(
        format
        COMMAND find ${CMAKE_SOURCE_DIR} -type f \( -iname "*.h" -o -iname "*.cpp" \) -exec clang-format -i {} +
)
