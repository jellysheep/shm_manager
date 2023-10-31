cmake_minimum_required(VERSION 3.10)
project(shm_manager)

include(FetchContent)

FetchContent_Declare(
        pybind
        GIT_REPOSITORY "https://github.com/pybind/pybind11"
        GIT_TAG "v2.10.0"
)

message(STATUS "Loading pybind ...")
FetchContent_MakeAvailable(pybind)

set(SHMMANAGER_INC_DIRS
        ${PROJECT_SOURCE_DIR}/src)

pybind11_add_module(shm_manager src/shm_manager_pybind.cpp)
target_compile_features(shm_manager PUBLIC cxx_std_17)
target_compile_options(shm_manager PUBLIC -Wall -Wextra -pedantic -Werror -Wno-error=terminate)
target_include_directories(shm_manager PRIVATE ${SHMMANAGER_INC_DIRS})
target_link_libraries(shm_manager PRIVATE
        rt)
set_target_properties(shm_manager PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

add_subdirectory(examples)
