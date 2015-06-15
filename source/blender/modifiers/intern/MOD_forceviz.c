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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_forceviz.c
 *  \ingroup modifiers
 */

#include <string.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_effect.h"
#include "BKE_image.h"
#include "BKE_modifier.h"
#include "BKE_texture.h"

#include "MOD_util.h"

#include "depsgraph_private.h"
#include "DEG_depsgraph_build.h"

static void initData(ModifierData *md) 
{
	ForceVizModifierData *fmd = (ForceVizModifierData *)md;
	
	BKE_texture_mapping_default(&fmd->tex_mapping, TEXMAP_TYPE_POINT);
	fmd->iuser.frames = 1;
	fmd->iuser.sfra = 1;
	fmd->iuser.fie_ima = 2;
	fmd->iuser.ok = 1;
	
	fmd->flag = MOD_FORCEVIZ_USE_IMG_VEC;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	ForceVizModifierData *fmd  = (ForceVizModifierData *)md;
	ForceVizModifierData *tfmd = (ForceVizModifierData *)target;
	
	memcpy(&tfmd->tex_mapping, &fmd->tex_mapping, sizeof(tfmd->tex_mapping));
	memcpy(&tfmd->iuser, &fmd->iuser, sizeof(tfmd->iuser));
}

static void freeData(ModifierData *UNUSED(md))
{
//	ForceVizModifierData *fmd = (ForceVizModifierData *)md;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	ForceVizModifierData *fmd = (ForceVizModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for UV coordinates if we need them */
	if (fmd->texmapping == MOD_DISP_MAP_UV) dataMask |= CD_MASK_MTFACE;

	return dataMask;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, 
                                  DerivedMesh *dm,
                                  ModifierApplyFlag UNUSED(flag))
{
	ForceVizModifierData *fmd = (ForceVizModifierData *)md;
	int numverts = dm->getNumVerts(dm);
	MVert *mverts = dm->getVertArray(dm);
	float (*vert_co)[3], (*tex_co)[3];
	int i;
	
	vert_co = MEM_mallocN(sizeof(*vert_co) * numverts, "forceviz modifier vert_co");
	for (i = 0; i < numverts; ++i)
		copy_v3_v3(vert_co[i], mverts[i].co);
	tex_co = MEM_mallocN(sizeof(*tex_co) * numverts, "forceviz modifier tex_co");
	get_texture_coords((MappingInfoModifierData *)fmd, ob, dm, vert_co, tex_co, numverts);

	BKE_forceviz_do(fmd, md->scene, ob, dm, tex_co);

	return dm;
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *UNUSED(bmain),
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	ForceVizModifierData *fmd = (ForceVizModifierData *)md;
	
	if ((fmd->texmapping == MOD_DISP_MAP_OBJECT) && fmd->map_object) {
		DagNode *curNode = dag_get_node(forest, fmd->map_object);
		dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "ForceViz modifier");
	}
	
	dag_add_relation(forest, obNode, obNode,
	                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "ForceViz modifier");
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *UNUSED(bmain),
                            struct Scene *UNUSED(scene),
                            Object *ob,
                            struct DepsNodeHandle *node)
{
	ForceVizModifierData *fmd = (ForceVizModifierData *)md;
	
	if ((fmd->texmapping == MOD_DISP_MAP_OBJECT) && fmd->map_object != NULL) {
		DEG_add_object_relation(node, fmd->map_object, DEG_OB_COMP_TRANSFORM, "ForceViz modifier");
	}

	DEG_add_object_relation(node, ob, DEG_OB_COMP_TRANSFORM, "ForceViz modifier");
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static void foreachIDLink(ModifierData *md, Object *ob,
                          IDWalkFunc walk, void *userData)
{
	ForceVizModifierData *fmd = (ForceVizModifierData *)md;

	walk(userData, ob, (ID **)&fmd->texture);
	walk(userData, ob, (ID **)&fmd->map_object);
	
	walk(userData, ob, (ID **)&fmd->image_vec);
	walk(userData, ob, (ID **)&fmd->image_div);
	walk(userData, ob, (ID **)&fmd->image_curl);
}

static void foreachObjectLink(ModifierData *md, Object *ob,
                              ObjectWalkFunc walk, void *userData)
{
	ForceVizModifierData *fmd = (ForceVizModifierData *)md;

	walk(userData, ob, &fmd->map_object);
}

static void foreachTexLink(ModifierData *md, Object *ob,
                           TexWalkFunc walk, void *userData)
{
	walk(userData, ob, md, "texture");
}

ModifierTypeInfo modifierType_ForceViz = {
	/* name */              "Force Visualization",
	/* structName */        "ForceVizModifierData",
	/* structSize */        sizeof(ForceVizModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    foreachTexLink,
};
