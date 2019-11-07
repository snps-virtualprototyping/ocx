# OpenCpuX - Open Source Instruction-Set Simulation Integration Kit

## Overview

### Why
* The open source community has a wealth of different CPU models, each with 
  their own API
* Similarly, various open source (and commercial) virtual prototype simulation 
  frameworks exists, again each with their own APIs
* Re-using CPU models across these frameworks requires individual, laborious 
  adaptation 

### What
* OpenCpuX provides a standard, open source set of APIs for Virtualizer 
  customers to integrate CPU models into a (virtual prototype) simulator
* The APIs are freely accessible under an open source license
* Synopsys will deliver a production implementation of the Simulator 
  Adaptor for Virtualizer

### Benefits
* Foster re-use of CPU models across various simulation frameworks and 
  reduces repetitive adaptation work

## How to build

* Clone the repository and `cd` into the repository
* Initialize and update the [googletest](https://github.com/google/googletest) 
  submodule:

        git submodule init
        git submodule update

* Create a `BUILD` directory

        mkdir BUILD
        cd BUILD

* Run [CMake](https://cmake.org), then `make` to build both the test harness 
  and the dummy core in x64 mode:

        CXX="g++ -m64" CC="gcc -m64" cmake ..
        make

* As a sanity test, you can try and run the test harness against 
  the dummy core integration. As this core integration does not 
  actually implement any core behavior, only the most basic test is executed
  by the `test` target

        make test

        Start 1: smoke
        1/1 Test #1: smoke ............................   Passed    0.00 sec

        100% tests passed, 0 tests failed out of 1

        Total Test time (real) =   0.01 sec


