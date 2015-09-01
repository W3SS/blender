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
 * Contributor(s): Daniel Genrich
 *                 Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_smoke.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <limits.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "BKE_modifier.h"
#include "BKE_smoke.h"

#include "BLI_threads.h"

#include "DNA_modifier_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"

#include "WM_types.h"

EnumPropertyItem mod_smoke_field_items[] = {
    {MOD_SMOKE_VDB_FIELD_DENSITY, "DENSITY", 0, "Density", "Show density field"},
    {MOD_SMOKE_VDB_FIELD_VELOCITY, "VELOCITY", 0, "Velocity", "Show velocity field"},
    {MOD_SMOKE_VDB_FIELD_PRESSURE, "PRESSURE", 0, "Pressure", "Show pressure field"},
    {MOD_SMOKE_VDB_FIELD_DIVERGENCE, "DIVERGENCE", 0, "Divergence", "Show divergence field"},
    {0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_particle.h"

#include "BLI_math.h"

#include "smoke_API.h"


static void rna_Smoke_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	DAG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
}

static void rna_Smoke_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	rna_Smoke_update(bmain, scene, ptr);
	DAG_relations_tag_update(bmain);
}

static void rna_Smoke_resetCache(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	SmokeDomainSettings *settings = (SmokeDomainSettings *)ptr->data;
	if (settings->smd && settings->smd->domain)
		settings->point_cache[0]->flag |= PTCACHE_OUTDATED;
	DAG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
}

static void rna_Smoke_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	SmokeDomainSettings *settings = (SmokeDomainSettings *)ptr->data;

	smokeModifier_reset(settings->smd);
	rna_Smoke_resetCache(bmain, scene, ptr);

	rna_Smoke_update(bmain, scene, ptr);
}

static void rna_Smoke_reset_dependency(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	SmokeDomainSettings *settings = (SmokeDomainSettings *)ptr->data;

	smokeModifier_reset(settings->smd);

	if (settings->smd && settings->smd->domain)
		settings->smd->domain->point_cache[0]->flag |= PTCACHE_OUTDATED;

	rna_Smoke_dependency_update(bmain, scene, ptr);
}

static char *rna_SmokeDomainSettings_path(PointerRNA *ptr)
{
	SmokeDomainSettings *settings = (SmokeDomainSettings *)ptr->data;
	ModifierData *md = (ModifierData *)settings->smd;
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"].domain_settings", name_esc);
}

static char *rna_SmokeDomainVDBSettings_path(PointerRNA *ptr)
{
	SmokeDomainVDBSettings *settings = (SmokeDomainVDBSettings *)ptr->data;
	ModifierData *md = (ModifierData *)settings->smd;
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"].domain_vdb_settings", name_esc);
}

static char *rna_SmokeFlowSettings_path(PointerRNA *ptr)
{
	SmokeFlowSettings *settings = (SmokeFlowSettings *)ptr->data;
	ModifierData *md = (ModifierData *)settings->smd;
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"].flow_settings", name_esc);
}

static char *rna_SmokeCollSettings_path(PointerRNA *ptr)
{
	SmokeCollSettings *settings = (SmokeCollSettings *)ptr->data;
	ModifierData *md = (ModifierData *)settings->smd;
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"].coll_settings", name_esc);
}

static int rna_SmokeModifier_grid_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
#ifdef WITH_SMOKE
	SmokeDomainSettings *sds = (SmokeDomainSettings *)ptr->data;
	float *density = NULL;
	int size = 0;

	if (sds->flags & MOD_SMOKE_HIGHRES && sds->wt) {
		/* high resolution smoke */
		int res[3];

		smoke_turbulence_get_res(sds->wt, res);
		size = res[0] * res[1] * res[2];

		density = smoke_turbulence_get_density(sds->wt);
	}
	else if (sds->fluid) {
		/* regular resolution */
		size = sds->res[0] * sds->res[1] * sds->res[2];
		density = smoke_get_density(sds->fluid);
	}

	length[0] = (density) ? size : 0;
#else
	(void)ptr;
	length[0] = 0;
#endif
	return length[0];
}

static int rna_SmokeModifier_color_grid_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	rna_SmokeModifier_grid_get_length(ptr, length);

	length[0] *= 4;
	return length[0];
}

static int rna_SmokeModifier_velocity_grid_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
#ifdef WITH_SMOKE
	SmokeDomainSettings *sds = (SmokeDomainSettings *)ptr->data;
	float *vx = NULL;
	float *vy = NULL;
	float *vz = NULL;
	int size = 0;

	/* Velocity data is always low-resolution. */
	if (sds->fluid) {
		size = 3 * sds->res[0] * sds->res[1] * sds->res[2];
		vx = smoke_get_velocity_x(sds->fluid);
		vy = smoke_get_velocity_y(sds->fluid);
		vz = smoke_get_velocity_z(sds->fluid);
	}

	length[0] = (vx && vy && vz) ? size : 0;
#else
	(void)ptr;
	length[0] = 0;
#endif
	return length[0];
}

static void rna_SmokeModifier_density_grid_get(PointerRNA *ptr, float *values)
{
#ifdef WITH_SMOKE
	SmokeDomainSettings *sds = (SmokeDomainSettings *)ptr->data;
	int length[RNA_MAX_ARRAY_DIMENSION];
	int size = rna_SmokeModifier_grid_get_length(ptr, length);
	float *density;

	BLI_rw_mutex_lock(sds->fluid_mutex, THREAD_LOCK_READ);
	
	if (sds->flags & MOD_SMOKE_HIGHRES && sds->wt)
		density = smoke_turbulence_get_density(sds->wt);
	else
		density = smoke_get_density(sds->fluid);

	memcpy(values, density, size * sizeof(float));

	BLI_rw_mutex_unlock(sds->fluid_mutex);
#else
	UNUSED_VARS(ptr, values);
#endif
}

static void rna_SmokeModifier_velocity_grid_get(PointerRNA *ptr, float *values)
{
#ifdef WITH_SMOKE
	SmokeDomainSettings *sds = (SmokeDomainSettings *)ptr->data;
	int length[RNA_MAX_ARRAY_DIMENSION];
	int size = rna_SmokeModifier_velocity_grid_get_length(ptr, length);
	float *vx, *vy, *vz;
	int i;

	BLI_rw_mutex_lock(sds->fluid_mutex, THREAD_LOCK_READ);

	vx = smoke_get_velocity_x(sds->fluid);
	vy = smoke_get_velocity_y(sds->fluid);
	vz = smoke_get_velocity_z(sds->fluid);

	for (i = 0; i < size; i += 3) {
		*(values++) = *(vx++);
		*(values++) = *(vy++);
		*(values++) = *(vz++);
	}

	BLI_rw_mutex_unlock(sds->fluid_mutex);
#else
	UNUSED_VARS(ptr, values);
#endif
}

static void rna_SmokeModifier_color_grid_get(PointerRNA *ptr, float *values)
{
#ifdef WITH_SMOKE
	SmokeDomainSettings *sds = (SmokeDomainSettings *)ptr->data;

	BLI_rw_mutex_lock(sds->fluid_mutex, THREAD_LOCK_READ);

	if (sds->flags & MOD_SMOKE_HIGHRES) {
		if (smoke_turbulence_has_colors(sds->wt))
			smoke_turbulence_get_rgba(sds->wt, values, 0);
		else
			smoke_turbulence_get_rgba_from_density(sds->wt, sds->active_color, values, 0);
	}
	else {
		if (smoke_has_colors(sds->fluid))
			smoke_get_rgba(sds->fluid, values, 0);
		else
			smoke_get_rgba_from_density(sds->fluid, sds->active_color, values, 0);
	}

	BLI_rw_mutex_unlock(sds->fluid_mutex);
#else
	UNUSED_VARS(ptr, values);
#endif
}

static void rna_SmokeModifier_flame_grid_get(PointerRNA *ptr, float *values)
{
#ifdef WITH_SMOKE
	SmokeDomainSettings *sds = (SmokeDomainSettings *)ptr->data;
	int length[RNA_MAX_ARRAY_DIMENSION];
	int size = rna_SmokeModifier_grid_get_length(ptr, length);
	float *flame;

	BLI_rw_mutex_lock(sds->fluid_mutex, THREAD_LOCK_READ);
	
	if (sds->flags & MOD_SMOKE_HIGHRES && sds->wt)
		flame = smoke_turbulence_get_flame(sds->wt);
	else
		flame = smoke_get_flame(sds->fluid);
	
	if (flame)
		memcpy(values, flame, size * sizeof(float));
	else
		memset(values, 0, size * sizeof(float));

	BLI_rw_mutex_unlock(sds->fluid_mutex);
#else
	UNUSED_VARS(ptr, values);
#endif
}

static void rna_SmokeFlow_density_vgroup_get(PointerRNA *ptr, char *value)
{
	SmokeFlowSettings *flow = (SmokeFlowSettings *)ptr->data;
	rna_object_vgroup_name_index_get(ptr, value, flow->vgroup_density);
}

static int rna_SmokeFlow_density_vgroup_length(PointerRNA *ptr)
{
	SmokeFlowSettings *flow = (SmokeFlowSettings *)ptr->data;
	return rna_object_vgroup_name_index_length(ptr, flow->vgroup_density);
}

static void rna_SmokeFlow_density_vgroup_set(PointerRNA *ptr, const char *value)
{
	SmokeFlowSettings *flow = (SmokeFlowSettings *)ptr->data;
	rna_object_vgroup_name_index_set(ptr, value, &flow->vgroup_density);
}

static void rna_SmokeFlow_uvlayer_set(PointerRNA *ptr, const char *value)
{
	SmokeFlowSettings *flow = (SmokeFlowSettings *)ptr->data;
	rna_object_uvlayer_name_set(ptr, value, flow->uvlayer_name, sizeof(flow->uvlayer_name));
}

static PointerRNA rna_SmokeModifier_active_openvdb_cache_get(PointerRNA *ptr)
{
    SmokeDomainSettings *sds = (SmokeDomainSettings *)ptr->data;
    OpenVDBCache *cache = NULL;

	cache = BKE_openvdb_get_current_cache(sds);
    return rna_pointer_inherit_refine(ptr, &RNA_OpenVDBCache, cache);
}

static void rna_SmokeModifier_active_openvdb_cache_index_range(PointerRNA *ptr, int *min, int *max,
                                                               int *UNUSED(softmin), int *UNUSED(softmax))
{
    SmokeDomainSettings *sds = (SmokeDomainSettings *)ptr->data;
    *min = 0;
    *max = max_ii(0, BLI_listbase_count(&sds->vdb_caches) - 1);
}

static int rna_SmokeModifier_active_openvdb_cache_index_get(PointerRNA *ptr)
{
    SmokeDomainSettings *sds = (SmokeDomainSettings *)ptr->data;
    OpenVDBCache *cache = (OpenVDBCache *)sds->vdb_caches.first;
    int i = 0;

    for (; cache; cache = cache->next, i++) {
        if (cache->flags & OPENVDB_CACHE_CURRENT)
            return i;
    }
    return 0;
}

static void rna_SmokeModifier_active_openvdb_cache_index_set(struct PointerRNA *ptr, int value)
{
    SmokeDomainSettings *sds = (SmokeDomainSettings *)ptr->data;
    OpenVDBCache *cache = (OpenVDBCache *)sds->vdb_caches.first;
    int i = 0;

    for (; cache; cache = cache->next, i++) {
        if (i == value)
            cache->flags |= OPENVDB_CACHE_CURRENT;
        else
            cache->flags &= ~OPENVDB_CACHE_CURRENT;
    }
}


#else

static void rna_def_openvdb_cache(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_compression_items[] = {
		{ VDB_COMPRESSION_ZIP, "ZIP", 0, "Zip", "Slow and effective compression" },
#ifdef WITH_OPENVDB_BLOSC
		{ VDB_COMPRESSION_BLOSC, "BLOSC", 0, "Blosc", "Multithreaded compression, almost similar in size and quality as 'Zip'" },
#endif
		{ VDB_COMPRESSION_NONE, "NONE", 0, "None", "Do not use any compression" },
		{ 0, NULL, 0, NULL, NULL }
	};

	srna = RNA_def_struct(brna, "OpenVDBCache", NULL);
	RNA_def_struct_ui_text(srna, "OpenVDB cache", "OpenVDB cache");

	prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "startframe");
	RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
	RNA_def_property_ui_range(prop, 1, MAXFRAME, 1, 1);
	RNA_def_property_ui_text(prop, "Start", "Frame on which the simulation starts");

	prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "endframe");
	RNA_def_property_range(prop, 1, MAXFRAME);
	RNA_def_property_ui_text(prop, "End", "Frame on which the simulation stops");

	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "path");
	RNA_def_property_ui_text(prop, "File Path", "Cache file path");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "Cache name");
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "is_baked", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", OPENVDB_CACHE_BAKED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "compression", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "compression");
	RNA_def_property_enum_items(prop, prop_compression_items);
	RNA_def_property_ui_text(prop, "File Compression",
	                         "Select what type of compression to use when writing the files");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

	prop = RNA_def_property(srna, "save_as_half", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", OPENVDB_CACHE_SAVE_AS_HALF);
	RNA_def_property_ui_text(prop, "Save as Half",
	                         "Write all scalar (including vector) grids to the file as 16-bit half floats to reduce file size");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);
}

static void rna_def_smoke_domain_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_noise_type_items[] = {
		{MOD_SMOKE_NOISEWAVE, "NOISEWAVE", 0, "Wavelet", ""},
#ifdef WITH_FFTW3
		{MOD_SMOKE_NOISEFFT, "NOISEFFT", 0, "FFT", ""},
#endif
		/*  {MOD_SMOKE_NOISECURL, "NOISECURL", 0, "Curl", ""}, */
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem smoke_cache_comp_items[] = {
		{SM_CACHE_LIGHT, "CACHELIGHT", 0, "Light", "Fast but not so effective compression"},
		{SM_CACHE_HEAVY, "CACHEHEAVY", 0, "Heavy", "Effective but slow compression"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem smoke_highres_sampling_items[] = {
		{SM_HRES_FULLSAMPLE, "FULLSAMPLE", 0, "Full Sample", ""},
		{SM_HRES_LINEAR, "LINEAR", 0, "Linear", ""},
		{SM_HRES_NEAREST, "NEAREST", 0, "Nearest", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem smoke_domain_colli_items[] = {
		{SM_BORDER_OPEN, "BORDEROPEN", 0, "Open", "Smoke doesn't collide with any border"},
		{SM_BORDER_VERTICAL, "BORDERVERTICAL", 0, "Vertically Open",
		 "Smoke doesn't collide with top and bottom sides"},
		{SM_BORDER_CLOSED, "BORDERCLOSED", 0, "Collide All", "Smoke collides with every side"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_cache_items[] = {
	    {SMOKE_CACHE_POINTCACHE, "POINTCACHE", 0, "Point Cache", "Use Point Cache for caching on disk"},
	    {SMOKE_CACHE_OPENVDB, "OPENVDB", 0, "OpenVDB", "Use OpenVDB for caching on disk"},
	    {0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "SmokeDomainSettings", NULL);
	RNA_def_struct_ui_text(srna, "Domain Settings", "Smoke domain settings");
	RNA_def_struct_sdna(srna, "SmokeDomainSettings");
	RNA_def_struct_path_func(srna, "rna_SmokeDomainSettings_path");

	prop = RNA_def_property(srna, "resolution_max", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxres");
	RNA_def_property_range(prop, 6, 512);
	RNA_def_property_ui_range(prop, 24, 512, 2, -1);
	RNA_def_property_ui_text(prop, "Max Res", "Maximal resolution used in the fluid domain");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "amplify", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "amplify");
	RNA_def_property_range(prop, 1, 10);
	RNA_def_property_ui_range(prop, 1, 10, 1, -1);
	RNA_def_property_ui_text(prop, "Amplification", "Enhance the resolution of smoke by this factor using noise");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "use_high_resolution", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_SMOKE_HIGHRES);
	RNA_def_property_ui_text(prop, "High res", "Enable high resolution (using amplification)");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "show_high_resolution", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "viewsettings", MOD_SMOKE_VIEW_SHOWBIG);
	RNA_def_property_ui_text(prop, "Show High Resolution", "Show high resolution (using amplification)");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "noise_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noise");
	RNA_def_property_enum_items(prop, prop_noise_type_items);
	RNA_def_property_ui_text(prop, "Noise Method", "Noise method which is used for creating the high resolution");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "alpha");
	RNA_def_property_range(prop, -5.0, 5.0);
	RNA_def_property_ui_range(prop, -5.0, 5.0, 0.02, 5);
	RNA_def_property_ui_text(prop, "Density",
	                         "How much density affects smoke motion (higher value results in faster rising smoke)");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "beta", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "beta");
	RNA_def_property_range(prop, -5.0, 5.0);
	RNA_def_property_ui_range(prop, -5.0, 5.0, 0.02, 5);
	RNA_def_property_ui_text(prop, "Heat",
	                         "How much heat affects smoke motion (higher value results in faster rising smoke)");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "collision_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "coll_group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Collision Group", "Limit collisions to this group");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset_dependency");

	prop = RNA_def_property(srna, "fluid_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "fluid_group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Fluid Group", "Limit fluid objects to this group");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset_dependency");

	prop = RNA_def_property(srna, "effector_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "eff_group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Group", "Limit effectors to this group");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset_dependency");

	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "strength");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_range(prop, 0.0, 10.0, 1, 2);
	RNA_def_property_ui_text(prop, "Strength", "Strength of noise");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "dissolve_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "diss_speed");
	RNA_def_property_range(prop, 1.0, 10000.0);
	RNA_def_property_ui_range(prop, 1.0, 10000.0, 1, -1);
	RNA_def_property_ui_text(prop, "Dissolve Speed", "Dissolve Speed");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "use_dissolve_smoke", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_SMOKE_DISSOLVE);
	RNA_def_property_ui_text(prop, "Dissolve Smoke", "Enable smoke to disappear over time");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "use_dissolve_smoke_log", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_SMOKE_DISSOLVE_LOG);
	RNA_def_property_ui_text(prop, "Logarithmic dissolve", "Using 1/x ");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "point_cache[0]");
	RNA_def_property_ui_text(prop, "Point Cache", "");

	prop = RNA_def_property(srna, "point_cache_compress_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "cache_comp");
	RNA_def_property_enum_items(prop, smoke_cache_comp_items);
	RNA_def_property_ui_text(prop, "Cache Compression", "Compression method to be used");

	prop = RNA_def_property(srna, "collision_extents", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "border_collisions");
	RNA_def_property_enum_items(prop, smoke_domain_colli_items);
	RNA_def_property_ui_text(prop, "Border Collisions",
	                         "Select which domain border will be treated as collision object");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EffectorWeights");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Weights", "");

	prop = RNA_def_property(srna, "highres_sampling", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, smoke_highres_sampling_items);
	RNA_def_property_ui_text(prop, "Emitter", "Method for sampling the high resolution flow");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "time_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "time_scale");
	RNA_def_property_range(prop, 0.2, 1.5);
	RNA_def_property_ui_range(prop, 0.2, 1.5, 0.02, 5);
	RNA_def_property_ui_text(prop, "Time Scale", "Adjust simulation speed");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "vorticity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vorticity");
	RNA_def_property_range(prop, 0.01, 4.0);
	RNA_def_property_ui_range(prop, 0.01, 4.0, 0.02, 5);
	RNA_def_property_ui_text(prop, "Vorticity", "Amount of turbulence/rotation in fluid");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "density_grid", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 32);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_dynamic_array_funcs(prop, "rna_SmokeModifier_grid_get_length");
	RNA_def_property_float_funcs(prop, "rna_SmokeModifier_density_grid_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Density Grid", "Smoke density grid");

	prop = RNA_def_property(srna, "velocity_grid", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 32);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_dynamic_array_funcs(prop, "rna_SmokeModifier_velocity_grid_get_length");
	RNA_def_property_float_funcs(prop, "rna_SmokeModifier_velocity_grid_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Velocity Grid", "Smoke velocity grid");

	prop = RNA_def_property(srna, "flame_grid", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 32);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_dynamic_array_funcs(prop, "rna_SmokeModifier_grid_get_length");
	RNA_def_property_float_funcs(prop, "rna_SmokeModifier_flame_grid_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Flame Grid", "Smoke flame grid");

	prop = RNA_def_property(srna, "color_grid", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 32);
	RNA_def_property_flag(prop, PROP_DYNAMIC);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_dynamic_array_funcs(prop, "rna_SmokeModifier_color_grid_get_length");
	RNA_def_property_float_funcs(prop, "rna_SmokeModifier_color_grid_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Color Grid", "Smoke color grid");

	prop = RNA_def_property(srna, "cell_size", PROP_FLOAT, PROP_XYZ); /* can change each frame when using adaptive domain */
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "cell_size", "Cell Size");

	prop = RNA_def_property(srna, "start_point", PROP_FLOAT, PROP_XYZ); /* can change each frame when using adaptive domain */
	RNA_def_property_float_sdna(prop, NULL, "p0");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "p0", "Start point");

	prop = RNA_def_property(srna, "domain_resolution", PROP_INT, PROP_XYZ); /* can change each frame when using adaptive domain */
	RNA_def_property_int_sdna(prop, NULL, "res");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "res", "Smoke Grid Resolution");

	prop = RNA_def_property(srna, "burning_rate", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 4.0);
	RNA_def_property_ui_range(prop, 0.01, 2.0, 1.0, 5);
	RNA_def_property_ui_text(prop, "Speed", "Speed of the burning reaction (use larger values for smaller flame)");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "flame_smoke", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 8.0);
	RNA_def_property_ui_range(prop, 0.0, 4.0, 1.0, 5);
	RNA_def_property_ui_text(prop, "Smoke", "Amount of smoke created by burning fuel");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "flame_vorticity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 2.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1.0, 5);
	RNA_def_property_ui_text(prop, "Vorticity", "Additional vorticity for the flames");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "flame_ignition", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.5, 5.0);
	RNA_def_property_ui_range(prop, 0.5, 2.5, 1.0, 5);
	RNA_def_property_ui_text(prop, "Ignition", "Minimum temperature of flames");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "flame_max_temp", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 1.0, 10.0);
	RNA_def_property_ui_range(prop, 1.0, 5.0, 1.0, 5);
	RNA_def_property_ui_text(prop, "Maximum", "Maximum temperature of flames");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "flame_smoke_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Smoke Color", "Color of smoke emitted from burning fuel");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "use_adaptive_domain", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_SMOKE_ADAPTIVE_DOMAIN);
	RNA_def_property_ui_text(prop, "Adaptive Domain", "Adapt simulation resolution and size to fluid");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "additional_res", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "adapt_res");
	RNA_def_property_range(prop, 0, 512);
	RNA_def_property_ui_range(prop, 0, 512, 2, -1);
	RNA_def_property_ui_text(prop, "Additional", "Maximum number of additional cells");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "adapt_margin", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "adapt_margin");
	RNA_def_property_range(prop, 2, 24);
	RNA_def_property_ui_range(prop, 2, 24, 2, -1);
	RNA_def_property_ui_text(prop, "Margin", "Margin added around fluid to minimize boundary interference");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "adapt_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 0.5);
	RNA_def_property_ui_range(prop, 0.01, 0.5, 1.0, 5);
	RNA_def_property_ui_text(prop, "Threshold",
	                         "Maximum amount of fluid cell can contain before it is considered empty");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_resetCache");

	prop = RNA_def_property(srna, "cache_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "cache_type");
	RNA_def_property_ui_text(prop, "", "Use OpenVDB to cache the simulation");
	RNA_def_property_enum_items(prop, prop_cache_items);
	RNA_def_property_ui_text(prop, "Cache Type",
	                         "Select the type of the caching system to use");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

	prop = RNA_def_property(srna, "cache", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "vdb_caches", NULL);
	RNA_def_property_struct_type(prop, "OpenVDBCache");

	prop = RNA_def_property(srna, "active_openvdb_cache", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "OpenVDBCache");
	RNA_def_property_pointer_funcs(prop, "rna_SmokeModifier_active_openvdb_cache_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Active OpenVDB cache", "");

	prop = RNA_def_property(srna, "active_openvdb_cache_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_SmokeModifier_active_openvdb_cache_index_get",
	                           "rna_SmokeModifier_active_openvdb_cache_index_set",
	                           "rna_SmokeModifier_active_openvdb_cache_index_range");
	RNA_def_property_ui_text(prop, "Active OpenVDB cache Index", "");

	rna_def_openvdb_cache(brna);
}

static void rna_def_smoke_domain_vdb_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem res_axis_items[] = {
	    {0, "X", 0, "X", "Use X axis for cell size"},
	    {1, "Y", 0, "Y", "Use Y axis for cell size"},
	    {2, "Z", 0, "Z", "Use Z axis for cell size"},
	    {0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem display_mode_items[] = {
	    {MOD_SMOKE_VDB_DISPLAY_BOUNDS, "BOUNDS", 0, "Bounds", "Show bounding box of active cells"},
	    {MOD_SMOKE_VDB_DISPLAY_BLEND, "BLEND", 0, "Blend", "Use smooth alpha-blended display"},
	    {MOD_SMOKE_VDB_DISPLAY_CELLS, "CELLS", 0, "Cells", "Show active grid cells"},
	    {MOD_SMOKE_VDB_DISPLAY_BOXES, "BOXES", 0, "Boxes", "Indicate field strength with boxes"},
	    {MOD_SMOKE_VDB_DISPLAY_NEEDLES, "NEEDLES", 0, "Needles", "Show field vectors with needles"},
	    {MOD_SMOKE_VDB_DISPLAY_STAGGERED, "STAGGERED", 0, "Staggered", "Show field vector components on cell faces"},
	    {0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "SmokeDomainVDBSettings", NULL);
	RNA_def_struct_ui_text(srna, "Domain VDB Settings", "Smoke VDB domain settings");
	RNA_def_struct_sdna(srna, "SmokeDomainVDBSettings");
	RNA_def_struct_path_func(srna, "rna_SmokeDomainVDBSettings_path");

	prop = RNA_def_property(srna, "collision_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "coll_group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Collision Group", "Limit collisions to this group");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset_dependency");

	prop = RNA_def_property(srna, "fluid_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "fluid_group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Fluid Group", "Limit fluid objects to this group");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset_dependency");

	prop = RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EffectorWeights");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Weights", "");
	
	prop = RNA_def_property(srna, "cache", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "OpenVDBCache");
	RNA_def_property_ui_text(prop, "OpenVDB cache", "");

	prop = RNA_def_property(srna, "show_grid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOKE_VDB_SHOW_GRID);
	RNA_def_property_ui_text(prop, "Show Grid", "Show grid-based data");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "show_material_points", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MOD_SMOKE_VDB_SHOW_MATPOINTS);
	RNA_def_property_ui_text(prop, "Show Material Points", "Show material points");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "resolution_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "res_axis");
	RNA_def_property_enum_items(prop, res_axis_items);
	RNA_def_property_enum_default(prop, 2);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "resolution", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "res");
	RNA_def_property_range(prop, 1, 1 << 16);
	RNA_def_property_int_default(prop, 32);
	RNA_def_property_ui_range(prop, 16, 512, 2, -1);
	RNA_def_property_ui_text(prop, "Resolution", "Resolution of the domain");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_enum_items(prop, display_mode_items);
	RNA_def_property_enum_default(prop, MOD_SMOKE_VDB_DISPLAY_BOUNDS);
	RNA_def_property_ui_text(prop, "Display Mode", "Mode of display for the smoke sim");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "display_field", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, mod_smoke_field_items);
	RNA_def_property_enum_default(prop, MOD_SMOKE_VDB_FIELD_DENSITY);
	RNA_def_property_ui_text(prop, "Display Field", "Primary field type to display");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "display_value_scale", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001f, 100.0f, 0.1f, 3);
	RNA_def_property_ui_text(prop, "Display Value Scale", "Scale displayed values to adjust visualization");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);
}

static void rna_def_smoke_flow_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem smoke_flow_types[] = {
		{MOD_SMOKE_FLOW_TYPE_OUTFLOW, "OUTFLOW", 0, "Outflow", "Delete smoke from simulation"},
		{MOD_SMOKE_FLOW_TYPE_SMOKE, "SMOKE", 0, "Smoke", "Add smoke"},
		{MOD_SMOKE_FLOW_TYPE_SMOKEFIRE, "BOTH", 0, "Fire + Smoke", "Add fire and smoke"},
		{MOD_SMOKE_FLOW_TYPE_FIRE, "FIRE", 0, "Fire", "Add fire"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem smoke_flow_sources[] = {
		{MOD_SMOKE_FLOW_SOURCE_PARTICLES, "PARTICLES", ICON_PARTICLES, "Particle System", "Emit smoke from particles"},
		{MOD_SMOKE_FLOW_SOURCE_MESH, "MESH", ICON_META_CUBE, "Mesh", "Emit smoke from mesh surface or volume"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem smoke_flow_texture_types[] = {
		{MOD_SMOKE_FLOW_TEXTURE_MAP_AUTO, "AUTO", 0, "Generated", "Generated coordinates centered to flow object"},
		{MOD_SMOKE_FLOW_TEXTURE_MAP_UV, "UV", 0, "UV", "Use UV layer for texture coordinates"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "SmokeFlowSettings", NULL);
	RNA_def_struct_ui_text(srna, "Flow Settings", "Smoke flow settings");
	RNA_def_struct_sdna(srna, "SmokeFlowSettings");
	RNA_def_struct_path_func(srna, "rna_SmokeFlowSettings_path");

	prop = RNA_def_property(srna, "density", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "density");
	RNA_def_property_range(prop, 0.0, 1);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1.0, 4);
	RNA_def_property_ui_text(prop, "Density", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "smoke_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "color");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Smoke Color", "Color of smoke");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "fuel_amount", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 10);
	RNA_def_property_ui_range(prop, 0.0, 5.0, 1.0, 4);
	RNA_def_property_ui_text(prop, "Flame Rate", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "temperature", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "temp");
	RNA_def_property_range(prop, -10, 10);
	RNA_def_property_ui_range(prop, -10, 10, 1, 1);
	RNA_def_property_ui_text(prop, "Temp. Diff.", "Temperature difference to ambient temperature");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");
	
	prop = RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "psys");
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Particle Systems", "Particle systems emitted from the object");
	RNA_def_property_update(prop, 0, "rna_Smoke_reset_dependency");

	prop = RNA_def_property(srna, "smoke_flow_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, smoke_flow_types);
	RNA_def_property_ui_text(prop, "Flow Type", "Change how flow affects the simulation");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "smoke_flow_source", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "source");
	RNA_def_property_enum_items(prop, smoke_flow_sources);
	RNA_def_property_ui_text(prop, "Source", "Change how smoke is emitted");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "use_absolute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_SMOKE_FLOW_ABSOLUTE);
	RNA_def_property_ui_text(prop, "Absolute Density", "Only allow given density value in emitter area");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "use_initial_velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_SMOKE_FLOW_INITVELOCITY);
	RNA_def_property_ui_text(prop, "Initial Velocity", "Smoke has some initial velocity when it is emitted");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "velocity_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vel_multi");
	RNA_def_property_range(prop, -100.0, 100.0);
	RNA_def_property_ui_range(prop, -2.0, 2.0, 0.05, 5);
	RNA_def_property_ui_text(prop, "Source", "Multiplier of source velocity passed to smoke");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "velocity_normal", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vel_normal");
	RNA_def_property_range(prop, -100.0, 100.0);
	RNA_def_property_ui_range(prop, -2.0, 2.0, 0.05, 5);
	RNA_def_property_ui_text(prop, "Normal", "Amount of normal directional velocity");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "velocity_random", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vel_random");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_range(prop, 0.0, 2.0, 0.05, 5);
	RNA_def_property_ui_text(prop, "Random", "Amount of random velocity");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "volume_density", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 0.05, 5);
	RNA_def_property_ui_text(prop, "Volume", "Factor for smoke emitted from inside the mesh volume");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "surface_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_range(prop, 0.5, 5.0, 0.05, 5);
	RNA_def_property_ui_text(prop, "Surface", "Maximum distance from mesh surface to emit smoke");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "particle_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.1, 20.0);
	RNA_def_property_ui_range(prop, 0.5, 5.0, 0.05, 5);
	RNA_def_property_ui_text(prop, "Size", "Particle size in simulation cells");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "use_particle_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_SMOKE_FLOW_USE_PART_SIZE);
	RNA_def_property_ui_text(prop, "Set Size", "Set particle size in simulation cells or use nearest cell");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "subframes", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 50);
	RNA_def_property_ui_range(prop, 0, 10, 1, -1);
	RNA_def_property_ui_text(prop, "Subframes", "Number of additional samples to take between frames to improve quality of fast moving flows");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "density_vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_SmokeFlow_density_vgroup_get",
	                              "rna_SmokeFlow_density_vgroup_length",
	                              "rna_SmokeFlow_density_vgroup_set");
	RNA_def_property_ui_text(prop, "Vertex Group",
	                         "Name of vertex group which determines surface emission rate");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "use_texture", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_SMOKE_FLOW_TEXTUREEMIT);
	RNA_def_property_ui_text(prop, "Use Texture", "Use a texture to control emission strength");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "texture_map_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "texture_type");
	RNA_def_property_enum_items(prop, smoke_flow_texture_types);
	RNA_def_property_ui_text(prop, "Mapping", "Texture mapping type");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
	RNA_def_property_ui_text(prop, "UV Map", "UV map name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SmokeFlow_uvlayer_set");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "noise_texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Texture", "Texture that controls emission strength");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "texture_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 10.0);
	RNA_def_property_ui_range(prop, 0.1, 5.0, 0.05, 5);
	RNA_def_property_ui_text(prop, "Size", "Size of texture mapping");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");

	prop = RNA_def_property(srna, "texture_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 200.0);
	RNA_def_property_ui_range(prop, 0.0, 100.0, 0.05, 5);
	RNA_def_property_ui_text(prop, "Offset", "Z-offset of texture mapping");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");
}

static void rna_def_smoke_coll_settings(BlenderRNA *brna)
{
	static EnumPropertyItem smoke_coll_type_items[] = {
		{SM_COLL_STATIC, "COLLSTATIC", 0, "Static", "Non moving obstacle"},
		{SM_COLL_RIGID, "COLLRIGID", 0, "Rigid", "Rigid obstacle"},
		{SM_COLL_ANIMATED, "COLLANIMATED", 0, "Animated", "Animated obstacle"},
		{0, NULL, 0, NULL, NULL}
	};

	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SmokeCollSettings", NULL);
	RNA_def_struct_ui_text(srna, "Collision Settings", "Smoke collision settings");
	RNA_def_struct_sdna(srna, "SmokeCollSettings");
	RNA_def_struct_path_func(srna, "rna_SmokeCollSettings_path");

	prop = RNA_def_property(srna, "collision_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, smoke_coll_type_items);
	RNA_def_property_ui_text(prop, "Collision type", "Collision type");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Smoke_reset");
}

void RNA_def_smoke(BlenderRNA *brna)
{
	rna_def_smoke_domain_settings(brna);
	rna_def_smoke_domain_vdb_settings(brna);
	rna_def_smoke_flow_settings(brna);
	rna_def_smoke_coll_settings(brna);
}

#endif
