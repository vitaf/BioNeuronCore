# Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
# E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
# Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

cmake_minimum_required(VERSION 3.20)
project(BioNeuronCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# ==================== ДИРЕКТОРИИ ====================
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/plugins/builtin)

# ==================== CATCH2 v2 (single-header) ====================
include(FetchContent)
FetchContent_Declare(
    Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v2.13.10.tar.gz
)
FetchContent_GetProperties(Catch2)
if(NOT Catch2_POPULATED)
    FetchContent_Populate(Catch2)
endif()
include_directories(${catch2_SOURCE_DIR}/single_include)

# ==================== БИБЛИОТЕКА BIOCORE (CPU, host compiler) ====================
# Defined BEFORE SYCL detection so it uses the host GCC, not Intel dpcpp.
# This avoids Intel-specific symbols (_intel_fast_memset, __svml_logf4) which
# would prevent linking with GCC-built executables like BioNeuronStudio.
add_library(biocore STATIC
    src/hh_biological.cpp
    src/receptors_biological.cpp
    src/bion_loader_host.cpp
    src/plugin_manager.cpp
    src/simulation_host.cpp
    src/consolidation.cpp
    src/dispatch_generated.cpp
)
target_compile_options(biocore PRIVATE -O2 -Wall -fPIC)

# Дополнительная сборка GCC-версии для линковки с GCC-проектами (BioNeuronStudio).
# biocore собирается Intel-компилятором (если SYCL найден), а Intel-объекты
# содержат ссылки на _intel_fast_memset и __svml_logf4, недоступные в GCC.
add_custom_target(biocore_gcc ALL
    COMMAND g++ -std=c++17 -fPIC -O2 -Wall
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/src/hh_biological.cpp
            -o ${CMAKE_BINARY_DIR}/hh_biological_gcc.o
    COMMAND g++ -std=c++17 -fPIC -O2 -Wall
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/src/receptors_biological.cpp
            -o ${CMAKE_BINARY_DIR}/receptors_biological_gcc.o
    COMMAND g++ -std=c++17 -fPIC -O2 -Wall
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/src/bion_loader_host.cpp
            -o ${CMAKE_BINARY_DIR}/bion_loader_host_gcc.o
    COMMAND g++ -std=c++17 -fPIC -O2 -Wall
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/src/plugin_manager.cpp
            -o ${CMAKE_BINARY_DIR}/plugin_manager_gcc.o
    COMMAND g++ -std=c++17 -fPIC -O2 -Wall
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/src/simulation_host.cpp
            -o ${CMAKE_BINARY_DIR}/simulation_host_gcc.o
    COMMAND g++ -std=c++17 -fPIC -O2 -Wall
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/src/consolidation.cpp
            -o ${CMAKE_BINARY_DIR}/consolidation_gcc.o
    COMMAND g++ -std=c++17 -fPIC -O2 -Wall
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/src/dispatch_generated.cpp
            -o ${CMAKE_BINARY_DIR}/dispatch_generated_gcc.o
    COMMAND ar rcs ${CMAKE_BINARY_DIR}/libbiocore_gcc.a
            ${CMAKE_BINARY_DIR}/hh_biological_gcc.o
            ${CMAKE_BINARY_DIR}/receptors_biological_gcc.o
            ${CMAKE_BINARY_DIR}/bion_loader_host_gcc.o
            ${CMAKE_BINARY_DIR}/plugin_manager_gcc.o
            ${CMAKE_BINARY_DIR}/simulation_host_gcc.o
            ${CMAKE_BINARY_DIR}/consolidation_gcc.o
            ${CMAKE_BINARY_DIR}/dispatch_generated_gcc.o
    COMMENT "Building libbiocore_gcc.a with GCC (for Studio linking)"
)
set_target_properties(biocore_gcc PROPERTIES FOLDER "host")

# ==================== BIOCORE DAEMON (executable) ====================
# Standalone process: listens on Unix socket + TCP, runs simulation independently.
# Built with GCC (not DPC++) to avoid Intel-specific symbols.
# Uses the same GCC-compiled .o files as biocore_gcc.
add_custom_target(biocore-daemon ALL
    COMMAND g++ -std=c++17 -O2 -Wall
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/src/daemon_main.cpp
            -o ${CMAKE_BINARY_DIR}/daemon_main.o
    COMMAND g++ -std=c++17 -O2 -Wall
            -I${CMAKE_SOURCE_DIR}/include
            -I${CMAKE_SOURCE_DIR}/../BioNeuronOrchestrator
            -c ${CMAKE_SOURCE_DIR}/src/daemon_config.cpp
            -o ${CMAKE_BINARY_DIR}/daemon_config.o
    COMMAND g++ -std=c++17 -O2 -Wall
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/src/daemon_server.cpp
            -o ${CMAKE_BINARY_DIR}/daemon_server.o
    COMMAND g++ -std=c++17 -O2 -Wall
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/src/daemon_state.cpp
            -o ${CMAKE_BINARY_DIR}/daemon_state.o
    COMMAND g++
            ${CMAKE_BINARY_DIR}/daemon_main.o
            ${CMAKE_BINARY_DIR}/daemon_config.o
            ${CMAKE_BINARY_DIR}/daemon_server.o
            ${CMAKE_BINARY_DIR}/daemon_state.o
            ${CMAKE_BINARY_DIR}/hh_biological_gcc.o
            ${CMAKE_BINARY_DIR}/receptors_biological_gcc.o
            ${CMAKE_BINARY_DIR}/bion_loader_host_gcc.o
            ${CMAKE_BINARY_DIR}/plugin_manager_gcc.o
            ${CMAKE_BINARY_DIR}/simulation_host_gcc.o
            ${CMAKE_BINARY_DIR}/consolidation_gcc.o
            ${CMAKE_BINARY_DIR}/dispatch_generated_gcc.o
            -o ${CMAKE_BINARY_DIR}/biocore-daemon
            -lpthread -ldl -rdynamic
    DEPENDS biocore_gcc
            ${CMAKE_SOURCE_DIR}/src/daemon_main.cpp
            ${CMAKE_SOURCE_DIR}/src/daemon_config.cpp
            ${CMAKE_SOURCE_DIR}/src/daemon_server.cpp
            ${CMAKE_SOURCE_DIR}/src/daemon_state.cpp
            ${CMAKE_SOURCE_DIR}/src/daemon_log.h
    COMMENT "Building biocore-daemon (GCC standalone server)"
)
set_target_properties(biocore-daemon PROPERTIES FOLDER "host")

# Copy example config to build dir
configure_file(${CMAKE_SOURCE_DIR}/biocore.conf.example ${CMAKE_BINARY_DIR}/biocore.conf.example COPYONLY)

# ==================== ВСТРОЕННЫЕ ПЛАГИНЫ (.so) ====================
# Сборка всех 63 модификаторов как отдельных .so через GCC.
# Важно: плагины компилируются GCC (не Intel dpcpp), т.к. загружаются
# через dlopen из Studio (GCC-сборка). Intel-сборка добавляет зависимости
# от libsycl/svml, которые недоступны в GCC-окружении.
set(PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugins/builtin")
file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})

file(GLOB PLUGIN_SOURCES "${CMAKE_SOURCE_DIR}/plugins/builtin/mod_*.cpp")
foreach(PLUGIN_SRC ${PLUGIN_SOURCES})
    get_filename_component(PLUGIN_NAME ${PLUGIN_SRC} NAME_WE)
    set(PLUGIN_SO "${PLUGIN_OUTPUT_DIR}/${PLUGIN_NAME}.so")
    add_custom_command(
        OUTPUT ${PLUGIN_SO}
        COMMAND g++ -std=c++17 -fPIC -shared -O2 -Wall
                -I${CMAKE_SOURCE_DIR}/include
                -I${CMAKE_SOURCE_DIR}/plugins/builtin
                -o ${PLUGIN_SO}
                ${PLUGIN_SRC}
        DEPENDS ${PLUGIN_SRC}
                ${CMAKE_SOURCE_DIR}/include/types.h
                ${CMAKE_SOURCE_DIR}/include/modifier_interface.h
                ${CMAKE_SOURCE_DIR}/include/plugin_manager.h
                ${CMAKE_SOURCE_DIR}/plugins/builtin/plugin_helpers.h
        COMMENT "Building plugin: ${PLUGIN_NAME}.so"
    )
    list(APPEND PLUGIN_OUTPUTS ${PLUGIN_SO})
endforeach()

# Копируем .desc файлы и plugin_helpers.h в выходную директорию
file(GLOB PLUGIN_DESC_FILES "${CMAKE_SOURCE_DIR}/plugins/builtin/mod_*.desc")
foreach(DESC_FILE ${PLUGIN_DESC_FILES})
    get_filename_component(DESC_NAME ${DESC_FILE} NAME)
    set(DESC_OUT "${PLUGIN_OUTPUT_DIR}/${DESC_NAME}")
    add_custom_command(
        OUTPUT ${DESC_OUT}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DESC_FILE} ${DESC_OUT}
        DEPENDS ${DESC_FILE}
        COMMENT "Copying ${DESC_NAME}"
    )
    list(APPEND PLUGIN_OUTPUTS ${DESC_OUT})
endforeach()

# Копируем .info справочные файлы в выходную директорию
file(GLOB PLUGIN_INFO_FILES "${CMAKE_SOURCE_DIR}/plugins/builtin/mod_*.info")
foreach(INFO_FILE ${PLUGIN_INFO_FILES})
    get_filename_component(INFO_NAME ${INFO_FILE} NAME)
    set(INFO_OUT "${PLUGIN_OUTPUT_DIR}/${INFO_NAME}")
    add_custom_command(
        OUTPUT ${INFO_OUT}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${INFO_FILE} ${INFO_OUT}
        DEPENDS ${INFO_FILE}
        COMMENT "Copying ${INFO_NAME}"
    )
    list(APPEND PLUGIN_OUTPUTS ${INFO_OUT})
endforeach()

# genes.info — трёхъязычный справочник генов
set(GENES_INFO_SRC "${CMAKE_SOURCE_DIR}/plugins/builtin/genes.info")
set(GENES_INFO_OUT "${PLUGIN_OUTPUT_DIR}/genes.info")
add_custom_command(
    OUTPUT ${GENES_INFO_OUT}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GENES_INFO_SRC} ${GENES_INFO_OUT}
    DEPENDS ${GENES_INFO_SRC}
    COMMENT "Copying genes.info"
)
list(APPEND PLUGIN_OUTPUTS ${GENES_INFO_OUT})

set(HELPER_OUT "${PLUGIN_OUTPUT_DIR}/plugin_helpers.h")
add_custom_command(
    OUTPUT ${HELPER_OUT}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/plugins/builtin/plugin_helpers.h ${HELPER_OUT}
    DEPENDS ${CMAKE_SOURCE_DIR}/plugins/builtin/plugin_helpers.h
    COMMENT "Copying plugin_helpers.h"
)
list(APPEND PLUGIN_OUTPUTS ${HELPER_OUT})

# Агрегатная цель для всех плагинов
add_custom_target(builtin_plugins ALL DEPENDS ${PLUGIN_OUTPUTS})
set_target_properties(builtin_plugins PROPERTIES FOLDER "plugins")

# ==================== ТЕСТЫ (CPU, host compiler) ====================
add_executable(test_hh tests/test_hh.cpp)
target_link_libraries(test_hh biocore)
target_compile_options(test_hh PRIVATE -O2 -Wall)

add_executable(test_synapses tests/test_synapses.cpp)
target_link_libraries(test_synapses biocore)
target_compile_options(test_synapses PRIVATE -O2 -Wall)

add_executable(test_cav33 tests/test_cav33.cpp)
target_link_libraries(test_cav33 biocore)
target_compile_options(test_cav33 PRIVATE -O2 -Wall)

add_executable(test_mod177_182 tests/test_mod177_182.cpp)
target_link_libraries(test_mod177_182 biocore)
target_compile_options(test_mod177_182 PRIVATE -O2 -Wall)

add_executable(test_rod_phototransduction tests/test_rod_phototransduction.cpp)
target_link_libraries(test_rod_phototransduction biocore)
target_compile_options(test_rod_phototransduction PRIVATE -O2 -Wall)

enable_testing()
add_executable(test_plugins tests/test_plugins.cpp)
target_link_libraries(test_plugins biocore)
target_compile_options(test_plugins PRIVATE -O2 -Wall)

add_executable(test_plugin_fallback_parity tests/test_plugin_fallback_parity.cpp)
target_link_libraries(test_plugin_fallback_parity biocore)
target_compile_options(test_plugin_fallback_parity PRIVATE -O2 -Wall)

add_executable(test_nb_receptors tests/test_nb_receptors.cpp)
target_link_libraries(test_nb_receptors biocore)
target_compile_options(test_nb_receptors PRIVATE -O2 -Wall)

add_executable(test_molecular_cascades tests/test_molecular_cascades.cpp)
target_link_libraries(test_molecular_cascades biocore)
target_compile_options(test_molecular_cascades PRIVATE -O2 -Wall)
add_test(NAME test_molecular_cascades COMMAND test_molecular_cascades)

add_executable(test_sensors tests/test_sensors.cpp)
target_link_libraries(test_sensors biocore)
target_compile_options(test_sensors PRIVATE -O2 -Wall)
add_test(NAME test_sensors COMMAND test_sensors)

add_executable(test_structural tests/test_structural.cpp)
target_link_libraries(test_structural biocore)
target_compile_options(test_structural PRIVATE -O2 -Wall)
add_test(NAME test_structural COMMAND test_structural)

add_executable(test_simulation_host tests/test_simulation_host.cpp)
target_link_libraries(test_simulation_host biocore)
target_compile_options(test_simulation_host PRIVATE -O2 -Wall)

add_executable(test_bion_loader_host tests/test_bion_loader_host.cpp)
target_link_libraries(test_bion_loader_host biocore)
target_compile_options(test_bion_loader_host PRIVATE -O2 -Wall)

add_executable(test_consolidation tests/test_consolidation.cpp)
target_link_libraries(test_consolidation biocore)
target_compile_options(test_consolidation PRIVATE -O2 -Wall)
add_test(NAME test_consolidation COMMAND test_consolidation)

add_executable(test_all_modifiers tests/test_all_modifiers.cpp)
target_link_libraries(test_all_modifiers biocore)
target_compile_options(test_all_modifiers PRIVATE -O2 -Wall)

add_executable(test_diffusion tests/test_diffusion.cpp)
target_link_libraries(test_diffusion biocore)
target_compile_options(test_diffusion PRIVATE -O2 -Wall)

add_executable(test_stimuli tests/test_stimuli.cpp)
target_link_libraries(test_stimuli biocore)
target_compile_options(test_stimuli PRIVATE -O2 -Wall)

add_executable(test_delay_buffer tests/test_delay_buffer.cpp)
target_link_libraries(test_delay_buffer biocore)
target_compile_options(test_delay_buffer PRIVATE -O2 -Wall)

add_executable(test_gpu_backend tests/test_gpu_backend.cpp)
target_link_libraries(test_gpu_backend biocore dl)
target_compile_options(test_gpu_backend PRIVATE -O2 -Wall)

# Dual-GPU ABI tests
add_executable(test_gpu_dual_abi tests/test_gpu_dual_abi.cpp)
target_link_libraries(test_gpu_dual_abi dl)
target_compile_options(test_gpu_dual_abi PRIVATE -O2 -Wall)

# Daemon dual-GPU tests (требует DaemonServer, daemon_config, daemon_state)
add_executable(test_daemon_dual_gpu tests/test_daemon_dual_gpu.cpp
    src/daemon_server.cpp src/daemon_config.cpp src/daemon_state.cpp
    src/simulation_host.cpp src/bion_loader_host.cpp src/plugin_manager.cpp
    src/hh_biological.cpp src/receptors_biological.cpp src/consolidation.cpp
    src/dispatch_generated.cpp)
target_include_directories(test_daemon_dual_gpu PRIVATE
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../BioNeuronOrchestrator
    ${catch2_SOURCE_DIR}/single_include)
target_link_libraries(test_daemon_dual_gpu dl pthread)
target_link_options(test_daemon_dual_gpu PRIVATE -rdynamic)
target_compile_options(test_daemon_dual_gpu PRIVATE -O2 -Wall)

# GPU regression: no crash on load/enumerate/create
add_executable(test_gpu_no_crash tests/test_gpu_no_crash.cpp)
target_link_libraries(test_gpu_no_crash dl)
target_compile_options(test_gpu_no_crash PRIVATE -O2 -Wall)

# CUDA parity test — GCC only, dlopen libbiocore_gpu_backend_cuda.so
add_executable(test_cpu_gpu_parity_cuda tests/test_cpu_gpu_parity_cuda.cpp)
target_link_libraries(test_cpu_gpu_parity_cuda biocore dl)
target_compile_options(test_cpu_gpu_parity_cuda PRIVATE -O2 -Wall)
add_test(NAME test_cpu_gpu_parity_cuda COMMAND test_cpu_gpu_parity_cuda)

# CUDA integration test
add_executable(test_cuda_integration tests/test_cuda_integration.cpp)
target_link_libraries(test_cuda_integration biocore dl)
target_compile_options(test_cuda_integration PRIVATE -O2 -Wall)
add_test(NAME test_cuda_integration COMMAND test_cuda_integration)

# CUDA performance, security, chaos tests
add_executable(test_cuda_performance tests/test_cuda_performance.cpp)
target_link_libraries(test_cuda_performance biocore dl)
target_compile_options(test_cuda_performance PRIVATE -O2 -Wall)
add_test(NAME test_cuda_performance COMMAND test_cuda_performance)

# All-modifier parity test — все 121 тип на CPU + GPU
add_executable(test_all_modifiers_parity tests/test_all_modifiers_parity.cpp)
target_link_libraries(test_all_modifiers_parity biocore dl)
target_compile_options(test_all_modifiers_parity PRIVATE -O2 -Wall)
add_test(NAME test_all_modifiers_parity COMMAND test_all_modifiers_parity)

add_executable(test_cuda_security tests/test_cuda_security.cpp)
target_link_libraries(test_cuda_security dl)
target_compile_options(test_cuda_security PRIVATE -O2 -Wall)
add_test(NAME test_cuda_security COMMAND test_cuda_security)

add_executable(test_cuda_chaos tests/test_cuda_chaos.cpp)
target_link_libraries(test_cuda_chaos dl)
target_compile_options(test_cuda_chaos PRIVATE -O2 -Wall)
add_test(NAME test_cuda_chaos COMMAND test_cuda_chaos)

# Regression tests for all BUG fixes (#1-#8)
# Step-by-step parity tests (diffusion, plasticity, GPU_ALL)
add_executable(test_step_parity tests/test_step_parity.cpp)
target_link_libraries(test_step_parity biocore dl)
target_compile_options(test_step_parity PRIVATE -O2 -Wall)
add_test(NAME test_step_parity COMMAND test_step_parity)

# dIN parity: CPU vs GPU trace (отладка гиперполяризации)
add_executable(test_din_parity tests/test_din_parity.cpp)
target_link_libraries(test_din_parity biocore dl)
target_compile_options(test_din_parity PRIVATE -O2 -Wall)
add_test(NAME test_din_parity COMMAND test_din_parity)

# NaN guard regression tests
add_executable(test_nan_guards tests/test_nan_guards.cpp)
target_link_libraries(test_nan_guards biocore dl)
target_compile_options(test_nan_guards PRIVATE -O2 -Wall)
add_test(NAME test_nan_guards COMMAND test_nan_guards)

# GPU_ALL longevity test (75000 ticks, NaN detection)
add_executable(test_gpu_all_longevity tests/test_gpu_all_longevity.cpp)
target_link_libraries(test_gpu_all_longevity biocore dl)
target_compile_options(test_gpu_all_longevity PRIVATE -O2 -Wall -pthread)
add_test(NAME test_gpu_all_longevity COMMAND test_gpu_all_longevity)

add_executable(test_cuda_regression_fixes tests/test_cuda_regression_fixes.cpp)
target_link_libraries(test_cuda_regression_fixes biocore dl)
target_compile_options(test_cuda_regression_fixes PRIVATE -O2 -Wall)
add_test(NAME test_cuda_regression_fixes COMMAND test_cuda_regression_fixes)

add_executable(test_v3v4_migration tests/test_v3v4_migration.cpp)
target_link_libraries(test_v3v4_migration biocore)
target_compile_options(test_v3v4_migration PRIVATE -O2 -Wall)

add_executable(test_consolidation_plasticity tests/test_consolidation_plasticity.cpp)
target_link_libraries(test_consolidation_plasticity biocore)
target_compile_options(test_consolidation_plasticity PRIVATE -O2 -Wall)

add_executable(test_plugin_manager tests/test_plugin_manager.cpp)
target_link_libraries(test_plugin_manager biocore)
target_compile_options(test_plugin_manager PRIVATE -O2 -Wall)

# ── Тест протокола демона (требует biocore-daemon) ──────────────────
add_executable(test_daemon_protocol tests/test_daemon_protocol.cpp)
target_include_directories(test_daemon_protocol PRIVATE ${CMAKE_SOURCE_DIR}/include ${catch2_SOURCE_DIR}/single_include)
target_link_libraries(test_daemon_protocol biocore pthread)
target_compile_options(test_daemon_protocol PRIVATE -O2 -Wall)

add_executable(test_daemon_config tests/test_daemon_config.cpp
    ${CMAKE_SOURCE_DIR}/src/daemon_config.cpp
    ${CMAKE_SOURCE_DIR}/src/daemon_state.cpp)
target_include_directories(test_daemon_config PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/../BioNeuronOrchestrator ${catch2_SOURCE_DIR}/single_include)
target_link_libraries(test_daemon_config biocore pthread)
target_compile_options(test_daemon_config PRIVATE -O2 -Wall)

add_executable(test_daemon_session_isolation tests/test_daemon_session_isolation.cpp
    ${CMAKE_SOURCE_DIR}/src/daemon_server.cpp
    ${CMAKE_SOURCE_DIR}/src/daemon_config.cpp
    ${CMAKE_SOURCE_DIR}/src/daemon_state.cpp)
target_include_directories(test_daemon_session_isolation PRIVATE
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src
    ${catch2_SOURCE_DIR}/single_include
    ${CMAKE_SOURCE_DIR}/../BioNeuronOrchestrator)
target_link_libraries(test_daemon_session_isolation biocore pthread dl)
target_link_options(test_daemon_session_isolation PRIVATE -rdynamic)
target_compile_options(test_daemon_session_isolation PRIVATE -O2 -Wall)

add_executable(test_dispatch_compare tests/test_dispatch_compare.cpp)
target_link_libraries(test_dispatch_compare biocore dl)
target_compile_options(test_dispatch_compare PRIVATE -O2 -Wall)

add_executable(test_cpu_k2p_direct tests/test_cpu_k2p_direct.cpp)
target_link_libraries(test_cpu_k2p_direct biocore dl)
target_compile_options(test_cpu_k2p_direct PRIVATE -O2 -Wall)

add_test(NAME test_hh COMMAND test_hh)
add_test(NAME test_synapses COMMAND test_synapses)
add_test(NAME test_cav33 COMMAND test_cav33)
add_test(NAME test_plugins COMMAND test_plugins)
add_test(NAME test_plugin_fallback_parity COMMAND test_plugin_fallback_parity)
add_test(NAME test_nb_receptors COMMAND test_nb_receptors)
add_test(NAME test_simulation_host COMMAND test_simulation_host)
add_test(NAME test_bion_loader_host COMMAND test_bion_loader_host)
add_test(NAME test_all_modifiers COMMAND test_all_modifiers)
add_test(NAME test_diffusion COMMAND test_diffusion)
add_test(NAME test_stimuli COMMAND test_stimuli)
add_test(NAME test_delay_buffer COMMAND test_delay_buffer)
add_test(NAME test_gpu_backend COMMAND test_gpu_backend)
add_test(NAME test_gpu_dual_abi COMMAND test_gpu_dual_abi)
add_test(NAME test_gpu_no_crash COMMAND test_gpu_no_crash)
add_test(NAME test_daemon_dual_gpu COMMAND test_daemon_dual_gpu)
set_tests_properties(test_daemon_dual_gpu PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 30 SKIP_RETURN_CODE 77
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME test_v3v4_migration COMMAND test_v3v4_migration)
add_test(NAME test_consolidation_plasticity COMMAND test_consolidation_plasticity)
add_test(NAME test_plugin_manager COMMAND test_plugin_manager)
add_test(NAME test_daemon_protocol COMMAND test_daemon_protocol)
set_tests_properties(test_daemon_protocol PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60 SKIP_RETURN_CODE 77
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_executable(test_daemon_protocol_full tests/test_daemon_protocol_full.cpp)
target_include_directories(test_daemon_protocol_full PRIVATE ${CMAKE_SOURCE_DIR}/include ${catch2_SOURCE_DIR}/single_include)
target_link_libraries(test_daemon_protocol_full biocore pthread)
target_compile_options(test_daemon_protocol_full PRIVATE -O2 -Wall)
add_test(NAME test_daemon_protocol_full COMMAND test_daemon_protocol_full)
set_tests_properties(test_daemon_protocol_full PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120 SKIP_RETURN_CODE 77
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_executable(test_daemon_multi_session
    ${CMAKE_SOURCE_DIR}/../StudioTestBench/test_daemon_multi_session.cpp
)
target_include_directories(test_daemon_multi_session PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${catch2_SOURCE_DIR}/single_include
)
target_link_libraries(test_daemon_multi_session pthread)
target_compile_options(test_daemon_multi_session PRIVATE -O2 -Wall)
add_test(NAME test_daemon_multi_session COMMAND test_daemon_multi_session)
set_tests_properties(test_daemon_multi_session PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60 SKIP_RETURN_CODE 77
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_plugin_lifecycle tests/test_plugin_lifecycle.cpp)
target_include_directories(test_plugin_lifecycle PRIVATE ${CMAKE_SOURCE_DIR}/include ${catch2_SOURCE_DIR}/single_include)
target_link_libraries(test_plugin_lifecycle biocore dl)
target_link_options(test_plugin_lifecycle PRIVATE -rdynamic)
target_compile_options(test_plugin_lifecycle PRIVATE -O2 -Wall)
add_test(NAME test_plugin_lifecycle COMMAND test_plugin_lifecycle)
set_tests_properties(test_plugin_lifecycle PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Проверка ВСЕХ .so в plugins/ на совместимость: dlopen + init + символы
add_test(NAME test_plugin_type_ids
    COMMAND python3 ${CMAKE_SOURCE_DIR}/../StudioTestBench/test_plugin_type_ids.py
)
set_tests_properties(test_plugin_type_ids PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 30
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/..
)

add_executable(test_plugin_compat tests/test_plugin_compat.cpp)
target_include_directories(test_plugin_compat PRIVATE ${CMAKE_SOURCE_DIR}/include ${catch2_SOURCE_DIR}/single_include)
target_link_libraries(test_plugin_compat biocore dl)
target_link_options(test_plugin_compat PRIVATE -rdynamic)
target_compile_options(test_plugin_compat PRIVATE -O2 -Wall)
add_test(NAME test_plugin_compat COMMAND test_plugin_compat)
set_tests_properties(test_plugin_compat PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(test_core_security tests/test_core_security.cpp)
target_include_directories(test_core_security PRIVATE ${CMAKE_SOURCE_DIR}/include ${catch2_SOURCE_DIR}/single_include)
target_link_libraries(test_core_security biocore dl)
target_compile_options(test_core_security PRIVATE -O2 -Wall)
add_test(NAME test_core_security COMMAND test_core_security)

add_executable(test_core_chaos tests/test_core_chaos.cpp)
target_include_directories(test_core_chaos PRIVATE ${CMAKE_SOURCE_DIR}/include ${catch2_SOURCE_DIR}/single_include)
target_link_libraries(test_core_chaos biocore dl)
target_link_options(test_core_chaos PRIVATE -rdynamic)
target_compile_options(test_core_chaos PRIVATE -O2 -Wall)
add_test(NAME test_core_chaos COMMAND test_core_chaos)
set_tests_properties(test_core_chaos PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(test_core_regression tests/test_core_regression.cpp)
target_include_directories(test_core_regression PRIVATE ${CMAKE_SOURCE_DIR}/include ${catch2_SOURCE_DIR}/single_include)
target_link_libraries(test_core_regression biocore)
target_compile_options(test_core_regression PRIVATE -O2 -Wall)
add_test(NAME test_core_regression COMMAND test_core_regression)

add_executable(test_core_performance tests/test_core_performance.cpp)
target_include_directories(test_core_performance PRIVATE ${CMAKE_SOURCE_DIR}/include ${catch2_SOURCE_DIR}/single_include)
target_link_libraries(test_core_performance biocore dl pthread)
target_link_options(test_core_performance PRIVATE -rdynamic)
target_compile_options(test_core_performance PRIVATE -O2 -Wall)
add_test(NAME test_core_performance COMMAND test_core_performance)
set_tests_properties(test_core_performance PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 30
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_core_smoke tests/test_core_smoke.cpp)
target_include_directories(test_core_smoke PRIVATE ${CMAKE_SOURCE_DIR}/include ${catch2_SOURCE_DIR}/single_include)
target_link_libraries(test_core_smoke biocore pthread)
target_compile_options(test_core_smoke PRIVATE -O2 -Wall)
add_test(NAME test_core_smoke COMMAND test_core_smoke)
set_tests_properties(test_core_smoke PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 30 SKIP_RETURN_CODE 77
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# ── ABI layout + clamp + glial + bouton + GPU_ALL + CPG tests (2026-06-27) ──

add_executable(test_abi_segment_layout tests/test_abi_segment_layout.cpp)
target_include_directories(test_abi_segment_layout PRIVATE ${CMAKE_SOURCE_DIR}/include ${catch2_SOURCE_DIR}/single_include)
target_compile_options(test_abi_segment_layout PRIVATE -O2 -Wall)
add_test(NAME test_abi_segment_layout COMMAND test_abi_segment_layout)

add_executable(test_clamp_membrane tests/test_clamp_membrane.cpp)
target_link_libraries(test_clamp_membrane biocore dl)
target_compile_options(test_clamp_membrane PRIVATE -O2 -Wall)
add_test(NAME test_clamp_membrane COMMAND test_clamp_membrane)

add_executable(test_glial_parity tests/test_glial_parity.cpp)
target_link_libraries(test_glial_parity biocore dl)
target_compile_options(test_glial_parity PRIVATE -O2 -Wall)
add_test(NAME test_glial_parity COMMAND test_glial_parity)

add_executable(test_bouton_coupling tests/test_bouton_coupling.cpp)
target_link_libraries(test_bouton_coupling biocore)
target_compile_options(test_bouton_coupling PRIVATE -O2 -Wall)
add_test(NAME test_bouton_coupling COMMAND test_bouton_coupling)

add_executable(test_gpu_all_slots tests/test_gpu_all_slots.cpp)
target_link_libraries(test_gpu_all_slots biocore dl)
target_compile_options(test_gpu_all_slots PRIVATE -O2 -Wall)
add_test(NAME test_gpu_all_slots COMMAND test_gpu_all_slots)

add_executable(test_cpg_parity tests/test_cpg_parity.cpp)
target_link_libraries(test_cpg_parity biocore dl)
target_compile_options(test_cpg_parity PRIVATE -O2 -Wall)
add_test(NAME test_cpg_parity COMMAND test_cpg_parity)
set_tests_properties(test_cpg_parity PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120 SKIP_RETURN_CODE 77
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cpg_alternation tests/test_cpg_alternation.cpp)

add_executable(test_cpg_tuning tests/test_cpg_tuning.cpp)
target_link_libraries(test_cpg_tuning biocore dl)
target_compile_options(test_cpg_tuning PRIVATE -O2 -Wall)
add_test(NAME test_cpg_tuning COMMAND test_cpg_tuning)
set_tests_properties(test_cpg_tuning PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 600
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_link_libraries(test_cpg_alternation biocore dl)
target_compile_options(test_cpg_alternation PRIVATE -O2 -Wall)
add_test(NAME test_cpg_alternation COMMAND test_cpg_alternation)
set_tests_properties(test_cpg_alternation PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120 SKIP_RETURN_CODE 77
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ghk_reference tests/test_ghk_reference.cpp)
target_link_libraries(test_ghk_reference biocore dl)
target_compile_options(test_ghk_reference PRIVATE -O2 -Wall)
add_test(NAME test_ghk_reference COMMAND test_ghk_reference)
set_tests_properties(test_ghk_reference PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_reference_ionics tests/test_reference_ionics.cpp)
target_link_libraries(test_reference_ionics biocore dl)
target_compile_options(test_reference_ionics PRIVATE -O2 -Wall)
add_test(NAME test_reference_ionics COMMAND test_reference_ionics)
set_tests_properties(test_reference_ionics PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_axon_nav16 tests/test_axon_nav16.cpp)
target_link_libraries(test_axon_nav16 biocore dl)
target_compile_options(test_axon_nav16 PRIVATE -O2 -Wall)
add_test(NAME test_axon_nav16 COMMAND test_axon_nav16)
set_tests_properties(test_axon_nav16 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_axon_nav12 tests/test_axon_nav12.cpp)
target_link_libraries(test_axon_nav12 biocore dl)
target_compile_options(test_axon_nav12 PRIVATE -O2 -Wall)
add_test(NAME test_axon_nav12 COMMAND test_axon_nav12)
set_tests_properties(test_axon_nav12 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_nav12dev tests/test_nav12dev.cpp)
target_link_libraries(test_nav12dev biocore dl)
target_compile_options(test_nav12dev PRIVATE -O2 -Wall)
add_test(NAME test_nav12dev COMMAND test_nav12dev)
set_tests_properties(test_nav12dev PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_dlc_axon_conduction tests/test_dlc_axon_conduction.cpp)
target_link_libraries(test_dlc_axon_conduction biocore dl)
target_compile_options(test_dlc_axon_conduction PRIVATE -O2 -Wall)
add_test(NAME test_dlc_axon_conduction COMMAND test_dlc_axon_conduction)
set_tests_properties(test_dlc_axon_conduction PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_channel_erev tests/test_channel_erev.cpp)
target_link_libraries(test_channel_erev biocore dl)
target_compile_options(test_channel_erev PRIVATE -O2 -Wall)
add_test(NAME test_channel_erev COMMAND test_channel_erev)
set_tests_properties(test_channel_erev PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_receptor_desens tests/test_receptor_desens.cpp)
target_link_libraries(test_receptor_desens biocore dl)
target_compile_options(test_receptor_desens PRIVATE -O2 -Wall)
add_test(NAME test_receptor_desens COMMAND test_receptor_desens)
set_tests_properties(test_receptor_desens PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_din_single_cell tests/test_din_single_cell.cpp)
target_link_libraries(test_din_single_cell biocore dl)
target_compile_options(test_din_single_cell PRIVATE -O2 -Wall)
add_test(NAME test_din_single_cell COMMAND test_din_single_cell)
set_tests_properties(test_din_single_cell PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cin_single_cell tests/test_cin_single_cell.cpp)
target_link_libraries(test_cin_single_cell biocore dl)
target_compile_options(test_cin_single_cell PRIVATE -O2 -Wall)
add_test(NAME test_cin_single_cell COMMAND test_cin_single_cell)
set_tests_properties(test_cin_single_cell PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_din_balance tests/test_din_balance.cpp)
target_link_libraries(test_din_balance biocore dl)
target_compile_options(test_din_balance PRIVATE -O2 -Wall)
add_test(NAME test_din_balance COMMAND test_din_balance)
set_tests_properties(test_din_balance PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cpg_rb_kickstart tests/test_cpg_rb_kickstart.cpp)
target_link_libraries(test_cpg_rb_kickstart biocore dl)
target_compile_options(test_cpg_rb_kickstart PRIVATE -O2 -Wall)
add_test(NAME test_cpg_rb_kickstart COMMAND test_cpg_rb_kickstart)
set_tests_properties(test_cpg_rb_kickstart PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_rb_balance tests/test_rb_balance.cpp)
target_link_libraries(test_rb_balance biocore dl)
target_compile_options(test_rb_balance PRIVATE -O2 -Wall)
add_test(NAME test_rb_balance COMMAND test_rb_balance)
set_tests_properties(test_rb_balance PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_d2c_synapse tests/test_d2c_synapse.cpp)
target_link_libraries(test_d2c_synapse biocore dl)
target_compile_options(test_d2c_synapse PRIVATE -O2 -Wall)
add_test(NAME test_d2c_synapse COMMAND test_d2c_synapse)
set_tests_properties(test_d2c_synapse PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_c2d_synapse tests/test_c2d_synapse.cpp)
target_link_libraries(test_c2d_synapse biocore dl)
target_compile_options(test_c2d_synapse PRIVATE -O2 -Wall)
add_test(NAME test_c2d_synapse COMMAND test_c2d_synapse)
set_tests_properties(test_c2d_synapse PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cpg2_debug tests/test_cpg2_debug.cpp)
target_link_libraries(test_cpg2_debug biocore dl)
target_compile_options(test_cpg2_debug PRIVATE -O2 -Wall)
add_test(NAME test_cpg2_debug COMMAND test_cpg2_debug)
set_tests_properties(test_cpg2_debug PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 300
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_glia_nan tests/test_glia_nan.cpp)
target_link_libraries(test_glia_nan biocore dl)
target_compile_options(test_glia_nan PRIVATE -O2 -Wall)
add_test(NAME test_glia_nan COMMAND test_glia_nan)
set_tests_properties(test_glia_nan PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_glia_nan2 tests/test_glia_nan2.cpp)
target_link_libraries(test_glia_nan2 biocore dl)
target_compile_options(test_glia_nan2 PRIVATE -O2 -Wall)
add_test(NAME test_glia_nan2 COMMAND test_glia_nan2)
set_tests_properties(test_glia_nan2 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_syn_nan tests/test_syn_nan.cpp)
target_link_libraries(test_syn_nan biocore dl)
target_compile_options(test_syn_nan PRIVATE -O2 -Wall)
add_test(NAME test_syn_nan COMMAND test_syn_nan)
set_tests_properties(test_syn_nan PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ng_nan tests/test_ng_nan.cpp)
target_link_libraries(test_ng_nan biocore dl)
target_compile_options(test_ng_nan PRIVATE -O2 -Wall)
add_test(NAME test_ng_nan COMMAND test_ng_nan)
set_tests_properties(test_ng_nan PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ais_single_ap tests/test_ais_single_ap.cpp)
target_link_libraries(test_ais_single_ap biocore dl)
target_compile_options(test_ais_single_ap PRIVATE -O2 -Wall)
add_test(NAME test_ais_single_ap COMMAND test_ais_single_ap)
set_tests_properties(test_ais_single_ap PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_bouton_compartment tests/test_bouton_compartment.cpp)
target_link_libraries(test_bouton_compartment biocore dl)
target_compile_options(test_bouton_compartment PRIVATE -O2 -Wall)
add_test(NAME test_bouton_compartment COMMAND test_bouton_compartment)
set_tests_properties(test_bouton_compartment PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_bouton_attenuation tests/test_bouton_attenuation.cpp)
target_link_libraries(test_bouton_attenuation biocore dl)
target_compile_options(test_bouton_attenuation PRIVATE -O2 -Wall)
add_test(NAME test_bouton_attenuation COMMAND test_bouton_attenuation)
set_tests_properties(test_bouton_attenuation PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cpg_chain_debug tests/test_cpg_chain_debug.cpp)
target_link_libraries(test_cpg_chain_debug biocore dl)
target_compile_options(test_cpg_chain_debug PRIVATE -O2 -Wall)

add_executable(test_din_oscillation tests/test_din_oscillation.cpp)
target_link_libraries(test_din_oscillation biocore dl)
target_compile_options(test_din_oscillation PRIVATE -O2 -Wall)
add_test(NAME test_din_oscillation COMMAND test_din_oscillation)
set_tests_properties(test_din_oscillation PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_din_ablation tests/test_din_ablation.cpp)
target_link_libraries(test_din_ablation biocore dl)
target_compile_options(test_din_ablation PRIVATE -O2 -Wall)
add_test(NAME test_din_ablation COMMAND test_din_ablation)
set_tests_properties(test_din_ablation PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ion_conservation tests/test_ion_conservation.cpp)
target_link_libraries(test_ion_conservation biocore dl)
target_compile_options(test_ion_conservation PRIVATE -O2 -Wall)
add_test(NAME test_ion_conservation COMMAND test_ion_conservation)
set_tests_properties(test_ion_conservation PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cpg_spikecount tests/test_cpg_spikecount.cpp)
target_link_libraries(test_cpg_spikecount biocore dl)
target_compile_options(test_cpg_spikecount PRIVATE -O2 -Wall)
add_test(NAME test_cpg_spikecount COMMAND test_cpg_spikecount)
set_tests_properties(test_cpg_spikecount PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 120
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_nalcn tests/test_nalcn.cpp)
target_link_libraries(test_nalcn biocore dl)
target_compile_options(test_nalcn PRIVATE -O2 -Wall)
add_test(NAME test_nalcn COMMAND test_nalcn)
set_tests_properties(test_nalcn PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_k2p tests/test_k2p.cpp)
target_link_libraries(test_k2p biocore dl)
target_compile_options(test_k2p PRIVATE -O2 -Wall)
add_test(NAME test_k2p COMMAND test_k2p)
set_tests_properties(test_k2p PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_nap tests/test_nap.cpp)
target_link_libraries(test_nap biocore dl)
target_compile_options(test_nap PRIVATE -O2 -Wall)
add_test(NAME test_nap COMMAND test_nap)
set_tests_properties(test_nap PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_nap_tadpole tests/test_nap_tadpole.cpp)
target_link_libraries(test_nap_tadpole biocore dl)
target_compile_options(test_nap_tadpole PRIVATE -O2 -Wall)
add_test(NAME test_nap_tadpole COMMAND test_nap_tadpole)
set_tests_properties(test_nap_tadpole PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kir tests/test_kir.cpp)
target_link_libraries(test_kir biocore dl)
target_compile_options(test_kir PRIVATE -O2 -Wall)
add_test(NAME test_kir COMMAND test_kir)
set_tests_properties(test_kir PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_bk tests/test_bk.cpp)
target_link_libraries(test_bk biocore dl)
target_compile_options(test_bk PRIVATE -O2 -Wall)
add_test(NAME test_bk COMMAND test_bk)
set_tests_properties(test_bk PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_pump_nak tests/test_pump_nak.cpp)
target_link_libraries(test_pump_nak biocore dl)
target_compile_options(test_pump_nak PRIVATE -O2 -Wall)
add_test(NAME test_pump_nak COMMAND test_pump_nak)
set_tests_properties(test_pump_nak PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_nak_pump_a3 tests/test_nak_pump_a3.cpp)
target_link_libraries(test_nak_pump_a3 biocore dl)
target_compile_options(test_nak_pump_a3 PRIVATE -O2 -Wall)
add_test(NAME test_nak_pump_a3 COMMAND test_nak_pump_a3)
set_tests_properties(test_nak_pump_a3 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_pmca tests/test_pmca.cpp)
target_link_libraries(test_pmca biocore dl)
target_compile_options(test_pmca PRIVATE -O2 -Wall)
add_test(NAME test_pmca COMMAND test_pmca)
set_tests_properties(test_pmca PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ncx tests/test_ncx.cpp)
target_link_libraries(test_ncx biocore dl)
target_compile_options(test_ncx PRIVATE -O2 -Wall)
add_test(NAME test_ncx COMMAND test_ncx)
set_tests_properties(test_ncx PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kcc2 tests/test_kcc2.cpp)
target_link_libraries(test_kcc2 biocore dl)
target_compile_options(test_kcc2 PRIVATE -O2 -Wall)
add_test(NAME test_kcc2 COMMAND test_kcc2)
set_tests_properties(test_kcc2 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ca_buffer tests/test_ca_buffer.cpp)
target_link_libraries(test_ca_buffer biocore dl)
target_compile_options(test_ca_buffer PRIVATE -O2 -Wall)
add_test(NAME test_ca_buffer COMMAND test_ca_buffer)
set_tests_properties(test_ca_buffer PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_glial_nak_pump tests/test_glial_nak_pump.cpp)
target_link_libraries(test_glial_nak_pump biocore dl)
target_compile_options(test_glial_nak_pump PRIVATE -O2 -Wall)
add_test(NAME test_glial_nak_pump COMMAND test_glial_nak_pump)
set_tests_properties(test_glial_nak_pump PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_nkcc1 tests/test_nkcc1.cpp)
target_link_libraries(test_nkcc1 biocore dl)
target_compile_options(test_nkcc1 PRIVATE -O2 -Wall)
add_test(NAME test_nkcc1 COMMAND test_nkcc1)
set_tests_properties(test_nkcc1 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_serca tests/test_serca.cpp)
target_link_libraries(test_serca biocore dl)
target_compile_options(test_serca PRIVATE -O2 -Wall)
add_test(NAME test_serca COMMAND test_serca)
set_tests_properties(test_serca PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ip3r tests/test_ip3r.cpp)
target_link_libraries(test_ip3r biocore dl)
target_compile_options(test_ip3r PRIVATE -O2 -Wall)
add_test(NAME test_ip3r COMMAND test_ip3r)
set_tests_properties(test_ip3r PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ryr tests/test_ryr.cpp)
target_link_libraries(test_ryr biocore dl)
target_compile_options(test_ryr PRIVATE -O2 -Wall)
add_test(NAME test_ryr COMMAND test_ryr)
set_tests_properties(test_ryr PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ampa tests/test_ampa.cpp)
target_link_libraries(test_ampa biocore dl)
target_compile_options(test_ampa PRIVATE -O2 -Wall)
add_test(NAME test_ampa COMMAND test_ampa)
set_tests_properties(test_ampa PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_nmda tests/test_nmda.cpp)
target_link_libraries(test_nmda biocore dl)
target_compile_options(test_nmda PRIVATE -O2 -Wall)
add_test(NAME test_nmda COMMAND test_nmda)
set_tests_properties(test_nmda PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kainate tests/test_kainate.cpp)
target_link_libraries(test_kainate biocore dl)
target_compile_options(test_kainate PRIVATE -O2 -Wall)
add_test(NAME test_kainate COMMAND test_kainate)
set_tests_properties(test_kainate PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_gabaa tests/test_gabaa.cpp)
target_link_libraries(test_gabaa biocore dl)
target_compile_options(test_gabaa PRIVATE -O2 -Wall)
add_test(NAME test_gabaa COMMAND test_gabaa)
set_tests_properties(test_gabaa PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_glycine tests/test_glycine.cpp)
target_link_libraries(test_glycine biocore dl)
target_compile_options(test_glycine PRIVATE -O2 -Wall)
add_test(NAME test_glycine COMMAND test_glycine)
set_tests_properties(test_glycine PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_nicotinic tests/test_nicotinic.cpp)
target_link_libraries(test_nicotinic biocore dl)
target_compile_options(test_nicotinic PRIVATE -O2 -Wall)
add_test(NAME test_nicotinic COMMAND test_nicotinic)
set_tests_properties(test_nicotinic PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ht3 tests/test_ht3.cpp)
target_link_libraries(test_ht3 biocore dl)
target_compile_options(test_ht3 PRIVATE -O2 -Wall)
add_test(NAME test_ht3 COMMAND test_ht3)
set_tests_properties(test_ht3 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_p2x tests/test_p2x.cpp)
target_link_libraries(test_p2x biocore dl)
target_compile_options(test_p2x PRIVATE -O2 -Wall)
add_test(NAME test_p2x COMMAND test_p2x)
set_tests_properties(test_p2x PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_trpv1 tests/test_trpv1.cpp)
target_link_libraries(test_trpv1 biocore dl)
target_compile_options(test_trpv1 PRIVATE -O2 -Wall)
add_test(NAME test_trpv1 COMMAND test_trpv1)
set_tests_properties(test_trpv1 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_gabab tests/test_gabab.cpp)
target_link_libraries(test_gabab biocore dl)
target_compile_options(test_gabab PRIVATE -O2 -Wall)
add_test(NAME test_gabab COMMAND test_gabab)
set_tests_properties(test_gabab PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_sk tests/test_sk.cpp)
target_link_libraries(test_sk biocore dl)
target_compile_options(test_sk PRIVATE -O2 -Wall)
add_test(NAME test_sk COMMAND test_sk)
set_tests_properties(test_sk PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kna tests/test_kna.cpp)
target_link_libraries(test_kna biocore dl)
target_compile_options(test_kna PRIVATE -O2 -Wall)
add_test(NAME test_kna COMMAND test_kna)
set_tests_properties(test_kna PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kv_dr tests/test_kv_dr.cpp)
target_link_libraries(test_kv_dr biocore dl)
target_compile_options(test_kv_dr PRIVATE -O2 -Wall)
add_test(NAME test_kv_dr COMMAND test_kv_dr)
set_tests_properties(test_kv_dr PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_hcn tests/test_hcn.cpp)
target_link_libraries(test_hcn biocore dl)
target_compile_options(test_hcn PRIVATE -O2 -Wall)
add_test(NAME test_hcn COMMAND test_hcn)
set_tests_properties(test_hcn PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cl tests/test_cl.cpp)
target_link_libraries(test_cl biocore dl)
target_compile_options(test_cl PRIVATE -O2 -Wall)
add_test(NAME test_cl COMMAND test_cl)
set_tests_properties(test_cl PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ca_l_type tests/test_ca_l_type.cpp)
target_link_libraries(test_ca_l_type biocore dl)
target_compile_options(test_ca_l_type PRIVATE -O2 -Wall)
add_test(NAME test_ca_l_type COMMAND test_ca_l_type)
set_tests_properties(test_ca_l_type PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_camp_pka tests/test_camp_pka.cpp)
target_link_libraries(test_camp_pka biocore dl)
target_compile_options(test_camp_pka PRIVATE -O2 -Wall)
add_test(NAME test_camp_pka COMMAND test_camp_pka)
set_tests_properties(test_camp_pka PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_gq_cascade tests/test_gq_cascade.cpp)
target_link_libraries(test_gq_cascade biocore dl)
target_compile_options(test_gq_cascade PRIVATE -O2 -Wall)
add_test(NAME test_gq_cascade COMMAND test_gq_cascade)
set_tests_properties(test_gq_cascade PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kv42 tests/test_kv42.cpp)
target_link_libraries(test_kv42 biocore dl)
target_compile_options(test_kv42 PRIVATE -O2 -Wall)
add_test(NAME test_kv42 COMMAND test_kv42)
set_tests_properties(test_kv42 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kv31 tests/test_kv31.cpp)
target_link_libraries(test_kv31 biocore dl)
target_compile_options(test_kv31 PRIVATE -O2 -Wall)
add_test(NAME test_kv31 COMMAND test_kv31)
set_tests_properties(test_kv31 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_task tests/test_task.cpp)
target_link_libraries(test_task biocore dl)
target_compile_options(test_task PRIVATE -O2 -Wall)
add_test(NAME test_task COMMAND test_task)
set_tests_properties(test_task PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_thik2 tests/test_thik2.cpp)
target_link_libraries(test_thik2 biocore dl)
target_compile_options(test_thik2 PRIVATE -O2 -Wall)
add_test(NAME test_thik2 COMMAND test_thik2)
set_tests_properties(test_thik2 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kv13 tests/test_kv13.cpp)
target_link_libraries(test_kv13 biocore dl)
target_compile_options(test_kv13 PRIVATE -O2 -Wall)
add_test(NAME test_kv13 COMMAND test_kv13)
set_tests_properties(test_kv13 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cng tests/test_cng.cpp)
target_link_libraries(test_cng biocore dl)
target_compile_options(test_cng PRIVATE -O2 -Wall)
add_test(NAME test_cng COMMAND test_cng)
set_tests_properties(test_cng PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kv11 tests/test_kv11.cpp)
target_link_libraries(test_kv11 biocore dl)
target_compile_options(test_kv11 PRIVATE -O2 -Wall)
add_test(NAME test_kv11 COMMAND test_kv11)
set_tests_properties(test_kv11 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kv12 tests/test_kv12.cpp)
target_link_libraries(test_kv12 biocore dl)
target_compile_options(test_kv12 PRIVATE -O2 -Wall)
add_test(NAME test_kv12 COMMAND test_kv12)
set_tests_properties(test_kv12 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cav23 tests/test_cav23.cpp)
target_link_libraries(test_cav23 biocore dl)
target_compile_options(test_cav23 PRIVATE -O2 -Wall)
add_test(NAME test_cav23 COMMAND test_cav23)
set_tests_properties(test_cav23 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kv34 tests/test_kv34.cpp)
target_link_libraries(test_kv34 biocore dl)
target_compile_options(test_kv34 PRIVATE -O2 -Wall)
add_test(NAME test_kv34 COMMAND test_kv34)
set_tests_properties(test_kv34 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_kv21 tests/test_kv21.cpp)
target_link_libraries(test_kv21 biocore dl)
target_compile_options(test_kv21 PRIVATE -O2 -Wall)
add_test(NAME test_kv21 COMMAND test_kv21)
set_tests_properties(test_kv21 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_girk tests/test_girk.cpp)
target_link_libraries(test_girk biocore dl)
target_compile_options(test_girk PRIVATE -O2 -Wall)
add_test(NAME test_girk COMMAND test_girk)
set_tests_properties(test_girk PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_katp tests/test_katp.cpp)
target_link_libraries(test_katp biocore dl)
target_compile_options(test_katp PRIVATE -O2 -Wall)
add_test(NAME test_katp COMMAND test_katp)
set_tests_properties(test_katp PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_trpc4 tests/test_trpc4.cpp)
target_link_libraries(test_trpc4 biocore dl)
target_compile_options(test_trpc4 PRIVATE -O2 -Wall)
add_test(NAME test_trpc4 COMMAND test_trpc4)
set_tests_properties(test_trpc4 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_trpc5 tests/test_trpc5.cpp)
target_link_libraries(test_trpc5 biocore dl)
target_compile_options(test_trpc5 PRIVATE -O2 -Wall)
add_test(NAME test_trpc5 COMMAND test_trpc5)
set_tests_properties(test_trpc5 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_trpm4 tests/test_trpm4.cpp)
target_link_libraries(test_trpm4 biocore dl)
target_compile_options(test_trpm4 PRIVATE -O2 -Wall)
add_test(NAME test_trpm4 COMMAND test_trpm4)
set_tests_properties(test_trpm4 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_trpm5 tests/test_trpm5.cpp)
target_link_libraries(test_trpm5 biocore dl)
target_compile_options(test_trpm5 PRIVATE -O2 -Wall)
add_test(NAME test_trpm5 COMMAND test_trpm5)
set_tests_properties(test_trpm5 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_thik1 tests/test_thik1.cpp)
target_link_libraries(test_thik1 biocore dl)
target_compile_options(test_thik1 PRIVATE -O2 -Wall)
add_test(NAME test_thik1 COMMAND test_thik1)
set_tests_properties(test_thik1 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)


add_executable(test_cav22 tests/test_cav22.cpp)
target_link_libraries(test_cav22 biocore dl)
target_compile_options(test_cav22 PRIVATE -O2 -Wall)
add_test(NAME test_cav22 COMMAND test_cav22)
set_tests_properties(test_cav22 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_nociceptor_channels tests/test_nociceptor_channels.cpp)
target_link_libraries(test_nociceptor_channels biocore dl)
target_compile_options(test_nociceptor_channels PRIVATE -O2 -Wall)
add_test(NAME test_nociceptor_channels COMMAND test_nociceptor_channels)
set_tests_properties(test_nociceptor_channels PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_other_channels tests/test_other_channels.cpp)
target_link_libraries(test_other_channels biocore dl)
target_compile_options(test_other_channels PRIVATE -O2 -Wall)
add_test(NAME test_other_channels COMMAND test_other_channels)
set_tests_properties(test_other_channels PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_gating_dynamics tests/test_gating_dynamics.cpp)
target_link_libraries(test_gating_dynamics biocore dl)
target_compile_options(test_gating_dynamics PRIVATE -O2 -Wall)
add_test(NAME test_gating_dynamics COMMAND test_gating_dynamics)
set_tests_properties(test_gating_dynamics PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_modulation tests/test_modulation.cpp)
target_link_libraries(test_modulation biocore dl)
target_compile_options(test_modulation PRIVATE -O2 -Wall)
add_test(NAME test_modulation COMMAND test_modulation)
set_tests_properties(test_modulation PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_can tests/test_can.cpp)
target_link_libraries(test_can biocore dl)
target_compile_options(test_can PRIVATE -O2 -Wall)
add_test(NAME test_can COMMAND test_can)
set_tests_properties(test_can PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_pkd2l1 tests/test_pkd2l1.cpp)
target_link_libraries(test_pkd2l1 biocore dl)
target_compile_options(test_pkd2l1 PRIVATE -O2 -Wall)
add_test(NAME test_pkd2l1 COMMAND test_pkd2l1)
set_tests_properties(test_pkd2l1 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cav13 tests/test_cav13.cpp)
target_link_libraries(test_cav13 biocore dl)
target_compile_options(test_cav13 PRIVATE -O2 -Wall)
add_test(NAME test_cav13 COMMAND test_cav13)
set_tests_properties(test_cav13 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_pir_ttype tests/test_pir_ttype.cpp)
target_link_libraries(test_pir_ttype biocore dl)
target_compile_options(test_pir_ttype PRIVATE -O2 -Wall)
add_test(NAME test_pir_ttype COMMAND test_pir_ttype)
set_tests_properties(test_pir_ttype PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cav21 tests/test_cav21.cpp)
target_link_libraries(test_cav21 biocore dl)
target_compile_options(test_cav21 PRIVATE -O2 -Wall)
add_test(NAME test_cav21 COMMAND test_cav21)
set_tests_properties(test_cav21 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cav14 tests/test_cav14.cpp)
target_link_libraries(test_cav14 biocore dl)
target_compile_options(test_cav14 PRIVATE -O2 -Wall)
add_test(NAME test_cav14 COMMAND test_cav14)
set_tests_properties(test_cav14 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_cacc tests/test_cacc.cpp)
target_link_libraries(test_cacc biocore dl)
target_compile_options(test_cacc PRIVATE -O2 -Wall)
add_test(NAME test_cacc COMMAND test_cacc)
set_tests_properties(test_cacc PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_glast tests/test_glast.cpp)
target_link_libraries(test_glast biocore dl)
target_compile_options(test_glast PRIVATE -O2 -Wall)
add_test(NAME test_glast COMMAND test_glast)
set_tests_properties(test_glast PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_glyt1_gat3 tests/test_glyt1_gat3.cpp)
target_link_libraries(test_glyt1_gat3 biocore dl)
target_compile_options(test_glyt1_gat3 PRIVATE -O2 -Wall)
add_test(NAME test_glyt1_gat3 COMMAND test_glyt1_gat3)
set_tests_properties(test_glyt1_gat3 PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_transporters_neuronal tests/test_transporters_neuronal.cpp)
target_link_libraries(test_transporters_neuronal biocore dl)
target_compile_options(test_transporters_neuronal PRIVATE -O2 -Wall)
add_test(NAME test_transporters_neuronal COMMAND test_transporters_neuronal)
set_tests_properties(test_transporters_neuronal PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ca_leak tests/test_ca_leak.cpp)
target_link_libraries(test_ca_leak biocore dl)
target_compile_options(test_ca_leak PRIVATE -O2 -Wall)
add_test(NAME test_ca_leak COMMAND test_ca_leak)
set_tests_properties(test_ca_leak PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_ghk_gating_reference tests/test_ghk_gating_reference.cpp)
target_link_libraries(test_ghk_gating_reference biocore dl)
target_compile_options(test_ghk_gating_reference PRIVATE -O2 -Wall)
add_test(NAME test_ghk_gating_reference COMMAND test_ghk_gating_reference)
set_tests_properties(test_ghk_gating_reference PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test_si_golden tests/test_si_golden.cpp)
target_link_libraries(test_si_golden biocore dl)
target_compile_options(test_si_golden PRIVATE -O2 -Wall)
add_test(NAME test_si_golden COMMAND test_si_golden)
set_tests_properties(test_si_golden PROPERTIES
    RUN_SERIAL TRUE TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test_daemon_config COMMAND test_daemon_config)
add_test(NAME test_daemon_session_isolation COMMAND test_daemon_session_isolation)
set_tests_properties(test_daemon_session_isolation PROPERTIES
    RUN_SERIAL TRUE    # потоки внутри теста, нельзя параллелить
    TIMEOUT 30)

# ==================== DUAL SYCL DETECTION ====================
# Intel oneAPI (icpx/dpcpp) для Intel GPU.
# NVIDIA GPU — через native CUDA (nvcc), см. секцию ниже.
find_program(ICPX_COMPILER icpx PATHS /opt/intel/oneapi/compiler/latest/bin)
find_program(DPCPP_COMPILER dpcpp PATHS /opt/intel/oneapi/compiler/latest/bin)

set(SYCL_BACKENDS "")  # список строк: "intel"

if(ICPX_COMPILER)
    message(STATUS "SYCL: Intel oneAPI (icpx) found at ${ICPX_COMPILER}")
    list(APPEND SYCL_BACKENDS "intel")
elseif(DPCPP_COMPILER)
    message(STATUS "SYCL: Intel DPC++ (dpcpp) found at ${DPCPP_COMPILER}")
    list(APPEND SYCL_BACKENDS "intel")
    set(ICPX_COMPILER ${DPCPP_COMPILER})  # унифицируем переменную
endif()

if(SYCL_BACKENDS)
    # Проверка что SYCL-заголовки доступны (а не только бинарник icpx)
    # try_compile ловит случай когда icpx есть но setvars.sh не загружен
    try_compile(ICPX_COMPILES
        ${CMAKE_BINARY_DIR}/sycl_check
        SOURCES ${CMAKE_SOURCE_DIR}/cmake/sycl_check.cpp
        CMAKE_FLAGS
            "-DCMAKE_CXX_COMPILER=${ICPX_COMPILER}"
            "-DCMAKE_CXX_FLAGS=-fsycl"
        OUTPUT_VARIABLE ICPX_TRY_OUTPUT
    )
    if(ICPX_COMPILES)
        set(SYCL_FOUND TRUE)
        message(STATUS "SYCL: Intel compiler + headers confirmed")
    else()
        set(SYCL_BACKENDS "")
        set(SYCL_FOUND FALSE)
        message(STATUS "SYCL: icpx found but sycl/sycl.hpp not available — skipping GPU targets")
        message(STATUS "SYCL: (source /opt/intel/oneapi/setvars.sh before cmake)")
    endif()
else()
    message(STATUS "SYCL: No Intel compilers found — CPU-only tests")
    set(SYCL_FOUND FALSE)
endif()

# ==================== .sycl → CXX ====================
set(KERNEL_SOURCES
    kernels/kernels_part1.sycl
    kernels/kernels_part2.sycl
)
set_source_files_properties(${KERNEL_SOURCES} PROPERTIES LANGUAGE CXX)

# Общие исходники для GPU-бекендов
set(GPU_BACKEND_SOURCES
    src/gpu_backend.cpp
    src/simulation.cpp src/simulation_host.cpp src/consolidation.cpp
    src/bion_loader.cpp src/bion_loader_host.cpp
    src/hh_biological.cpp src/receptors_biological.cpp
    src/plugin_manager.cpp src/dispatch_generated.cpp
    ${KERNEL_SOURCES}
)
set(GPU_BACKEND_INCLUDES -I${CMAKE_SOURCE_DIR}/include -I${CMAKE_SOURCE_DIR}/plugins/builtin)

# Заголовки — в DEPENDS custom-команд GPU-бекендов, чтобы изменение .h пересобирало
# объектники (иначе sizeof(Simulation)/структур рассинхронизируется → heap corruption).
file(GLOB GPU_BACKEND_HEADERS
    ${CMAKE_SOURCE_DIR}/include/*.h
    ${CMAKE_SOURCE_DIR}/plugins/builtin/*.h
)

# ==================== GPU BACKEND: Intel oneAPI (icpx) ====================
if(ICPX_COMPILER)
    set(INTEL_OBJS "")
    foreach(SRC ${GPU_BACKEND_SOURCES})
        get_filename_component(SRC_NAME ${SRC} NAME_WE)
        set(OBJ "${CMAKE_BINARY_DIR}/${SRC_NAME}_intel.o")
        add_custom_command(
            OUTPUT ${OBJ}
            COMMAND ${ICPX_COMPILER} -std=c++17 -fPIC -O2 -Wall -fsycl -fp-model=strict
                    ${GPU_BACKEND_INCLUDES} -x c++
                    -c ${CMAKE_SOURCE_DIR}/${SRC} -o ${OBJ}
            DEPENDS ${CMAKE_SOURCE_DIR}/${SRC} ${GPU_BACKEND_HEADERS}
            COMMENT "icpx: ${SRC_NAME}"
        )
        list(APPEND INTEL_OBJS ${OBJ})
    endforeach()
    add_custom_target(biocore_gpu_backend_intel ALL
        COMMAND ${ICPX_COMPILER} -fPIC -shared -fsycl ${INTEL_OBJS}
                -o ${CMAKE_BINARY_DIR}/libbiocore_gpu_backend_intel.so
        DEPENDS ${INTEL_OBJS}
        COMMENT "Linking libbiocore_gpu_backend_intel.so"
    )
    set_target_properties(biocore_gpu_backend_intel PROPERTIES FOLDER "gpu")
endif()

# ==================== GPU BACKEND: Native CUDA (nvcc) ====================
# Прямая компиляция через nvcc для NVIDIA GPU.
find_program(NVCC_COMPILER nvcc)
if(NVCC_COMPILER)
    message(STATUS "CUDA: nvcc found at ${NVCC_COMPILER}")

    # Общие флаги для всех исходников
    set(NVCC_FLAGS -std=c++17 -arch=sm_75 -rdc=true)
    set(NVCC_INCLUDES -I${CMAKE_SOURCE_DIR}/include -I${CMAKE_SOURCE_DIR}/plugins/builtin)

    set(CUDA_OBJ_DIR ${CMAKE_BINARY_DIR}/cuda_objs)
    file(MAKE_DIRECTORY ${CUDA_OBJ_DIR})

    # Компиляция hh_biological.cpp → hh_bio.o
    add_custom_command(
        OUTPUT ${CUDA_OBJ_DIR}/hh_bio.o
        COMMAND ${NVCC_COMPILER} ${NVCC_FLAGS} -Xcompiler -fPIC,-O2,-Wall
                ${NVCC_INCLUDES} -x cu -c
                ${CMAKE_SOURCE_DIR}/src/hh_biological.cpp
                -o ${CUDA_OBJ_DIR}/hh_bio.o
        DEPENDS ${CMAKE_SOURCE_DIR}/src/hh_biological.cpp ${GPU_BACKEND_HEADERS}
        COMMENT "nvcc: compiling hh_biological.cpp"
    )

    # Компиляция receptors_biological.cpp → rec_bio.o
    add_custom_command(
        OUTPUT ${CUDA_OBJ_DIR}/rec_bio.o
        COMMAND ${NVCC_COMPILER} ${NVCC_FLAGS} -Xcompiler -fPIC,-O2,-Wall
                ${NVCC_INCLUDES} -x cu -c
                ${CMAKE_SOURCE_DIR}/src/receptors_biological.cpp
                -o ${CUDA_OBJ_DIR}/rec_bio.o
        DEPENDS ${CMAKE_SOURCE_DIR}/src/receptors_biological.cpp ${GPU_BACKEND_HEADERS}
        COMMENT "nvcc: compiling receptors_biological.cpp"
    )

    # Компиляция dispatch_generated.cpp → dispatch.o
    add_custom_command(
        OUTPUT ${CUDA_OBJ_DIR}/dispatch.o
        COMMAND ${NVCC_COMPILER} ${NVCC_FLAGS} -Xcompiler -fPIC,-O2,-Wall
                ${NVCC_INCLUDES} -x cu -c
                ${CMAKE_SOURCE_DIR}/src/dispatch_generated.cpp
                -o ${CUDA_OBJ_DIR}/dispatch.o
        DEPENDS ${CMAKE_SOURCE_DIR}/src/dispatch_generated.cpp ${GPU_BACKEND_HEADERS}
        COMMENT "nvcc: compiling dispatch_generated.cpp"
    )

    # Компиляция bion_loader.cpp → bion_loader.o
    add_custom_command(
        OUTPUT ${CUDA_OBJ_DIR}/bion_loader.o
        COMMAND ${NVCC_COMPILER} ${NVCC_FLAGS} -Xcompiler -fPIC,-O2,-Wall
                ${NVCC_INCLUDES} -x cu -c
                ${CMAKE_SOURCE_DIR}/src/bion_loader.cpp
                -o ${CUDA_OBJ_DIR}/bion_loader.o
        DEPENDS ${CMAKE_SOURCE_DIR}/src/bion_loader.cpp ${GPU_BACKEND_HEADERS}
        COMMENT "nvcc: compiling bion_loader.cpp"
    )

    # Компиляция kernels_cuda.cu → kernels.o
    add_custom_command(
        OUTPUT ${CUDA_OBJ_DIR}/kernels.o
        COMMAND ${NVCC_COMPILER} ${NVCC_FLAGS} -Xcompiler -fPIC,-O2,-Wall
                ${NVCC_INCLUDES} -x cu -c
                ${CMAKE_SOURCE_DIR}/kernels/kernels_cuda.cu
                -o ${CUDA_OBJ_DIR}/kernels.o
        DEPENDS ${CMAKE_SOURCE_DIR}/kernels/kernels_cuda.cu ${GPU_BACKEND_HEADERS}
        COMMENT "nvcc: compiling kernels_cuda.cu"
    )

    # Компиляция gpu_backend_cuda.cu → backend.o
    add_custom_command(
        OUTPUT ${CUDA_OBJ_DIR}/backend.o
        COMMAND ${NVCC_COMPILER} ${NVCC_FLAGS} -Xcompiler -fPIC,-O2,-Wall
                ${NVCC_INCLUDES} -x cu -c
                ${CMAKE_SOURCE_DIR}/src/gpu_backend_cuda.cu
                -o ${CUDA_OBJ_DIR}/backend.o
        DEPENDS ${CMAKE_SOURCE_DIR}/src/gpu_backend_cuda.cu ${GPU_BACKEND_HEADERS}
        COMMENT "nvcc: compiling gpu_backend_cuda.cu"
    )

    # Device-линковка + shared library
    set(CUDA_ALL_OBJS
        ${CUDA_OBJ_DIR}/hh_bio.o
        ${CUDA_OBJ_DIR}/rec_bio.o
        ${CUDA_OBJ_DIR}/dispatch.o
        ${CUDA_OBJ_DIR}/bion_loader.o
        ${CUDA_OBJ_DIR}/kernels.o
        ${CUDA_OBJ_DIR}/backend.o
    )

    # Промежуточный device-linked объект
    add_custom_command(
        OUTPUT ${CUDA_OBJ_DIR}/device_link.o
        COMMAND ${NVCC_COMPILER} -arch=sm_75 -rdc=true -dlink
                -Xcompiler -fPIC
                ${CUDA_ALL_OBJS}
                -o ${CUDA_OBJ_DIR}/device_link.o
        DEPENDS ${CUDA_ALL_OBJS}
        COMMENT "nvcc: device linking"
    )

    # Финальная сборка .so
    add_custom_target(biocore_gpu_backend_cuda_native ALL
        COMMAND ${NVCC_COMPILER} -shared -arch=sm_75
                -Xcompiler -fPIC
                ${CUDA_ALL_OBJS} ${CUDA_OBJ_DIR}/device_link.o
                -o ${CMAKE_BINARY_DIR}/libbiocore_gpu_backend_cuda.so
                -lcudart
        DEPENDS ${CUDA_ALL_OBJS} ${CUDA_OBJ_DIR}/device_link.o
        COMMENT "Building libbiocore_gpu_backend_cuda.so (nvcc native)"
    )
    set_target_properties(biocore_gpu_backend_cuda_native PROPERTIES FOLDER "gpu")
    message(STATUS "CUDA: native backend enabled (sm_75, nvcc)")
else()
    message(STATUS "CUDA: nvcc not found — native CUDA backend disabled")
endif()

# ==================== SYCL STATIC LIB + TESTS (Intel only) ====================
# biocore_sycl нужен для test_gpu, test_cpu_gpu_parity и test_bion (SYCL-версия).
# Эти тесты собираются только icpx — NVIDIA тестируется через CUDA native backend (ядра требуют отладки).
if(SYCL_FOUND)
    add_library(biocore_sycl STATIC
        src/simulation.cpp
        src/simulation_host.cpp
        src/consolidation.cpp
        src/bion_loader.cpp
        src/bion_loader_host.cpp
        src/hh_biological.cpp
        src/receptors_biological.cpp
        src/plugin_manager.cpp
        src/dispatch_generated.cpp
        ${KERNEL_SOURCES}
    )
    target_compile_options(biocore_sycl PRIVATE -O2 -Wall)

    add_executable(test_bion tests/test_bion.cpp)
    target_compile_options(test_bion PRIVATE -O2 -Wall)
    target_link_libraries(test_bion biocore_sycl)
    add_test(NAME test_bion COMMAND test_bion)

    add_executable(test_gpu tests/test_gpu.cpp)
    target_link_libraries(test_gpu biocore_sycl)
    target_compile_options(test_gpu PRIVATE -O2 -Wall)
    add_test(NAME test_gpu COMMAND test_gpu)

    add_executable(test_cpu_gpu_parity tests/test_cpu_gpu_parity.cpp)
    target_link_libraries(test_cpu_gpu_parity biocore_sycl)
    target_compile_options(test_cpu_gpu_parity PRIVATE -O2 -Wall)
    add_test(NAME test_cpu_gpu_parity COMMAND test_cpu_gpu_parity)
endif()
