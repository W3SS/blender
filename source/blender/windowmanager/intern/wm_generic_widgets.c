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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_generic_widgets.c
 *  \ingroup edinterface
 */

#include "RNA_types.h"
#include "RNA_access.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_matrix.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "WM_types.h"
#include "WM_api.h"

#include "GL/glew.h"
#include "GPU_select.h"

#include "BIF_glutil.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"

#include "3d_widgets/ui_widget_library.h"

#include "wm.h"
#include "WM_types.h"


/******************************************************
 *            GENERIC WIDGET LIBRARY                  *
 ******************************************************/

typedef struct WidgetDrawInfo {
	int nverts;
	int ntris;
	float (*verts)[3];
	float (*normals)[3];
	unsigned short *indices;
	bool init;
} WidgetDrawInfo;


WidgetDrawInfo arraw_head_draw_info = {0};
WidgetDrawInfo dial_draw_info = {0};

static void widget_draw_intern(WidgetDrawInfo *info, bool select)
{
	GLuint buf[3];

	bool use_lighting = !select && ((U.tw_flag & V3D_SHADED_WIDGETS) != 0);

	if (use_lighting)
		glGenBuffers(3, buf);
	else
		glGenBuffers(2, buf);

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * info->nverts, info->verts, GL_STATIC_DRAW);
	glVertexPointer(3, GL_FLOAT, 0, NULL);

	if (use_lighting) {
		glEnableClientState(GL_NORMAL_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, buf[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * info->nverts, info->normals, GL_STATIC_DRAW);
		glNormalPointer(GL_FLOAT, 0, NULL);
		glShadeModel(GL_SMOOTH);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * (3 * info->ntris), info->indices, GL_STATIC_DRAW);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glDrawElements(GL_TRIANGLES, info->ntris * 3, GL_UNSIGNED_SHORT, NULL);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glDisableClientState(GL_VERTEX_ARRAY);

	if (use_lighting) {
		glDisableClientState(GL_NORMAL_ARRAY);
		glShadeModel(GL_FLAT);
		glDeleteBuffers(3, buf);
	}
	else {
		glDeleteBuffers(2, buf);
	}
}

/********* Arrow widget ************/

#define ARROW_UP_VECTOR_SET 1

typedef struct ArrowWidget {
	wmWidget widget;
	int style;
	int flag;
	float direction[3];
	float up[3];
	float color[4];
	float offset;
	/* property range and minimum for constrained arrows */
	float range, min;
} ArrowWidget;

typedef struct ArrowInteraction {
	float orig_origin[3];
	float orig_mouse[2];
	float orig_offset;
	float orig_scale;

	/* direction vector, projected in screen space */
	float proj_direction[2];
} ArrowInteraction;


static void widget_arrow_get_final_pos(struct wmWidget *widget, float pos[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;

	mul_v3_v3fl(pos, arrow->direction, arrow->offset);
	add_v3_v3(pos, arrow->widget.origin);
}

static void arrow_draw_geom(ArrowWidget *arrow, bool select)
{
	if (arrow->style & UI_ARROW_STYLE_CROSS) {
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_LIGHTING);
		glBegin(GL_LINES);
		glVertex2f(-1.0, 0.f);
		glVertex2f(1.0, 0.f);
		glVertex2f(0.f, -1.0);
		glVertex2f(0.f, 1.0);
		glEnd();

		glPopAttrib();
	}
	else {
		widget_draw_intern(&arraw_head_draw_info, select);
	}
}

static void arrow_draw_intern(ArrowWidget *arrow, bool select, bool highlight)
{
	float rot[3][3];
	float mat[4][4];
	float up[3] = {0.0f, 0.0f, 1.0f};
	float final_pos[3];

	widget_arrow_get_final_pos(&arrow->widget, final_pos);

	if (arrow->flag & ARROW_UP_VECTOR_SET) {
		copy_v3_v3(rot[2], arrow->direction);
		copy_v3_v3(rot[1], arrow->up);
		cross_v3_v3v3(rot[0], arrow->up, arrow->direction);
	}
	else {
		rotation_between_vecs_to_mat3(rot, up, arrow->direction);
	}
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], final_pos);
	mul_mat3_m4_fl(mat, arrow->widget.scale);

	glPushMatrix();
	glMultMatrixf(&mat[0][0]);

	if (highlight)
		glColor4f(1.0, 1.0, 0.0, 1.0);
	else
		glColor4fv(arrow->color);

	arrow_draw_geom(arrow, select);

	glPopMatrix();

	if (arrow->widget.interaction_data) {
		ArrowInteraction *data = arrow->widget.interaction_data;

		copy_m4_m3(mat, rot);
		copy_v3_v3(mat[3], data->orig_origin);
		mul_mat3_m4_fl(mat, data->orig_scale);

		glPushMatrix();
		glMultMatrixf(&mat[0][0]);

		glEnable(GL_BLEND);
		glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
		arrow_draw_geom(arrow, select);

		glDisable(GL_BLEND);

		glPopMatrix();
	}
}

static void widget_arrow_render_3d_intersect(const struct bContext *UNUSED(C), struct wmWidget *widget, int selectionbase)
{
	GPU_select_load_id(selectionbase);
	arrow_draw_intern((ArrowWidget *)widget, true, false);
}

static void widget_arrow_draw(struct wmWidget *widget, const struct bContext *UNUSED(C))
{
	arrow_draw_intern((ArrowWidget *)widget, false, (widget->flag & WM_WIDGET_HIGHLIGHT) != 0);
}

#define ARROW_RANGE 1.5f

static int widget_arrow_handler(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget, struct wmOperator *op)
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	ArrowInteraction *data = widget->interaction_data;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	float orig_origin[4];
	float viewvec[3], tangent[3], plane[3];
	float offset[4];
	float m_diff[2];
	float dir_2d[2], dir2d_final[2];
	float fac, zfac;
	float facdir = 1.0f;
	bool use_vertical = false;

	copy_v3_v3(orig_origin, data->orig_origin);
	orig_origin[3] = 1.0f;
	add_v3_v3v3(offset, orig_origin, arrow->direction);
	offset[3] = 1.0f;

	/* calculate view vector */
	if (rv3d->is_persp) {
		sub_v3_v3v3(viewvec, orig_origin, rv3d->viewinv[3]);
	}
	else {
		copy_v3_v3(viewvec, rv3d->viewinv[2]);
	}
	normalize_v3(viewvec);

	zfac = ED_view3d_calc_zfac(rv3d, orig_origin, NULL);

	/* first determine if view vector is really close to the direction. If it is, we use vertical movement to determine offset,
	 * just like transform system does */
	if (RAD2DEG(acos(dot_v3v3(viewvec, arrow->direction))) > 5.0f) {
		/* multiply to projection space */
		mul_m4_v4(rv3d->persmat, orig_origin);
		mul_v4_fl(orig_origin, 1.0f/orig_origin[3]);
		mul_m4_v4(rv3d->persmat, offset);
		mul_v4_fl(offset, 1.0f/offset[3]);

		sub_v2_v2v2(dir_2d, offset, orig_origin);
		dir_2d[0] *= ar->winx;
		dir_2d[1] *= ar->winy;
		normalize_v2(dir_2d);
	}
	else {
		dir_2d[0] = 0.0f;
		dir_2d[1] = 1.0f;
		use_vertical = true;
	}

	/* find mouse difference */
	m_diff[0] = event->mval[0] - data->orig_mouse[0];
	m_diff[1] = event->mval[1] - data->orig_mouse[1];

	/* project the displacement on the screen space arrow direction */
	project_v2_v2v2(dir2d_final, m_diff, dir_2d);

	ED_view3d_win_to_delta(ar, dir2d_final, offset, zfac);

	add_v3_v3v3(orig_origin, offset, data->orig_origin);

	/* calculate view vector for the new position */
	if (rv3d->is_persp) {
		sub_v3_v3v3(viewvec, orig_origin, rv3d->viewinv[3]);
	}
	else {
		copy_v3_v3(viewvec, rv3d->viewinv[2]);
	}

	normalize_v3(viewvec);
	if (!use_vertical) {
		/* now find a plane parallel to the view vector so we can intersect with the arrow direction */
		cross_v3_v3v3(tangent, viewvec, offset);
		cross_v3_v3v3(plane, tangent, viewvec);
		fac = dot_v3v3(plane, offset) / dot_v3v3(arrow->direction, plane);

		facdir = (fac < 0.0) ? -1.0 : 1.0;
		mul_v3_v3fl(offset, arrow->direction, fac);
	}
	else {
		facdir = (m_diff[1] < 0.0) ? -1.0 : 1.0;
	}

	/* set the property for the operator and call its modal function */
	if (op && widget->propname) {

	}
	else if (widget->prop) {
		float value;

		value = data->orig_offset + facdir * len_v3(offset);
		if (arrow->style & UI_ARROW_STYLE_CONSTRAINED) {
			if (arrow->style & UI_ARROW_STYLE_INVERTED)
				value = arrow->min + arrow->range - (value * arrow->range / ARROW_RANGE);
			else
				value = arrow->min + (value * arrow->range / ARROW_RANGE);
		}

		RNA_property_float_set(widget->ptr, widget->prop, value);
		RNA_property_update(C, widget->ptr, widget->prop);

		/* accounts for clamping properly */
		if (arrow->style & UI_ARROW_STYLE_CONSTRAINED) {
			if (arrow->style & UI_ARROW_STYLE_INVERTED)
				arrow->offset = ARROW_RANGE * (arrow->min + arrow->range - (RNA_property_float_get(widget->ptr, widget->prop))) / arrow->range;
			else
				arrow->offset = ARROW_RANGE * ((RNA_property_float_get(widget->ptr, widget->prop) - arrow->min) / arrow->range);
		}
		else
			arrow->offset = RNA_property_float_get(widget->ptr, widget->prop);
	}
	else {
		arrow->offset = facdir * len_v3(offset);
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(ar);

	return OPERATOR_PASS_THROUGH;
}


static int widget_arrow_activate(struct bContext *UNUSED(C), const struct wmEvent *event, struct wmWidget *widget, int state)
{
	if (state == WIDGET_ACTIVATE) {
		ArrowWidget *arrow = (ArrowWidget *) widget;
		ArrowInteraction *data = MEM_callocN(sizeof (ArrowInteraction), "arrow_interaction");

		if (widget->prop) {
			data->orig_offset = arrow->offset;
		}

		data->orig_mouse[0] = event->mval[0];
		data->orig_mouse[1] = event->mval[1];

		data->orig_scale = widget->scale;

		widget_arrow_get_final_pos(widget, data->orig_origin);

		widget->interaction_data = data;
	}
	else if (state == WIDGET_DEACTIVATE) {
		MEM_freeN(widget->interaction_data);
		widget->interaction_data = NULL;
	}
	return OPERATOR_FINISHED;
}

static void widget_arrow_bind_to_prop(struct wmWidget *widget)
{
	ArrowWidget *arrow = (ArrowWidget *) widget;

	if (widget->prop) {
		if (arrow->style & UI_ARROW_STYLE_CONSTRAINED) {
			float min, max, step, precision;

			RNA_property_float_ui_range(widget->ptr, widget->prop, &min, &max, &step, &precision);
			arrow->range = max - min;
			arrow->min = min;
			if (arrow->style & UI_ARROW_STYLE_INVERTED)
				arrow->offset = ARROW_RANGE * (max - (RNA_property_float_get(widget->ptr, widget->prop))) / arrow->range;
			else
				arrow->offset = ARROW_RANGE * ((RNA_property_float_get(widget->ptr, widget->prop) - arrow->min) / arrow->range);
		}
		else {
			/* we'd need to check the property type here but for now assume always float */
			arrow->offset = RNA_property_float_get(widget->ptr, widget->prop);
		}
	}
	else
		arrow->offset = 0.0f;
}

wmWidget *WIDGET_arrow_new(int style, void *customdata)
{
	float dir_default[3] = {0.0f, 0.0f, 1.0f};
	ArrowWidget *arrow;

	if (!arraw_head_draw_info.init) {
		arraw_head_draw_info.nverts = _WIDGET_nverts_arrow,
		arraw_head_draw_info.ntris = _WIDGET_ntris_arrow,
		arraw_head_draw_info.verts = _WIDGET_verts_arrow,
		arraw_head_draw_info.normals = _WIDGET_normals_arrow,
		arraw_head_draw_info.indices = _WIDGET_indices_arrow,
		arraw_head_draw_info.init = true;
	}

	/* inverted only makes sense in a constrained arrow */
	if (style & UI_ARROW_STYLE_INVERTED)
		style |= UI_ARROW_STYLE_CONSTRAINED;
	
	arrow = MEM_callocN(sizeof(ArrowWidget), "arrowwidget");
	

	arrow->widget.draw = widget_arrow_draw;
	arrow->widget.get_final_position = 	widget_arrow_get_final_pos;
	arrow->widget.intersect = NULL;
	arrow->widget.handler = widget_arrow_handler;
	arrow->widget.activate_state = widget_arrow_activate;
	arrow->widget.render_3d_intersection = widget_arrow_render_3d_intersect;
	arrow->widget.customdata = customdata;
	arrow->widget.bind_to_prop = widget_arrow_bind_to_prop;
	arrow->widget.user_scale = 1.0f;
	arrow->widget.flag |= WM_WIDGET_SCALE_3D;
	arrow->style = style;
	copy_v3_v3(arrow->direction, dir_default);
	
	return (wmWidget *)arrow;
}

void WIDGET_arrow_set_color(struct wmWidget *widget, float color[4])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	
	copy_v4_v4(arrow->color, color);
}

void WIDGET_arrow_set_direction(struct wmWidget *widget, float direction[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	
	copy_v3_v3(arrow->direction, direction);
	normalize_v3(arrow->direction);
}

void WIDGET_arrow_set_up_vector(struct wmWidget *widget, float direction[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;

	if (direction) {
		copy_v3_v3(arrow->up, direction);
		normalize_v3(arrow->up);
		arrow->flag |= ARROW_UP_VECTOR_SET;
	}
	else {
		arrow->flag &= ~ARROW_UP_VECTOR_SET;
	}
}


/********* Dial widget ************/

typedef struct DialWidget {
	wmWidget widget;
	int style;
	float direction[3];
	float color[4];
} DialWidget;

static void dial_draw_intern(DialWidget *dial, bool select, bool highlight, float scale)
{
	float rot[3][3];
	float mat[4][4];
	float up[3] = {0.0f, 0.0f, 1.0f};

	rotation_between_vecs_to_mat3(rot, up, dial->direction);
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], dial->widget.origin);
	mul_mat3_m4_fl(mat, scale);

	glPushMatrix();
	glMultMatrixf(&mat[0][0]);

	if (highlight)
		glColor4f(1.0, 1.0, 0.0, 1.0);
	else
		glColor4fv(dial->color);

	widget_draw_intern(&dial_draw_info, select);

	glPopMatrix();

}

static void widget_dial_render_3d_intersect(const struct bContext *C, struct wmWidget *widget, int selectionbase)
{
	DialWidget *dial = (DialWidget *)widget;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	/* enable clipping if needed */
	if (dial->style == UI_DIAL_STYLE_RING_CLIPPED) {
		double plane[4];
		copy_v3db_v3fl(plane, rv3d->viewinv[2]);
		plane[3] = -dot_v3v3(rv3d->viewinv[2], widget->origin);
		glClipPlane(GL_CLIP_PLANE0, plane);
		glEnable(GL_CLIP_PLANE0);
	}

	GPU_select_load_id(selectionbase);
	dial_draw_intern(dial, true, false, dial->widget.scale);

	if (dial->style == UI_DIAL_STYLE_RING_CLIPPED) {
		glDisable(GL_CLIP_PLANE0);
	}
}

static void widget_dial_draw(struct wmWidget *widget, const struct bContext *C)
{
	DialWidget *dial = (DialWidget *)widget;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	/* enable clipping if needed */
	if (dial->style == UI_DIAL_STYLE_RING_CLIPPED) {
		double plane[4];
		copy_v3db_v3fl(plane, rv3d->viewinv[2]);
		plane[3] = -dot_v3v3(rv3d->viewinv[2], widget->origin);
		glClipPlane(GL_CLIP_PLANE0, plane);
		glEnable(GL_CLIP_PLANE0);
	}

	dial_draw_intern(dial, false, (widget->flag & WM_WIDGET_HIGHLIGHT) != 0, widget->scale);

	if (dial->style == UI_DIAL_STYLE_RING_CLIPPED) {
		glDisable(GL_CLIP_PLANE0);
	}
}

wmWidget *WIDGET_dial_new(int style, void *customdata)
{
	float dir_default[3] = {0.0f, 0.0f, 1.0f};
	DialWidget *dial;

	if (!dial_draw_info.init) {
		dial_draw_info.nverts = _WIDGET_nverts_dial,
		dial_draw_info.ntris = _WIDGET_ntris_dial,
		dial_draw_info.verts = _WIDGET_verts_dial,
		dial_draw_info.normals = _WIDGET_normals_dial,
		dial_draw_info.indices = _WIDGET_indices_dial,
		dial_draw_info.init = true;
	}

	dial = MEM_callocN(sizeof(ArrowWidget), "arrowwidget");

	dial->widget.draw = widget_dial_draw;
	dial->widget.intersect = NULL;
	dial->widget.render_3d_intersection = widget_dial_render_3d_intersect;
	dial->widget.customdata = customdata;

	dial->style = style;
	copy_v3_v3(dial->direction, dir_default);

	return (wmWidget *)dial;
}

void WIDGET_dial_set_color(struct wmWidget *widget, float color[4])
{
	DialWidget *arrow = (DialWidget *)widget;

	copy_v4_v4(arrow->color, color);
}

void WIDGET_dial_set_direction(struct wmWidget *widget, float direction[3])
{
	DialWidget *dial = (DialWidget *)widget;

	copy_v3_v3(dial->direction, direction);
	normalize_v3(dial->direction);
}

/********* Cage widget ************/


typedef struct CageWidget {
	wmWidget widget;
	float rotation;
	rctf bound;
	int style;
} CageWidget;

static void cage_draw_corners(rctf *r, float offsetx, float offsety)
{
	glBegin(GL_LINES);
	glVertex2f(r->xmin, r->ymin + offsety);
	glVertex2f(r->xmin, r->ymin);
	glVertex2f(r->xmin, r->ymin);
	glVertex2f(r->xmin + offsetx, r->ymin);

	glVertex2f(r->xmax, r->ymin + offsety);
	glVertex2f(r->xmax, r->ymin);
	glVertex2f(r->xmax, r->ymin);
	glVertex2f(r->xmax - offsetx, r->ymin);

	glVertex2f(r->xmax, r->ymax - offsety);
	glVertex2f(r->xmax, r->ymax);
	glVertex2f(r->xmax, r->ymax);
	glVertex2f(r->xmax - offsetx, r->ymax);

	glVertex2f(r->xmin, r->ymax - offsety);
	glVertex2f(r->xmin, r->ymax);
	glVertex2f(r->xmin, r->ymax);
	glVertex2f(r->xmin + offsetx, r->ymax);
	glEnd();	
}

static void widget_cage_draw(struct wmWidget *widget, const struct bContext *UNUSED(C))
{
	CageWidget *cage = (CageWidget *)widget;
	float w = BLI_rctf_size_x(&cage->bound);
	float h = BLI_rctf_size_y(&cage->bound);
	
	glColor4f(1.0f, 0.6f, 0.0f, 0.1f);

//	glEnable(GL_BLEND);
//	glRectf(cage->bound.xmin, cage->bound.ymin, cage->bound.xmax, cage->bound.ymax);
//	glDisable(GL_BLEND);
	
	/* corner widgets */
	glColor3f(0.0, 0.0, 0.0);
	glLineWidth(3.0);
	cage_draw_corners(&cage->bound, w/8.0f, h/8.0f);

	/* corner widgets */
	glColor3f(1.0, 1.0, 1.0);
	glLineWidth(1.0);
	cage_draw_corners(&cage->bound, w/8.0f, h/8.0f);
}

static int widget_cage_intersect(struct bContext *UNUSED(C), const struct wmEvent *event, struct wmWidget *widget)
{
	CageWidget *cage = (CageWidget *)widget;
	float mouse[2] = {event->mval[0], event->mval[1]};
	float pointrot[2];
	float matrot[2][2];

	/* rotate mouse in relation to the center and relocate it */
	sub_v2_v2v2(pointrot, mouse, widget->origin);

	rotate_m2(matrot, -cage->rotation);

	add_v2_v2(pointrot, widget->origin);

	return BLI_rctf_isect_pt_v(&cage->bound, pointrot);
}

struct wmWidget *WIDGET_cage_new(int style, void *customdata)
{
	CageWidget *cage = MEM_callocN(sizeof(CageWidget), "CageWidget");

	cage->widget.customdata = customdata;
	cage->widget.draw = widget_cage_draw;
	cage->widget.intersect = widget_cage_intersect;
	cage->widget.user_scale = 1.0f;
	cage->style = style;

	return (wmWidget *)cage;
}

void WIDGET_cage_bind_to_rotation(struct wmWidget *widget, float rotation)
{
	CageWidget *cage = (CageWidget *)widget;
	
	cage->rotation = rotation;
}

void WIDGET_cage_bounds_set(struct wmWidget *widget, float w, float h)
{
	CageWidget *cage = (CageWidget *)widget;
	cage->bound.xmax = w/2;
	cage->bound.ymax = h/2;
	cage->bound.xmin = -w/2;
	cage->bound.ymin = -h/2;
}


void fix_linking_widget_lib(void)
{
	(void) 0;
}
