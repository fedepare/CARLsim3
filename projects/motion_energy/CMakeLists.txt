cmake_minimum_required(VERSION 3.7)
project(motion_energy)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        results/carlsim.log
        results/conn_input_output.dat
        results/sim_hello_world.dat
        results/spk_input.dat
        results/spk_output.dat
        scripts/demoOAT.m
        scripts/initOAT.m
        main_motion_energy.cpp
        Makefile
        motion_energy
        motion_energy.vcxproj)

add_executable(motion_energy ${SOURCE_FILES})