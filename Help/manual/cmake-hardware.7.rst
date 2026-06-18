.. cmake-manual-description: CMake Hardware Acceleration Reference Manual

cmake-hardware(7)
=================

NAME
----

cmake-hardware - CMake NVIDIA CUDA and Multicore CPU Acceleration Manual

DESCRIPTION
-----------

This manual outlines the hardware-accelerated processing blocks embedded within 
the runtime suite. It manages concurrent execution split across host 
**Multicore CPUs** and **NVIDIA GPUs** to optimize high-performance computing loops.

NVIDIA CUDA CONFIGURATION
-------------------------

Hardware-level parallel processing loops run directly inside the NVIDIA GPU Core matrix. 
To pass parameters to the underlying execution engine, leverage the following global 
target options:

``--cuda-arch=<architectures>``
  Overrides the active NVIDIA target GPU microarchitectures. Provide a semicoloned 
  list of target device architectures. Common profiles include:

  * ``80``: NVIDIA Ampere Architecture (e.g., A100, RTX 3090)
  * ``86``: Mobile Ampere Architecture (RTX 30-series laptops)
  * ``89``: NVIDIA Ada Lovelace Architecture (e.g., RTX 40-series)
  * ``90``: NVIDIA Hopper Architecture (e.g., H100)

``--cuda-fast-math``
  Enables the underlying compiler flag (``--use_fast_math``) within the NVIDIA 
  ``nvcc`` pipeline. This forces the device to utilize hardware-level floating-point 
  mathematical shortcuts for rapid vector operations.

MULTICORE CPU OPTIMIZATION (OPENMP)
-----------------------------------

Host task scheduling workloads are handled via OpenMP multi-threading constructs. 
By default, the engine will attempt to spawn a thread pool matching the maximum 
logical processor count available to the shell session.

You can strictly control core consumption parameters by passing the standard 
environment execution token before running a command invocation sequence:

.. code-block:: bash

  # Bind execution loops strictly to exactly 4 logical CPU cores
  export OMP_NUM_THREADS=4
  ./bin/cmake --some-compute-task

UNPRIVILEGED COMPILATION AND LINKING
------------------------------------

Because the build pipeline utilizes specialized compilers and runtime files, 
unprivileged user-space compilation configurations require explicit paths.

To safely construct the binaries without root or administrative access:

1. Map the system paths targeting your localized user-space CUDA Toolkit setup:

   .. code-block:: bash

     export PATH=/usr/local/cuda/bin:$PATH
     export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH

2. Point the CMake initialization sequence directly to the NVIDIA compiler executable binary:

   .. code-block:: bash

     mkdir build && cd build
     ../bootstrap --prefix=$HOME/.local
     cmake .. -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc
     cmake --build . -j$(nproc)
