file(GLOB TESTS *.c)

foreach(t ${TESTS})
    get_filename_component(name ${t} NAME_WE)    
    add_executable(test-${name} ${t} ${SRC})
    target_add_dependencies(test-${name}) # should be defined in parent dir
    add_test(${name} test-${name})
    install(TARGETS test-${name} DESTINATION bin/test)
endforeach()