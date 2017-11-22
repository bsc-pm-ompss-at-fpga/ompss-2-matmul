# Matmul

**Name**: Matrix Multiplication Kernel  
**Contact Person**: PM Group, pm-tools@bsc.es  
**License Agreement**: GPL  
**Platform**: OmpSs  
[![build status](https://pm.bsc.es/gitlab/applications/ompss/matmul/badges/master/build.svg)](https://pm.bsc.es/gitlab/applications/ompss/matmul/commits/master)


### Description
This application performs the multiplication of two square matrices. The matrices are allocated by blocks of contiguous memory.

The task implementation may be changed if support for some external library is enabled at compile time. Otherwise, a basic implementation is provided. See the *Build variables* section for more information.

### Build instructions
Clone the repository:
```
git clone https://pm.bsc.es/gitlab/applications/ompss/matmul.git
cd matmul
```

Build the application binaries:
```
make
```
##### Build variables
You can change the build process defining or modifying some environment variables.
The supported ones are:
  - `CFLAGS`
    - `-DUSE_DOUBLE`. Defining the `USE_DOUBLE` variable the matix elements are of type `double` instead of `float`.
    - `-DTIMING_ALL`. Defining the `TIMING_ALL` variable 'taskifies' the matrices initialization and the result checking by blocks. In addition, the timing shown at the execution end will include those new tasks.
  - `LDFLAGS`
  - `MCC`. If not defined, the default value is: `mcc`. However, for SMP machines we recommend the use of `smpcc`.
  - `CROSS_COMPILE`
  - `MKL_DIR`. Installation directory of MKL library. The default value is: `$MKLROOT`.
    - `MKL_INC_DIR`. Installation directory of includes for MKL library. The default value is: `$MKL_DIR/include`.
    - `MKL_LIB_DIR`. Installation directory of OS libraries for MKL library. The default value is: `$MKL_DIR/lib`.
  - `OPENBLAS_DIR`. Installation directory of OpenBLAS library. The default value is: `$OPENBLAS_HOME`.
    - `OPENBLAS_INC_DIR`. Installation directory of includes for OpenBLAS library. The default value is: `$OPENBLAS_DIR/include`.
    - `OPENBLAS_LIB_DIR`. Installation directory of OS libraries for OpenBLAS library. The default value is: `$OPENBLAS_DIR/lib`.

To check the correct support detection of backend libraries, you can use the `make info` target once the environment variables are properly set.

For example, the build step to cross-compile the application for ARM using the `smpcc` profile may be:
```
export MCC=smpcc
export CROSS_COMPILE=arm-linux-gnueabihf-
make
```

### Run instructions
The name of each binary file created by build step ends with a suffix which determines the version:
 - program-p: performance version
 - program-i: instrumented version
 - program-d: debug version

All versions use the same arguments structure:
```
./sparselu <matrix size> <block size> [<check>]
```
where:
 - `matrix size` is the dimension of the matrices. (Mandatory)
 - `block size` is the dimension of the matrices sub-blocks. (Mandatory)
 - `check` defines if the result must be checked. (Optional)
