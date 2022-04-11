cmake_minimum_required(VERSION 3.16)
project(untitled3)

set(CMAKE_CXX_STANDARD 11)

add_executable(untitled32 test_2/test7_no_out_different.cpp uthreads.cpp uthreads.h)
#add_executable(untitled31 test_2/test.in.cpp uthreads.cpp uthreads.h)
#add_executable(untitled30 test_2/jona3.cpp.in.cpp uthreads.cpp uthreads.h)
#add_executable(untitled32 test/test1.in.cpp uthreads.cpp uthreads.h)
#add_executable(untitled2 demo_itimer.c)
add_executable(untitled3 main.cpp uthreads.h uthreads.cpp)
# main.cpp demo_itimer.c demo_jmp.c demo_singInt_handler.c