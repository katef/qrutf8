/*
 * File       : pnmio.h
 * Description: Header file for pnmio.c.
 * Author     : Nikolaos Kavvadias <nikolaos.kavvadias@gmail.com>
 * Copyright  : (C) Nikolaos Kavvadias 2012, 2013, 2014, 2015, 2016, 2017
 * Website    : http://www.nkavvadias.com
 *
 * This file is part of libpnmio, and is distributed under the terms of the
 * Modified BSD License.
 *
 * A copy of the Modified BSD License is included with this distribution
 * in the file LICENSE.
 * libpnmio is free software: you can redistribute it and/or modify it under the
 * terms of the Modified BSD License.
 * libpnmio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the Modified BSD License for more details.
 *
 * You should have received a copy of the Modified BSD License along with
 * libpnmio. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PNMIO_H
#define PNMIO_H

#define PBM_ASCII         1
#define PBM_BINARY        4

int  get_pnm_type(FILE *f);
void read_pbm_header(FILE *f, int *img_xdim, int *img_ydim, int *is_ascii);
void read_pbm_data(FILE *f, int *img_in, int is_ascii);

#endif /* PNMIO_H */
