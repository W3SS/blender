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

#ifndef __BVM_NODE_PARSER_H__
#define __BVM_NODE_PARSER_H__

/** \file bvm_node_parser.h
 *  \ingroup bvm
 */

#include "bvm_source_location.h"
#include "bvm_util_string.h"

namespace bvm {

struct NodeGraph;
struct NodeInstance;

namespace ast {
struct FunctionDecl;
}

struct NodeParser {
	NodeParser();
	~NodeParser();
	
	ast::FunctionDecl *parse(const NodeGraph &graph);
	
protected:
	const NodeInstance *find_output_node(const NodeGraph &graph) const;
	
	void node_function_call(const NodeInstance *node);
};

} /* namespace bvm */

#endif /* __BVM_NODE_PARSER_H__ */
