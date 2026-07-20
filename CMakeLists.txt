cmake_minimum_required(VERSION 3.20)
project(MetricFieldEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mfe_core
    src/core/metric.cpp
    src/core/mesh.cpp
    src/core/dec.cpp
    src/core/ricci_flow.cpp
    src/transport/power_diagram.cpp
    src/transport/ot_placer.cpp
    src/transport/legalizer.cpp
    src/thermo/thermal_solver.cpp
    src/geodesic/fmm_router.cpp
    src/logic/sat_window.cpp
    src/cts/dme_cts.cpp
    src/pdk/technology.cpp
    src/diagnostics/congestion.cpp
    src/flow/contracts.cpp
    src/flow/evidence_graph.cpp
    src/flow/manifest.cpp
    src/flow/rtl_importer.cpp
    src/flow/physical_flow.cpp
    src/flow/opensta_importer.cpp
    src/flow/openrcx_importer.cpp
    src/eco/eco_planner.cpp
    src/eco/eco_validator.cpp
    src/io/lef_def.cpp
    src/io/bookshelf.cpp
    src/io/netlist_importer.cpp
    src/io/routed_def_writer.cpp
    src/io/gdsii_writer.cpp
    src/io/oasis_writer.cpp
    src/io/artifacts.cpp
    src/analysis/routing_estimator.cpp
)
find_package(Eigen3 REQUIRED)
target_include_directories(mfe_core PUBLIC include)
target_link_libraries(mfe_core PUBLIC Eigen3::Eigen)

if(MSVC)
    target_compile_options(mfe_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(mfe_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(mfe apps/mfe/main.cpp)
target_link_libraries(mfe PRIVATE mfe_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(mfe_core_tests tests/core_tests.cpp)
    target_link_libraries(mfe_core_tests PRIVATE mfe_core)
    add_test(NAME mfe_core_tests COMMAND mfe_core_tests)

    add_executable(ricci_tests tests/ricci_tests.cpp)
    target_link_libraries(ricci_tests PRIVATE mfe_core)
    add_test(NAME ricci_tests COMMAND ricci_tests)

    add_executable(ot_tests tests/ot_tests.cpp)
    target_link_libraries(ot_tests PRIVATE mfe_core)
    add_test(NAME ot_tests COMMAND ot_tests)

    add_executable(thermal_tests tests/thermal_tests.cpp)
    target_link_libraries(thermal_tests PRIVATE mfe_core)
    add_test(NAME thermal_tests COMMAND thermal_tests)

    add_executable(routing_tests tests/routing_tests.cpp)
    target_link_libraries(routing_tests PRIVATE mfe_core)
    add_test(NAME routing_tests COMMAND routing_tests)

    add_executable(sat_cts_tests tests/sat_cts_tests.cpp)
    target_link_libraries(sat_cts_tests PRIVATE mfe_core)
    add_test(NAME sat_cts_tests COMMAND sat_cts_tests)

    add_executable(bookshelf_tests tests/bookshelf_tests.cpp)
    target_link_libraries(bookshelf_tests PRIVATE mfe_core)
    add_test(NAME bookshelf_tests COMMAND bookshelf_tests)

    add_executable(flow_contract_tests tests/flow_contract_tests.cpp)
    target_link_libraries(flow_contract_tests PRIVATE mfe_core)
    add_test(NAME flow_contract_tests COMMAND flow_contract_tests)

    add_executable(evidence_graph_tests tests/evidence_graph_tests.cpp)
    target_link_libraries(evidence_graph_tests PRIVATE mfe_core)
    add_test(NAME evidence_graph_tests COMMAND evidence_graph_tests)

    add_executable(manifest_tests tests/unit/manifest_tests.cpp)
    target_link_libraries(manifest_tests PRIVATE mfe_core)
    target_compile_definitions(manifest_tests PRIVATE MFE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME manifest_tests COMMAND manifest_tests)

    add_executable(netlist_importer_tests tests/integration/netlist_importer_tests.cpp)
    target_link_libraries(netlist_importer_tests PRIVATE mfe_core)
    target_compile_definitions(netlist_importer_tests PRIVATE MFE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME netlist_importer_tests COMMAND netlist_importer_tests)

    add_executable(rtl_importer_tests tests/unit/rtl_importer_tests.cpp)
    target_link_libraries(rtl_importer_tests PRIVATE mfe_core)
    add_test(NAME rtl_importer_tests COMMAND rtl_importer_tests)

    add_executable(physical_flow_tests tests/integration/physical_flow_tests.cpp)
    target_link_libraries(physical_flow_tests PRIVATE mfe_core)
    target_compile_definitions(physical_flow_tests PRIVATE MFE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME physical_flow_tests COMMAND physical_flow_tests)

    add_executable(timing_eco_tests tests/integration/timing_eco_tests.cpp)
    target_link_libraries(timing_eco_tests PRIVATE mfe_core)
    add_test(NAME timing_eco_tests COMMAND timing_eco_tests)

    add_test(NAME mfe_cli_help COMMAND mfe --help)
    add_test(NAME mfe_cli_flow COMMAND mfe thermo)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME mfe_agent_cli COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/agent_cli_test.py)
    endif()
endif()
