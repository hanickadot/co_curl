function(add_example NAME)
	add_executable(${NAME} ${NAME}.cpp)
	target_link_libraries(${NAME} co_curl)
endfunction()

add_example(hello-world)
add_example(lazy-perform)
add_example(multiple-awaits-on-one-object)
