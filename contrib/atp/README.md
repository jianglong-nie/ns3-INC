# ATP Module for ns-3

This module implements a custom transport protocol (ATP) for ns-3 network simulator.

## Overview

The ATP module provides a complete implementation of a custom transport protocol, including:
- Core protocol implementation
- Network devices (CSMA and Bridge)
- Application layer support
- Routing capabilities
- Various helper classes for easy configuration

## Components

### Core Protocol
- Basic ATP protocol implementation
- Protocol headers and tags
- L4 protocol layer
- Socket implementation
- Congestion control
- Transmission buffer

### Network Devices
- ATP CSMA device and channel
- ATP Bridge device and channel

### Applications
- Bulk send application
- Packet sink application

### Routing
- Static routing implementation

### Helper Classes
- Various helper classes for configuration
- Data aggregator

## Dependencies
- ns-3 Internet module
- ns-3 Applications module
- ns-3 CSMA module
- ns-3 Bridge module

## Building
This module is part of the ns-3 build system. To build it:

1. Configure ns-3 with this module enabled
2. Build ns-3 as usual

## Usage
The module provides helper classes to simplify the configuration and usage of ATP components in ns-3 simulations.

## Testing
The module includes a test suite (`test/atp-test-suite.cc`) for testing the implementation.

## License
This module is part of ns-3 and follows the same licensing terms as ns-3. 