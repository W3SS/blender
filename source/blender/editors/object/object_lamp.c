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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */


/** \file blender/editors/object/object_lamp.c
 *  \ingroup edobj
 */

#include "WM_types.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_matrix.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "WM_types.h"
#include "WM_api.h"

#include "BIF_glutil.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "object_intern.h"

#include "RNA_define.h"
#include "RNA_access.h"

typedef struct LampPositionData {
	int pos[2];
	float quat[4];
	float lvec[3];
} LampPositionData;

/* Modal Operator init */
static int lamp_position_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *ob = CTX_data_active_object(C);
	LampPositionData *data;
	data = op->customdata = MEM_mallocN(sizeof (LampPositionData), "lamp_position_data");

	copy_v2_v2_int(data->pos, event->mval);

	mat4_to_quat(data->quat, ob->obmat);
	copy_v3_v3(data->lvec, ob->obmat[2]);
	negate_v3(data->lvec);
	normalize_v3(data->lvec);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* Repeat operator */
static int lamp_position_modal(bContext *C, wmOperator *op, const wmEvent *event)
{

	LampPositionData *data = op->customdata;

	switch (event->type) {
		case MOUSEMOVE:
		{
			Object *ob = CTX_data_active_object(C);
			Lamp *la = ob->data;
			Scene *scene = CTX_data_scene(C);
			ARegion *ar = CTX_wm_region(C);
			View3D *v3d = CTX_wm_view3d(C);
			float world_pos[3];
			int flag = v3d->flag2;

			v3d->flag2 |= V3D_RENDER_OVERRIDE;

			view3d_operator_needs_opengl(C);
			if (ED_view3d_autodist(scene, ar, v3d, event->mval, world_pos, true, NULL)) {
				float axis[3];

				/* restore the floag here */
				v3d->flag2 = flag;

				sub_v3_v3(world_pos, ob->obmat[3]);
				la->dist = normalize_v3(world_pos);

				cross_v3_v3v3(axis, data->lvec, world_pos);
				if (normalize_v3(axis) > 0.0001) {
					float mat[4][4];
					float quat[4], qfinal[4];
					float angle = saacos(dot_v3v3(world_pos, data->lvec));

					/* transform the initial rotation quaternion to the new position and set the matrix to the lamp */
					axis_angle_to_quat(quat, axis, angle);
					mul_qt_qtqt(qfinal, quat, data->quat);
					quat_to_mat4(mat, qfinal);
					copy_v3_v3(mat[3], ob->obmat[3]);

					BKE_object_apply_mat4(ob, mat, true, false);
				}

				DAG_id_tag_update(&ob->id, OB_RECALC_OB);

				ED_region_tag_redraw(ar);
			}

			v3d->flag2 = flag;

			break;
		}

		case LEFTMOUSE:
			if (event->val == KM_RELEASE) {
				MEM_freeN(op->customdata);
				return OPERATOR_FINISHED;
			}

		case EVT_WIDGET_UPDATE:
		{
			ARegion *ar = CTX_wm_region(C);
			Object *ob = CTX_data_active_object(C);
			Lamp *la = ob->data;
			float value[3], len;

			RNA_float_get_array(op->ptr, "value", value);

			sub_v3_v3(value, ob->obmat[3]);

			len = len_v3(value);

			la->spotsize = len * 0.1f;
			DAG_id_tag_update(&ob->id, OB_RECALC_OB);

			ED_region_tag_redraw(ar);
			break;
		}

		case EVT_WIDGET_RELEASED:
		{
			MEM_freeN(op->customdata);
			return OPERATOR_FINISHED;
		}
	}

	return OPERATOR_RUNNING_MODAL;
}

static int lamp_position_poll(bContext *C)
{
	return CTX_wm_region_view3d(C) != NULL;
}


void LAMP_OT_lamp_position(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Lamp Position";
	ot->idname = "UI_OT_lamp_position";
	ot->description = "Sample a color from the Blender Window to store in a property";

	/* api callbacks */
	ot->invoke = lamp_position_invoke;
	ot->modal = lamp_position_modal;
	ot->poll = lamp_position_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector_xyz(ot->srna, "value", 3, NULL, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);
}

bool WIDGETGROUP_lamp_poll(struct wmWidgetGroup *UNUSED(wgroup), const struct bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if (ob && ob->type == OB_LAMP) {
		Lamp *la = ob->data;
		return (la->type == LA_SPOT);
	}
	return false;
}

void WIDGETGROUP_lamp_update(struct wmWidgetGroup *wgroup, const struct bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	Lamp *la = ob->data;
	wmWidget *widget = WM_widgetgroup_widgets(wgroup)->first;
	WidgetGroupLamp *data = WM_widgetgroup_customdata(wgroup);
	float dir[3];

	RNA_pointer_create(&la->id, &RNA_Lamp, la, data->lamp);
	WM_widget_set_origin(widget, ob->obmat[3]);
	WM_widget_property(widget, data->lamp, "spot_size");
	negate_v3_v3(dir, ob->obmat[2]);
	WIDGET_arrow_set_direction(widget, dir);
}


void WIDGETGROUP_lamp_free(struct wmWidgetGroup *wgroup)
{
	WidgetGroupLamp *data = WM_widgetgroup_customdata(wgroup);
	MEM_freeN(data->lamp);
	MEM_freeN(data);
}

void WIDGETGROUP_lamp_create(struct wmWidgetGroup *wgroup)
{
	float color_lamp[4] = {0.5f, 0.5f, 1.0f, 1.0f};
	wmWidget *widget = NULL;
	WidgetGroupLamp *lampgroup = MEM_callocN(sizeof(WidgetGroupLamp), "lamp_manipulator_data");

	lampgroup->lamp = MEM_callocN(sizeof(PointerRNA), "lampwidgetptr");

	widget = WIDGET_arrow_new(UI_ARROW_STYLE_INVERTED, NULL);
	WM_widget_register(wgroup, widget);
	WIDGET_arrow_set_color(widget, color_lamp);

	WM_widgetgroup_customdata_set(wgroup, lampgroup);
}
