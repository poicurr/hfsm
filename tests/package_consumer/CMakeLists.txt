cmake_minimum_required(VERSION 3.20)
project(hfsm_package_consumer LANGUAGES CXX)

find_package(hfsm CONFIG REQUIRED)

add_executable(hfsm_package_consumer main.cpp)
target_link_libraries(hfsm_package_consumer PRIVATE hfsm::hfsm)
target_compile_features(hfsm_package_consumer PRIVATE cxx_std_20)
