cmake_minimum_required(VERSION 3.21)

project(CO_CURL VERSION 1.0 LANGUAGES CXX)

add_subdirectory(include/co_curl)

if (PROJECT_IS_TOP_LEVEL) 
  add_executable(example example.cpp)
  target_link_libraries(example co_curl)
  
  add_executable(example2 example2.cpp)
  target_link_libraries(example2 co_curl)
endif()
