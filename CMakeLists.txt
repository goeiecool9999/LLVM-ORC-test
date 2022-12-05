cmake_minimum_required(VERSION 3.23)
project(llvm_ORC_example)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(llvm_ORC_example main.cpp)

target_precompile_headers(llvm_ORC_example PRIVATE precompiled.h)
target_link_libraries(llvm_ORC_example PRIVATE LLVM LLVMX86Disassembler)