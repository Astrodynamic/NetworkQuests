# FlatBuffers Network Examples

This directory contains the original network examples using FlatBuffers for data serialization.

## Overview

These examples demonstrate basic network programming concepts using:
- **FlatBuffers**: For efficient data serialization
- **TCP/UDP Protocols**: For network communication
- **C++20 Features**: Modern C++ programming practices

## Structure

```
flatbuffers/
├── README.md                 # This file
├── CMakeLists.txt            # Build configuration
├── flatbuffers/              # FlatBuffers schema definitions
│   ├── robot/                # Robot control message schemas
│   │   ├── common.fbs        # Common data structures
│   │   ├── commands.fbs      # Robot command messages
│   │   └── rtd.fbs           # Real-time data messages
│   └── CMakeLists.txt        # Schema build configuration
├── tcp/                      # TCP examples with FlatBuffers
│   ├── server/               # TCP server implementation
│   └── client/               # TCP client implementation
└── udp/                      # UDP examples with FlatBuffers
    ├── server/               # UDP server implementation
    └── client/               # UDP client implementation
```

## Building

To build these examples:

```bash
cd examples/flatbuffers
mkdir build && cd build
cmake ..
make
```

## Requirements

- **FlatBuffers library**: Install using package manager or build from source
- **C++20 compiler**: GCC 10+, Clang 11+, or MSVC 2019+
- **CMake**: Version 3.24+

## Examples

### TCP Communication
```bash
# Start TCP server
./tcp_server

# In another terminal, run TCP client
./tcp_client
```

### UDP Communication
```bash
# Start UDP server
./udp_server

# In another terminal, run UDP client
./udp_client
```

## FlatBuffers Schemas

### Robot Command Messages
- **Vector3D**: 3D position and orientation data
- **MoveCommand**: Robot movement commands (Cartesian and Joint space)
- **StopCommand**: Emergency stop command

### Real-Time Data
- **MotorData**: Individual motor telemetry
- **RTD**: Complete real-time data structure

## Educational Value

These examples are useful for learning:
- FlatBuffers serialization/deserialization
- Network protocol implementation
- Client-server architecture
- Binary data transmission
- Cross-platform networking

## Note

This is the original implementation that served as the foundation for the comprehensive NetworkQuests library. The main library now includes 8 complete protocol implementations with modern C++20 features.