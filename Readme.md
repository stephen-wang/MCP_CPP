# MCP C++ SDK

A modern C++ SDK for building [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) clients, servers, and tools.

## Overview

`mcp_cpp` is intended to provide a clean, type-safe, and portable C++ foundation for implementing MCP integrations. The goal is to make it straightforward to:

- build MCP servers that expose tools, resources, and prompts
- build MCP clients that connect to local or remote MCP servers
- work with MCP messages and protocol types using idiomatic modern C++
- integrate MCP into native applications, services, and developer tooling

## Project Goals

This project is designed around a few core goals:

- **Modern C++ API** — ergonomic interfaces using C++20 or later
- **Protocol-first design** — strong support for MCP schemas, messages, and lifecycle flows
- **Transport abstraction** — support for transports such as stdio, pipes, sockets, or HTTP-based integrations
- **Extensibility** — easy customization for logging, serialization, executors, and application glue code
- **Cross-platform portability** — practical support for macOS, Linux, and Windows
- **Production readiness** — predictable error handling, validation, and testability

## Planned Feature Set

The SDK is expected to cover the following areas:

### Core Protocol Support

- MCP message models
- request / response / notification handling
- protocol version negotiation
- capability advertisement
- structured error handling

### Client SDK

- connect to MCP servers
- send requests and handle responses
- subscribe to server notifications
- discover tools, resources, and prompts

### Server SDK

- register tools, resources, and prompts
- expose server capabilities
- validate inbound requests
- simplify request routing and handler wiring

### Platform and Infrastructure

- pluggable transport layer
- JSON serialization and parsing
- configurable logging hooks
- test utilities and mock transports

## Current Status

This repository is currently in the **bootstrap / early development** stage.

At this point, the main purpose of the project is to define the SDK direction, structure, and implementation plan. As source code is added, this README should evolve with concrete examples, API references, and build instructions.

## Design Principles

- Prefer explicit and type-safe APIs over overly implicit behavior.
- Keep protocol models close to the MCP specification.
- Separate transport concerns from protocol concerns.
- Make synchronous and asynchronous integration both possible.
- Avoid unnecessary framework lock-in.
- Keep the public surface small, stable, and well documented.

## Proposed Repository Layout

As implementation grows, the repository may follow a structure similar to:

```text
mcp_cpp/
├── CMakeLists.txt
├── include/
│   └── mcp/
│       ├── client/
│       ├── server/
│       ├── protocol/
│       ├── transport/
│       └── utils/
├── src/
├── tests/
├── examples/
└── docs/
```

## Toolchain Expectations

Recommended baseline:

- **Language:** C++20 or newer
- **Build system:** CMake
- **Compiler:** Clang, GCC, or MSVC with solid C++20 support
- **Testing:** a unit test framework such as Catch2 or GoogleTest
- **Formatting / linting:** `clang-format` and optional `clang-tidy`

## Build Strategy

Current build workflow:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Useful CMake options:

- `-DMCP_CPP_BUILD_SHARED=ON` — build a shared library instead of a static one
- `-DMCP_CPP_BUILD_EXAMPLES=ON` — build the sample executable target
- `-DBUILD_TESTING=ON` — enable the smoke-test target and CTest integration

## Sample Executable

The repository includes a small sample target that shows how to create a `Client`,
attach a transport, and send a notification.

Build and run it with:

```bash
cmake -S . -B build -DMCP_CPP_BUILD_EXAMPLES=ON
cmake --build build --target mcp_cpp_client_sample
./build/mcp_cpp_client_sample
```

See [examples/client_sample.cpp](examples/client_sample.cpp).

## Test Scaffolding

The project also includes a lightweight smoke test that validates the current
client lifecycle against a recording transport.

Run the test suite with:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

See [tests/client_smoke_test.cpp](tests/client_smoke_test.cpp).

## Install and Export Usage

Install the library locally:

```bash
cmake -S . -B build
cmake --build build
cmake --install build --prefix ./install
```

Consume the installed package from another CMake project:

```cmake
find_package(mcp_cpp CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE mcp::mcp_cpp)
```

If the package is installed in a non-standard location, point CMake at it with
`CMAKE_PREFIX_PATH`:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/mcp_cpp/install
```

## Intended Use Cases

`mcp_cpp` is a good fit for:

- native desktop applications integrating MCP
- command-line tools that act as MCP clients or servers
- backend services exposing internal capabilities through MCP
- C++ wrappers around model tooling and local developer workflows

## Roadmap

Short-term priorities:

1. define core protocol types
2. establish CMake project structure
3. add JSON serialization layer
4. implement basic stdio transport
5. provide minimal client and server examples
6. add unit tests and CI

Longer-term priorities:

- async execution model integration
- richer transport options
- stronger validation utilities
- improved diagnostics and tracing
- packaged releases and versioning strategy

## Contributing

Contributions are welcome as the project takes shape. Good early contributions include:

- protocol model design
- transport abstractions
- example applications
- tests and CI setup
- documentation improvements

When contributing, aim to keep changes focused, documented, and aligned with the design principles above.

## Documentation

This README is the starting point for the project. Over time, it should be expanded with:

- setup instructions
- architecture notes
- API usage examples
- supported transports
- version compatibility details

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).

## Summary

`mcp_cpp` aims to become a practical and modern C++ SDK for the Model Context Protocol, with clear abstractions for protocol handling, transport integration, and application development.
