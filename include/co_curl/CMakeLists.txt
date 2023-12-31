option(CO_CURL_SHARED "Build CO_CURL library as a shared library." OFF)

if (CO_CURL_SHARED)
  add_library(co_curl SHARED)
else()
  add_library(co_curl STATIC)
endif()

find_package(CURL REQUIRED)

if (NOT ${CURL_VERSION_STRING} VERSION_GREATER_EQUAL "7.55")
	message(WARNING "Some features of libcurl are not available!")
	target_compile_definitions(co_curl PRIVATE LIBCURL_BEFORE_NEEDED)
endif()

configure_file(version.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)

target_sources(co_curl PUBLIC co_curl.hpp easy.hpp multi.hpp out_ptr.hpp scheduler.hpp task_counter.hpp promise.hpp zstring.hpp format.hpp list.hpp function.hpp all.hpp url.hpp)
target_sources(co_curl PRIVATE co_curl.cpp ${CMAKE_CURRENT_BINARY_DIR}/version.cpp curl-version.cpp easy.cpp multi.cpp list.cpp scheduler.cpp url.cpp)

target_include_directories(co_curl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. co_curl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(co_curl PUBLIC cxx_std_23)

target_link_libraries(co_curl PRIVATE CURL::libcurl)

set_target_properties(co_curl PROPERTIES 
  LINKER_LANGUAGE CXX
  VERSION ${CO_CURL_VERSION}
  SOVERSION 1.0
)
