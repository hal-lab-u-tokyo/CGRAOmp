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
*    File:          /share/samples/simple_3nested/simple_3nested.c
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  08-09-2021 14:17:51
*    Last Modified: 20-07-2022 14:36:19
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <cgraomp.h>

#define N_X 10
#define N_Y 20
#define N_Z 30

int main(int argc, char* argv[])
{
	int A[N_X][N_Y][N_Z];
	int B[N_X][N_Y][N_Z];
	const int32_t c = 100;
	int C[N_X][N_Y][N_Z];

	int64_t i, j, k;

	#pragma omp target parallel for map(to:A[:0],B[:0]) map(from:C[:0]) private(i,j,k)
	for (i = 0; i < N_X; i += 1) {
		for (j = 0; j < N_Y; j += 1) {
			for (k = 0; k < N_Z; k += 1) {
				C[i][j][k] = A[i][j][k] + B[i][j][k] * c;
			}
		}
	}

	printf("%d\n", C[argc][argc][argc]);

	return 0;

}
