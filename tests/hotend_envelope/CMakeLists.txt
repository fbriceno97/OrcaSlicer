cmake_minimum_required(VERSION 3.16)
project(OrcaHotendEnvelopeTests LANGUAGES CXX)

# Exercise the production clearance mathematics without waiting for the GUI and
# slicer dependency build. The full libslic3r suite compiles this same test file.
get_filename_component(ORCA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory("${ORCA_SOURCE_DIR}/tests/catch2" catch2)
add_executable(hotend_envelope_tests
    "${ORCA_SOURCE_DIR}/src/libslic3r/HotendEnvelope.cpp"
    "${ORCA_SOURCE_DIR}/tests/libslic3r/test_hotend_envelope.cpp")
target_compile_features(hotend_envelope_tests PRIVATE cxx_std_17)
target_include_directories(hotend_envelope_tests PRIVATE "${ORCA_SOURCE_DIR}/src")
target_link_libraries(hotend_envelope_tests PRIVATE Catch2::Catch2WithMain)
if(MSVC)
    target_compile_options(hotend_envelope_tests PRIVATE /W4 /WX)
else()
    target_compile_options(hotend_envelope_tests PRIVATE -Wall -Wextra -Werror)
endif()
enable_testing()
list(APPEND CMAKE_MODULE_PATH "${ORCA_SOURCE_DIR}/tests/catch2/extras")
include(Catch)
catch_discover_tests(hotend_envelope_tests)
