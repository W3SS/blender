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
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_nparticle.c
 *  \ingroup RNA
 */

#include "DNA_nparticle_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_pagedbuffer.h"

#include "BKE_nparticle.h"
#include "BKE_report.h"

#include "RNA_access.h"

static StructRNA *rna_NParticleBufferAttribute_refine(PointerRNA *ptr)
{
	NParticleBufferAttribute *attr = ptr->data;
	switch (attr->desc.datatype) {
		case PAR_ATTR_DATATYPE_FLOAT:
			return &RNA_NParticleBufferAttributeFloat;
		case PAR_ATTR_DATATYPE_INT:
			return &RNA_NParticleBufferAttributeInt;
		case PAR_ATTR_DATATYPE_BOOL:
			return &RNA_NParticleBufferAttributeBool;
		case PAR_ATTR_DATATYPE_VECTOR:
			return &RNA_NParticleBufferAttributeVector;
		case PAR_ATTR_DATATYPE_POINT:
			return &RNA_NParticleBufferAttributePoint;
		case PAR_ATTR_DATATYPE_NORMAL:
			return &RNA_NParticleBufferAttributeNormal;
		case PAR_ATTR_DATATYPE_COLOR:
			return &RNA_NParticleBufferAttributeColor;
		case PAR_ATTR_DATATYPE_MATRIX:
			return &RNA_NParticleBufferAttributeMatrix;
		
		default:
			BLI_assert(false);	/* unknown data type, should never happen */
			return &RNA_NParticleBufferAttribute;
	}
}

/* Returns the RNA type used for data elements in the attribute buffer.
 * This is mirrors the collection type (see rna_NParticleBufferAttribute_refine),
 * which must also be specified since each collection has a fixed data type.
 */
static StructRNA *rna_NParticleBufferAttribute_data_srna(NParticleBufferAttribute *attr)
{
	switch (attr->desc.datatype) {
		case PAR_ATTR_DATATYPE_FLOAT:
			return &RNA_NParticleDataFloat;
		case PAR_ATTR_DATATYPE_INT:
			return &RNA_NParticleDataInt;
		case PAR_ATTR_DATATYPE_BOOL:
			return &RNA_NParticleDataBool;
		case PAR_ATTR_DATATYPE_VECTOR:
			return &RNA_NParticleDataVector;
		case PAR_ATTR_DATATYPE_POINT:
			return &RNA_NParticleDataPoint;
		case PAR_ATTR_DATATYPE_NORMAL:
			return &RNA_NParticleDataNormal;
		case PAR_ATTR_DATATYPE_COLOR:
			return &RNA_NParticleDataColor;
		case PAR_ATTR_DATATYPE_MATRIX:
			return &RNA_NParticleDataMatrix;
		
		default:
			BLI_assert(false);	/* unknown data type, should never happen */
			return &RNA_NParticleBufferAttribute;
	}
}

static void rna_NParticleAttribute_datatype_set(PointerRNA *ptr, int value)
{
//	NParticleAttribute *attr = ptr->data;
	/* XXX TODO */
	BLI_assert(false);
}

static void rna_NParticleBufferAttribute_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	NParticleBufferAttribute *attr = ptr->data;
	bPagedBufferIterator *internal = MEM_callocN(sizeof(bPagedBufferIterator), "particle data iterator");
	BLI_pbuf_iter_init(&attr->data, internal);
	
	iter->internal = internal;
	iter->valid = BLI_pbuf_iter_valid(&attr->data, internal);
}

static void rna_NParticleBufferAttribute_data_next(CollectionPropertyIterator *iter)
{
	NParticleBufferAttribute *attr = iter->ptr.data;
	bPagedBufferIterator *internal = iter->internal;
	BLI_pbuf_iter_next(&attr->data, internal);
	iter->valid = BLI_pbuf_iter_valid(&attr->data, internal);
}

static void rna_NParticleBufferAttribute_data_end(CollectionPropertyIterator *iter)
{
	MEM_freeN(iter->internal);
	iter->internal = NULL;
}

static PointerRNA rna_NParticleBufferAttribute_data_get(CollectionPropertyIterator *iter)
{
	NParticleBufferAttribute *attr = iter->ptr.data;
	bPagedBufferIterator *internal = iter->internal;
	StructRNA *data_srna = rna_NParticleBufferAttribute_data_srna(attr);
	PointerRNA ptr;
	RNA_pointer_create(iter->ptr.id.data, data_srna, internal->data, &ptr);
	return ptr;
}

static int rna_NParticleBufferAttribute_data_length(PointerRNA *ptr)
{
	NParticleBufferAttribute *attr = ptr->data;
	return attr->data.totelem;
}

int rna_NParticleBufferAttribute_data_lookup_int(PointerRNA *ptr, int key, PointerRNA *r_ptr)
{
	NParticleBufferAttribute *attr = ptr->data;
	void *data = BLI_pbuf_get(&attr->data, key);
	StructRNA *data_srna = rna_NParticleBufferAttribute_data_srna(attr);
	RNA_pointer_create(ptr->id.data, data_srna, data, r_ptr);
	return true;
}

int rna_NParticleBufferAttribute_data_assign_int(PointerRNA *ptr, int key, const PointerRNA *assign_ptr)
{
	NParticleBufferAttribute *attr = ptr->data;
	void *data = BLI_pbuf_get(&attr->data, key);
	StructRNA *data_srna = rna_NParticleBufferAttribute_data_srna(attr);
	PointerRNA data_ptr;
	RNA_pointer_create(ptr->id.data, data_srna, data, &data_ptr);
	/* XXX TODO */
	BLI_assert(false);
	return true;
}

static float rna_NParticleDataFloat_get(PointerRNA *ptr)
{
	return *(float *)ptr->data;
}

static void rna_NParticleDataFloat_set(PointerRNA *ptr, float value)
{
	*(float *)ptr->data = value;
}

static int rna_NParticleDataInt_get(PointerRNA *ptr)
{
	return *(int *)ptr->data;
}

static void rna_NParticleDataInt_set(PointerRNA *ptr, int value)
{
	*(int *)ptr->data = value;
}

static int rna_NParticleDataBool_get(PointerRNA *ptr)
{
	return *(bool *)ptr->data;
}

static void rna_NParticleDataBool_set(PointerRNA *ptr, int value)
{
	*(bool *)ptr->data = value;
}

static void rna_NParticleDataVector_get(PointerRNA *ptr, float *result)
{
	copy_v3_v3(result, (float *)ptr->data);
}

static void rna_NParticleDataVector_set(PointerRNA *ptr, const float *value)
{
	copy_v3_v3((float *)ptr->data, value);
}

static void rna_NParticleDataColor_get(PointerRNA *ptr, float *result)
{
	copy_v4_v4(result, (float *)ptr->data);
}

static void rna_NParticleDataColor_set(PointerRNA *ptr, const float *value)
{
	copy_v4_v4((float *)ptr->data, value);
}

static void rna_NParticleDataMatrix_get(PointerRNA *ptr, float *result)
{
	copy_m4_m4((float(*)[4])result, (float(*)[4])ptr->data);
}

static void rna_NParticleDataMatrix_set(PointerRNA *ptr, const float *value)
{
	copy_m4_m4((float(*)[4])ptr->data, (float(*)[4])value);
}

static NParticleBufferAttribute *rna_NParticleBuffer_attributes_new(NParticleBuffer *buf, ReportList *reports, const char *name, int datatype)
{
	if (BKE_nparticle_attribute_find(buf, name)) {
		BKE_reportf(reports, RPT_ERROR_INVALID_INPUT, "Particle attribute with name %s already exists", name);
		return NULL;
	}
	return BKE_nparticle_attribute_new(buf, name, datatype);
}

static void rna_NParticleBuffer_attributes_remove(NParticleBuffer *buf, NParticleBufferAttribute *attr)
{
	BKE_nparticle_attribute_remove(buf, attr);
}

static void rna_NParticleBuffer_attributes_clear(NParticleBuffer *buf)
{
	BKE_nparticle_attribute_remove_all(buf);
}

static void rna_NParticleBuffer_attributes_move(NParticleBuffer *buf, int from_index, int to_index)
{
	BKE_nparticle_attribute_move(buf, from_index, to_index);
}

#else

EnumPropertyItem nparticle_attribute_datatype_all[] = {
    {PAR_ATTR_DATATYPE_INTERNAL,    "INTERNAL",     0,  "Internal"      ""},
    {PAR_ATTR_DATATYPE_FLOAT,       "FLOAT",        0,  "Float"         ""},
    {PAR_ATTR_DATATYPE_INT,         "INT",          0,  "Int"           ""},
    {PAR_ATTR_DATATYPE_BOOL,        "BOOL",         0,  "Bool"          ""},
    {PAR_ATTR_DATATYPE_VECTOR,      "VECTOR",       0,  "Vector"        ""},
    {PAR_ATTR_DATATYPE_POINT,       "POINT",        0,  "Point"         ""},
    {PAR_ATTR_DATATYPE_NORMAL,      "NORMAL",       0,  "Normal"        ""},
    {PAR_ATTR_DATATYPE_COLOR,       "COLOR",        0,  "Color"         ""},
    {PAR_ATTR_DATATYPE_MATRIX,      "MATRIX",       0,  "Matrix"        ""},
    {0, NULL, 0, NULL, NULL}
};

EnumPropertyItem nparticle_attribute_datatype_user[] = {
    {PAR_ATTR_DATATYPE_FLOAT,       "FLOAT",        0,  "Float"         ""},
    {PAR_ATTR_DATATYPE_INT,         "INT",          0,  "Int"           ""},
    {PAR_ATTR_DATATYPE_BOOL,        "BOOL",         0,  "Bool"          ""},
    {PAR_ATTR_DATATYPE_VECTOR,      "VECTOR",       0,  "Vector"        ""},
    {PAR_ATTR_DATATYPE_POINT,       "POINT",        0,  "Point"         ""},
    {PAR_ATTR_DATATYPE_NORMAL,      "NORMAL",       0,  "Normal"        ""},
    {PAR_ATTR_DATATYPE_COLOR,       "COLOR",        0,  "Color"         ""},
    {PAR_ATTR_DATATYPE_MATRIX,      "MATRIX",       0,  "Matrix"        ""},
    {0, NULL, 0, NULL, NULL}
};

static void def_nparticle_attribute(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "Unique name");
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "datatype", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "datatype");
	RNA_def_property_enum_items(prop, nparticle_attribute_datatype_all);
	RNA_def_property_enum_funcs(prop, NULL, "rna_NParticleAttribute_datatype_set", NULL);
	RNA_def_property_ui_text(prop, "Data Type", "Basic data type");
}

/* defines a subtype of NParticleBufferAttribute with a specific collection property for actual data */
static StructRNA *def_nparticle_buffer_attribute_type(BlenderRNA *brna, const char *structname, const char *data_structname)
{
	StructRNA *srna = RNA_def_struct(brna, structname, "NParticleBufferAttribute");
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, data_structname);
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_NParticleBufferAttribute_data_begin", "rna_NParticleBufferAttribute_data_next",
	                                  "rna_NParticleBufferAttribute_data_end", "rna_NParticleBufferAttribute_data_get",
	                                  "rna_NParticleBufferAttribute_data_length", "rna_NParticleBufferAttribute_data_lookup_int",
	                                  NULL, "rna_NParticleBufferAttribute_data_assign_int");

	return srna;
}

static void rna_def_nparticle_buffer_attribute(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "NParticleBufferAttribute", NULL);
	RNA_def_struct_sdna(srna, "NParticleBufferAttribute");
	RNA_def_struct_ui_text(srna, "Particle Buffer Attribute", "Attribute buffer in a particle system");
	RNA_def_struct_refine_func(srna, "rna_NParticleBufferAttribute_refine");

	RNA_def_struct_sdna_from(srna, "NParticleAttribute", "desc");
	def_nparticle_attribute(srna);
	RNA_def_struct_sdna_from(srna, "NParticleBufferAttribute", NULL); /* reset */


	/*** Subtypes for data access ***/

	/* FLOAT */
	srna = RNA_def_struct(brna, "NParticleDataFloat", NULL);
	RNA_def_struct_ui_text(srna, "Particle Float Data", "");

	prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_NParticleDataFloat_get", "rna_NParticleDataFloat_set", NULL);
	RNA_def_property_ui_text(prop, "Value", "");

	/* INT */
	srna = RNA_def_struct(brna, "NParticleDataInt", NULL);
	RNA_def_struct_ui_text(srna, "Particle Int Data", "");

	prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_NParticleDataInt_get", "rna_NParticleDataInt_set", NULL);
	RNA_def_property_ui_text(prop, "Value", "");

	/* BOOL */
	srna = RNA_def_struct(brna, "NParticleDataBool", NULL);
	RNA_def_struct_ui_text(srna, "Particle Bool Data", "");

	prop = RNA_def_property(srna, "value", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_NParticleDataBool_get", "rna_NParticleDataBool_set");
	RNA_def_property_ui_text(prop, "Value", "");

	/* VECTOR */
	srna = RNA_def_struct(brna, "NParticleDataVector", NULL);
	RNA_def_struct_ui_text(srna, "Particle Vector Data", "");

	prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_NParticleDataVector_get", "rna_NParticleDataVector_set", NULL);
	RNA_def_property_ui_text(prop, "Value", "");

	/* POINT */
	srna = RNA_def_struct(brna, "NParticleDataPoint", NULL);
	RNA_def_struct_ui_text(srna, "Particle Point Data", "");

	prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_NParticleDataVector_get", "rna_NParticleDataVector_set", NULL);
	RNA_def_property_ui_text(prop, "Value", "");

	/* NORMAL */
	srna = RNA_def_struct(brna, "NParticleDataNormal", NULL);
	RNA_def_struct_ui_text(srna, "Particle Normal Data", "");

	prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_DIRECTION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_NParticleDataVector_get", "rna_NParticleDataVector_set", NULL);
	RNA_def_property_ui_text(prop, "Value", "");

	/* COLOR */
	srna = RNA_def_struct(brna, "NParticleDataColor", NULL);
	RNA_def_struct_ui_text(srna, "Particle Color Data", "");

	prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 4);
	RNA_def_property_float_funcs(prop, "rna_NParticleDataColor_get", "rna_NParticleDataColor_set", NULL);
	RNA_def_property_ui_text(prop, "Value", "");

	/* MATRIX */
	srna = RNA_def_struct(brna, "NParticleDataMatrix", NULL);
	RNA_def_struct_ui_text(srna, "Particle Matrix Data", "");

	prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_array(prop, 16);
	RNA_def_property_float_funcs(prop, "rna_NParticleDataMatrix_get", "rna_NParticleDataMatrix_set", NULL);
	RNA_def_property_ui_text(prop, "Value", "");

	def_nparticle_buffer_attribute_type(brna, "NParticleBufferAttributeFloat", "NParticleDataFloat");
	def_nparticle_buffer_attribute_type(brna, "NParticleBufferAttributeInt", "NParticleDataInt");
	def_nparticle_buffer_attribute_type(brna, "NParticleBufferAttributeBool", "NParticleDataBool");
	def_nparticle_buffer_attribute_type(brna, "NParticleBufferAttributeVector", "NParticleDataVector");
	def_nparticle_buffer_attribute_type(brna, "NParticleBufferAttributePoint", "NParticleDataPoint");
	def_nparticle_buffer_attribute_type(brna, "NParticleBufferAttributeNormal", "NParticleDataNormal");
	def_nparticle_buffer_attribute_type(brna, "NParticleBufferAttributeColor", "NParticleDataColor");
	def_nparticle_buffer_attribute_type(brna, "NParticleBufferAttributeMatrix", "NParticleDataMatrix");
}

static void rna_def_nparticle_buffer_attributes_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;

	RNA_def_property_srna(cprop, "NParticleBufferAttributes");
	srna = RNA_def_struct(brna, "NParticleBufferAttributes", NULL);
	RNA_def_struct_sdna(srna, "NParticleBuffer");
	RNA_def_struct_ui_text(srna, "Attributes", "Collection of particle attributes");

	func = RNA_def_function(srna, "new", "rna_NParticleBuffer_attributes_new");
	RNA_def_function_ui_description(func, "Add a particle attribute");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_string(func, "name", "", MAX_NAME, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_enum(func, "datatype", nparticle_attribute_datatype_user, PAR_ATTR_DATATYPE_FLOAT, "Data Type", "Base data type");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return value */
	parm = RNA_def_pointer(func, "attr", "NParticleBufferAttribute", "", "Attribute");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_NParticleBuffer_attributes_remove");
	RNA_def_function_ui_description(func, "Remove an attribute from the buffer");
	parm = RNA_def_pointer(func, "attr", "NParticleBufferAttribute", "", "The attribute to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "clear", "rna_NParticleBuffer_attributes_clear");
	RNA_def_function_ui_description(func, "Remove all attributes from the buffer");

	func = RNA_def_function(srna, "move", "rna_NParticleBuffer_attributes_move");
	RNA_def_function_ui_description(func, "Move an attribute to another position");
	parm = RNA_def_int(func, "from_index", -1, 0, INT_MAX, "From Index", "Index of the attribute to move", 0, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "to_index", -1, 0, INT_MAX, "To Index", "Target index for the attribute", 0, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

static void rna_def_nparticle_buffer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "NParticleBuffer", NULL);
	RNA_def_struct_ui_text(srna, "Particle Buffer", "Container for particles");

	prop = RNA_def_property(srna, "attributes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "attributes", NULL);
	RNA_def_property_ui_text(prop, "Attributes", "Data layers associated to particles");
	RNA_def_property_struct_type(prop, "NParticleBufferAttribute");
	rna_def_nparticle_buffer_attributes_api(brna, prop);
}

void RNA_def_nparticle(BlenderRNA *brna)
{
	rna_def_nparticle_buffer_attribute(brna);
	
	rna_def_nparticle_buffer(brna);
}

#endif
