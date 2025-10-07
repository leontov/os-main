cmake_minimum_required(VERSION 3.16)
project(kolibri LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(KOLIBRI_ENABLE_TESTS "Enable Kolibri tests" ON)

add_library(kolibri_core
    backend/src/decimal.c
    backend/src/genome.c
    backend/src/random.c
    backend/src/formula.c
    backend/src/roy.c
    backend/src/script.c
    backend/src/net.c
    backend/src/knp_core.c
)

target_include_directories(kolibri_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/backend/include>
        $<INSTALL_INTERFACE:include>
)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(kolibri_core PRIVATE OpenSSL::Crypto m)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set_source_files_properties(backend/src/knp_core.c PROPERTIES COMPILE_OPTIONS "-ffast-math")
endif()

add_executable(kolibri_node apps/kolibri_node.c)
add_executable(ks_compiler apps/ks_compiler.c)

target_link_libraries(kolibri_node PRIVATE kolibri_core)
target_link_libraries(ks_compiler PRIVATE kolibri_core)

if(KOLIBRI_ENABLE_TESTS)
    enable_testing()
    add_executable(kolibri_tests
        tests/test_main.c
        tests/test_decimal.c
        tests/test_genome.c
        tests/test_formula.c
        tests/test_roy.c
        tests/test_script.c
        tests/test_net.c
        tests/test_knp_core.c
    )
    target_link_libraries(kolibri_tests PRIVATE kolibri_core Threads::Threads)
    add_test(NAME kolibri_tests COMMAND kolibri_tests)

    configure_file(tests/ks_compiler_roundtrip.cmake
                   ${CMAKE_CURRENT_BINARY_DIR}/ks_compiler_roundtrip.cmake
                   @ONLY)
    add_test(NAME ks_compiler_roundtrip
             COMMAND ${CMAKE_COMMAND}
                     -Dks_compiler=$<TARGET_FILE:ks_compiler>
                     -P ${CMAKE_CURRENT_BINARY_DIR}/ks_compiler_roundtrip.cmake)
endif()

# === Kolibri digits module wiring ===
if(TARGET kolibri_core)
  target_sources(kolibri_core PRIVATE
    backend/src/digits.c
    backend/src/wasm_bridge.c
  )
endif()

# (опционально) добавим минимальные тесты по digits
if(TARGET kolibri_tests)
  target_sources(kolibri_tests PRIVATE tests/test_digits.c)
endif()

set(KOLIBRI_WASM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/src/decimal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/src/digits.c
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/src/formula.c
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/src/random.c
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/src/script.c
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/src/wasm_bridge.c
)

set(KOLIBRI_WASM_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build/wasm)
set(KOLIBRI_WASM_ARTIFACTS
    ${KOLIBRI_WASM_OUTPUT_DIR}/kolibri.wasm
    ${KOLIBRI_WASM_OUTPUT_DIR}/kolibri.wasm.sha256
    ${KOLIBRI_WASM_OUTPUT_DIR}/kolibri.wasm.txt
)

add_custom_target(kolibri_wasm
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_wasm.sh
    BYPRODUCTS ${KOLIBRI_WASM_ARTIFACTS}
    DEPENDS ${KOLIBRI_WASM_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_wasm.sh
    COMMENT "Building kolibri WebAssembly artifacts"
    VERBATIM
)
