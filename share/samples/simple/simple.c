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
*    File:          /build/CGRAOmp_build/test/simple.c
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  08-09-2021 14:17:51
*    Last Modified: 30-06-2022 14:08:44
*/
#include <stdio.h>
#include <omp.h>
#include <math.h>

#define CGRAOMP_CUSTOM_INST_INLINE __attribute__((annotate("cgra_custom_inst")))  inline static
#define CGRAOMP_CUSTOM_INST __attribute__((annotate("cgra_custom_inst"))) 

CGRAOMP_CUSTOM_INST_INLINE int some_custom(int x) {
  return x * x;
}

#define N 1024

int main(int argc, char* argv[])
{
  int A[N];
  int B[N];
  const int c = 10;
  int C[N];
  int64_t i, j;
  int *x;
  *x = argc;


  #pragma omp target parallel for map(to:A[:N],B[:N]) map(from:C[:N])
  for (i = 0; i < N; i += 1) {
    C[i] = A[i] + c * B[i];
  }

  /* #pragma omp target parallel for map(to:A,B) map(from:C) */
  /* for (i = 0; i < N; i++) { */
  /*   C[i] = some_custom(A[i] + c * B[i]); */
  /* } */

  printf("%d\n", C[argc]);

  return 0;

}
