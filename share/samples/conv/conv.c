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
*    File:          /share/samples/conv/conv.c
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  10-12-2021 17:52:45
*    Last Modified: 20-07-2022 14:34:57
*/
#include <cgraomp.h>

#define HEIGHT 100
#define WIDTH 256
#define N (HEIGHT*WIDTH)

#ifndef UNROLL_COUT
#define UNROLL_COUNT 1
#endif

void convolution3x3(float * array, float * arraySol){
	int64_t x,y;
	#pragma omp target parallel for map(to:array[:N]) map(from:arraySol[:N]) private(x,y)
	for (y = 1; y < HEIGHT - 1; y++) {
		const float weights[] = {3, 5, 7, 9, 11, 13, 15, 17, 19};    //Immediate value
		#pragma clang loop unroll_count(UNROLL_COUNT)
		for (x = 1; x < WIDTH - 1; x++) {
			arraySol[x + y * WIDTH]=
			weights[0] * array[(x + y * WIDTH) - WIDTH - 1] + // (-1, -1)
			weights[1] * array[(x + y * WIDTH) - WIDTH    ] + // ( 0, -1)
			weights[2] * array[(x + y * WIDTH) - WIDTH + 1] + // (+1, -1)
			weights[3] * array[(x + y * WIDTH)         - 1] + // (-1,  0)
			weights[4] * array[(x + y * WIDTH)            ] + // ( 0,  0)
			weights[5] * array[(x + y * WIDTH)         + 1] + // (+1,  0)
			weights[6] * array[(x + y * WIDTH) + WIDTH - 1] + // (-1, +1)
			weights[7] * array[(x + y * WIDTH) + WIDTH    ] + // ( 0, +1)
			weights[8] * array[(x + y * WIDTH) + WIDTH + 1];  // (+1, +1)
		}
	}
}
