cmake_minimum_required(VERSION 3.16)
project(poker_bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(poker_lib STATIC
    src/card.cpp
    src/card_string.cpp
    src/deck.cpp
    src/game_engine.cpp
    src/hand_evaluator.cpp
    src/poker_math.cpp
    src/icm.cpp
    src/side_pot.cpp
    src/exact_equity.cpp
    src/range.cpp
    src/range_equity.cpp
    src/equity_matrix.cpp
    src/monte_carlo_detail.cpp
    src/strategy.cpp
    src/opponent_model.cpp
    src/monte_carlo.cpp
    src/state_codec.cpp
    src/fast_evaluator.cpp
    src/bot_config.cpp
    src/poker_bot_interface.cpp
    src/runout_enumerator.cpp
    src/combinatorics_exact.cpp
    src/shapley_icm.cpp
    src/icm_sensitivity.cpp
    src/tournament_duel.cpp
    src/range_inference.cpp
    src/subgame_solvers.cpp
    src/side_pot_icm.cpp
)
target_include_directories(poker_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# Required on Linux (and other ELF): static lib is linked into poker_calculations.node (SHARED).
set_target_properties(poker_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(poker_lib PUBLIC $<$<BOOL:${WIN32}>:NOMINMAX>)

if(DEFINED CMAKE_JS_INC AND NOT CMAKE_JS_INC STREQUAL "")
    execute_process(
        COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REPLACE "\n" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")
    string(REPLACE "\"" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")

    add_library(poker_calculations SHARED
        native/binding.cpp
        native/binding_register.cpp
        native/binding_common.cpp
        native/binding_numeric.cpp
        native/binding_cards.cpp
        native/binding_init.cpp
        native/binding_state.cpp
        native/binding_batch.cpp
        native/binding_range.cpp
        native/binding_combinatorics.cpp
        native/binding_cooperative_icm.cpp
        native/binding_subgame.cpp
        native/async_workers.cpp
        ${CMAKE_JS_SRC}
    )
    set_target_properties(poker_calculations PROPERTIES PREFIX "" SUFFIX ".node")
    target_include_directories(poker_calculations PRIVATE ${CMAKE_JS_INC} ${NODE_ADDON_API_DIR})
    target_link_libraries(poker_calculations PRIVATE poker_lib ${CMAKE_JS_LIB})
    if(MSVC)
        target_link_libraries(poker_calculations PRIVATE delayimp)
    endif()
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
