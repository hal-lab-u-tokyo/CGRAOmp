/*
*    MIT License
*    
*    Copyright (c) 2021 Amano laboratory, Keio University & Processor Research Team, RIKEN Center for Computational Science
*    
*    Permission is hereby granted, free of charge, to any person obtaining a copy of
*    this software and associated documentation files (the "Software"), to deal in
*    the Software without restriction, including without limitation the rights to
*    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
*    of the Software, and to permit persons to whom the Software is furnished to do
*    so, subject to the following conditions:
*    
*    The above copyright notice and this permission notice shall be included in all
*    copies or substantial portions of the Software.
*    
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*    SOFTWARE.
*    
*    File:          /build/CGRAOmp_build/test/simple_3nested.c
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  08-09-2021 14:17:51
*    Last Modified: 20-02-2022 08:10:12
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// #include <omp.h>
// #include <math.h>

// #define CGRAOMP_CUSTOM_INST_INLINE __attribute__((annotate("cgra_custom_inst")))  inline static
// #define CGRAOMP_CUSTOM_INST __attribute__((annotate("cgra_custom_inst"))) 

// CGRAOMP_CUSTOM_INST_INLINE double fsin(double x) {
//   return sin(x);
// }

#define N_X 10
#define N_Y 20
#define N_Z 30

#define N (N_X * N_Y * N_Z)

int main(int argc, char* argv[])
{
  int A[N_X][N_Y][N_Z];
  int B[N_X][N_Y][N_Z];
  const int32_t c = 100;
  int C[N_X][N_Y][N_Z];
  // int (*A)[N_Y][N_Z] = (int(*)[N_Y][N_Z])malloc(N_X*N_Y*N_Z*sizeof(int));
  // int (*B)[N_Y][N_Z] = (int(*)[N_Y][N_Z])malloc(N_X*N_Y*N_Z*sizeof(int));
  // int (*C)[N_Y][N_Z] = (int(*)[N_Y][N_Z])malloc(N_X*N_Y*N_Z*sizeof(int));
  // int ***A = (int***)(N_X*N_Y*N_Z*sizeof(int));
  // int ***B = (int***)(N_X*N_Y*N_Z*sizeof(int));
  // int ***C = (int***)(N_X*N_Y*N_Z*sizeof(int));

  // int *AA = (int*)A;
  // int *BB = (int*)B;
  // int *CC = (int*)C;

  int64_t i, j, k;

  //#pragma omp target parallel for map(to:A[:N_X][:N_Y][:N_Z],B[:N_X][:N_Y][:N_Z]) map(from:C[:N_X][:N_Y][:N_Z]) private(i,j,k)
  #pragma omp target parallel for map(to:A[:0],B[:0]) map(from:C[:0]) private(i,j,k)
  for (i = 0; i < N_X; i += 1) {
  	for (j = 0; j < N_Y; j += 1) {
	    for (k = 0; k < N_Z; k += 1) {
		     C[i][j][k] = A[i][j][k] + B[i][j][k] * c;
        // CC[N_Y * N_Z * i + N_Z * j + k] = AA[N_Y * N_Z * i + N_Z * j + k] + BB[N_Y * N_Z * i + N_Z * j + k] * c + k;
	    }
    	}
  }

//  #pragma omp target parallel for map(to:A[:N_X][:N_Y][:N_Z],B[:N_X][:N_Y][:N_Z]) map(from:C[:N_X][:N_Y][:N_Z]) private(i,j,k)

//   int *AA = (int*)(&A[0][0][0]);
//   int *BB = (int*)(&B[0][0][0]);
//   int *CC = (int*)(&C[0][0][0]);
// //    #pragma omp target parallel for map(to:AA[:N],BB[:N]) map(from:CC[:N]) private(i,j,k)
//   #pragma omp parallel for private(i,j,k)
//   for (i = 0; i < N_X; i += 1) {
// 	for (j = 0; j < N_Y; j += 1) {
// 	  for (k = 0; k < N_Z; k += 1) {
//       int64_t index = j * N_Z + k;
//       CC[index] = AA[index] + c * BB[index];
// 	  }
// 	}
//  }
		  

 printf("%d\n", C[argc][argc][argc]);

  return 0;

}
