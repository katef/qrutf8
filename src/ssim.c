
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
#include "ssim.h"

#define C1 (float)(64 * 64 * 0.01*255*0.01*255)
#define C2 (float)(64 * 64 * 0.03*255*0.03*255)

static int width_y;
static int height_y;
static int height_uv;
static int width_uv;
static int stride_uv;
static int stride;
static int lumimask;
static int luminance;
static double plane_summed_weights = 0;

static short img12_sum_block[8*4096*4096*2] ;

static short img1_sum[8*4096*2];
static short img2_sum[8*4096*2];
static int   img1_sq_sum[8*4096*2];
static int   img2_sq_sum[8*4096*2];
static int   img12_mul_sum[8*4096*2];


double vp8_similarity
(
    int mu_x,
    int mu_y,
    int pre_mu_x2,
    int pre_mu_y2,
    int pre_mu_xy2
)
{
    int mu_x2, mu_y2, mu_xy, theta_x2, theta_y2, theta_xy;

    mu_x2 = mu_x * mu_x;
    mu_y2 = mu_y * mu_y;
    mu_xy = mu_x * mu_y;

    theta_x2 = 64 * pre_mu_x2 - mu_x2;
    theta_y2 = 64 * pre_mu_y2 - mu_y2;
    theta_xy = 64 * pre_mu_xy2 - mu_xy;

    return (2 * mu_xy + C1) * (2 * theta_xy + C2) / ((mu_x2 + mu_y2 + C1) * (theta_x2 + theta_y2 + C2));
}

double vp8_ssim
(
    const unsigned char *img1,
    const unsigned char *img2,
    int stride_img1,
    int stride_img2,
    int width,
    int height
)
{
    int x, y, x2, y2, img1_block, img2_block, img1_sq_block, img2_sq_block, img12_mul_block, temp;

    double plane_quality, weight, mean;

    short *img1_sum_ptr1, *img1_sum_ptr2;
    short *img2_sum_ptr1, *img2_sum_ptr2;
    int *img1_sq_sum_ptr1, *img1_sq_sum_ptr2;
    int *img2_sq_sum_ptr1, *img2_sq_sum_ptr2;
    int *img12_mul_sum_ptr1, *img12_mul_sum_ptr2;

    plane_quality = 0;

    if (lumimask)
        plane_summed_weights = 0.0f;
    else
        plane_summed_weights = (height - 7) * (width - 7);

    //some prologue for the main loop
    temp = 8 * width;

    img1_sum_ptr1      = img1_sum + temp;
    img2_sum_ptr1      = img2_sum + temp;
    img1_sq_sum_ptr1   = img1_sq_sum + temp;
    img2_sq_sum_ptr1   = img2_sq_sum + temp;
    img12_mul_sum_ptr1 = img12_mul_sum + temp;

    for (x = 0; x < width; x++)
    {
        img1_sum[x]      = img1[x];
        img2_sum[x]      = img2[x];
        img1_sq_sum[x]   = img1[x] * img1[x];
        img2_sq_sum[x]   = img2[x] * img2[x];
        img12_mul_sum[x] = img1[x] * img2[x];

        img1_sum_ptr1[x]      = 0;
        img2_sum_ptr1[x]      = 0;
        img1_sq_sum_ptr1[x]   = 0;
        img2_sq_sum_ptr1[x]   = 0;
        img12_mul_sum_ptr1[x] = 0;
    }

    //the main loop
    for (y = 1; y < height; y++)
    {
        img1 += stride_img1;
        img2 += stride_img2;

        temp = (y - 1) % 9 * width;

        img1_sum_ptr1      = img1_sum + temp;
        img2_sum_ptr1      = img2_sum + temp;
        img1_sq_sum_ptr1   = img1_sq_sum + temp;
        img2_sq_sum_ptr1   = img2_sq_sum + temp;
        img12_mul_sum_ptr1 = img12_mul_sum + temp;

        temp = y % 9 * width;

        img1_sum_ptr2      = img1_sum + temp;
        img2_sum_ptr2      = img2_sum + temp;
        img1_sq_sum_ptr2   = img1_sq_sum + temp;
        img2_sq_sum_ptr2   = img2_sq_sum + temp;
        img12_mul_sum_ptr2 = img12_mul_sum + temp;

        for (x = 0; x < width; x++)
        {
            img1_sum_ptr2[x]      = img1_sum_ptr1[x] + img1[x];
            img2_sum_ptr2[x]      = img2_sum_ptr1[x] + img2[x];
            img1_sq_sum_ptr2[x]   = img1_sq_sum_ptr1[x] + img1[x] * img1[x];
            img2_sq_sum_ptr2[x]   = img2_sq_sum_ptr1[x] + img2[x] * img2[x];
            img12_mul_sum_ptr2[x] = img12_mul_sum_ptr1[x] + img1[x] * img2[x];
        }

        if (y > 6)
        {
            //calculate the sum of the last 8 lines by subtracting the total sum of 8 lines back from the present sum
            temp = (y + 1) % 9 * width;

            img1_sum_ptr1      = img1_sum + temp;
            img2_sum_ptr1      = img2_sum + temp;
            img1_sq_sum_ptr1   = img1_sq_sum + temp;
            img2_sq_sum_ptr1   = img2_sq_sum + temp;
            img12_mul_sum_ptr1 = img12_mul_sum + temp;

            for (x = 0; x < width; x++)
            {
                img1_sum_ptr1[x]      = img1_sum_ptr2[x] - img1_sum_ptr1[x];
                img2_sum_ptr1[x]      = img2_sum_ptr2[x] - img2_sum_ptr1[x];
                img1_sq_sum_ptr1[x]   = img1_sq_sum_ptr2[x] - img1_sq_sum_ptr1[x];
                img2_sq_sum_ptr1[x]   = img2_sq_sum_ptr2[x] - img2_sq_sum_ptr1[x];
                img12_mul_sum_ptr1[x] = img12_mul_sum_ptr2[x] - img12_mul_sum_ptr1[x];
            }

            //here we calculate the sum over the 8x8 block of pixels
            //this is done by sliding a window across the column sums for the last 8 lines
            //each time adding the new column sum, and subtracting the one which fell out of the window
            img1_block      = 0;
            img2_block      = 0;
            img1_sq_block   = 0;
            img2_sq_block   = 0;
            img12_mul_block = 0;

            //prologue, and calculation of simularity measure from the first 8 column sums
            for (x = 0; x < 8; x++)
            {
                img1_block      += img1_sum_ptr1[x];
                img2_block      += img2_sum_ptr1[x];
                img1_sq_block   += img1_sq_sum_ptr1[x];
                img2_sq_block   += img2_sq_sum_ptr1[x];
                img12_mul_block += img12_mul_sum_ptr1[x];
            }

            if (lumimask)
            {
                y2 = y - 7;
                x2 = 0;

                if (luminance)
                {
                    mean = (img2_block + img1_block) / 128.0f;

                    if (!(y2 % 2 || x2 % 2))
                        *(img12_sum_block + y2 / 2 * width_uv + x2 / 2) = img2_block + img1_block;
                }
                else
                {
                    mean = *(img12_sum_block + y2 * width_uv + x2);
                    mean += *(img12_sum_block + y2 * width_uv + x2 + 4);
                    mean += *(img12_sum_block + (y2 + 4) * width_uv + x2);
                    mean += *(img12_sum_block + (y2 + 4) * width_uv + x2 + 4);

                    mean /= 512.0f;
                }

                weight = mean < 40 ? 0.0f :
                         (mean < 50 ? (mean - 40.0f) / 10.0f : 1.0f);
                plane_summed_weights += weight;

                plane_quality += weight * vp8_similarity(img1_block, img2_block, img1_sq_block, img2_sq_block, img12_mul_block);
            }
            else
                plane_quality += vp8_similarity(img1_block, img2_block, img1_sq_block, img2_sq_block, img12_mul_block);

            //and for the rest
            for (x = 8; x < width; x++)
            {
                img1_block      = img1_block + img1_sum_ptr1[x] - img1_sum_ptr1[x - 8];
                img2_block      = img2_block + img2_sum_ptr1[x] - img2_sum_ptr1[x - 8];
                img1_sq_block   = img1_sq_block + img1_sq_sum_ptr1[x] - img1_sq_sum_ptr1[x - 8];
                img2_sq_block   = img2_sq_block + img2_sq_sum_ptr1[x] - img2_sq_sum_ptr1[x - 8];
                img12_mul_block = img12_mul_block + img12_mul_sum_ptr1[x] - img12_mul_sum_ptr1[x - 8];

                if (lumimask)
                {
                    y2 = y - 7;
                    x2 = x - 7;

                    if (luminance)
                    {
                        mean = (img2_block + img1_block) / 128.0f;

                        if (!(y2 % 2 || x2 % 2))
                            *(img12_sum_block + y2 / 2 * width_uv + x2 / 2) = img2_block + img1_block;
                    }
                    else
                    {
                        mean = *(img12_sum_block + y2 * width_uv + x2);
                        mean += *(img12_sum_block + y2 * width_uv + x2 + 4);
                        mean += *(img12_sum_block + (y2 + 4) * width_uv + x2);
                        mean += *(img12_sum_block + (y2 + 4) * width_uv + x2 + 4);

                        mean /= 512.0f;
                    }

                    weight = mean < 40 ? 0.0f :
                             (mean < 50 ? (mean - 40.0f) / 10.0f : 1.0f);
                    plane_summed_weights += weight;

                    plane_quality += weight * vp8_similarity(img1_block, img2_block, img1_sq_block, img2_sq_block, img12_mul_block);
                }
                else
                    plane_quality += vp8_similarity(img1_block, img2_block, img1_sq_block, img2_sq_block, img12_mul_block);
            }
        }
    }

    if (plane_summed_weights == 0)
        return 1.0f;
    else
        return plane_quality / plane_summed_weights;
}

double vp8_calc_ssim
(
    YV12_BUFFER_CONFIG *source,
    YV12_BUFFER_CONFIG *dest,
    int lumamask,
    double *weight
)
{
    double a, b, c;
    double frame_weight;
    double ssimv;

    width_y = source->y_width;
    height_y = source->y_height;
    height_uv = source->uv_height;
    width_uv = source->uv_width;
    stride_uv = dest->uv_stride;
    stride = dest->y_stride;

    lumimask = lumamask;

    luminance = 1;
    a = vp8_ssim(source->y_buffer, dest->y_buffer,
                 source->y_stride, dest->y_stride, source->y_width, source->y_height);
    luminance = 0;

    frame_weight = plane_summed_weights / ((width_y - 7) * (height_y - 7));

    if (frame_weight == 0)
        a = b = c = 1.0f;
    else
    {
        b = vp8_ssim(source->u_buffer, dest->u_buffer,
                     source->uv_stride, dest->uv_stride, source->uv_width, source->uv_height);

        c = vp8_ssim(source->v_buffer, dest->v_buffer,
                     source->uv_stride, dest->uv_stride, source->uv_width, source->uv_height);
    }

    ssimv = a * .8 + .1 * (b + c);

    *weight = frame_weight;

    return ssimv;
}

