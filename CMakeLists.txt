cmake_minimum_required(VERSION 3.16)

project(ZZmazon LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

function(add_zzmazon_executable target source)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_17)
endfunction()

add_zzmazon_executable(zzmazon_cli src/zzmazon_cli.cpp)
add_zzmazon_executable(zzmazon_botzone src/zzmazon_botzone.cpp)

option(ZZMAZON_BUILD_GUI "Build the Qt-based ZZmazon Studio client" ON)

if(ZZMAZON_BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets Concurrent QUIET)
    if(Qt6_FOUND)
        add_library(
            zzmazon_game_engine STATIC
            src/cli_plus/amazons_game.cpp
            src/cli_plus/amazons_game.h
        )
        target_compile_features(zzmazon_game_engine PUBLIC cxx_std_17)
        target_include_directories(
            zzmazon_game_engine
            PUBLIC src/cli_plus
        )

        add_executable(
            zzmazon_cli_plus
            src/zzmazon_cli_plus.cpp
            src/cli_plus/board_widget.cpp
            src/cli_plus/board_widget.h
            src/cli_plus/main_window.cpp
            src/cli_plus/main_window.h
            src/cli_plus/resources.qrc
        )
        target_compile_features(zzmazon_cli_plus PRIVATE cxx_std_17)
        target_include_directories(zzmazon_cli_plus PRIVATE src/cli_plus)
        target_link_libraries(
            zzmazon_cli_plus
            PRIVATE
                zzmazon_game_engine
                Qt6::Widgets
                Qt6::Concurrent
        )
        set_target_properties(
            zzmazon_cli_plus
            PROPERTIES
                AUTOMOC ON
                AUTORCC ON
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )

        if(BUILD_TESTING)
            add_executable(
                cli_plus_engine_test
                tests/cli_plus_engine_test.cpp
            )
            target_link_libraries(
                cli_plus_engine_test
                PRIVATE zzmazon_game_engine
            )
            add_test(NAME cli_plus_engine COMMAND cli_plus_engine_test)

            find_package(Qt6 COMPONENTS Test QUIET)
            if(TARGET Qt6::Test)
                add_executable(
                    cli_plus_board_test
                    tests/cli_plus_board_test.cpp
                    src/cli_plus/board_widget.cpp
                    src/cli_plus/board_widget.h
                )
                target_compile_features(cli_plus_board_test PRIVATE cxx_std_17)
                target_link_libraries(
                    cli_plus_board_test
                    PRIVATE
                        zzmazon_game_engine
                        Qt6::Widgets
                        Qt6::Test
                )
                set_target_properties(
                    cli_plus_board_test
                    PROPERTIES AUTOMOC ON
                )
                add_test(NAME cli_plus_board COMMAND cli_plus_board_test)
                set_tests_properties(
                    cli_plus_board
                    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                )
            endif()
        endif()
    else()
        message(WARNING "Qt 6 was not found; zzmazon_cli_plus will not be built")
    endif()
endif()

option(ZZMAZON_BUILD_EXPERIMENTS "Build historical self-play variants" OFF)

if(ZZMAZON_BUILD_EXPERIMENTS)
    foreach(variant IN ITEMS original t1 t2 test)
        add_zzmazon_executable(
            selfplay_${variant}
            experiments/self_play/${variant}.cpp
        )
        set_target_properties(
            selfplay_${variant}
            PROPERTIES OUTPUT_NAME ${variant}
        )
    endforeach()
endif()
