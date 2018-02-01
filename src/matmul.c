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

#include "matmul.h"

#if defined(TIMING_ALL)
#  pragma omp task out([size]v)
#endif
void setBlock (unsigned int const size, elem_t* v, const elem_t val) {
   for (unsigned int i = 0; i < size; ++i) {
      v[i] = val;
   }
}

#if defined(TIMING_ALL)
#  pragma omp task in([size]v) concurrent([1]check_ok)
#endif
void checkBlock (unsigned int const size, unsigned int * check_ok,
   elem_t* v, const elem_t val, const float threshold )
{
   const elem_t maxv = val * (1.0 + (val < 0 ? -threshold : threshold));
   const elem_t minv = val * (1.0 - (val < 0 ? -threshold : threshold));
   for (unsigned int i = 0; i < size && ( *check_ok ); ++i) {
      elem_t tmp = v[i];
      if (tmp > maxv || tmp < minv) {
         *check_ok = FALSE;
         fprintf(stderr, "ERROR:\t Expected a %lf but found %lf.\n", (double)val, (double)tmp);
      }
   }
}

#pragma omp task in([bsize*bsize]a, [bsize*bsize]b) inout([bsize*bsize]c)
void matmulBlock (unsigned int const bsize, elem_t* a, elem_t* b, elem_t* c) {
#if defined(USE_MKL)
   elem_t const alpha = 1.0;
   elem_t const beta = 1.0;
   char const transa = 'n';
   char const transb = 'n';
   GEMM(&transa, &transb, &bsize, &bsize, &bsize, &alpha, a,
         &bsize, b, &bsize, &beta, c, &bsize);
#elif defined(USE_OPENBLAS)
   elem_t const alpha = 1.0;
   elem_t const beta = 1.0;
   cblas_gemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, bsize, bsize,
      bsize, alpha, a, bsize, b, bsize, beta, c, bsize);
#else
   for (unsigned int i = 0; i < bsize; ++i) {
      for (unsigned int j = 0; j < bsize; ++j) {
         elem_t l = 0;
         for (unsigned int k = 0; k < bsize; ++k) {
            l += a[i*bsize + k] * b[k*bsize + j];
         }
         c[i*bsize + j] += l;
      }
   }
#endif
}

int main(int argc, char** argv) {
   if (argc < 3) {
      usage(argv[0]);
      exit(1);
   }

   unsigned int const msize = atoi(argv[1]);
   unsigned int const m2size = msize*msize;
   unsigned char const check = argc > 3 ? atoi(argv[3]) : 1;
   unsigned int const bsize = atoi(argv[2]);
   unsigned int const b2size = bsize*bsize;
   if (msize%bsize != 0) {
      fprintf(stderr, "ERROR:\t<matrix size> must be multiple of <block size>\n");
      usage(argv[0]);
      exit(1);
   }

   elem_t* a = (elem_t *)(malloc(m2size*sizeof(elem_t)));
   elem_t* b = (elem_t *)(malloc(m2size*sizeof(elem_t)));
   elem_t* c = (elem_t *)(malloc(m2size*sizeof(elem_t)));
   if (a == NULL || b == NULL || c == NULL) {
      fprintf(stderr, "ERROR:\tCannot allocate memory for the matrices\n");
      exit(1);
   }

   double t_start, t_end;
#if defined(TIMING_ALL)
   t_start = wall_time();
#endif

   for (unsigned int i = 0; i < m2size/b2size; i++) {
      setBlock(b2size, &a[i*b2size], (elem_t)VAL_A + i);
      setBlock(b2size, &b[i*b2size], (elem_t)VAL_B - i);
      setBlock(b2size, &c[i*b2size], (elem_t)VAL_C);
   }

#if !defined(TIMING_ALL)
   t_start = wall_time();
#endif

   for (unsigned int i = 0; i < msize/bsize; i++) {
      for (unsigned int j = 0; j < msize/bsize; j++) {
         for (unsigned int k = 0; k < msize/bsize; k++) {
            unsigned int const ai = k*b2size + i*bsize*msize;
            unsigned int const bi = j*b2size + k*bsize*msize;
            unsigned int const ci = j*b2size + i*bsize*msize;
            matmulBlock(bsize, &a[ai], &b[bi], &c[ci]);
         }
      }
   }

#if !defined(TIMING_ALL)
   #pragma omp taskwait
   t_end = wall_time();
#endif

   unsigned int check_ok = TRUE;
   if (check) {
      printf( "=================== CHECKING ===================== \n" );
      for (unsigned int i = 0; i < msize/bsize && check_ok; i++) {
         for (unsigned int j = 0; j < msize/bsize && check_ok; j++) {
            elem_t val = VAL_C;
            for (unsigned int k = 0; k < msize/bsize; k++) {
               val += a[i*msize*bsize + k*b2size]*b[k*msize*bsize + j*b2size]*bsize;
            }
            checkBlock(b2size, &check_ok, &c[i*bsize*msize + j*b2size], val, threshold);
         }
      }
   }

#if defined(TIMING_ALL)
   #pragma omp taskwait
   t_end = wall_time();
#endif

   if (check) {
      if (check_ok) {
         printf( "Output matrix is OK!\n" );
      } else {
         printf( "Output matrix is Wrong!\n" );
      }
      printf( "================================================== \n" );
   }

   free(a);
   free(b);
   free(c);

   printf( "==================== RESULTS ===================== \n" );
   printf( "  Benchmark: %s (%s)\n", "Matmul", "OmpSs" );
   printf( "  Elements type: %s\n", ELEM_T_STR );
   printf( "  Execution time (secs): %f\n", t_end - t_start );
   printf( "================================================== \n" );

}
