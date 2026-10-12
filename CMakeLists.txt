cmake_minimum_required(VERSION 3.5)
project(keeb VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KeeB_BUILD_CLI "Build CLI tools" OFF)
option(KeeB_BUILD_TESTS "Build unit and integration tests" OFF)
option(KeeB_USE_KISSFFT "Use vendored kiss_fft instead of in-house FFT" OFF)
option(KeeB_BUILD_STATIC "Build static library" ON)

set(KEEB_SOURCES
    src/wav_io.cpp
    src/dsp.cpp
    src/fft.cpp
    src/onset.cpp
    src/tempo.cpp
    src/chroma.cpp
    src/key.cpp
    src/api.cpp
    src/stream.cpp
    src/c_api.cpp
)

if(KeeB_BUILD_STATIC)
    add_library(keeb STATIC ${KEEB_SOURCES})
else()
    add_library(keeb SHARED ${KEEB_SOURCES})
endif()

add_library(keeb::keeb ALIAS keeb)

target_include_directories(keeb
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/data
)

if(KeeB_USE_KISSFFT)
    target_compile_definitions(keeb PRIVATE KEEB_USE_KISSFFT=1)
    target_include_directories(keeb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/kiss_fft)
endif()

if(MSVC)
    target_compile_options(keeb PRIVATE /W3)
else()
    target_compile_options(keeb PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(keeb PROPERTIES
    OUTPUT_NAME keeb
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

if(KeeB_BUILD_CLI)
    add_executable(keeb_cli cli/keeb.cpp)
    target_link_libraries(keeb_cli PRIVATE keeb)
    target_include_directories(keeb_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)
    set_target_properties(keeb_cli PROPERTIES OUTPUT_NAME keeb)

    add_executable(analyze_wav cli/analyze_wav.cpp)
    target_link_libraries(analyze_wav PRIVATE keeb)
    target_include_directories(analyze_wav PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)

    add_executable(bpm_probe cli/bpm_probe.cpp)
    target_link_libraries(bpm_probe PRIVATE keeb)
    target_include_directories(bpm_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)

    add_executable(key_probe cli/key_probe.cpp)
    target_link_libraries(key_probe PRIVATE keeb)
    target_include_directories(key_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)

    add_executable(run_benchmark cli/run_benchmark.cpp)
    target_link_libraries(run_benchmark PRIVATE keeb)
    target_include_directories(run_benchmark PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/cli
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit
    )

    install(TARGETS keeb_cli analyze_wav bpm_probe key_probe run_benchmark RUNTIME DESTINATION bin)
endif()

if(KeeB_BUILD_TESTS)
    enable_testing()

    add_executable(keeb_unit_tests tests/unit/test_all.cpp)
    target_link_libraries(keeb_unit_tests PRIVATE keeb)
    target_include_directories(keeb_unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit
    )
    add_test(NAME unit COMMAND keeb_unit_tests)

    add_executable(keeb_integration_tests tests/integration/test_integration.cpp)
    target_link_libraries(keeb_integration_tests PRIVATE keeb)
    target_include_directories(keeb_integration_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration
    )
    add_test(NAME integration COMMAND keeb_integration_tests)
endif()

install(TARGETS keeb
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY include/keeb DESTINATION include)
