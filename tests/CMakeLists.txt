add_executable(lightengine_tests application_test.cpp)
target_link_libraries(lightengine_tests PRIVATE lightengine::core)

if(MSVC)
    target_compile_options(lightengine_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(
        lightengine_tests
        PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
endif()

add_test(NAME lightengine.application COMMAND lightengine_tests)
