/*
* Copyright (c) 2020, BSC (Barcelona Supercomputing Center)
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
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>

// General definitions
#include "matmul.h"
#include "matmul.fpga.h"

const unsigned int BSIZE = MATMUL_BLOCK_SIZE;
const unsigned int MBLOCK_II = MATMUL_BLOCK_II;
const unsigned int MBLOCK_FPGA_PWIDTH = FPGA_MEMORY_PORT_WIDTH;
const unsigned int MBLOCK_NUM_ACCS = MATMUL_NUM_ACCS;

void usage (char* argv0) {
   fprintf(stderr, "USAGE:\t%s <matrix size> <check> <create from>\n", argv0);
   fprintf(stderr, "      \t<block size> is fixed to %u\n", BSIZE);
   fprintf(stderr, "      \t<check> values:\n");
   fprintf(stderr, "      \t  - 0 to disable checking\n");
   fprintf(stderr, "      \t  - 1 to enable checking\n");
   fprintf(stderr, "      \t  - 2 to generate checking reference\n");
   fprintf(stderr, "      \t<create from> values:\n");
   fprintf(stderr, "      \t  - 0 to create block tasks in FPGA\n");
   fprintf(stderr, "      \t  - 1 to create block tasks in SMP\n");
}

#pragma oss task in([m2size]data)
void flushData(float *data, int m2size) {
    //dummy task to pull data from fpga
}

#pragma oss task
void setBlock(elem_t* v, const elem_t val) {
   for (unsigned int i = 0; i < BSIZE*BSIZE; ++i) {
      v[i] = val;
   }
}

#pragma oss task
void setBlockSeq(elem_t* v, int base) {
   for (unsigned int i = 0; i < BSIZE*BSIZE; ++i) {
      v[i] = ((elem_t)((base/1024)%2)) - 1.0 + ((elem_t)(base%512))/1000;
      base = (base*97 + 89)%65536;
   }
}

#pragma oss task
void checkBlock(unsigned int* check_ok, const elem_t* res, const elem_t* ref, const float threshold)
{
   for (unsigned int i = 0; i < BSIZE*BSIZE && ( *check_ok ); ++i) {
      const elem_t res_val = res[i];
      const elem_t ref_val = ref[i];
      const elem_t maxv = ref_val * (1.0 + (ref_val < 0 ? -threshold : threshold));
      const elem_t minv = ref_val * (1.0 - (ref_val < 0 ? -threshold : threshold));
      if (res_val > maxv || res_val < minv) {
         *check_ok = 0;
         fprintf(stderr, "ERROR:\t Expected a %lf but found %lf.\n", (double)ref_val, (double)res_val);
      }
   }
}

unsigned int matmulCheck(const unsigned int check, const elem_t* c, const unsigned int msize)
{
   const unsigned int m2size = msize*msize;
   const unsigned int b2size = BSIZE*BSIZE;
   unsigned int check_ok = 1;

   if (check == 1) {
      //Check the result matrix against the reference solution
      printf( "=================== CHECKING ===================== \n" );
      char ref_filename[64];
      sprintf(ref_filename, "ref/matmul_%s_%u_%u_%u.ref", ELEM_T_STR, msize, BSIZE, 2 /*numReps*/);
      int ref_file = open(ref_filename, O_RDONLY);
      if (ref_file == -1) {
         fprintf(stderr, "Cannot open '%s' as a reference solution\n", ref_filename);
         check_ok = 0;
      } else {
         const elem_t* c_ref = (const elem_t *)mmap(NULL, m2size*sizeof(elem_t), PROT_READ, MAP_SHARED, ref_file, 0);
         if (c_ref == (const elem_t *)-1) {
            fprintf(stderr, "Cannot map '%s' as a reference solution\n", ref_filename);
            check_ok = 0;
         } else {
            for (unsigned int i = 0; i < msize/BSIZE && check_ok; i++) {
               for (unsigned int j = 0; j < msize/BSIZE && check_ok; j++) {
                  unsigned int const ci = j*b2size + i*BSIZE*msize;
                  checkBlock(&check_ok, &c[ci], &c_ref[ci], THRESHOLD);
               }
            }
            #pragma oss taskwait
            munmap((void *)c_ref, m2size*sizeof(elem_t));
            close(ref_file);
         }
      }
      printf( "Output matrix is %s!\n", (check_ok ? "OK" : "WRONG") );
      printf( "================================================== \n" );
   } else if (check == 2) {
     //Write the reference file
      printf( "============= GENERATING REFERENCE =============== \n" );
      char ref_filename[64];
      sprintf(ref_filename, "matmul_%s_%u_%u_%u.ref", ELEM_T_STR, msize, BSIZE, 2 /*numReps*/);
      FILE *ref_file = fopen(ref_filename, "w+");
      if (fwrite(c, sizeof(elem_t), m2size, ref_file) != m2size) {
         fprintf(stderr, "Error writing reference file\n");
         check_ok = 0;
      }
      fclose(ref_file);
      printf( "Output wrote to '%s'\n", ref_filename );
      printf( "Move the file inside the 'ref' folder to use it as a reference\n" );
      printf( "================================================== \n" );
   }
   return check_ok;
}

#pragma oss task device(fpga) num_instances(MATMUL_NUM_ACCS) copy_deps in([BSIZE*BSIZE]a, [BSIZE*BSIZE]b) inout([BSIZE*BSIZE]c) affinity(af)
void matmulBlock(const elem_t a[BSIZE*BSIZE], const elem_t b[BSIZE*BSIZE], elem_t c[BSIZE*BSIZE], int af)
{
   #pragma HLS INLINE
   #pragma HLS array_partition variable=a cyclic factor=MBLOCK_FPGA_PWIDTH/64
   #pragma HLS array_partition variable=b cyclic factor=BSIZE/(MBLOCK_II*2)
   #pragma HLS array_partition variable=c cyclic factor=BSIZE/MBLOCK_II
#ifdef USE_URAM
   #if defined(__VITIS_HLS__)
      #pragma HLS bind_storage variable=A type=RAM_T2P impl=URAM
      #pragma HLS bind_storage variable=B type=RAM_T2P impl=URAM
   #else
      #pragma HLS resource variable=A core=XPM_MEMORY uram
      #pragma HLS resource variable=B core=XPM_MEMORY uram
   #endif
#endif

   for (int k = 0; k < BSIZE; ++k) {
      for (int i = 0; i < BSIZE; ++i) {
         #pragma HLS pipeline II=MBLOCK_II
         for (int j = 0; j < BSIZE; ++j) {
            c[i*BSIZE + j] += a[i*BSIZE + k] * b[k*BSIZE + j];
         }
      }
   }
}

#if defined(USE_IMPLEMENTS)
//#pragma omp target device(smp) copy_deps implements(matmulBlock)
#pragma omp target device(smp) no_copy_deps implements(matmulBlock) copy_inout([BSIZE*BSIZE]c)
#pragma omp task in([BSIZE*BSIZE]a, [BSIZE*BSIZE]b) inout([BSIZE*BSIZE]c)
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
#endif // defined(USE_IMPLEMENTS)

#pragma oss task device(fpga) in([msize*msize]a, [msize*msize]b) inout([msize*msize]c)
void matmulFPGA(const elem_t *a, const elem_t *b, elem_t *c, const unsigned int msize) {
#pragma HLS inline
   const unsigned int factor = MBLOCK_NUM_ACCS;
   const unsigned int b2size = BSIZE*BSIZE;
   const unsigned int num_blocks_side = msize/BSIZE;
   const unsigned int num_blocks_matrix = msize*msize/b2size;
   const unsigned int num_blocks_loop = num_blocks_matrix - num_blocks_matrix%factor;
   const unsigned int max_created_count = 8*MBLOCK_NUM_ACCS;
   for (unsigned int l = 0; l < num_blocks_loop; l+=factor) {
      for (unsigned int k = 0; k < msize/BSIZE; k++) {
#pragma HLS loop_flatten off
         for (unsigned int ll = l; ll < (l+factor); ll++) {
            const unsigned int i = ll/num_blocks_side;
            const unsigned int j = ll%num_blocks_side;
            const unsigned int ai = k*b2size + i*BSIZE*msize;
            const unsigned int bi = j*b2size + k*BSIZE*msize;
            const unsigned int ci = ll*b2size;
	    //Not implemented yet
            //#pragma oss taskcall affinity(ll-l)
            matmulBlock(a + ai, b + bi, c + ci, ll-l);
         }
      }
   }
   for (unsigned int k = 0; k < msize/BSIZE; k++) {
      for (unsigned int l = num_blocks_loop; l < num_blocks_matrix; l++) {
         const unsigned int i = l/num_blocks_side;
         const unsigned int j = l%num_blocks_side;
         const unsigned int ai = k*b2size + i*BSIZE*msize;
         const unsigned int bi = j*b2size + k*BSIZE*msize;
         const unsigned int ci = l*b2size;
         matmulBlock(a + ai, b + bi, c + ci, 0xFF);
      }
      #pragma oss taskwait
   }
}

void matmulSMP(const elem_t *a, const elem_t *b, elem_t *c, const unsigned int msize) {
   const unsigned int b2size = BSIZE*BSIZE;
   for (unsigned int i = 0; i < msize/BSIZE; i++) {
      for (unsigned int k = 0; k < msize/BSIZE; k++) {
         unsigned int const ai = k*b2size + i*BSIZE*msize;
         for (unsigned int j = 0; j < msize/BSIZE; j++) {
            unsigned int const bi = j*b2size + k*BSIZE*msize;
            unsigned int const ci = j*b2size + i*BSIZE*msize;
            matmulBlock(a + ai, b + bi, c + ci, 0xFF);
         }
      }
   }
}

int main(int argc, char** argv) {
   if (argc != 4) {
      usage(argv[0]);
      exit(1);
   }

   unsigned int const b2size = BSIZE*BSIZE;
   unsigned int const msize = atoi(argv[1]);
   unsigned int const m2size = msize*msize;
   unsigned char const check = atoi(argv[2]);
   unsigned char const createFrom = atoi(argv[3]);
   char const * createFromStr = createFrom == 0 ? "cFPGA" : "cHOST";
   if (msize%BSIZE != 0) {
      fprintf(stderr, "ERROR:\t<matrix size> must be multiple of <block size>\n");
      usage(argv[0]);
      exit(1);
   } else if (createFrom > 1) {
      fprintf(stderr, "ERROR:\tUnsupported value in <create from>\n");
      usage(argv[0]);
      exit(1);
   }

   unsigned int s = m2size*sizeof(elem_t);
   elem_t* a = (elem_t *)(malloc(s));
   elem_t* b = (elem_t *)(malloc(s));
   elem_t* c = (elem_t *)(malloc(s));
   if (a == NULL || b == NULL || c == NULL) {
      fprintf(stderr, "ERROR:\tCannot allocate memory for the matrices\n");
      exit(1);
   }

   double tIniStart = wall_time();

   srand(2019);
   for (unsigned int i = 0; i < m2size/b2size; i++) {
      setBlockSeq(&a[i*b2size], rand());
      setBlockSeq(&b[i*b2size], rand());
      setBlock(&c[i*b2size], 0);
   }

   #pragma oss taskwait
   const double tEndStart = wall_time();
   const double tIniWarm = tEndStart;

   //Warm up execution
   if (createFrom == 0) {
     matmulFPGA(a, b, c, msize);
   } else if (createFrom == 1) {
     matmulSMP(a, b, c, msize);
   }

   //Noflush is not yet implemented
   #pragma oss taskwait noflush([m2size]a, [m2size]b, [m2size]c)
   const double tEndWarm = wall_time();
   const double tIniExec = tEndWarm;

   //Performance execution
   if (createFrom == 0) {
     matmulFPGA(a, b, c, msize);
   } else if (createFrom == 1) {
     matmulSMP(a, b, c, msize);
   }

   //taskwait is not implemented (yet)
   #pragma oss taskwait noflush([m2size]a, [m2size]b, [m2size]c)
   const double tEndExec = wall_time();
   const double tIniFlush = tEndExec;

   //This would be needed in case of using noflush
   //flushData(c, m2size);
   //#pragma oss taskwait
   const double tEndFlush = wall_time();
   const double tIniCheck = tEndFlush;

   //Check the output matrix
   unsigned int check_ok = matmulCheck(check, c, msize);

   const double tEndCheck = wall_time();

   free(a);
   free(b);
   free(c);

   //Print the execution report
   const float gflops = m2size/1000.0*msize/1000.0*2.0/1000.0/(tEndExec - tIniExec);
   printf( "==================== RESULTS ===================== \n" );
   printf( "  Benchmark: %s (%s)\n", "Matmul", "OmpSs" );
   printf( "  Elements type: %s\n", ELEM_T_STR );
   printf( "  Create from: %s\n", createFromStr );
   printf( "  Init. time (secs):     %f\n", tEndStart  - tIniStart );
   printf( "  Warm up time (secs):   %f\n", tEndWarm   - tIniWarm );
   printf( "  Execution time (secs): %f\n", tEndExec   - tIniExec );
   printf( "  Flush time (secs):     %f\n", tEndFlush  - tIniFlush );
   printf( "  Checking time (secs):  %f\n", tEndCheck  - tIniCheck );
   printf( "  Performance (GFLOPS):  %f\n", gflops );
   printf( "================================================== \n" );

   //Create the JSON result file
   FILE *res_file = fopen("test_result.json", "w+");
   if (res_file == NULL) {
      printf( "Cannot open 'test_result.json' file\n" );
      exit(1);
   }
   fprintf(res_file,
      "{ \
         \"benchmark\": \"%s\", \
         \"toolchain\": \"%s\", \
         \"board\": \"%s\", \
         \"version\": \"%uaccs %uBS kij memport_128 noflush\", \
         \"exectype\": \"%s\", \
         \"argv\": \"%d %d %s\", \
         \"exectime\": \"%f\", \
         \"performance\": \"%f\", \
         \"note\": \"datatype %s, init %f, warm %f, exec %f, flush %f, check %f\" \
      }",
      "matmul",
      "ompss-2",
      BOARD,
      MBLOCK_NUM_ACCS, BSIZE,
      RUNTIME_MODE,
      msize, BSIZE, createFromStr,
      tEndExec - tIniExec,
      gflops,
      ELEM_T_STR,
      tEndStart - tIniStart,
      tEndWarm - tIniWarm,
      tEndExec - tIniExec,
      tEndFlush - tIniFlush,
      tEndCheck - tIniCheck
   );
   fclose(res_file);

   return check_ok ? 0 : 1;
}
