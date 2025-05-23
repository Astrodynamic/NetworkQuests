# Legacy L4NetworkQuests Project

This directory contains the original L4NetworkQuests project that used FlatBuffers for network communication.

## Original Project Structure

This was the starting point for NetworkQuests - a project that demonstrated network programming using FlatBuffers for serialization.

### Components

- **flatbuffers/**: FlatBuffers schema definitions and CMake configuration
- **tcp/**: TCP client and server implementations using FlatBuffers
- **udp/**: UDP client and server implementations using FlatBuffers

### Note

This legacy code is preserved for historical reference and compatibility. The main NetworkQuests project has evolved into a comprehensive network protocol educational library with modern C++20 implementations.

To build the legacy project:

```bash
cd examples/legacy
mkdir build && cd build
cmake ..
make
```

### Dependencies

- FlatBuffers library
- C++17 compiler
- CMake 3.15+