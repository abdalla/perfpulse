add_executable(perfpulse_tests
    test_main.cpp
    bottleneck_analyzer_tests.cpp
    config_tests.cpp
    energy_analyzer_tests.cpp
    frame_statistics_tests.cpp
    json_tests.cpp
    report_tests.cpp
    stutter_detector_tests.cpp
)
target_link_libraries(perfpulse_tests PRIVATE perfpulse_core)
perfpulse_set_compiler_options(perfpulse_tests)

add_test(NAME perfpulse_tests COMMAND perfpulse_tests)

# Stand-in game used for manual end-to-end runs (not part of ctest).
add_executable(perfpulse_fake_game WIN32 fake_game/fake_game.cpp)
target_link_libraries(perfpulse_fake_game PRIVATE d3d11 dxgi user32 shell32)
perfpulse_set_compiler_options(perfpulse_fake_game)
