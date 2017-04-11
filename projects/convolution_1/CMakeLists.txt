cmake_minimum_required(VERSION 3.7)
project(convolution_1)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        results/carlsim.log
        results/conn_input_output.dat
        results/sim_hello_world.dat
        results/spk_input.dat
        results/spk_output.dat
        scripts/demoOAT.m
        scripts/initOAT.m
        main_convolution_1.cpp
        Makefile
        convolution_1
        convolution_1.vcxproj)

add_executable(convolution_1 ${SOURCE_FILES})
