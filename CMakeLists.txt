cmake_minimum_required(VERSION 3.23)
project(JsonParser)

set(CMAKE_CXX_STANDARD 23)

add_executable(Test test.cpp)
add_executable(JsonParser stream_wrapper.h stream_wrapper.cpp lexer.h lexer.cpp json_parser.h json_parser.cpp main.cpp)
