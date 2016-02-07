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

#ifndef __BVM_AST_CONTEXT_H__
#define __BVM_AST_CONTEXT_H__

/** \file bvm_ast_context.h
 *  \ingroup bvm
 */

#include "MEM_guardedalloc.h"

#include "bvm_util_string.h"

struct MemArena;

namespace bvm {
namespace ast {

struct ASTContext {
	ASTContext();
	~ASTContext();
	
	/* AST objects are never explicitly deleted, but all allocated memory
	 * is released when the ASTContext is destroyed.
	 */
	void *allocate(size_t size) const;
	
private:
	MemArena *m_arena;
};

} /* namespace ast */
} /* namespace bvm */

inline void* operator new (size_t size, const bvm::ast::ASTContext &C)
{
	return C.allocate(size);
}

inline void* operator new[] (size_t size, const bvm::ast::ASTContext &C)
{
	return C.allocate(size);
}

#endif /* __BVM_AST_CONTEXT_H__ */