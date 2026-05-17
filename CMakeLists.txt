cmake_minimum_required(VERSION 3.16)
project(poker_bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# cmake-js passes CMAKE_JS_INC; skip heavy dev targets so npm install stays lean.
if(DEFINED CMAKE_JS_INC AND NOT CMAKE_JS_INC STREQUAL "")
    set(_poker_build_tests_default OFF)
else()
    set(_poker_build_tests_default ON)
endif()

option(POKER_BUILD_TESTS "Build unit tests" ${_poker_build_tests_default})

add_library(poker_lib STATIC
    src/card.cpp
    src/deck.cpp
    src/game_engine.cpp
    src/hand_evaluator.cpp
    src/poker_math.cpp
    src/icm.cpp
    src/side_pot.cpp
    src/exact_equity.cpp
    src/strategy.cpp
    src/opponent_model.cpp
    src/monte_carlo.cpp
    src/bot_config.cpp
    src/poker_bot_interface.cpp
)
target_include_directories(poker_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# Required on Linux (and other ELF): static lib is linked into poker_calculations.node (SHARED).
set_target_properties(poker_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(poker_lib PUBLIC $<$<BOOL:${WIN32}>:NOMINMAX>)

if(POKER_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_executable(poker_tests
        tests/test_deck.cpp
        tests/test_game_engine.cpp
        tests/test_hand_evaluator.cpp
        tests/test_poker_math.cpp
        tests/test_icm.cpp
        tests/test_side_pot.cpp
        tests/test_exact_equity.cpp
        tests/test_strategy.cpp
        tests/test_opponent_model.cpp
        tests/test_monte_carlo.cpp
        tests/test_bot_config.cpp
    )
    target_link_libraries(poker_tests PRIVATE poker_lib GTest::gtest_main)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(poker_tests PRIVATE stdc++fs)
    endif()
    include(GoogleTest)
    gtest_discover_tests(poker_tests)
endif()

if(DEFINED CMAKE_JS_INC AND NOT CMAKE_JS_INC STREQUAL "")
    execute_process(
        COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REPLACE "\n" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")
    string(REPLACE "\"" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")

    add_library(poker_calculations SHARED native/binding.cpp ${CMAKE_JS_SRC})
    set_target_properties(poker_calculations PROPERTIES PREFIX "" SUFFIX ".node")
    target_include_directories(poker_calculations PRIVATE ${CMAKE_JS_INC} ${NODE_ADDON_API_DIR})
    target_link_libraries(poker_calculations PRIVATE poker_lib ${CMAKE_JS_LIB})
    target_compile_definitions(poker_calculations PRIVATE NAPI_VERSION=8)
    target_compile_definitions(poker_calculations PRIVATE $<$<BOOL:${WIN32}>:NOMINMAX>)
    # Node-API on Windows: cmake-js skips full Node download; generate node.lib from node-api-headers .def (cmake-js README).
    if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
        execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
    endif()
    # Bundle libstdc++/libgcc into the .node so hosts with older GLIBCXX (e.g. many Linux serverless images) still load it.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(poker_calculations PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()
