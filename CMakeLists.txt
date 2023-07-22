cmake_minimum_required(VERSION 3.21)

project(CO_CURL VERSION 1.0 LANGUAGES CXX)

if (PROJECT_IS_TOP_LEVEL) 
	if (APPLE)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3")
	endif() 
	
	add_subdirectory(examples)
endif()

add_subdirectory(include/co_curl)
