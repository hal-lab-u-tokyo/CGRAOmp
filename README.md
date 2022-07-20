![MIT license](https://img.shields.io/github/license/hal-lab-u-tokyo/CGRAOmp?style=plastic)
![build workflow](https://github.com/hal-lab-u-tokyo/CGRAOmp/actions/workflows/build.yml/badge.svg)
![latest release](https://img.shields.io/github/v/release/hal-lab-u-tokyo/CGRAOmp)
# CGRAOmp: OpenMP Compiler for CGRAs
CGRAOmp is a front-end to compile OpenMP codes for Coarse-Grained Reconfigurable Architectures (CGRAs) based on LLVM.

[The API document](https://tkojima0107.github.io/CGRAOmp-docs-dev/index.html) generated by doxgen is available.

To use this tool, you can either use docker images or build the LLVM libraries on your machine.

# Publication
1. Takuya Kojima, Boma Adhi, Carlos Cortes, Yiyu Tan, Kentaro Sano, "An Architecture-Independent CGRA Compiler enabling OpenMP Applications", 2022 IEEE International Parallel and Distributed Processing Symposium Workshops (IPDPSW), virtual, May 2022. 

# Using CGRAOmp with Docker
There are two options: 1) using prebuilt images and 2) building images with Dockerfile.

## Prebuilt images
You can pull the prebuilt images from DockerHub.

### CentOS based image
```
$ docker pull tkojima0107/cgraomp:centos
```

### Ubuntu 20.04 based image
```
$ docker pull tkojima0107/cgraomp:ubuntu-20.04
```

They uses [llvm_docker](https://github.com/tkojima0107/llvm_docker/) as base images.


## Building these images
You can build the above images with Dockerfiles in [docker](./docker)

# Building CGRAOmp 
## Prerequisites
### Installation of LLVM 12.0.x
CGRAOmp consists of several out-of-tree Passes.
In the beginning, you need to build and install LLVM 12.0.x according to the official guide (see [llvm-project](https://github.com/llvm/llvm-project)).

### Python3 packages (Optional)
Compiler driver needs the following python packages:
* To visualize data flow graph
	1. graphviz
	1. watchdog
* To run backend mapping process
	1. watchdog
	1. rich (for rich console mode)

# Build and Install
```
git clone https://github.com/hal-lab-u-tokyo/CGRAOmp.git
cd CGRAOmp
mkdir build
cmake .. [-DCMAKE_INSTALL_PREFIX=<path>] [-DCGRAOMP_ENABLE_DOXYGEN=On]
make -j`proc`
make install
```

If you want to change the path where the tools and libraries are installed, please specify the directory path by `-DCMAKE_INSTALL_PREFIX=<path>`.

If you want to build the API document with Doxgen, please use `-DCGRAOMP_ENABLE_DOXYGEN=On`.


# Architecture types
The target architecture is defined by a JSON file.
This repository includes the following types in [share/presents](share/presets/)

1. Decoupled CGRA
2. Time-Multiplexed CGRA (Experimental)

# Usage

## Sample code (vec_madd.c)
```
#define N 100
int X[N], Y[N], Z[N]
const int a = 3;

#pragma omp target parallel for map(to:A[0:N],B[0:N]) map(from:C[0:N])
for (int i = 0; i < N; i++) {
	Z[i] = X[i] * a + Y[i];
}
```
## Compilation commmad
```
$ cgraomp-cc vec_madd.c --cgra-config decoupled_affine_AG.json
```
If no error occurs, it will generate a dot file as a data flow graph.

## How to connect your back-end mapper
This compiler only extracts a data flow graph of the OpenMP offloading kernel.
To run mapping with the generated DFGs automatically, a script file is needed.

```
#!/bin/sh

your_mapping_command ${DOTFILE_NAME}
```

In the script, an environment variable `${DOTFILE_NAME}` provides an actual file name of the generated DFG file.

## Options
### Necessary option
* `--cgra-config` (`-cc`): specify the path of CGRA configuration file

###  Generation options
* `-o`: specifies the output file name
* `-v`: enables verbose mode
* `-save-temps`: saves temporary files during the compilation
* `--enable-cgraomp-debug`: shows debug messsage in the CGRA OpenMP pass

### Options for host code or pre-optimization
* `-O(0|1|2|3|s|z)`: optimization level for host code
* `-Xclang=<arg>`: passes an argument for clang
* `--preopt-pipeline`: changes optimizaiton pipeline in pre-optimization stage

### Options for CGRAOmp Pass
* `--enable-custom-inst`: enables custom instruction support
* `--diagnostic-file=<path>`: specifies the file path of a diagnostic file
* `-Xcgraomp=<arg>`: passes other options for CGRAOmp passes

### Options for DFG generation
* `--load-dfg-pass-plugin=<path>`: loads DFG pass plugins
* `--dfg-pass-pipeline`: specifies the pass pipeline for DFG optimization (comma-separeted)
* `--dfg-file-prefix`: specifies the prefix used for data flow graph file name
* `--visualize-dfg`: generates image files of DFGs (graphviz is needed)
* `--visualize-dfg-type`: specifies file type of the visualized file (default: png)
* `--simplify-dfg-name`: uses simplified file name for generated DFG files
* `--cgra-dfg-plain`: uses plain node label

### Options for backend process
* `--backend-runner`: specifies a runner script to drive a back-end mapping
* `--backend-runner-command`: specifies command to run the runner script (default: bash)
* ` --backend-proc-num`: specifies the number of backend process to run simulatanceously (default: 1)
* `--backend-panel-num`: specifies the number of panel to view the backend process stdout (only in rich console mode)
* `--backend-nowait`: finishes immediately when all the backend processes are finished
* `--no-rich-console`: disables rich console mode

# Related tools
* [GenMap](https://github.com/hal-lab-u-tokyo/GenMap): a mapping algorith based on genetic algortihm


# License
CGRAOmp is published under the MIT license (see [LICENSE](./LICENSE) file).
