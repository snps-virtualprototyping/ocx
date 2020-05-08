# OpenCpuX - Open Source Instruction-Set Simulation Integration Kit 

[![Build Status](https://travis-ci.org/snps-virtualprototyping/ocx.svg?branch=master)](https://travis-ci.org/snps-virtualprototyping/ocx) 
[![Coverity Scan Build Status](https://scan.coverity.com/projects/20236/badge.svg)](https://scan.coverity.com/projects/snps-virtualprototyping-ocx)

## Overview

### Why
* The open source community has a wealth of different CPU models, each with 
  their own API
* Similarly, various open source (and commercial) virtual prototype simulation 
  frameworks exists, again each with their own APIs
* Re-using CPU models across these frameworks requires individual, laborious 
  adaptation 

### What
* OpenCpuX provides a standard, open source set of APIs to integrate CPU models 
into a (virtual prototype) simulator
* The APIs are freely accessible under an open source license

### Benefits
* Foster re-use of CPU models across various simulation frameworks and 
  reduces repetitive adaptation work

## System Requirements
* [CMake](https://cmake.org), version 3.6 or higher
* `gcc` and `g++`

## How to build

### Platform independent

* Clone the repository and `cd` into the repository
* Initialize and update the [googletest](https://github.com/google/googletest) 
  submodule:

        git submodule init
        git submodule update

### For Linux

* Create a `BUILD` directory

        mkdir BUILD
        cd BUILD

* Run [CMake](https://cmake.org), then `make` to build both the test harness 
  and the dummy core in x64 mode:

        CXX="g++ -m64" CC="gcc -m64" cmake ..             # on bash, or
        set CXX="g++ -m64"; set CC="gcc -m64";  cmake ..  # on csh 
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

### For Windows Visual Studio 2017 and up

* Start Visual Studio 

* Use File -> Open Folder... to open the directory to which you have cloned
  the ocx repository.

* Visual Studio will detect that this is a CMake project and will generate the
  necessary build files. Once this has completed ...

* Build -> Build all

* As a sanity test, you can try and run the test harness against
  the dummy core integration. As this core integration does not 
  actually implement any core behavior, only the most basic test is executed
  by the `test` target

        Test -> Run CTest for ocx

        Start 1: smoke
        1/1 Test #1: smoke ............................   Passed    0.16 sec
        100% tests passed, 0 tests failed out of 1
        Total Test time (real) =   0.18 sec
  
