# pgasus
[![Build Status](https://travis-ci.org/osmhpi/pgasus.svg?branch=master)](https://travis-ci.org/osmhpi/pgasus)

A C++ parallel programming framework for NUMA systems, based on PGAS semantics

# Dependencies
Following dependencies are required for building PGASUS on Ubuntu systems:
* cmake (minimum version 3.1)
* a C++11 conforming C++ compiler (tested with gcc/g++ and clang)
* libnuma-dev, libhwloc-dev
* Boost 1.60, if you want to build the tasking module (default). Current Ubuntu releases include newer, incompatible versions of Boost, so you probably have to provide your own build.

Additionally, following optional dependencies may be provided:
* For tests/benchmarks: libtbb-dev, zlib1g-dev
* For the source documentation: doxygen
