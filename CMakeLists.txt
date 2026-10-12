cmake_minimum_required(VERSION 3.5)
project(taktful VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(Taktful_BUILD_CLI "Build CLI tools" OFF)
option(Taktful_BUILD_TESTS "Build unit and integration tests" OFF)
option(Taktful_USE_KISSFFT "Use vendored kiss_fft instead of in-house FFT" OFF)
option(Taktful_BUILD_STATIC "Build static library" ON)
option(Taktful_ENABLE_NEON "ARM NEON kernels for FFT, magnitude, window, RMS" ON)

set(TAKTFUL_SOURCES
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

if(Taktful_BUILD_STATIC)
    add_library(taktful STATIC ${TAKTFUL_SOURCES})
else()
    add_library(taktful SHARED ${TAKTFUL_SOURCES})
endif()

add_library(taktful::taktful ALIAS taktful)

target_include_directories(taktful
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/data
)

if(Taktful_USE_KISSFFT)
    target_compile_definitions(taktful PRIVATE TAKTFUL_USE_KISSFFT=1)
    target_include_directories(taktful PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/kiss_fft)
endif()

if(Taktful_ENABLE_NEON)
    target_compile_definitions(taktful PRIVATE TAKTFUL_ENABLE_NEON=1)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|ARM)" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|AARCH64")
        if(NOT MSVC)
            target_compile_options(taktful PRIVATE -mfpu=neon)
        endif()
    endif()
else()
    target_compile_definitions(taktful PRIVATE TAKTFUL_ENABLE_NEON=0)
endif()

if(MSVC)
    target_compile_options(taktful PRIVATE /W3)
else()
    target_compile_options(taktful PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(taktful PROPERTIES
    OUTPUT_NAME taktful
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

if(Taktful_BUILD_CLI)
    add_executable(taktful_cli cli/taktful.cpp)
    target_link_libraries(taktful_cli PRIVATE taktful)
    target_include_directories(taktful_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)
    set_target_properties(taktful_cli PROPERTIES OUTPUT_NAME taktful)

    add_executable(analyze_wav cli/analyze_wav.cpp)
    target_link_libraries(analyze_wav PRIVATE taktful)
    target_include_directories(analyze_wav PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)

    add_executable(bpm_probe cli/bpm_probe.cpp)
    target_link_libraries(bpm_probe PRIVATE taktful)
    target_include_directories(bpm_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)

    add_executable(key_probe cli/key_probe.cpp)
    target_link_libraries(key_probe PRIVATE taktful)
    target_include_directories(key_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)

    add_executable(run_benchmark cli/run_benchmark.cpp)
    target_link_libraries(run_benchmark PRIVATE taktful)
    target_include_directories(run_benchmark PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/cli
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit
    )

    install(TARGETS taktful_cli analyze_wav bpm_probe key_probe run_benchmark RUNTIME DESTINATION bin)
endif()

if(Taktful_BUILD_TESTS)
    enable_testing()

    add_executable(taktful_unit_tests tests/unit/test_all.cpp)
    target_link_libraries(taktful_unit_tests PRIVATE taktful)
    target_include_directories(taktful_unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit
    )
    add_test(NAME unit COMMAND taktful_unit_tests)

    add_executable(taktful_integration_tests tests/integration/test_integration.cpp)
    target_link_libraries(taktful_integration_tests PRIVATE taktful)
    target_include_directories(taktful_integration_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration
    )
    add_test(NAME integration COMMAND taktful_integration_tests)
endif()

install(TARGETS taktful
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY include/taktful DESTINATION include)
