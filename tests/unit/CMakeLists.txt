cmake_minimum_required(VERSION 3.16)

# ---------------------------------------------------------------------------
# Unit test: band-math placeholder
# ---------------------------------------------------------------------------
add_executable(magn_test_unit_placeholder
    placeholder_test.cpp
)

target_include_directories(magn_test_unit_placeholder
    PRIVATE
        # CMAKE_SOURCE_DIR == native/ (cmake -S native); src/ contains core/ and render/
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(magn_test_unit_placeholder PRIVATE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(magn_test_unit_placeholder PRIVATE
        -Wall -Wextra -Wpedantic -Wno-unused-parameter
    )
elseif(MSVC)
    target_compile_options(magn_test_unit_placeholder PRIVATE /W4 /wd4100)
endif()

add_test(NAME unit_placeholder COMMAND magn_test_unit_placeholder)
