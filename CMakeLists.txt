cmake_minimum_required(VERSION 3.21)

project(CO_CURL VERSION 1.0 LANGUAGES CXX)

add_subdirectory(include/co_curl)

if (PROJECT_IS_TOP_LEVEL) 
	add_subdirectory(examples)
endif()
