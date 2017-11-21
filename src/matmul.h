/*
* Copyright (c) 2017, BSC (Barcelona Supercomputing Center)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the <organization> nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY BSC ''AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _MATMUL_H_
#define _MATMUL_H_

#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#ifdef USE_MKL
#  include <mkl.h>
#elif USE_OPENBLAS
#  include <cblas.h>
#endif

#define FALSE (0)
#define TRUE (1)
#define VAL_A 587
#define VAL_B 437
#define VAL_C 0

// Global variables
unsigned int bsize, b2size, check_ok;
const float threshold = 1e-4;

// Elements type and MKL/OpenBLAS interface
#if defined(USE_FLOAT)
   typedef float      elem_t;
#  define  GEMM       SGEMM
#  define  cblas_gemm cblas_sgemm
#else
   typedef double     elem_t;
#  define  GEMM       DGEMM
#  define  cblas_gemm cblas_dgemm
#endif /* defined(USE_FLOAT) */

void usage (char* argv0) {
   fprintf(stderr, "USAGE:\t%s <matrix size> [<check>]\n", argv0);
   fprintf(stderr, "      \t<block size> is fixed to %u\n", bsize);
}

double wall_time () {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC,&ts);
   return (double) (ts.tv_sec) + (double) ts.tv_nsec * 1.0e-9;
}

#endif /* _MATMUL_H_ */
