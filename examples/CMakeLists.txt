cmake_minimum_required(VERSION 3.10)
project(shm_manager_examples)

foreach (EXAMPLE_TARGET example)
    add_executable(${EXAMPLE_TARGET} ${EXAMPLE_TARGET}.cpp)
    target_compile_features(${EXAMPLE_TARGET} PUBLIC cxx_std_17)
    target_compile_options(${EXAMPLE_TARGET} PUBLIC -Wall -Wextra -pedantic -Werror)
    target_include_directories(${EXAMPLE_TARGET} PRIVATE ${SHMMANAGER_INC_DIRS})
    target_link_libraries(${EXAMPLE_TARGET} PRIVATE
            rt)
endforeach ()
