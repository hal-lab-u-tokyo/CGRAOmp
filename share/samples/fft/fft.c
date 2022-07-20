/*
*    MIT License
*    
*    Copyright (c) 2022 Amano laboratory, Keio University & Processor Research Team, RIKEN Center for Computational Science
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
*    File:          /share/samples/fft/fft.c
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  14-02-2022 09:57:29
*    Last Modified: 20-07-2022 14:35:48
*/
#include <cgraomp.h>
#include <math.h>


CGRAOMP_CUSTOM_INST float FMA(float x, float y, float z) {
	return x * y + z;
}

#define PI (3.1415926f)

#ifndef N
#define N 1024
#endif

#ifndef LOG_N
#define LOG_N 10
#endif




void fft_stockham_radix2(float * in_re, float * in_im, float *out_re, float *out_im)
{
	int64_t l = LOG_N / 2;
	int64_t m = 1;
	int64_t i, j, k;

	#pragma omp target enter data map(alloc:in_re[0:N], in_im[0:N], out_re[0:N], out_im[0:N])
	#pragma omp target update to(in_re[0:N], in_im[0:N])

	for (i = 0; i < LOG_N; i++) {
		for (j = 0; j < l; j++) {
			const float W_re = cosf(-2 * PI * j / (2 * l));
			const float W_im = sinf(-2 * PI * j / (2 * l));
			#pragma omp target parallel for private(k) shared(l,m,j)
			for (k = 0; k < m; k++) {
				float in0_re, in0_im, in1_re, in1_im, out0_re, out0_im, d0_re, d0_im, out1_re, out1_im;
				in0_re = in_re[k + j * m];
				in0_im = in_im[k + j * m];
				in1_re = in_re[k + j * m + l * m];
				in1_im = in_im[k + j * m + l * m];
				out0_re = in0_re + in1_re;
				out0_im = in0_im + in1_im;
				d0_re = in0_re - in1_re;
				d0_im = in0_im - in1_im;

				out1_re = FMA(d0_re, W_re, - d0_im * W_im);
				out1_im = FMA(d0_re, W_im, d0_im * W_re);

				out_re[k + 2 * j * m] = out0_re;
				out_im[k + 2 * j * m] = out0_im;
				out_re[k + 2 * j * m + m] = out1_re;
				out_im[k + 2 * j * m + m] = out1_im;
			}
		}
		l /= 2;
		m *= 2;
	}

	#pragma omp target update from(out_re[0:N], out_im[0:N])
	#pragma omp target exit data map (release:in_re[0:N], in_im[0:N], out_re[0:N], out_im[0:N])
}


void fft_stockham_radix3(float * in_re, float * in_im, float *out_re, float *out_im)
{
	int64_t l = LOG_N / 3;
	int64_t m = 1;
	int64_t i, j, k;

	#pragma omp target enter data map(alloc:in_re[0:N], in_im[0:N], out_re[0:N], out_im[0:N])
	#pragma omp target update to(in_re[0:N], in_im[0:N])

	for (i = 0; i < LOG_N; i++) {
		for (j = 0; j < l; j++) {
			const float W0_re = cosf(-2 * PI * j / (3 * l));
			const float W0_im = sinf(-2 * PI * j / (3 * l));
			const float W1_re = cosf(-2 * PI * 2 * j / (3 * l));
			const float W1_im = sinf(-2 * PI * 2 * j / (3 * l));
			#pragma omp target parallel for private(k)
			for (k = 0; k < m; k++) {
				const float C = sinf(PI / 3);
				float in0_re, in0_im, in1_re, in1_im, in2_re, in2_im, out0_re, out0_im, d0_re, d0_im, d1_re, d1_im, d2_re, d2_im, d3_re, d3_im, d4_re, d4_im, out1_re, out1_im, out2_re, out2_im;
				in0_re = in_re[k + j * m];
				in0_im = in_im[k + j * m];
				in1_re = in_re[k + j * m + l * m];
				in1_im = in_im[k + j * m + l * m];
				in2_re = in_re[k + j * m + 2 * l * m];
				in2_im = in_im[k + j * m + 2 * l * m];

				d0_re = in1_re + in2_re;
				d0_im = in1_im + in2_im;
				d1_re = in0_re - d0_re / 2;
				d1_im = in0_im - d0_im / 2;
				d2_im = (in1_re - in2_re) * (-C);
				d2_re = (in1_im - in2_im) * C;

				out0_re = in0_re + d0_re;
				out0_im = in0_im + d0_im;

				d3_re = d1_re + d2_re;
				d3_im = d1_im + d2_im;
				d4_re = d1_re - d2_re;
				d4_im = d1_im - d2_im;

				out1_re = FMA(d3_re, W0_re, - d3_im * W0_im);
				out1_im = FMA(d3_re, W0_im, + d3_im * W0_re);
				out2_re = FMA(d4_re, W1_re, - d4_im * W1_im);
				out2_im = FMA(d4_re, W1_im, + d4_im * W1_re);

				out_re[k + 3 * j * m] = out0_re;
				out_im[k + 3 * j * m] = out0_im;
				out_re[k + 3 * j * m + m] = out1_re;
				out_im[k + 3 * j * m + m] = out1_im;
				out_re[k + 3 * j * m + 2 * m] = out2_re;
				out_im[k + 3 * j * m + 2 * m] = out2_im;

			}
		}
		l /= 3;
		m *= 3;
	}

	#pragma omp target update from(out_re[0:N], out_im[0:N])
	#pragma omp target exit data map (release:in_re[0:N], in_im[0:N], out_re[0:N], out_im[0:N])
}


void fft_stockham_radix4(float * in_re, float * in_im, float *out_re, float *out_im)
{
	int64_t l = LOG_N / 4;
	int64_t m = 1;
	int64_t i, j, k;


	#pragma omp target enter data map(alloc:in_re[0:N], in_im[0:N], out_re[0:N], out_im[0:N])
	#pragma omp target update to(in_re[0:N], in_im[0:N])
	for (i = 0; i < LOG_N; i++) {
		for (j = 0; j < l; j++) {
			const float W0_re = cosf(-2 * PI * j / (4 * l));
			const float W0_im = sinf(-2 * PI * j / (4 * l));
			const float W1_re = cosf(-2 * PI * 2 * j / (4 * l));
			const float W1_im = sinf(-2 * PI * 2 * j / (4 * l));
			const float W2_re = cosf(-2 * PI * 3 * j / (4 * l));
			const float W2_im = sinf(-2 * PI * 3 * j / (4 * l));
			#pragma omp target parallel for private(k)
			for (k = 0; k < m; k++) {
				//input to butterfly
				float in0_re, in0_im, in1_re, in1_im, in2_re, in2_im, in3_re, in3_im;
				//output from butterfly
				float out0_re, out0_im, out1_re, out1_im, out2_re, out2_im, out3_re, out3_im;
				//intermediaries
				float d0_re, d0_im, d1_re, d1_im, d2_re, d2_im, d3_re, d3_im, d4_re, d4_im, d5_re, d5_im, d6_re, d6_im;

				in0_re = in_re[k + j * m];
				in0_im = in_im[k + j * m];
				in1_re = in_re[k + j * m + l * m];
				in1_im = in_im[k + j * m + l * m];
				in2_re = in_re[k + j * m + 2 * l * m];
				in2_im = in_im[k + j * m + 2 * l * m];
				in3_re = in_re[k + j * m + 3 * l * m];
				in3_im = in_im[k + j * m + 3 * l * m];

				d0_re = in0_re + in2_re;
				d0_im = in0_im + in2_im;
				d1_re = in0_re - in2_re;
				d1_im = in0_im - in2_im;
				d2_re = in1_re + in3_re;
				d2_im = in1_im + in3_im;
				d3_re = in1_im - in3_im;
				d3_im = - (in1_re - in3_re);

				out0_re  = d0_re + d2_re;
				out0_im  = d0_im + d2_im;

				d4_re = d1_re + d3_re;
				d4_im = d1_im + d3_im;
				d5_re = d0_re - d2_re;
				d5_im = d0_im - d2_im;
				d6_re = d1_re - d3_re;
				d6_im = d1_im - d3_im;

				out1_re = FMA(d4_re, W0_re, - d4_im * W0_im);
				out1_im = FMA(d4_re, W0_im, d4_im * W0_re);
				out2_re = FMA(d5_re, W1_re, - d5_im * W1_im);
				out2_im = FMA(d5_re, W1_im, + d5_im * W1_re);
				out3_re = FMA(d6_re, W2_re, - d6_im * W2_im);
				out3_im = FMA(d6_re, W2_im, + d6_im * W2_re);

				out_re[k + 4 * j * m] = out0_re;
				out_im[k + 4 * j * m] = out0_im;
				out_re[k + 4 * j * m + m] = out1_re;
				out_im[k + 4 * j * m + m] = out1_im;
				out_re[k + 4 * j * m + 2 * m] = out2_re;
				out_im[k + 4 * j * m + 2 * m] = out2_im;
				out_re[k + 4 * j * m + 3 * m] = out3_re;
				out_im[k + 4 * j * m + 3 * m] = out3_im;
			}
		}
		l /= 4;
		m *= 4;
	}

	#pragma omp target update from(out_re[0:N], out_im[0:N])
	#pragma omp target exit data map (release:in_re[0:N], in_im[0:N], out_re[0:N], out_im[0:N])
}


void fft_stockham_radix5(float * in_re, float * in_im, float *out_re, float *out_im)
{
	int64_t l = LOG_N / 5;
	int64_t m = 1;
	int64_t i, j, k;


	#pragma omp target enter data map(alloc:in_re[0:N], in_im[0:N], out_re[0:N], out_im[0:N])
	#pragma omp target update to(in_re[0:N], in_im[0:N])

	for (i = 0; i < LOG_N; i++) {
		for (j = 0; j < l; j++) {
			const float W0_re = cosf(-2 * PI * j / (5 * l));
			const float W0_im = sinf(-2 * PI * j / (5 * l));
			const float W1_re = cosf(-2 * PI * 2 * j / (5 * l));
			const float W1_im = sinf(-2 * PI * 2 * j / (5 * l));
			const float W2_re = cosf(-2 * PI * 3 * j / (5 * l));
			const float W2_im = sinf(-2 * PI * 3 * j / (5 * l));
			const float W3_re = cosf(-2 * PI * 4 * j / (5 * l));
			const float W3_im = sinf(-2 * PI * 4 * j / (5 * l));
			#pragma omp target parallel for private(k)
			for (k = 0; k < m; k++) {
				//input to butterfly
				float in0_re, in0_im, in1_re, in1_im, in2_re, in2_im, in3_re, in3_im, in4_re, in4_im;
				//output from butterfly
				float out0_re, out0_im, out1_re, out1_im, out2_re, out2_im, out3_re, out3_im, out4_re, out4_im;
				//intermediaries
				float d0_re, d0_im, d1_re, d1_im, d2_re, d2_im, d3_re, d3_im, d4_re, d4_im, d5_re, d5_im, d6_re, d6_im, d7_re, d7_im, d8_re, d8_im, d9_re, d9_im, d10_re, d10_im, d11_re, d11_im, d12_re, d12_im, d13_re, d13_im, d14_re, d14_im;
				const float C0 = sinf(2/5*PI);
				const float C1 = sqrtf(5.0) / 4.0;
				const float C2 = sinf(1.0/5.0*PI) / sinf(2.0/5.0*PI);

				in0_re = in_re[k + j * m];
				in0_im = in_im[k + j * m];
				in1_re = in_re[k + j * m + l * m];
				in1_im = in_im[k + j * m + l * m];
				in2_re = in_re[k + j * m + 2 * l * m];
				in2_im = in_im[k + j * m + 2 * l * m];
				in3_re = in_re[k + j * m + 3 * l * m];
				in3_im = in_im[k + j * m + 3 * l * m];
				in4_re = in_re[k + j * m + 4 * l * m];
				in4_im = in_im[k + j * m + 4 * l * m];

				d0_re = in1_re + in4_re;
				d0_im = in1_im + in4_im;
				d1_re = in2_re + in3_re;
				d1_im = in2_im + in3_im;
				d2_re = (in1_re - in4_re) * C0;
				d2_im = (in1_im - in4_im) * C0;
				d3_re = (in2_re - in3_re) * C0;
				d3_im = (in2_im - in3_im) * C0;

				d4_re = d0_re + d1_re;
   				d4_im = d0_im + d1_im;

				d5_re = (d0_re - d1_re) * C1;
				d5_im = (d0_im - d1_im) * C1;

				d6_re = in0_re - d4_re / 4;
				d6_im = in0_im - d4_im / 4;

				d7_re = d6_re + d5_re;
				d7_im = d6_im + d5_im;

				d8_re = d6_re - d5_re;
				d8_im = d6_im - d5_im;

				d9_im = -(d2_re + d3_re * C2);
				d9_re = d2_im + d3_im * C2;
				d10_im = -(d2_re * C2 - d3_re);
				d10_re = d2_im * C2 - d3_im;

				out0_re  = in0_re + d4_re;
				out0_im  = in0_im + d4_im;

				d11_re = d7_re + d9_re;
				d11_im = d7_im + d9_im;
				d12_re = d8_re + d10_re;
				d12_im = d8_im + d10_im;
				d13_re = d8_re - d10_re;
				d13_im = d8_im - d10_im;
				d14_re = d7_re - d9_re;
				d14_im = d7_im - d9_im;

				out1_re = FMA(d11_re, W0_re, - d11_im * W0_im);
				out1_im = FMA(d11_re, W0_im, + d11_im * W0_re);
				out2_re = FMA(d12_re, W1_re, - d12_im * W1_im);
				out2_im = FMA(d12_re, W1_im, + d12_im * W1_re);
				out3_re = FMA(d13_re, W2_re, - d13_im * W2_im);
				out3_im = FMA(d13_re, W2_im, + d13_im * W2_re);
				out4_re = FMA(d14_re, W3_re, - d14_im * W3_im);
				out4_im = FMA(d14_re, W3_im, + d14_im * W3_re);

				out_re[k + 5 * j * m] = out0_re;
				out_im[k + 5 * j * m] = out0_im;
				out_re[k + 5 * j * m + m] = out1_re;
				out_im[k + 5 * j * m + m] = out1_im;
				out_re[k + 5 * j * m + 2 * m] = out2_re;
				out_im[k + 5 * j * m + 2 * m] = out2_im;
				out_re[k + 5 * j * m + 3 * m] = out3_re;
				out_im[k + 5 * j * m + 3 * m] = out3_im;
				out_re[k + 5 * j * m + 4 * m] = out4_re;
				out_im[k + 5 * j * m + 4 * m] = out4_im;
			}
		}
		l /= 5;
		m *= 5;
	}

	#pragma omp target update from(out_re[0:N], out_im[0:N])
	#pragma omp target exit data map (release:in_re[0:N], in_im[0:N], out_re[0:N], out_im[0:N])
}
