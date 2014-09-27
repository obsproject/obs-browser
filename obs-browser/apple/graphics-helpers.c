/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "graphics-helpers.h"

gs_vertbuffer_t *create_vertex_buffer()
{
	struct gs_vb_data *vb_data = gs_vbdata_create();

	vb_data->num = 4;
	vb_data->points = (struct vec3 *)bzalloc(sizeof(struct vec3) * 4);
	if (!vb_data->points) {
		gs_vbdata_destroy(vb_data);
		return NULL;
	}

	vb_data->num_tex = 1;
	vb_data->tvarray = (struct gs_tvertarray *)
	bzalloc(sizeof(struct gs_tvertarray));

	if (!vb_data->tvarray) {
		gs_vbdata_destroy(vb_data);
		return NULL;
	}

	vb_data->tvarray[0].width = 2;
	vb_data->tvarray[0].array = bzalloc(sizeof(struct vec2) * 4);

	if (!vb_data->tvarray[0].array) {
		gs_vbdata_destroy(vb_data);
		return NULL;
	}

	return gs_vertexbuffer_create(vb_data, GS_DYNAMIC);
}

void build_sprite(struct gs_vb_data *data, float fcx, float fcy,
		float start_u, float end_u, float start_v, float end_v)
{
	struct vec2 *tvarray = (struct vec2 *)data->tvarray[0].array;

	vec3_set(data->points+1,  fcx, 0.0f, 0.0f);
	vec3_set(data->points+2, 0.0f,  fcy, 0.0f);
	vec3_set(data->points+3,  fcx,  fcy, 0.0f);
	vec2_set(tvarray,   start_u, start_v);
	vec2_set(tvarray+1, end_u,   start_v);
	vec2_set(tvarray+2, start_u, end_v);
	vec2_set(tvarray+3, end_u,   end_v);
}

void build_sprite_rect(struct gs_vb_data *data, float origin_x, float origin_y,
		float end_x, float end_y)
{
	build_sprite(data, fabs(end_x - origin_x), fabs(end_y - origin_y),
			origin_x, end_x, origin_y, end_y);
}