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

#include <stdlib.h>
#include <unistd.h>

// General definitions
#include "matmul.h"
// General definitions which are required to build the FPGA accelerators
#include "matmul.fpga.h"

#if defined(TIMING_ALL)
#  if defined(USE_DMA_MEM)
#    pragma omp task device(smp) no_copy_deps
#    pragma omp task out([BSIZE*BSIZE]v)
#  else
#    pragma omp task device(smp) copy_deps
#    pragma omp task out([BSIZE*BSIZE]v)
#  endif //defined(USE_DMA_MEM)
#endif
void setBlock (elem_t* v, const elem_t val) {
   for (unsigned int i = 0; i < BSIZE*BSIZE; ++i) {
      v[i] = val;
   }
}

#if defined(TIMING_ALL)
#  if defined(USE_DMA_MEM)
#    pragma omp task device(smp) no_copy_deps
#    pragma omp task in([BSIZE*BSIZE]v)
#  else
#    pragma omp task device(smp) copy_deps
#    pragma omp task in([BSIZE*BSIZE]v)
#  endif //defined(USE_DMA_MEM)
#endif
void checkBlock (unsigned int * check_ok, elem_t* v, const elem_t val, const float threshold )
{
   const elem_t maxv = val * (1.0 + (val < 0 ? -threshold : threshold));
   const elem_t minv = val * (1.0 - (val < 0 ? -threshold : threshold));
   for (unsigned int i = 0; i < BSIZE*BSIZE && ( *check_ok ); ++i) {
      elem_t tmp = v[i];
      if (tmp > maxv || tmp < minv) {
         *check_ok = FALSE;
         fprintf(stderr, "ERROR:\t Expected a %lf but found %lf.\n", (double)val, (double)tmp);
      }
   }
}

#if defined(USE_DMA_MEM)
#  pragma omp target device(fpga) no_copy_deps onto(0) num_instances(1)
#  pragma omp task in([BSIZE*BSIZE]a, [BSIZE*BSIZE]b) inout([BSIZE*BSIZE]c)
#else
#  pragma omp target device(fpga) copy_deps onto(0) num_instances(1)
#  pragma omp task in([BSIZE*BSIZE]a, [BSIZE*BSIZE]b) inout([BSIZE*BSIZE]c)
#endif //defined(USE_DMA_MEM)
void matmulBlock(elem_t *a, elem_t *b, elem_t *c) {
   unsigned int i, j, k;

#pragma HLS array_partition variable=a cyclic factor=BSIZE/2
#pragma HLS array_partition variable=b block factor=BSIZE/2
   for (i = 0; i < BSIZE; i++) {
      for (j = 0; j < BSIZE; j++) {
#pragma HLS pipeline II=1
         elem_t sum = c[i*BSIZE + j];
         for (k = 0; k < BSIZE; k++) {
            sum += a[i*BSIZE + k] * b[k*BSIZE + j];
         }
         c[i*BSIZE + j] = sum;
      }
   }
}

#if defined(USE_IMPLEMENTS)
#  if defined(USE_DMA_MEM)
#    pragma omp target device(smp) no_copy_deps implements(matmulBlock)
#    pragma omp task in([BSIZE*BSIZE]a, [BSIZE*BSIZE]b) inout([BSIZE*BSIZE]c)
#  else
//#    pragma omp target device(smp) copy_deps implements(matmulBlock)
#    pragma omp target device(smp) no_copy_deps implements(matmulBlock) copy_inout([BSIZE*BSIZE]c)
#    pragma omp task in([BSIZE*BSIZE]a, [BSIZE*BSIZE]b) inout([BSIZE*BSIZE]c)
#  endif //defined(USE_DMA_MEM)
void matmulBlockSmp(elem_t *a, elem_t *b, elem_t *c) {
#if defined(USE_MKL)
   elem_t const alpha = 1.0;
   elem_t const beta = 1.0;
   char const transa = 'n';
   char const transb = 'n';
   GEMM(&transa, &transb, &BSIZE, &BSIZE, &BSIZE, &alpha, a,
         &BSIZE, b, &BSIZE, &beta, c, &BSIZE);
#elif defined(USE_OPENBLAS)
   elem_t const alpha = 1.0;
   elem_t const beta = 1.0;
   cblas_gemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, BSIZE, BSIZE,
      BSIZE, alpha, a, BSIZE, b, BSIZE, beta, c, BSIZE);
#else
   for (unsigned int i = 0; i < BSIZE; ++i) {
      for (unsigned int j = 0; j < BSIZE; ++j) {
         elem_t l = 0;
         for (unsigned int k = 0; k < BSIZE; ++k) {
            l += a[i*BSIZE + k] * b[k*BSIZE + j];
         }
         c[i*BSIZE + j] += l;
      }
   }
#endif
}
#endif

int main(int argc, char** argv) {
   if (argc < 2) {
      usage(argv[0]);
      exit(1);
   }

   unsigned int const b2size = BSIZE*BSIZE;
   unsigned int const msize = atoi(argv[1]);
   unsigned int const m2size = msize*msize;
   unsigned char const check = argc > 2 ? atoi(argv[2]) : 1;
   if (msize%BSIZE != 0) {
      fprintf(stderr, "ERROR:\t<matrix size> must be multiple of <block size>\n");
      usage(argv[0]);
      exit(1);
   }

   size_t s = m2size*sizeof(elem_t);
#if defined(USE_DMA_MEM)
   elem_t* a = (elem_t *)(nanos_fpga_alloc_dma_mem(s));
   elem_t* b = (elem_t *)(nanos_fpga_alloc_dma_mem(s));
   elem_t* c = (elem_t *)(nanos_fpga_alloc_dma_mem(s));
#else
   elem_t* a = (elem_t *)(malloc(s));
   elem_t* b = (elem_t *)(malloc(s));
   elem_t* c = (elem_t *)(malloc(s));
#endif //defined(USE_DMA_MEM)
   if (a == NULL || b == NULL || c == NULL) {
      fprintf(stderr, "ERROR:\tCannot allocate memory for the matrices\n");
      exit(1);
   }

#if defined(TIMING_ALL)
   double t_ini_start = wall_time();
#endif

   for (unsigned int i = 0; i < m2size/b2size; i++) {
      setBlock(&a[i*b2size], (elem_t)VAL_A + i);
      setBlock(&b[i*b2size], (elem_t)VAL_B - i);
      setBlock(&c[i*b2size], (elem_t)VAL_C);
   }

#if defined(TIMING_ALL)
   #pragma omp taskwait
#endif
   double t_start = wall_time();

   for (unsigned int i = 0; i < msize/BSIZE; i++) {
//NOTE: Assuming that the following task will be executed in a shared memory environment.
//      Otherwise, it must define the input and output data of child tasks.
#pragma omp task firstprivate(i)
{
      for (unsigned int j = 0; j < msize/BSIZE; j++) {
         unsigned int const ci = j*b2size + i*BSIZE*msize;
         for (unsigned int k = 0; k < msize/BSIZE; k++) {
            unsigned int const ai = k*b2size + i*BSIZE*msize;
            unsigned int const bi = j*b2size + k*BSIZE*msize;
            matmulBlock(&a[ai], &b[bi], &c[ci]);
         }
      }
      #pragma omp taskwait
}
   }

   #pragma omp taskwait
   double t_end = wall_time();

   unsigned int check_ok = TRUE;
   if (check) {
      printf( "=================== CHECKING ===================== \n" );
      for (unsigned int i = 0; i < msize/BSIZE && check_ok; i++) {
         for (unsigned int j = 0; j < msize/BSIZE && check_ok; j++) {
            elem_t val = VAL_C;
            for (unsigned int k = 0; k < msize/BSIZE; k++) {
               unsigned int const ai = k*b2size + i*BSIZE*msize;
               unsigned int const bi = j*b2size + k*BSIZE*msize;
               val += a[ai]*b[bi]*BSIZE;
            }
            unsigned int const ci = j*b2size + i*BSIZE*msize;
            checkBlock(&check_ok, &c[ci], val, THRESHOLD);
         }
      }
   }

#if defined(TIMING_ALL)
   #pragma omp taskwait
   double t_check_end = wall_time();
#endif

   if (check) {
      if (check_ok) {
         printf( "Output matrix is OK!\n" );
      } else {
         printf( "Output matrix is Wrong!\n" );
      }
      printf( "================================================== \n" );
   }

#if defined(USE_DMA_MEM)
   nanos_fpga_free_dma_mem(a);
   nanos_fpga_free_dma_mem(b);
   nanos_fpga_free_dma_mem(c);
#else
   free(a);
   free(b);
   free(c);
#endif //defined(USE_DMA_MEM)

   printf( "==================== RESULTS ===================== \n" );
   printf( "  Benchmark: %s (%s)\n", "Matmul", "OmpSs" );
   printf( "  Elements type: %s\n", ELEM_T_STR );
   printf( "  Execution time (secs): %f\n", t_end - t_start );
#if defined(TIMING_ALL)
   printf( "  Initialization time (secs): %f\n", t_start - t_ini_start );
   printf( "  Checking time (secs): %f\n", t_check_end - t_end );
#endif //defined(TIMING_ALL)
   printf( "================================================== \n" );

}

#if defined(USE_DMA_MEM) && defined(USE_IMPLEMENTS)
#  warning Using DMA memory (-DUSE_DMA_MEM) when using the implements option  (-DUSE_IMPLEMENTS). The performance of SMP tasks may be wery poor.
#endif //defined(USE_DMA_MEM) && defined(USE_IMPLEMENTS)
