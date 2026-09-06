cmake_minimum_required(VERSION 3.20)
project(neuroevo_cpp VERSION 0.1.0 LANGUAGES CXX)

option(NEUROEVO_ENABLE_ACCELERATE "Use Apple Accelerate when available" ON)

add_library(neuroevo_lib src/neuroevo.cpp)
target_include_directories(neuroevo_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(neuroevo_lib PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(neuroevo_lib PRIVATE /W4 /permissive-)
else()
  target_compile_options(neuroevo_lib PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_library(neuroevo_capi src/c_api.cpp)
target_include_directories(neuroevo_capi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/native/CNeuroevo/include)
target_link_libraries(neuroevo_capi PUBLIC neuroevo_lib)
target_compile_features(neuroevo_capi PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(neuroevo_capi PRIVATE /W4 /permissive-)
else()
  target_compile_options(neuroevo_capi PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(APPLE AND NEUROEVO_ENABLE_ACCELERATE)
  find_library(ACCELERATE_FRAMEWORK Accelerate REQUIRED)
  target_link_libraries(neuroevo_lib PUBLIC ${ACCELERATE_FRAMEWORK})
  target_compile_definitions(neuroevo_lib PUBLIC
    NEUROEVO_USE_ACCELERATE=1
    ACCELERATE_NEW_LAPACK=1
  )
endif()

add_executable(neuroevo src/main.cpp)
target_link_libraries(neuroevo PRIVATE neuroevo_lib)

add_executable(neuroevo_benchmark tools/benchmark.cpp)
target_link_libraries(neuroevo_benchmark PRIVATE neuroevo_lib)

include(CTest)
if(BUILD_TESTING)
  add_executable(neuroevo_tests tests/test_neuroevo.cpp)
  target_link_libraries(neuroevo_tests PRIVATE neuroevo_capi)
  add_test(NAME neuroevo_tests COMMAND neuroevo_tests)
endif()
