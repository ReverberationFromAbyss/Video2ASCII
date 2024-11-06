cmake_minimum_required(VERSION 3.1)

project(Video2Ascii VERSION 0.0.1)

configure_file(Config.h.in Config.h)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

set(SRC main.cpp)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(
  ${PROJECT_NAME}
  ${OpenCV_LIBS}
)

target_include_directories(${PROJECT_NAME} PUBLIC
  ${PROJECT_BINARY_DIR})
