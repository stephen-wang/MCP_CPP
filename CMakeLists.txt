cmake_minimum_required(VERSION 3.21)

project(mcp_cpp
    VERSION 0.1.0
    DESCRIPTION "C++ SDK for the Model Context Protocol"
    LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(MCP_CPP_BUILD_SHARED "Build mcp_cpp as a shared library" OFF)
option(MCP_CPP_BUILD_EXAMPLES "Build example executables" ON)

if(MCP_CPP_BUILD_SHARED)
    set(MCP_CPP_LIBRARY_TYPE SHARED)
else()
    set(MCP_CPP_LIBRARY_TYPE STATIC)
endif()

add_library(mcp_cpp ${MCP_CPP_LIBRARY_TYPE}
    src/client.cpp
    src/server.cpp)

add_library(mcp::mcp_cpp ALIAS mcp_cpp)

set_target_properties(mcp_cpp PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME mcp_cpp)

target_include_directories(mcp_cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(MSVC)
    target_compile_options(mcp_cpp PRIVATE /W4 /permissive-)
else()
    target_compile_options(mcp_cpp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MCP_CPP_BUILD_EXAMPLES)
    add_executable(mcp_cpp_client_sample
        examples/client_sample.cpp)

    add_executable(mcp_cpp_server_sample
        examples/server_sample.cpp)

    set_target_properties(mcp_cpp_client_sample PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    set_target_properties(mcp_cpp_server_sample PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    target_link_libraries(mcp_cpp_client_sample PRIVATE mcp_cpp)
    target_link_libraries(mcp_cpp_server_sample PRIVATE mcp_cpp)
endif()

if(BUILD_TESTING)
    add_executable(mcp_cpp_client_smoke_test
        tests/client_smoke_test.cpp)

    add_executable(mcp_cpp_server_smoke_test
        tests/server_smoke_test.cpp)

    set_target_properties(mcp_cpp_client_smoke_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    set_target_properties(mcp_cpp_server_smoke_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    target_link_libraries(mcp_cpp_client_smoke_test PRIVATE mcp_cpp)
    target_link_libraries(mcp_cpp_server_smoke_test PRIVATE mcp_cpp)

    add_test(NAME mcp_cpp.client_smoke_test COMMAND mcp_cpp_client_smoke_test)
    add_test(NAME mcp_cpp.server_smoke_test COMMAND mcp_cpp_server_smoke_test)
endif()

install(TARGETS mcp_cpp
    EXPORT mcp_cppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT mcp_cppTargets
    FILE mcp_cppTargets.cmake
    NAMESPACE mcp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mcp_cpp)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mcp_cppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/mcp_cppConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mcp_cpp)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/mcp_cppConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mcp_cppConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/mcp_cppConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mcp_cpp)
