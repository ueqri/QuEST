# QuEST Distributed GPU version

## Usage

To run this version in multi-GPU environments (either in one node, or across the network), you can use the commands as follows:

```bash
# init MPI environment, e.g. source /path/to/intel/setvars.sh
mkdir build
./build-cuMPI-QuEST.sh empty
mpirun -np 2 build/random
```

if you already build before and `/path/to/QuEST/build` exists, just run `./build-cuMPI-QuEST.sh` in the QuEST root directory for incremental build.

## Details

To implement the distributed GPU version, we mainly referred to the distributed CPU version to find some parallel tactics. And our work is abstracted below:

* Implemented CUDA kernel functions to vectorize all quantum gates in origin v3.0.0, mainly based on state vectors representation method.
* Utilized multi-GPU Direct RDMA to improve network communications (using a self-made runtime library with NCCL + MPI, [public in GitHub](https://github.com/ueqri/cuMPI)).
* Performed overlapped computing on GPU, while swapping data between VRAM and RAM.
* Introduced pipelining by Unified Virtual Addressing to fine-tune some parts, in order to:
  * solve the memory-bound issue
  * extend the probability of GPU acceleration for more QuBit numbers (without UVA, 30 qubits use ~32GB VRAM totally, 31 -> ~64GB, ... the usage of VRAM is increased exponentially)
  * (only tested in `uva-test` branch)

Note: the source files structure in `/path/to/QuEST/QuEST/src/GPU/` **only resembles** `/path/to/QuEST/QuEST/src/CPU/`, but actually not the same. For example, QuEST_gpu_local.cu is not similar to QuEST_cpu_local.c, thus you can't use the former source independently as the latter one to build a single GPU/CPU binary.

# Origin Repository: [QuEST](https://quest.qtechtheory.org)

## Introduction

The **Quantum Exact Simulation Toolkit** is a high performance simulator of universal quantum circuits, state-vectors and density matrices. QuEST is written in C, hybridises OpenMP and MPI, and can run on a GPU. Needing only compilation, QuEST is easy to run both on laptops and supercomputers (in both C and C++), where it can take advantage of multicore, GPU-accelerated and networked machines to quickly simulate circuits on many qubits.

QuEST has a simple interface, independent of its run environment (on CPUs, GPUs or over networks),
```C
hadamard(qubits, 0);

controlledNot(qubits, 0, 1);

rotateY(qubits, 0, .1);
```
though is flexible
```C
Vector v;
v.x = 1; v.y = .5; v.z = 0;
rotateAroundAxis(qubits, 0, 3.14/2, v);
```
and powerful
```C
// sqrt(X) with pi/4 global phase
ComplexMatrix2 u = {
    .real = {{.5, .5}, { .5,.5}},
    .imag = {{.5,-.5}, {-.5,.5}}};
unitary(qubits, 0, u);

int controls[] = {1, 2, 3, 4, 5};
multiControlledUnitary(qureg, controls, 5, 0, u);
```

QuEST can simulate decoherence on mixed states, output [QASM](https://arxiv.org/abs/1707.03429), perform measurements, apply general unitaries with any number of control and target qubits, and boasts cheap/fast access to the underlying numerical representation of the state. QuEST offers precision-agnostic real and imaginary (additionally include `QuEST_complex.h`) number types, the precision of which can be modified at compile-time, as can the target hardware.

Learn more about QuEST at [quest.qtechtheory.org](https://quest.qtechtheory.org), or read the [whitepaper](https://www.nature.com/articles/s41598-019-47174-9). If you find QuEST useful, feel free to cite
```
Jones, T., Brown, A., Bush, I. et al. 
QuEST and High Performance Simulation of Quantum Computers. 
Sci Rep 9, 10736 (2019) doi:10.1038/s41598-019-47174-9
```
```
@article{Jones2019,
  title={{QuEST} and high performance simulation of quantum computers},
  author={Jones, Tyson and Brown, Anna and Bush, Ian and Benjamin, Simon C},
  journal={Scientific reports},
  volume={9},
  number={1},
  pages={1--11},
  year={2019},
  publisher={Nature Publishing Group}
}
```

---------------------------------

## Documentation

Full documentation is available at [quest.qtechtheory.org/docs](https://quest.qtechtheory.org/docs/), and the API is available [here](https://quest-kit.github.io/QuEST/modules.html) (all functions listed [here](https://quest-kit.github.io/QuEST/QuEST_8h.html)). See also the [tutorial](/examples/README.md).

> **For developers**: To regenerate the API doc after making changes to the code, run `doxygen doxyconf` in the root directory. This will generate documentation in `Doxygen_doc/html`, the contents of which should be copied into [`docs/`](/docs/)) 

---------------------------------

## Getting started

QuEST is contained entirely in the files in the `QuEST/` folder. To use QuEST, copy this folder to your computer and include `QuEST.h` in your `C` or `C++` code, and compile using cmake with the provided [CMakeLists.txt file](/CMakeLists.txt). See the [tutorial](/examples/README.md) for an introduction. We also include example [submission scripts](examples/submissionScripts/) for using QuEST with SLURM and PBS. 

### Quick Start

#### MacOS & Linux

MacOS and Linux users can clone this repository to your machine through the terminal:
```bash
git clone https://github.com/quest-kit/QuEST.git
cd QuEST
```
Compile the [example](examples/tutorial_example.c) using
```bash
mkdir build
cd build
cmake ..
make
```
then run it with
```bash
./demo
```

#### Windows 

Windows users should install [Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2019) for Visual Studio, [CMake](https://cmake.org/download/) and [MinGW-w64](https://sourceforge.net/projects/mingw-w64/). 
Then, in a *Developer Command Prompt for VS*, run
```bash
git clone "https://github.com/quest-kit/QuEST.git"
cd QuEST
```
```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
make
```
```bash
demo.exe
```


#### Tests
Additionally, you can run some tests to see if QuEST runs correctly in your environment, using
```bash
make test
```
though this requires Python 3.4+. 

---------------------------------

## Contact

To file a bug report or feature request, [raise a github issue](https://github.com/QuEST-Kit/QuEST/issues). For additional support, email quest@materials.ox.ac.uk. You can view the list of contributors to QuEST in [`AUTHORS.txt`](AUTHORS.txt).

---------------------------------

## Acknowledgements

QuEST uses the [mt19937ar](http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/emt19937ar.html) Mersenne Twister algorithm for random number generation, under the BSD licence. QuEST optionally (by additionally importing `QuEST_complex.h`) integrates the [language agnostic complex type](http://collaboration.cmc.ec.gc.ca/science/rpn/biblio/ddj/Website/articles/CUJ/2003/0303/cuj0303meyers/index.htm) by Randy Meyers and Dr. Thomas Plum

Thanks to [HQS Quantum simulations](https://quantumsimulations.de/) for contributing the `mixDamping` function.

---------------------------------

## Licence

QuEST is released under a [MIT Licence](LICENCE.txt)

---------------------------------

## Related projects -- QuEST utilities and extensions

* [PyQuEST-cffi](https://github.com/HQSquantumsimulations/PyQuEST-cffi): a python interface to QuEST based on cffi developed by HQS Quantum Simulations. Please note, PyQuEST-cffi is currently in the alpha stage and not an official QuEST project.
* [QuESTlink](https://questlink.qtechtheory.org): a Mathematica package allowing symbolic circuit manipulation and high performance simulation with remote accelerated hardware.
