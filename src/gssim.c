
/*

Copyright (c) 2010, The WebM Project authors. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * Neither the name of Google, nor the WebM Project, nor the names
    of its contributors may be used to endorse or promote products
    derived from this software without specific prior written
    permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <math.h>

#include "yv12.h"
#include "ssim.h"

// Google version of SSIM
// SSIM
#define KERNEL 3
#define KERNEL_SIZE  (2 * KERNEL + 1)

typedef unsigned char uint8;
typedef unsigned int uint32;

static const int K[KERNEL_SIZE] =
{
    1, 4, 11, 16, 11, 4, 1    // 16 * exp(-0.3 * i * i)
};
static const double ki_w = 1. / 2304.;  // 1 / sum(i:0..6, j..6) K[i]*K[j]
double get_ssimg(const uint8 *org, const uint8 *rec,
                 int xo, int yo, int W, int H,
                 const int stride1, const int stride2
                )
{
    // TODO(skal): use summed tables
    int y, x;

    const int ymin = (yo - KERNEL < 0) ? 0 : yo - KERNEL;
    const int ymax = (yo + KERNEL > H - 1) ? H - 1 : yo + KERNEL;
    const int xmin = (xo - KERNEL < 0) ? 0 : xo - KERNEL;
    const int xmax = (xo + KERNEL > W - 1) ? W - 1 : xo + KERNEL;
    // worst case of accumulation is a weight of 48 = 16 + 2 * (11 + 4 + 1)
    // with a diff of 255, squares. That would a max error of 0x8ee0900,
    // which fits into 32 bits integers.
    uint32 w = 0, xm = 0, ym = 0, xxm = 0, xym = 0, yym = 0;
    org += ymin * stride1;
    rec += ymin * stride2;

    for (y = ymin; y <= ymax; ++y, org += stride1, rec += stride2)
    {
        const int Wy = K[KERNEL + y - yo];

        for (x = xmin; x <= xmax; ++x)
        {
            const  int Wxy = Wy * K[KERNEL + x - xo];
            // TODO(skal): inlined assembly
            w   += Wxy;
            xm  += Wxy * org[x];
            ym  += Wxy * rec[x];
            xxm += Wxy * org[x] * org[x];
            xym += Wxy * org[x] * rec[x];
            yym += Wxy * rec[x] * rec[x];
        }
    }

    {
        const double iw = 1. / w;
        const double iwx = xm * iw;
        const double iwy = ym * iw;
        double sxx = xxm * iw - iwx * iwx;
        double syy = yym * iw - iwy * iwy;

        // small errors are possible, due to rounding. Clamp to zero.
        if (sxx < 0.) sxx = 0.;

        if (syy < 0.) syy = 0.;

        {
            const double sxsy = sqrt(sxx * syy);
            const double sxy = xym * iw - iwx * iwy;
            static const double C11 = (0.01 * 0.01) * (255 * 255);
            static const double C22 = (0.03 * 0.03) * (255 * 255);
            static const double C33 = (0.015 * 0.015) * (255 * 255);
            const double l = (2. * iwx * iwy + C11) / (iwx * iwx + iwy * iwy + C11);
            const double c = (2. * sxsy      + C22) / (sxx + syy + C22);

            const double s = (sxy + C33) / (sxsy + C33);
            return l * c * s;

        }
    }

}

double get_ssimfull_kernelg(const uint8 *org, const uint8 *rec,
                            int xo, int yo, int W, int H,
                            const int stride1, const int stride2)
{
	(void) W;
	(void) H;

    // TODO(skal): use summed tables
    // worst case of accumulation is a weight of 48 = 16 + 2 * (11 + 4 + 1)
    // with a diff of 255, squares. That would a max error of 0x8ee0900,
    // which fits into 32 bits integers.
    int y_, x_;
    uint32 xm = 0, ym = 0, xxm = 0, xym = 0, yym = 0;
    org += (yo - KERNEL) * stride1;
    org += (xo - KERNEL);
    rec += (yo - KERNEL) * stride2;
    rec += (xo - KERNEL);

    for (y_ = 0; y_ < KERNEL_SIZE; ++y_, org += stride1, rec += stride2)
    {
        const int Wy = K[y_];

        for (x_ = 0; x_ < KERNEL_SIZE; ++x_)
        {
            const int Wxy = Wy * K[x_];
            // TODO(skal): inlined assembly
            const int org_x = org[x_];
            const int rec_x = rec[x_];
            xm  += Wxy * org_x;
            ym  += Wxy * rec_x;
            xxm += Wxy * org_x * org_x;
            xym += Wxy * org_x * rec_x;
            yym += Wxy * rec_x * rec_x;
        }
    }

    {
        const double iw = ki_w;
        const double iwx = xm * iw;
        const double iwy = ym * iw;
        double sxx = xxm * iw - iwx * iwx;
        double syy = yym * iw - iwy * iwy;

        // small errors are possible, due to rounding. Clamp to zero.
        if (sxx < 0.) sxx = 0.;

        if (syy < 0.) syy = 0.;

        {
            const double sxsy = sqrt(sxx * syy);
            const double sxy = xym * iw - iwx * iwy;
            static const double C11 = (0.01 * 0.01) * (255 * 255);
            static const double C22 = (0.03 * 0.03) * (255 * 255);
            static const double C33 = (0.015 * 0.015) * (255 * 255);
            const double l = (2. * iwx * iwy + C11) / (iwx * iwx + iwy * iwy + C11);
            const double c = (2. * sxsy      + C22) / (sxx + syy + C22);
            const double s = (sxy + C33) / (sxsy + C33);
            return l * c * s;
        }
    }
}

double calc_ssimg(const uint8 *org, const uint8 *rec,
                  const int image_width, const int image_height,
                  const int stride1, const int stride2
                 )
{
    int j, i;
    double SSIM = 0.;

    for (j = 0; j < KERNEL; ++j)
    {
        for (i = 0; i < image_width; ++i)
        {
            SSIM += get_ssimg(org, rec, i, j, image_width, image_height, stride1, stride2);
        }
    }

    for (j = KERNEL; j < image_height - KERNEL; ++j)
    {
        for (i = 0; i < KERNEL; ++i)
        {
            SSIM += get_ssimg(org, rec, i, j, image_width, image_height, stride1, stride2);
        }

        for (i = KERNEL; i < image_width - KERNEL; ++i)
        {
            SSIM += get_ssimfull_kernelg(org, rec, i, j,
                                         image_width, image_height, stride1, stride2);
        }

        for (i = image_width - KERNEL; i < image_width; ++i)
        {
            SSIM += get_ssimg(org, rec, i, j, image_width, image_height, stride1, stride2);
        }
    }

    for (j = image_height - KERNEL; j < image_height; ++j)
    {
        for (i = 0; i < image_width; ++i)
        {
            SSIM += get_ssimg(org, rec, i, j, image_width, image_height, stride1, stride2);
        }
    }

    return SSIM;
}


double vp8_calc_ssimg
(
    YV12_BUFFER_CONFIG *source,
    YV12_BUFFER_CONFIG *dest
)
{
    double ssim_y;
    double ssim_u;
    double ssim_v;

    int ysize  = source->y_width * source->y_height;
    int uvsize = ysize;

    ssim_y = calc_ssimg(source->y_buffer, dest->y_buffer,
                        source->y_width, source->y_height,
                        source->y_stride, dest->y_stride);


    ssim_u = calc_ssimg(source->u_buffer, dest->u_buffer,
                        source->uv_width, source->uv_height,
                        source->uv_stride, dest->uv_stride);


    ssim_v = calc_ssimg(source->v_buffer, dest->v_buffer,
                        source->uv_width, source->uv_height,
                        source->uv_stride, dest->uv_stride);

    return (ssim_y + ssim_u + ssim_v) / (ysize + uvsize + uvsize);
}
