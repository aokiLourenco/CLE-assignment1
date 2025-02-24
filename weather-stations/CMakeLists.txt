cmake_minimum_required(VERSION 3.22.1)
project(cle-weather-stations CXX)

find_package(OpenMP REQUIRED)

add_executable(cle-ws
    src/main.cpp
    src/Threadpool.cpp
)
target_compile_features(cle-ws PRIVATE cxx_std_20)
target_compile_options(cle-ws PRIVATE -Wall -Wextra)
target_link_libraries(cle-ws PRIVATE OpenMP::OpenMP_CXX)

add_executable(cle-samples src/create-samples.cpp)
target_compile_features(cle-samples PRIVATE cxx_std_20)
target_compile_options(cle-samples PRIVATE -Wall -Wextra)
target_link_libraries(cle-samples PRIVATE OpenMP::OpenMP_CXX)