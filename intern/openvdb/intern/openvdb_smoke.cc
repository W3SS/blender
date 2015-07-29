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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Kevin Dietrich, Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>

#include <openvdb/tools/ValueTransformer.h>  /* for tools::foreach */

#include "openvdb_smoke.h"

namespace internal {

using namespace openvdb;
using namespace openvdb::math;

OpenVDBSmokeData::OpenVDBSmokeData(const Mat4R &cell_transform) :
    cell_transform(cell_transform)
{
}

OpenVDBSmokeData::~OpenVDBSmokeData()
{
}

void OpenVDBSmokeData::add_obstacle(const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles)
{
//	Transform::Ptr t = cell_transform.copy();
//	Mat4R m = (tfm * cell_transform).inverse();
//	Mat4R m = tfm * cell_transform;
	Mat4R m = cell_transform;
	Transform::Ptr t = Transform::createLinearTransform(m);
//	t->preMult(tfm);
//	Transform::Ptr t = Transform::createLinearTransform(1.0f/4096.0f);
//	Transform::Ptr t = Transform::createLinearTransform(1.0f/64.0f);
//	tools::MeshToVolume<FloatGrid> converter(t);
	
//	converter.convertToLevelSet(vertices, triangles);
//	density = converter.distGridPtr();
	density = tools::meshToLevelSet<FloatGrid>(*t, vertices, triangles,
	                                           std::vector<Vec4I>(), 1.0f);
//	density->pruneGrid();
	density->transform().print();
	
	printf("Made Grid: %d voxels\n", (int)density->activeVoxelCount());
#if 0
	typename FloatTree::NodeCIter node_iter;
	int i;
	for (node_iter = density->tree().cbeginNode(), i = 0; node_iter; ++node_iter, ++i) {
//		const int level = node_iter.getLevel();
		CoordBBox bbox;
		node_iter.getBoundingBox(bbox);
		float a = 9;
//		if (i % 100 == 0) {
//			printf("BBOX: (%.4f, %.4f, %.4f) <-> (%.4f, %.4f, %.4f)\n",
//			       );
//		}
	}
#endif
}

void OpenVDBSmokeData::clear_obstacles()
{
	if (density)
		density->clear();
}

bool OpenVDBSmokeData::step(float /*dt*/, int /*num_substeps*/)
{
	return true;
}

}  /* namespace internal */
