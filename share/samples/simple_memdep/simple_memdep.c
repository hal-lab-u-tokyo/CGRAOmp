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
*    File:          /share/samples/simple_memdep/simple_memdep.c
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  08-09-2021 14:17:51
*    Last Modified: 20-07-2022 14:33:45
*/
#include <stdio.h>
#include <cgraomp.h>

CGRAOMP_CUSTOM_INST_INLINE double fsin(double x) {
	return sin(x);
}

#define N 1024
#define DEP_N 1

int main(int argc, char* argv[])
{
	int A[N];
	int B[N];
	const int c = 1;
	int64_t i, j;
	int *x;
	*x = argc;

	#pragma omp target parallel for map(to:A) map(from:B)
	for (i = DEP_N; i < N; i += 1) {
		B[i] = A[i] + B[i - DEP_N];
	}

	printf("%d\n", B[argc]);

	return 0;

}
