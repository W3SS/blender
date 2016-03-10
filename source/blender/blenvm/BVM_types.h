/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BVM_TYPES_H__
#define __BVM_TYPES_H__

/** \file BVM_types.h
 *  \ingroup bvm
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum BVMType {
	BVM_FLOAT,
	BVM_FLOAT3,
	BVM_FLOAT4,
	BVM_INT,
	BVM_MATRIX44,
	BVM_STRING,
	BVM_RNAPOINTER,
	BVM_MESH,
	BVM_DUPLIS,
} BVMType;

typedef enum BVMBufferType {
	BVM_BUFFER_SINGLE,
	BVM_BUFFER_ARRAY,
	BVM_BUFFER_IMAGE,
} BVMBufferType;

typedef enum BVMInputValueType {
	INPUT_CONSTANT,
	INPUT_EXPRESSION,
	INPUT_VARIABLE,
} BVMInputValueType;

typedef enum BVMOutputValueType {
	OUTPUT_EXPRESSION,
	OUTPUT_VARIABLE,
} BVMOutputValueType;

#ifdef __cplusplus
}
#endif

#endif /* __BVM_TYPES_H__ */
