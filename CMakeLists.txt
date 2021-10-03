cmake_minimum_required(VERSION 3.15)
include(FetchContent)

project(RayTracing)

string(FIND "${CMAKE_CXX_COMPILER}" "clang++" ANTON_COMPILER_CLANGPP)
if(ANTON_COMPILER_CLANGPP GREATER -1)
    set(ANTON_COMPILER_CLANGPP ON)
else()
    set(ANTON_COMPILER_CLANGPP OFF)
endif()

if(ANTON_COMPILER_CLANGPP)
    set(ANTON_COMPILER_CLANGPP
        # -march=native
        -fno-math-errno
        -fno-char8_t # Yea, just no.
        -fno-rtti 
        -fno-exceptions
        -Wall -Wextra -pedantic
        -ferror-limit=1
        
        -Werror=return-type
        -Werror=uninitialized
        -Werror=reorder-init-list

        -Wnon-virtual-dtor
        -Wnewline-eof
    )
elseif(MSVC)
    set(ANTON_COMPILER_CLANGPP
        /GF # Kill duplicated strings
    )
endif()

# Add anton_core
FetchContent_Declare(
    anton_core
    GIT_REPOSITORY https://github.com/kociap/anton_core.git
)
FetchContent_MakeAvailable(anton_core)

add_library(anton_import)
set_target_properties(anton_import PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS OFF)
target_compile_options(anton_import PRIVATE ${ANTON_COMPILER_CLANGPP})
target_link_libraries(anton_import PUBLIC anton_core)
target_include_directories(anton_import PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/public" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_sources(anton_import 
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private/obj.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/public/anton/import.hpp"
)
