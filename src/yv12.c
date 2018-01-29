
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <eci.h>
#include <qr.h>

#include "yv12.h"
#include "xalloc.h"

void
qr_yv12(const struct qr *q, YV12_BUFFER_CONFIG *img)
{
	size_t x, y;

	img->y_width   = q->size;
	img->y_height  = q->size;
	img->y_stride  = q->size;
	img->uv_width  = q->size;
	img->uv_height = q->size;
	img->uv_stride = q->size;

	img->y_buffer  = xmalloc(q->size * q->size);
	img->u_buffer  = img->y_buffer;
	img->v_buffer  = img->y_buffer;

	for (y = 0; y < q->size; y++) {
		for (x = 0; x < q->size; x++) {
			img->y_buffer[y * q->size + x] = qr_get_module(q, x, y) ? 255 : 0;
		}
	}
}

