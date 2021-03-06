/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2017-2020 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Linking this software statically or dynamically with other modules is making 
 *  a combined work based on this software. Thus, the terms and conditions of 
 *  the GNU General Public License cover the whole combination. 
 *  
 *  As a special exception, the copyright holders of Permafrost Engine give 
 *  you permission to link Permafrost Engine with independent modules to produce 
 *  an executable, regardless of the license terms of these independent 
 *  modules, and to copy and distribute the resulting executable under 
 *  terms of your choice, provided that you also meet, for each linked 
 *  independent module, the terms and conditions of the license of that 
 *  module. An independent module is a module which is not derived from 
 *  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
 *  extend this exception to your version of Permafrost Engine, but you are not 
 *  obliged to do so. If you do not wish to do so, delete this exception 
 *  statement from your version.
 *
 */

#ifndef ENTITY_H
#define ENTITY_H

#include "pf_math.h"
#include "collision.h"
#include "lib/public/vec.h"
#include "map/public/tile.h"

#include <stdbool.h>

#define MAX_JOINTS                (96)

#define ENTITY_FLAG_ANIMATED      (1 << 0)
#define ENTITY_FLAG_COLLISION     (1 << 1)
#define ENTITY_FLAG_SELECTABLE    (1 << 2)
#define ENTITY_FLAG_STATIC        (1 << 3)
#define ENTITY_FLAG_COMBATABLE    (1 << 4)
#define ENTITY_FLAG_INVISIBLE     (1 << 5)
/* zombie entities are those that have died in the game simulation, 
 * but are still retained by some scripting variable */
#define ENTITY_FLAG_ZOMBIE        (1 << 6)
#define ENTITY_FLAG_MARKER        (1 << 7)

struct entity{
    uint32_t     uid;
    char         name[32];
    char         basedir[64];
    char         filename[32];
    vec3_t       scale;
    quat_t       rotation;
    uint32_t     flags;
    void        *render_private;
    void        *anim_private;
    void        *anim_ctx;
    /* For animated entities, this is the bind pose AABB. Each
     * animation sample also has its' own AABB. */
    struct aabb  identity_aabb;
    float        selection_radius; /* The radius of the selection circle in OpenGL coordinates */
    float        max_speed;        /* The base movement speed in units of OpenGL coords / second */
    int          faction_id;       /* The faction to which this entity belongs to. */
    int          max_hp;           /* 0 for 'invulnerable' entities */
    float        vision_range;     /* in OpenGL coordinates */
};

/* State needed for rendering a static entity */
struct ent_stat_rstate{
    void            *render_private;
    mat4x4_t         model;
    struct tile_desc td; 
};

/* State needed for rendering an animated entity */
struct ent_anim_rstate{
    void           *render_private;
    mat4x4_t        model;
    size_t          njoints;
    const mat4x4_t *inv_bind_pose; /* static, use shallow copy */
    mat4x4_t        curr_pose[MAX_JOINTS];
};

struct ent_vis_state{
    vec2_t xz_pos;
    float  vis_range;
};

VEC_TYPE(rstat, struct ent_stat_rstate)
VEC_IMPL(static inline, rstat, struct ent_stat_rstate)

VEC_TYPE(ranim, struct ent_anim_rstate)
VEC_IMPL(static inline, ranim, struct ent_anim_rstate)


void     Entity_ModelMatrix(const struct entity *ent, mat4x4_t *out);
uint32_t Entity_NewUID(void);
void     Entity_SetNextUID(uint32_t uid);
void     Entity_CurrentOBB(const struct entity *ent, struct obb *out);
vec3_t   Entity_TopCenterPointWS(const struct entity *ent);

#endif
