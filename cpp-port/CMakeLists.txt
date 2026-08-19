cmake_minimum_required(VERSION 3.20)
project(panpath_audit_cpp VERSION 0.1.0 LANGUAGES C CXX)

option(PANPATH_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)

add_library(panpath_core
    src/audit.cpp
    src/cli.cpp
    src/digests.cpp
    src/input.cpp
    src/memory.cpp
    src/report.cpp
    src/graph.cpp
    src/sequence.cpp
    src/spool.cpp
    src/statistics.cpp
    vendor/blake3/blake3.c
    vendor/blake3/blake3_dispatch.c
    vendor/blake3/blake3_portable.c
    vendor/sha256/sha-256.c
)
target_include_directories(panpath_core PUBLIC include PRIVATE vendor/blake3 vendor/sha256 vendor)
target_compile_definitions(panpath_core PRIVATE
    BLAKE3_NO_SSE2 BLAKE3_NO_SSE41 BLAKE3_NO_AVX2 BLAKE3_NO_AVX512
    BLAKE3_USE_NEON=0
    PANPATH_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(panpath_core PUBLIC ZLIB::ZLIB)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(panpath_core PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>)
    if(PANPATH_SANITIZERS)
        target_compile_options(panpath_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(panpath_core PUBLIC -fsanitize=address,undefined)
    endif()
endif()

add_executable(panpath-audit-cpp src/main.cpp)
target_link_libraries(panpath-audit-cpp PRIVATE panpath_core)
target_compile_definitions(panpath-audit-cpp PRIVATE PANPATH_VERSION="${PROJECT_VERSION}")

install(TARGETS panpath-audit-cpp RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
    add_executable(panpath-core-tests tests/core_tests.cpp)
    target_link_libraries(panpath-core-tests PRIVATE panpath_core)
    add_test(NAME panpath-core-tests COMMAND panpath-core-tests)
    set(RUST_BINARY "${CMAKE_CURRENT_SOURCE_DIR}/../target/release/panpath-audit" CACHE FILEPATH "Rust reference binary")
    if(EXISTS "${RUST_BINARY}")
        foreach(case identical divergent walk)
            if(case STREQUAL "divergent")
                set(fasta divergent.fa)
                set(gfa identical.gfa)
            else()
                set(fasta ${case}.fa)
                set(gfa ${case}.gfa)
            endif()
            add_test(NAME conformance-${case}
                COMMAND ${CMAKE_COMMAND}
                    -DRUST=${RUST_BINARY}
                    -DCPP=$<TARGET_FILE:panpath-audit-cpp>
                    -DFASTA=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/${fasta}
                    -DGFA=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/${gfa}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance.cmake)
        endforeach()
        foreach(format human json tsv)
            add_test(NAME conformance-comprehensive-${format}
                COMMAND ${CMAKE_COMMAND}
                    -DRUST=${RUST_BINARY}
                    -DCPP=$<TARGET_FILE:panpath-audit-cpp>
                    -DFASTA=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/divergent.fa
                    -DGFA=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/identical.gfa
                    -DCOMPREHENSIVE=ON
                    -DFORMAT=${format}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance.cmake)
        endforeach()
        foreach(format human json tsv)
            add_test(NAME conformance-invalid-${format}
                COMMAND ${CMAKE_COMMAND}
                    -DRUST=${RUST_BINARY}
                    -DCPP=$<TARGET_FILE:panpath-audit-cpp>
                    -DFASTA=missing-source.fa
                    -DGFA=missing-graph.gfa
                    -DFORMAT=${format}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance.cmake)
        endforeach()
    endif()
endif()
