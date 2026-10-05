/*
RENDER.C

	Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
	and the "Aleph One" developers.
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html

Thursday, September 8, 1994 1:58:20 PM  (Jason')

Friday, September 9, 1994 1:36:15 PM  (Jason')
	on the quads, in the sun.
Sunday, September 11, 1994 7:32:49 PM  (Jason')
	the clock on the 540 was wrong, yesterday was Saturday (not Friday).  on quads again, but
	back home now.  something will draw before i go to bed tonight.  dinner at the nile?
Tuesday, September 13, 1994 2:54:56 AM  (Jason')
	no fair!—— it’s still monday, really.  with the aid of some graphical debugging the clipping
	all works now and i’m trying to have the entire floor/ceiling thing going tonight (the nile
	was closed by the time i got around to taking a shower and heading out).
Friday, September 16, 1994 4:06:17 AM  (Jason')
	walls, floors and ceilings texture, wobble, etc.  contemplating objects ... maybe this will
	work after all.
Monday, September 19, 1994 11:03:49 AM  (Jason')
	unified xz_clip_vertical_polygon() and z_clip_horizontal_polygon() to get rid of the last
	known whitespace problem.  can’t wait to see what others i have.  objects now respect the
	clipping windows of all nodes they cross.
Monday, October 24, 1994 4:35:38 PM (Jason)
	fixed render sorting problem with objects of equal depth (i.e., parasitic objects). 
Tuesday, October 25, 1994 5:14:27 PM  (Jason')
	fixed object sort order with respect to nodes and height.
Wednesday, October 26, 1994 3:18:59 PM (Jason)
	fixed half of the object sort order back so that it worked: in order to correctly handle
	objects below the viewer projecting into higher polygons we need to sort objects inside
	nodes (to be draw after their walls and ceilings but before their floors).
Wednesday, November 2, 1994 3:49:57 PM (Jason)
	the bottom panel of short split sides sometimes takes on the ceiling lightsource.
Tuesday, November 8, 1994 5:29:12 PM  (Jason')
	implemented new transfer modes: _slide, _wander.  _render_effect_earthquake doesn’t work
	yet because the player can shake behind his own shape.
Thursday, December 15, 1994 12:15:55 AM  (Jason)
	the object depth sort order problem ocurrs with multiple objects in the same polygon,
	is some function of relative depths and overlap, and does not seem to involve objects at
	the same depth.  also, it seems to sort many further objects in front of a single closer
	one.
Monday, January 23, 1995 6:53:26 AM  (Jason')
	the way to fix the render object sorting problem is to ignore overlap and sort everything
	by depth, regardless.  imagine: two, far, non-overlapping objects; by the old algorithm their
	drawing order is irrelevant.  when a closer object which overlaps both of them is sorted, it
	only attempts to lie in front of the closest of the two (leaving the farthest one in an
	uncertain position).  unfortunately this will cause us to do unnecessary promotion and might
	look stupid.
Sunday, March 26, 1995 12:57:39 AM  (Jason')
	media modifications for marathon2; the object sort order problem still exists (the above
	solution indeed looked stupid).
Thursday, March 30, 1995 11:23:35 PM  (Jason')
	tried to fix object sort problem by attempting to assure that objects are always drawn in
	depth-order within a node.
Monday, June 5, 1995 8:37:42 AM  (Jason)
	blood and fire (baby).

Jan 30, 2000 (Loren Petrich):
	Added some typecasts
	Increased MAXIMUM_NODE_ALIASES to 32
	Added an "assert" for when DEBUG is off in aliases-building section in sort_render_tree().

Feb 1, 2000 (Loren Petrich):
	Added growable list of node aliases; replaced static-list node-alias code
	
Feb 4, 2000 (Loren Petrich):
	Changed halt() to assert(false) for better debugging

Feb 5, 2000 (Loren Petrich):
	Added growable lists of nodes and also clips for endpoints and lines.

Feb 6, 2000 (Loren Petrich):
	Doing initial allocations of the growable lists of various quantities as a defensive measure
	against memory leaks that seem to occur.

Feb 9, 2000 (Loren Petrich):
	Suppressed ambiguous-clip-flag debugging statement;
	it gets activated only for excessively distant objects

Feb 10, 2000 (Loren Petrich):
	Added dynamic-limits setting of MAXIMUM_RENDER_OBJECTS

Feb 14, 2000 (Loren Petrich):
	Added test for other-side polygon to LINE_IS_TRANSPARENT() check in next_polygon_along_line()

Feb 16, 2000 (Loren Petrich):
	Put in handling of overflow digits for the purpose of doing long distance correctly;
	also turning several horizontal-coordinate short integers into long ones.

Feb 17, 2000 (Loren Petrich):
	Made the sprites long-distance-friendly; there is a bug where they flip around when they
	go past half the world size, but that's a short-integer wraparound, and the relevant routine
	is in map.c.

Feb 18, 2000 (Loren Petrich):
	Added support for conditional display of weapons-in-hand; so as to support third-person
	as well as first-person view.

Feb 21, 2000 (Loren Petrich):
	Idiot-proofed next_polygon_along_line(), making it quit a loop if it searches a whole circle.

Feb 24, 2000 (Loren Petrich):
	Added animated-texture support

Mar 3, 2000 (Loren Petrich):
	Set view to normal in initialize_view_data();
	squashed persistent-extravision bug.

Mar 5, 2000 (Loren Petrich):
	Moved extravision-persistence bug fix out of this file.

Mar 9, 2000 (Loren Petrich):
	Sorted the nodes by polygon index in sort_render_tree() and used them to speed up searches
	for nodes with the same polygon index; maps with slowed-down visibility calculations,
	such as Desla, can become twice as fast.

Mar 12, 2000 (Loren Petrich):
	Added OpenGL support

Mar 14, 2000 (Loren Petrich):
	Modified data transmitted to OpenGL renderer; it's now collection/color/frame for
	both walls and sprites. Also added transmission of view data.

Mar 24, 2000 (Loren Petrich):
	Added landscape_yaw calculation; this is the yaw of the landscapes' left edges

Mar 30, 2000 (Loren Petrich):
	Inspired by Rhys Hill's work, I've created a second tree to contain the visibility nodes;
	in addition to their visibility tree, they have a polygon-sort tree.
	This tree is implemented by setting up some additional members of node_data
	for indicating its structure; there are members for polygon >, polygon <,
	and the next member of a chain that shares polygon-index values.
	As a result, the node-aliases list can now be abolished once and for all.

Jun 11, 2000 (Loren Petrich):
	Added support for see-through liquids; this requires several changes.
	The rendering of each map polygon had to be changed so that there would be a
	separate liquid surface; it would no longer replace the floor or the ceiling.
	Next, the inhabitant objects had to be done in two passes, one other side, and one view side.
	Also, whether there is void on the other side had to be indicated, so that
	waterfalls and the like may look right.

Jun 28, 2000 (Loren Petrich):
	Fixed Aaron Davies bug; if a polygon is completely below a liquid, it will not be rendered
	if the viewpoint is above the liquid; the bug was that it was not rendered if the viewpoint
	was below the liquid. This only happened if semitransparent liquid surfaces was turned off.

Jul 10, 2000 (Loren Petrich):
	Fixed liquid visibility bug in render_tree() that happens when liquid surfaces are not semitransparent;
	rendering is skipped if the viewpoint is under a liquid and the polygon is high and dry,
	or else if the viewpoint is above a liquid and the polygon is submerged.

Jul 17, 2000 (Loren Petrich):
	Suppressed view-effect resetting in initialize_view_data(),
	in order to make teleport view-stretching work correctly.

Aug 9, 2000 (Loren Petrich):
	Moved most of the rendering code here into separate files with these classes:
	
	RenderVisTreeClass --
		creates the visibility tree (which polygons can be seen from which other ones)
	RenderSortPolyClass --
		uses that tree to sort the polygons into appropriate depth order
	RenderPlaceObjsClass --
		finds which objects are visible and places them into appropriately sorted order
	RenderRasterize --
		handles the clipping of each object and requests those objects' rasterization
	
	Added a rasterizer class; currently, it does everything from the base class,
	though what it does will be moved into subclasses.

Sep 2, 2000 (Loren Petrich):
	Added some idiot-proofing, since the shapes accessor now returns NULL for nonexistent bitmaps

Nov 12, 2000 (Loren Petrich):
	Added automap reset before rendering

Nov 29, 2000 (Loren Petrich):
	Made teleport static/fold effect optional

Jan 17, 2001 (Loren Petrich):
	Added vertical flipping
*/


#ifdef QUICKDRAW_DEBUG
#include "macintosh_cseries.h"
#else
#include "cseries.h"
#endif

#include "map.h"
#include "render.h"
#include "interface.h"
#include "lightsource.h"
#include "media.h"
#include "weapons.h"
#include "player.h"

// LP additions
#include "dynamic_limits.h"
#include "AnimatedTextures.h"
#ifdef HAVE_OPENGL
#include "OGL_Render.h"
#include "OGL_Model_Def.h"
#include "OGL_Headers.h"   // gl* symbols for the cross-platform network VR pose debug lines
#endif

#ifdef QUICKDRAW_DEBUG
#include "shell.h"
extern WindowPtr screen_window;
#endif

#include <math.h>
#include <string.h>
#include <stdlib.h>


// LP additions for decomposition of this code:
#include "RenderVisTree.h"
#include "RenderSortPoly.h"
#include "RenderPlaceObjs.h"
#include "RenderRasterize.h"
#include "Rasterizer_SW.h"
#ifdef HAVE_OPENGL
#include "Rasterizer_OGL.h"
#include "RenderRasterize_Shader.h"
#include "Rasterizer_Shader.h"
#include "vr_openxr.h"
#include "vr_weapons.h"

// VRRELOAD diagnostic logging (2026-09-27 shotgun second-hand bug). Off: file logging costs frames.
static const bool kVRReloadDiag = false;
static const bool kVRJitterDiag = false;   // A1VRJIT per-frame weapon placement trace (logcat only)
#include <chrono>
#include "items.h"         // physical weapons: magazine item pictures (get_item_shape)
#include "monsters.h"      // physical weapons: the player's map object (dropped magazines become pickups)
#include "Logging.h"   // attach-point tuner MML dump -> on-device log
#include <vector>
#include <algorithm>
#include <string>
#include "vr_net.h"
#endif
#include "preferences.h"
#include "screen.h"

/* use native alignment */
#if defined (powerc) || defined (__powerc)
#pragma options align=power
#endif

/*
//render transparent walls (if a bit is set or if the transparent texture is non-NULL?)
//use side lightsources instead of taking them from their polygons
//respect dark side bit (darken light intensity by k)
//fix solid/opaque endpoint confusion (solidity does not imply opacity)

there exists a problem where an object can overlap into a polygon which is clipped by something
	behind the object but that will clip the object because clip windows are subtractive; how
	is this solved?
it’s still possible to get ambiguous clip flags, usually in very narrow (e.g., 1 pixel) windows
the renderer has a maximum range beyond which it shits bricks yet which it allows to be exceeded
it’s still possible, especially in high-res full-screen, for points to end up (slightly) off
	the screen (usually discarding these has no noticable effect on the scene)
whitespace results when two adjacent polygons are clipped to different vertical windows.  this
	is not trivially solved with the current implementation, and may be acceptable (?)

//build_base_polygon_index_list() should discard lower polygons for objects above the viewer and
//	higher polygons for objects below the viewer because we certainly don’t sort objects
//	correctly in these cases
//in strange cases, objects are sorted out of order.  this seems to involve players in some way
//	(i.e., parasitic objects).
*/

/* ---------- constants */

#define EXPLOSION_EFFECT_RANGE (WORLD_ONE/12)

/* ---------- clip buffer */
// Not used for anything
#define CLIP_INDEX_BUFFER_SIZE 4096

vector<uint16> RenderFlagList;

// uint16 *render_flags;

// LP additions: decomposition of the rendering code into various objects

static RenderVisTreeClass RenderVisTree;			// Visibility-tree object
static RenderSortPolyClass RenderSortPoly;			// Polygon-sorting object
static RenderPlaceObjsClass RenderPlaceObjs;		// Object-placement object
static RenderRasterizerClass Render_Classic;		// Clipping and rasterization class

static Rasterizer_SW_Class Rasterizer_SW;			// Software rasterizer
#ifdef HAVE_OPENGL
static Rasterizer_OGL_Class Rasterizer_OGL;			// OpenGL rasterizer
static Rasterizer_Shader_Class Rasterizer_Shader;   // Shader rasterizer
static RenderRasterize_Shader Render_Shader;       // Shader clipping and rasterization class
#endif

// In Marathon 1-style exploration missions, we check
// each player's view for exploration polygons after
// this many ticks have elapsed
static const int TICKS_PER_EXPLORE = 4;

// M1 exploration mission helpers
static struct view_data explore_view;
static RenderVisTreeClass explore_tree;

void OGL_Rasterizer_Init() {
	
#ifdef HAVE_OPENGL
	if (graphics_preferences->screen_mode.acceleration == _opengl_acceleration) {
		Rasterizer_Shader.setupGL();
		Render_Shader.setupGL(Rasterizer_Shader);
	}
#endif
}

/* ---------- private prototypes */

static void update_view_data(struct view_data *view);
static void update_render_effect(struct view_data *view);
static void shake_view_origin(struct view_data *view, world_distance delta);

static void render_viewer_sprite_layer(view_data *view, RasterizerClass *RasPtr);
void position_sprite_axis(short *x0, short *x1, short scale_width, short screen_width,
	short positioning_mode, _fixed position, bool flip, world_distance world_left, world_distance world_right);


#ifdef QUICKDRAW_DEBUG
static void debug_flagged_points(flagged_world_point2d *points, short count);
static void debug_flagged_points3d(flagged_world_point3d *points, short count);
static void debug_vector(world_vector2d *v);
static void debug_x_line(world_distance x);
#endif

/* ---------- code */

void allocate_render_memory(
	void)
{
	assert(NUMBER_OF_RENDER_FLAGS<=16);
	RenderFlagList.resize(RENDER_FLAGS_BUFFER_SIZE);
	
	// LP addition: check out pointer-arithmetic hack
	assert(sizeof(void *) == sizeof(POINTER_DATA));
	
	// LP change: do max allocation
	RenderVisTree.Resize(MAXIMUM_ENDPOINTS_PER_MAP,MAXIMUM_LINES_PER_MAP);
	RenderSortPoly.Resize(MAXIMUM_POLYGONS_PER_MAP);
	
	// Reset to have the tree correctly resized if m1 exploration level
	explore_tree.view = nullptr;
	// LP change: set up pointers
	RenderSortPoly.RVPtr = &RenderVisTree;
	RenderPlaceObjs.RVPtr = &RenderVisTree;
	RenderPlaceObjs.RSPtr = &RenderSortPoly;
#ifdef HAVE_OPENGL	
	Render_Classic.RSPtr = Render_Shader.RSPtr = &RenderSortPoly;
#else
	Render_Classic.RSPtr = &RenderSortPoly;	
#endif	
}

/* just in case anyone was wondering, standard_screen_width will usually be the same as
	screen_width.  the renderer assumes that the given field_of_view matches the standard
	width provided (so if the actual width provided is larger, you'll be able to see more;
	if it's smaller you'll be able to see less).  this allows the destination bitmap to not
	only grow and shrink while maintaining a constant aspect ratio, but to also change in
	geometry without effecting the image being projected onto it.  if you don't understand
	this, pass standard_width==width */
void initialize_view_data(
	struct view_data *view,
	bool ignore_preferences)
{
	double two_pi= 8.0*atan(1.0);
	double half_cone= view->field_of_view*(two_pi/360.0)/2;
 	/* half_cone needs to be extended for non oblique perspective projection (gluPerspective).
	 this is required because the viewing angle is different for about the same field of view */
	if (!ignore_preferences && graphics_preferences->screen_mode.acceleration == _opengl_acceleration)
		half_cone= (view->field_of_view * 1.3)*(two_pi/360.0)/2;

	double adjusted_half_cone= (ignore_preferences || View_FOV_FixHorizontalNotVertical()) ?
		half_cone :
		atan(view->screen_width*tan(half_cone)/view->standard_screen_width);
	double world_to_screen;
	
	view->half_screen_width= view->screen_width/2;
	view->half_screen_height= view->screen_height/2;
	
	/* if there’s a round-off error in half_cone, we want to make the cone too big (so when we clip
		lines ‘to the edge of the screen’ they’re actually off the screen, thus +1.0) */
	view->half_cone= (angle) (adjusted_half_cone*((double)NUMBER_OF_ANGLES)/two_pi+1.0);
	
	// LP change: find the adjusted yaw for the landscapes;
	// this is the effective yaw value for the left edge.
	// A landscape rotation can also be added if desired.
	view->landscape_yaw = view->yaw - view->half_cone;

	/* calculate world_to_screen; we could calculate this with standard_screen_width/2 and
		the old half_cone and get the same result */
	world_to_screen= view->half_screen_width/tan(adjusted_half_cone);
	view->world_to_screen_x= view->real_world_to_screen_x= (short) ((world_to_screen/view->horizontal_scale)+0.5);
	view->world_to_screen_y= view->real_world_to_screen_y= (short) ((world_to_screen/view->vertical_scale)+0.5);
	
	/* calculate the vertical cone angle; again, overflow instead of underflow when rounding */
	view->half_vertical_cone= (angle) (NUMBER_OF_ANGLES*atan(((double)view->half_screen_height*view->vertical_scale)/world_to_screen)/two_pi+1.0);

	/* view needs to know if OpenGL renderer should mimic software's pitch */
	if (!ignore_preferences && graphics_preferences->screen_mode.acceleration == _opengl_acceleration)
	{
		view->mimic_sw_perspective = TEST_FLAG(Get_OGL_ConfigureData().Flags, OGL_Flag_MimicSW);
		view->billboard_xy = Get_OGL_ConfigureData().BillboardXY;
	}

	/* reset any active effects */
	// LP: this is now called in render_screen(), so we need to disable the initializing
}

#if defined(__ANDROID__)
#include <android/log.h>
// ---- 3D aim reticle (red dot projected onto world geometry) ---------------------------------
// Raycasts from the dominant controller along its aim direction and draws a camera-facing red
// filled disc at the hit point. Drawn per-eye while the world projection/modelview from SetView
// are active, so vertices are in Marathon world units.

// Raycast from `origin` (inside `poly`) along the unit direction (dx,dy,dz) up to maxDist; stop at the
// first solid wall / floor / ceiling. Geometry only (ignores monsters). Returns the hit point.
// MARCHED in short steps: find_line_crossed_leaving_polygon()'s cross-products are int32 and OVERFLOW
// for far endpoints (giving wrong/inverted lines). It's built for short per-tick projectile moves, so
// we feed it short segments. Wall/floor/ceiling intersections are still exact (find_*_intersection).
static world_point3d vr_raycast_geometry(world_point3d origin, short poly, double dx, double dy, double dz, double maxDist)
{
	if (poly == NONE) return origin;
	world_point3d cur = origin;
	short curPoly = poly;
	const double STEP = WORLD_ONE / 2;   // 512 WU: short enough that the cross-products stay << 2^31
	const double maxDist2 = maxDist * maxDist;
	for (int guard = 0; guard < 1000; ++guard) {
		polygon_data* pd = get_polygon_data(curPoly);
		world_point3d nxt;
		nxt.x = (world_distance)(cur.x + dx*STEP);
		nxt.y = (world_distance)(cur.y + dy*STEP);
		nxt.z = (world_distance)(cur.z + dz*STEP);
		short li = find_line_crossed_leaving_polygon(curPoly, (world_point2d*)&cur, (world_point2d*)&nxt);
		if (li == NONE) {
			world_point3d out;
			if (nxt.z <= pd->floor_height)   { find_floor_or_ceiling_intersection(pd->floor_height,   &cur, &nxt, &out); return out; }
			if (nxt.z >= pd->ceiling_height) { find_floor_or_ceiling_intersection(pd->ceiling_height, &cur, &nxt, &out); return out; }
			cur = nxt;
		} else {
			line_data* line = get_line_data(li);
			world_point3d ix;
			find_line_intersection(&get_endpoint_data(line->endpoint_indexes[0])->vertex,
								   &get_endpoint_data(line->endpoint_indexes[1])->vertex, &cur, &nxt, &ix);
			short adj = find_adjacent_polygon(curPoly, li);
			bool can_pass = (!LINE_IS_SOLID(line) || LINE_HAS_TRANSPARENT_SIDE(line)) && adj != NONE;
			if (!can_pass) return ix;                                   // solid wall
			polygon_data* ad = get_polygon_data(adj);
			world_point3d out;
			if (ix.z <= pd->floor_height)   { find_floor_or_ceiling_intersection(pd->floor_height,   &cur, &nxt, &out); return out; }
			if (ix.z >= pd->ceiling_height) { find_floor_or_ceiling_intersection(pd->ceiling_height, &cur, &nxt, &out); return out; }
			if (ix.z <= ad->floor_height || ix.z >= ad->ceiling_height) return ix;   // step wall
			if (LINE_HAS_TRANSPARENT_SIDE(line)) return ix;             // treat transparent-solid as a hit
			// pass through the portal: nudge a few WU past the boundary into the adjacent polygon so the
			// next find_line_crossed_leaving_polygon doesn't start on the shared edge (degenerate).
			curPoly = adj;
			cur.x = (world_distance)(ix.x + dx*4.0);
			cur.y = (world_distance)(ix.y + dy*4.0);
			cur.z = (world_distance)(ix.z + dz*4.0);
		}
		const double ex = cur.x-origin.x, ey = cur.y-origin.y, ez = cur.z-origin.z;
		if (ex*ex + ey*ey + ez*ez >= maxDist2) return cur;
	}
	return cur;
}

// Two-handed weapon-model basis. Forward = th_fwd (the inter-hand aim); ROLL comes from the DOMINANT
// hand (the gun rolls with your dominant wrist, it is NOT forced world-vertical). Method: take the
// dominant controller's own orientation and rotate it by the SHORTEST ARC that swings its forward onto
// th_fwd, carrying its right/up along. This preserves the wrist roll about the barrel and has no gimbal
// / sign-flip (unlike the old cross(dom_right,th_fwd), which flipped when th_fwd neared the dominant
// right axis). Only the exact 180 deg reversal is degenerate -- guarded with a world-up upright basis.
// Inputs unit; outputs unit right/up (forward stays th_fwd).
static void vr_two_handed_model_basis(const float th_fwd[3], const float dom_right[3],
                                      const float dom_up[3], float out_right[3], float out_up[3])
{
	auto dot3   = [](const float a[3], const float b[3]) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; };
	auto cross3 = [](const float a[3], const float b[3], float o[3]) {
		o[0]=a[1]*b[2]-a[2]*b[1]; o[1]=a[2]*b[0]-a[0]*b[2]; o[2]=a[0]*b[1]-a[1]*b[0];
	};
	// Dominant controller forward = aim -Z = -(right x up).
	float fd[3]; cross3(dom_right, dom_up, fd); fd[0]=-fd[0]; fd[1]=-fd[1]; fd[2]=-fd[2];

	const float c = dot3(fd, th_fwd);          // cos(angle) between dominant forward and inter-hand fwd
	if (c > 0.99999f) {                        // already aligned -> dominant axes unchanged
		out_right[0]=dom_right[0]; out_right[1]=dom_right[1]; out_right[2]=dom_right[2];
		out_up[0]=dom_up[0]; out_up[1]=dom_up[1]; out_up[2]=dom_up[2];
		return;
	}
	float axis[3]; cross3(fd, th_fwd, axis);
	const float al = sqrtf(dot3(axis, axis));  // = |sin(angle)|
	if (al < 1e-6f) {                          // ~180 deg reversal -> world-up upright fallback
		float right[3] = { -th_fwd[2], 0.0f, th_fwd[0] };
		float rl = sqrtf(dot3(right, right)); if (rl < 1e-5f) rl = 1.0f;
		out_right[0]=right[0]/rl; out_right[1]=right[1]/rl; out_right[2]=right[2]/rl;
		cross3(out_right, th_fwd, out_up);
		return;
	}
	axis[0]/=al; axis[1]/=al; axis[2]/=al;
	const float s = al;                        // sin(angle) (fd, th_fwd both unit)
	// Rodrigues: rotate v about `axis` by the angle with cos=c, sin=s.
	auto rot = [&](const float v[3], float o[3]) {
		float axv[3]; cross3(axis, v, axv);
		const float d = dot3(axis, v) * (1.0f - c);
		o[0]=v[0]*c + axv[0]*s + axis[0]*d;
		o[1]=v[1]*c + axv[1]*s + axis[1]*d;
		o[2]=v[2]*c + axv[2]*s + axis[2]*d;
	};
	rot(dom_right, out_right);
	rot(dom_up,    out_up);
}

static void render_vr_aim_reticle(view_data* view)
{
	if (!VR_IsActive()) return;
	if (!VR_Settings()->showLaserSight) return;

	short weap_type = NONE, weap_mode = 0;
	get_player_weapon_mode_and_type(current_player_index, &weap_type, &weap_mode);
	if (weap_type == _weapon_fist) return;
	const bool weapon_is_dual = (weap_type == _weapon_doublefisted_pistols ||
	                              weap_type == _weapon_doublefisted_shotguns);

	const double W = VR_Settings()->worldScaleWUM;
	const double yaw = view->virtual_yaw * (360.0 / (double(FIXED_ONE) * double(FULL_CIRCLE))) * (8.0 * atan(1.0) / 360.0);
	const double cy = cos(yaw), sy = sin(yaw);
	const double camx = view->origin.x;
	const double camy = view->origin.y;
	const double camz = view->origin.z;
	const int domHand = VR_Settings()->dominantHand ? 0 : 1;
	const int offHand = 1 - domHand;

	float hp[3];
	if (!VR_GetHeadPosStage(hp)) return;

	glUseProgram(0);
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(1.0f, 0.0f, 0.0f, 0.9f);

	const float dotR = (float)(0.05 / 3.0 * W);
	const int NSEG = 18;
	GLfloat dv[(NSEG+2)*3];

	auto draw_dot = [&](int hand) {
		float ps[3], fs[3];
		if (!VR_GetAimPoseStage(hand, ps, fs)) return;

		if (hand == domHand && VR_IsTwoHandedActive()) {
			float th_fwd[3];
			if (VR_GetTwoHandedFwdStage(th_fwd)) { fs[0]=th_fwd[0]; fs[1]=th_fwd[1]; fs[2]=th_fwd[2]; }
		}

		const double qx = (ps[0]-hp[0])*W, qsyr = (ps[1]-hp[1])*W, qz = (ps[2]-hp[2])*W;
		const double rx = -qx, ry = -qz, rz = qsyr;
		world_point3d cw;
		cw.x = (world_distance)(camx + rx*cy - ry*sy);
		cw.y = (world_distance)(camy + rx*sy + ry*cy);
		cw.z = (world_distance)(camz + rz);

		double dx = -fs[0], dy = -fs[2], dz = fs[1];
		double wx = dx*cy - dy*sy, wy = dx*sy + dy*cy, wz = dz;
		double dl = sqrt(wx*wx + wy*wy + wz*wz); if (dl < 1e-6) dl = 1;
		wx/=dl; wy/=dl; wz/=dl;

		short cpoly = find_new_object_polygon((world_point2d*)&view->origin, (world_point2d*)&cw, view->origin_polygon_index);
		if (cpoly == NONE) cpoly = view->origin_polygon_index;
		world_point3d hit = vr_raycast_geometry(cw, cpoly, wx, wy, wz, 64.0 * WORLD_ONE);

		double cdir[3] = { camx-hit.x, camy-hit.y, camz-hit.z };
		double cdl = sqrt(cdir[0]*cdir[0]+cdir[1]*cdir[1]+cdir[2]*cdir[2]); if (cdl < 1.0) cdl = 1.0;
		cdir[0]/=cdl; cdir[1]/=cdl; cdir[2]/=cdl;
		double right[3] = { -cdir[1], cdir[0], 0.0 };
		double rl = sqrt(right[0]*right[0]+right[1]*right[1]); if (rl < 1e-6) { right[0]=1.0; right[1]=0.0; rl=1.0; }
		right[0]/=rl; right[1]/=rl;
		double up[3] = { cdir[1]*right[2]-cdir[2]*right[1],
		                 cdir[2]*right[0]-cdir[0]*right[2],
		                 cdir[0]*right[1]-cdir[1]*right[0] };

		const double hx = hit.x + cdir[0]*1.5;
		const double hy = hit.y + cdir[1]*1.5;
		const double hz = hit.z + cdir[2]*1.5;

		int dn = 0;
		auto push = [&](double x, double y, double z){ dv[dn++]=(float)x; dv[dn++]=(float)y; dv[dn++]=(float)z; };
		push(hx, hy, hz);
		for (int i = 0; i <= NSEG; ++i) {
			float a = (float)(i * 6.28318530718f / NSEG);
			float cs = cosf(a), sn = sinf(a);
			push(hx + dotR*(cs*right[0]+sn*up[0]),
			     hy + dotR*(cs*right[1]+sn*up[1]),
			     hz + dotR*(cs*right[2]+sn*up[2]));
		}
		glVertexPointer(3, GL_FLOAT, 0, dv);
		glDrawArrays(GL_TRIANGLE_FAN, 0, NSEG+2);
	};

	draw_dot(domHand);
	if (weapon_is_dual) draw_dot(offHand);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

// ---- Controller aim diagnostic gizmos -------------------------------------------------------
// Draws, in world space (depth test off so always visible):
//   * a cross marker at each physical hand (dominant = red, off-hand = blue)
//   * each controller's OWN aim ray (thin, hand colour) — the single-handed aim direction
//   * when two-handed hold is active: the inter-hand line (green) and the ACTUAL fired aim
//     direction (thick yellow, from the dominant hand along VR_GetTwoHandedFwdStage)
// Purpose: separate an orientation problem from a hit/geometry problem and see exactly where the
// two-handed model points vs where the player thinks the gun is aimed. Toggle: VR "Aim Debug Gizmos".
// ---- VR physical weapons: live weapon pose capture ---------------------------------------------
// render_vr_weapon_sprites_3d works out where each hand's weapon model actually sits (anchor +
// orthonormal basis, Marathon world units). The attach-point gizmos -- and later the grab/seat
// proximity tests -- need model-local points to land exactly on the rendered mesh, so rather than
// re-deriving that transform (and drifting out of sync with it), the render pass stashes it here.
// See docs/VR_PHYSICAL_WEAPONS.md.
struct vr_weapon_pose
{
	bool  valid;
	short weapon_type;
	float anchor[3];                  // world position of the model origin / grip
	float right[3], up[3], fwd[3];    // orthonormal basis, world space
};
static vr_weapon_pose s_vrWeaponPose[2];   // indexed by hand (0 = right, 1 = left)

// ---- VR physical weapons: holster <-> hand swing ------------------------------------------------------
// Drawing a weapon, it flies from its holster into the hand over kWeaponSwingMs; letting go, it flies back.
// Both ends remember where they last drew each weapon (per frame), and whichever end a weapon newly appears
// at blends in from the other end's last pose. Render only -- the live pose used by the magwells and aim
// is the real hand.
static const float kWeaponSwingMs = 110.f;
static uint32_t    s_vrFrameNo = 0;            // advanced once per frame (eye 0)
struct vr_pose_snap { short wt; uint32_t frame; float p[3], r[3], u[3], f[3]; };
static vr_pose_snap s_holsterSnap[VR_HOLSTER_COUNT];   // where each slot last drew its weapon
static vr_pose_snap s_handSnap[2][MAXIMUM_NUMBER_OF_WEAPONS];   // where each hand last drew each weapon
struct vr_swing { bool on; uint64_t startMs; vr_pose_snap from; };
static int s_holsterLastHand[VR_HOLSTER_COUNT] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
static vr_swing s_handSwing[2], s_holsterSwing[VR_HOLSTER_COUNT];

static void vr_snap_set(vr_pose_snap& s, short wt, const float p[3], const float r[3], const float u[3], const float f[3])
{
	s.wt = wt; s.frame = s_vrFrameNo;
	for (int k = 0; k < 3; ++k) { s.p[k] = p[k]; s.r[k] = r[k]; s.u[k] = u[k]; s.f[k] = f[k]; }
}

// A snapshot that drew `wt` in the last few frames but NOT this one -- the end the weapon just left.
static bool vr_snap_just_left(const vr_pose_snap& s, short wt)
{
	return s.wt == wt && s.frame != s_vrFrameNo && s_vrFrameNo - s.frame <= 3;
}

// Blend a pose in place from the swing's start toward (p, r, u, f). Returns false once the swing is over.
static bool vr_apply_swing(vr_swing& sw, float p[3], float r[3], float u[3], float f[3])
{
	if (!sw.on) return false;
	const float t = float(machine_tick_count() - sw.startMs) / kWeaponSwingMs;
	if (t >= 1.f) { sw.on = false; return false; }
	const float s = t * t * (3.f - 2.f * t);
	auto blend = [&](const float a[3], float b[3], bool unit) {
		for (int k = 0; k < 3; ++k) b[k] = a[k] + (b[k] - a[k]) * s;
		if (!unit) return;
		const float n = sqrtf(b[0]*b[0] + b[1]*b[1] + b[2]*b[2]);
		if (n > 1e-5f) for (int k = 0; k < 3; ++k) b[k] /= n;
	};
	blend(sw.from.p, p, false);
	blend(sw.from.r, r, true);
	blend(sw.from.u, u, true);
	blend(sw.from.f, f, true);
	return true;
}

static void vr_swing_start(vr_swing& sw, const vr_pose_snap& from)
{
	sw.on = true; sw.startMs = machine_tick_count(); sw.from = from;
}

// Map a MODEL-LOCAL point in metres (+X right, +Y up, +Z forward down the barrel) to world space.
static void vr_weapon_local_to_world(const vr_weapon_pose& pose, float W,
                                     float lx, float ly, float lz, float out[3])
{
	for (int i = 0; i < 3; ++i)
		out[i] = pose.anchor[i] + (lx * pose.right[i] + ly * pose.up[i] + lz * pose.fwd[i]) * W;
}

static void render_vr_aim_gizmos(view_data* view)
{
	if (!VR_IsActive()) return;
	if (!VR_Settings()->showAimGizmos) return;

	const double W = VR_Settings()->worldScaleWUM;
	const double yaw = view->virtual_yaw * (360.0 / (double(FIXED_ONE) * double(FULL_CIRCLE))) * (8.0 * atan(1.0) / 360.0);
	const double cy = cos(yaw), sy = sin(yaw);
	const double camx = view->origin.x, camy = view->origin.y, camz = view->origin.z;
	const int domHand = VR_Settings()->dominantHand ? 0 : 1;
	const int offHand = 1 - domHand;

	float hp[3];
	if (!VR_GetHeadPosStage(hp)) return;

	// stage (metres, Y-up) position relative to head -> Marathon world (WU, Z-up), anchored at camera.
	auto stagePosToWorld = [&](const float ps[3], world_point3d& out) {
		const double qx = (ps[0]-hp[0])*W, qy = (ps[1]-hp[1])*W, qz = (ps[2]-hp[2])*W;
		const double rx = -qx, ry = -qz, rz = qy;
		out.x = (world_distance)(camx + rx*cy - ry*sy);
		out.y = (world_distance)(camy + rx*sy + ry*cy);
		out.z = (world_distance)(camz + rz);
	};
	// stage forward -> world unit direction (rotation only; length cancels).
	auto stageDirToWorld = [&](const float fs[3], double w[3]) {
		double dx = -fs[0], dy = -fs[2], dz = fs[1];
		double wx = dx*cy - dy*sy, wy = dx*sy + dy*cy, wz = dz;
		double l = sqrt(wx*wx + wy*wy + wz*wz); if (l < 1e-6) l = 1;
		w[0] = wx/l; w[1] = wy/l; w[2] = wz/l;
	};

	glUseProgram(0);
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(2.0f);

	world_point3d handW[2]; bool handOk[2] = { false, false };

	for (int h = 0; h < 2; ++h) {
		float ps[3], fs[3];
		if (!VR_GetAimPoseStage(h, ps, fs)) continue;   // ps = physical hand (grip) pos, fs = aim fwd
		world_point3d cw; stagePosToWorld(ps, cw);
		handW[h] = cw; handOk[h] = true;
		double w[3]; stageDirToWorld(fs, w);
		const double rayLen = 4.0 * WORLD_ONE;
		world_point3d e;
		e.x = (world_distance)(cw.x + w[0]*rayLen);
		e.y = (world_distance)(cw.y + w[1]*rayLen);
		e.z = (world_distance)(cw.z + w[2]*rayLen);
		const float hm = (float)(0.04 * W);
		GLfloat v[8*3]; int n = 0;
		auto push = [&](double x, double y, double z){ v[n++]=(float)x; v[n++]=(float)y; v[n++]=(float)z; };
		push(cw.x-hm,cw.y,cw.z); push(cw.x+hm,cw.y,cw.z);
		push(cw.x,cw.y-hm,cw.z); push(cw.x,cw.y+hm,cw.z);
		push(cw.x,cw.y,cw.z-hm); push(cw.x,cw.y,cw.z+hm);
		push(cw.x,cw.y,cw.z);    push(e.x,e.y,e.z);
		bool tracked = true; VR_GetHandTracking(h, &tracked, nullptr);
		if (!tracked)          glColor4f(1.0f, 0.0f, 1.0f, 1.0f);   // MAGENTA = occluded / dead-reckoned
		else if (h == domHand) glColor4f(1.0f, 0.2f, 0.2f, 0.9f);   // dominant = red
		else                   glColor4f(0.2f, 0.5f, 1.0f, 0.9f);   // off-hand = blue
		glVertexPointer(3, GL_FLOAT, 0, v);
		glDrawArrays(GL_LINES, 0, 8);
	}

	if (VR_IsTwoHandedActive() && handOk[0] && handOk[1]) {
		float thf[3];
		if (VR_GetTwoHandedFwdStage(thf)) {
			double w[3]; stageDirToWorld(thf, w);
			const world_point3d o = handW[domHand];
			world_point3d e;
			e.x = (world_distance)(o.x + w[0]*8.0*WORLD_ONE);
			e.y = (world_distance)(o.y + w[1]*8.0*WORLD_ONE);
			e.z = (world_distance)(o.z + w[2]*8.0*WORLD_ONE);
			GLfloat v[4*3]; int n = 0;
			auto push = [&](double x, double y, double z){ v[n++]=(float)x; v[n++]=(float)y; v[n++]=(float)z; };
			// inter-hand line (green): dominant -> off-hand
			push(handW[domHand].x,handW[domHand].y,handW[domHand].z);
			push(handW[offHand].x,handW[offHand].y,handW[offHand].z);
			glColor4f(0.1f, 1.0f, 0.2f, 0.9f);
			glLineWidth(2.0f);
			glVertexPointer(3, GL_FLOAT, 0, v);
			glDrawArrays(GL_LINES, 0, 2);
			// actual fired aim (thick yellow) from dominant hand along the two-handed direction
			n = 0; push(o.x,o.y,o.z); push(e.x,e.y,e.z);
			glColor4f(1.0f, 0.9f, 0.1f, 0.95f);
			glLineWidth(4.0f);
			glDrawArrays(GL_LINES, 0, 2);

			// throttled diagnostic: hand separation + fired direction + per-hand TRACKED state and
			// linear speed (grep A1VR "2hand"). If a hand shows trk=0 (dead-reckoned / occluded) while
			// the aim drifts, the cause is controller occlusion, not our math.
			{
				const double sep = sqrt(
					pow((handW[domHand].x-handW[offHand].x)/W,2) +
					pow((handW[domHand].y-handW[offHand].y)/W,2) +
					pow((handW[domHand].z-handW[offHand].z)/W,2));
				bool dTrk = true, oTrk = true; float dSpd = 0, oSpd = 0;
				VR_GetHandTracking(domHand, &dTrk, &dSpd);
				VR_GetHandTracking(offHand, &oTrk, &oSpd);
				// Also capture the raw dominant aim-pose stage forward + yawOffset + body pos, so a
				// "steady hands while walking" test shows whether wdir drifts because domFwd drifts
				// (pose), yawOffset drifts (locomotion), or neither (parallax/anchor lag, not rotation).
				float dps[3], dfs[3]; VR_GetAimPoseStage(domHand, dps, dfs);
				static int c = 0;
				if ((c++ % 30) == 0)
					__android_log_print(ANDROID_LOG_INFO, "A1VR",
						"2hand sep=%.3fm wdir=(%.3f,%.3f,%.3f) domFwdStage=(%.3f,%.3f,%.3f) yawOff=%.2f body=(%d,%d) trk=%d/%d",
						sep, w[0], w[1], w[2], dfs[0], dfs[1], dfs[2], VR_GetYawOffset(),
						(int)view->origin.x, (int)view->origin.y, (int)dTrk, (int)oTrk);
			}
		}
	}

	glLineWidth(1.0f);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

// ---- VR physical weapons: the body frame ----------------------------------------------------------
// Holsters hang off the BODY, which has to be derived from the head: the port tracks the head, not the
// torso. The trap here: view->virtual_yaw is the ROOM's rotation into the world (the snap/smooth-turn
// offset) -- head yaw rides in via the eye pose, not through it (vr_openxr.h). Hands are measured in
// room space, so they are right to use it. Body offsets are not: laid out along the room's axes they
// ignore where you are looking, and "forward" is wherever the play area's forward happens to point --
// which put the pistols backwards and the chest rifle on your back when that was behind you.
//
// So: a torso direction in ROOM space that follows the head's horizontal facing -- with a little lag
// and a hard 35 degree leash, so glancing sideways does not swing your hips around, but turning does
// -- then the same room->world mapping the hands use. Snap turns rotate room->world, so they carry the
// body along automatically.
static float s_torsoYaw      = 0.f;     // room-space azimuth of the torso's forward, radians
static bool  s_torsoYawValid = false;

// Once per frame (eye 0): ease the torso toward the head's facing.
static void vr_update_torso(void)
{
	float hf[3];
	if (!VR_GetHeadFwdStage(hf)) return;
	const float lh = sqrtf(hf[0]*hf[0] + hf[2]*hf[2]);
	if (lh < 0.2f) return;                       // looking straight up/down: keep the last facing
	const float headYaw = atan2f(hf[0], -hf[2]);   // 0 = room -Z (room forward), + = toward room +X
	if (!s_torsoYawValid) { s_torsoYaw = headYaw; s_torsoYawValid = true; return; }

	const float kPi = 3.14159265f;
	float d = headYaw - s_torsoYaw;
	while (d >  kPi) d -= 2.f * kPi;
	while (d < -kPi) d += 2.f * kPi;
	const float kLeash = 35.f * kPi / 180.f;     // never more than this far behind the head
	if (d >  kLeash) { s_torsoYaw += d - kLeash; d = kLeash; }
	if (d < -kLeash) { s_torsoYaw += d + kLeash; d = -kLeash; }
	s_torsoYaw += d * 0.06f;                     // and drift the rest of the way in over ~a quarter second
}

struct vr_body_frame
{
	float origin[3];                  // head position, world units
	float W;                          // world units per metre
	float right[3], up[3], fwd[3];    // body axes in ROOM (stage) space, unit length
	double cyw, syw;                  // room -> world rotation (same as the hands)
};

static bool vr_get_body_frame(const view_data* view, vr_body_frame& bf)
{
	bf.W = (float)VR_Settings()->worldScaleWUM;
	const double yaw = view->virtual_yaw * (360.0 / (double(FIXED_ONE) * double(FULL_CIRCLE))) * (8.0 * atan(1.0) / 360.0);
	bf.cyw = cos(yaw); bf.syw = sin(yaw);

	bf.origin[0] = (float)view->origin.x; bf.origin[1] = (float)view->origin.y; bf.origin[2] = (float)view->origin.z;
	float rcx, rcy, rcz;
	if (VR_GetRenderCamera(&rcx, &rcy, &rcz)) { bf.origin[0] = rcx; bf.origin[1] = rcy; bf.origin[2] = rcz + VR_GetEyeZOffset(); }

	if (!s_torsoYawValid) vr_update_torso();
	const float t = s_torsoYawValid ? s_torsoYaw : 0.f;
	// room space: +X right, +Y up, -Z forward
	bf.fwd[0] = sinf(t);  bf.fwd[1] = 0.f; bf.fwd[2] = -cosf(t);
	bf.up[0]  = 0.f;      bf.up[1]  = 1.f; bf.up[2]  = 0.f;
	bf.right[0] = -bf.fwd[2]; bf.right[1] = 0.f; bf.right[2] = bf.fwd[0];   // cross(fwd, up)
	return true;
}

// A room-space DIRECTION to world (rotation only).
static void vr_room_dir_to_world(const vr_body_frame& bf, const float s[3], float out[3])
{
	const double zx = -s[0], zy = -s[2], zz = s[1];      // same axis swap the hands use
	out[0] = (float)(zx * bf.cyw - zy * bf.syw);
	out[1] = (float)(zx * bf.syw + zy * bf.cyw);
	out[2] = (float)zz;
}

// A body-space offset in metres (+X right, +Y up, +Z forward) from the head, to a world position.
static void vr_body_to_world(const vr_body_frame& bf, float bx, float by, float bz, float out[3])
{
	float s[3], w[3];
	for (int k = 0; k < 3; ++k) s[k] = bx * bf.right[k] + by * bf.up[k] + bz * bf.fwd[k];
	vr_room_dir_to_world(bf, s, w);
	for (int k = 0; k < 3; ++k) out[k] = bf.origin[k] + w[k] * bf.W;
}

// How a weapon hangs in a slot, as a WORLD-space model basis. Built in room space and mapped through,
// because the room->world axis swap is a reflection: cross products taken after mapping would flip.
//   sides (hips/thighs/chest): barrel down, top of the slide facing forward -- a drawn gun pitched down
//   front centre: slung flat across the chest, muzzle down toward the off-hand side, side facing you
//   backs: barrel up over the shoulder, top facing back
static void vr_holster_model_basis(const vr_body_frame& bf, int slot, float wr[3], float wu[3], float wf[3])
{
	// The anchor's own hang (a set-up override, or the slot's default), already mirrored for left-handers,
	// as body-space directions. Expanded into room space, orthonormalised there, and only then mapped to
	// the world -- the room->world axis swap is a reflection, so a cross product taken after it would flip.
	vr_holster_anchor a;
	VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &a);
	float f[3], u[3], r[3];
	for (int k = 0; k < 3; ++k) {
		f[k] = a.fwd[0] * bf.right[k] + a.fwd[1] * bf.up[k] + a.fwd[2] * bf.fwd[k];
		u[k] = a.up[0]  * bf.right[k] + a.up[1]  * bf.up[k] + a.up[2]  * bf.fwd[k];
	}
	float lf = sqrtf(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
	if (lf < 1e-4f) { f[0] = -bf.up[0]; f[1] = -bf.up[1]; f[2] = -bf.up[2]; lf = 1.f; }
	for (int k = 0; k < 3; ++k) f[k] /= lf;
	const float d = u[0]*f[0] + u[1]*f[1] + u[2]*f[2];
	for (int k = 0; k < 3; ++k) u[k] -= d * f[k];
	float lu = sqrtf(u[0]*u[0] + u[1]*u[1] + u[2]*u[2]);
	if (lu < 1e-4f) { u[0] = bf.fwd[0]; u[1] = bf.fwd[1]; u[2] = bf.fwd[2]; lu = 1.f; }
	for (int k = 0; k < 3; ++k) u[k] /= lu;
	r[0] = f[1]*u[2] - f[2]*u[1]; r[1] = f[2]*u[0] - f[0]*u[2]; r[2] = f[0]*u[1] - f[1]*u[0];  // cross(fwd, up)
	vr_room_dir_to_world(bf, r, wr);
	vr_room_dir_to_world(bf, u, wu);
	vr_room_dir_to_world(bf, f, wf);
}

// ---- VR physical weapons: holstered weapons on the body ----------------------------------------
// Every weapon the player is carrying is drawn at its assigned body anchor, and gripping a hand
// near one draws it. This is the visible half of the feature; the engine still runs a single
// current_weapon underneath, so drawing from a holster is an ordinary weapon select. Per-hand
// weapons (Phase 1) later upgrade this from "select that weapon" to "put it in THAT hand".
//
// The grab request is produced here, where the controller poses live, but is CONSUMED in the input
// tick (vbl.cpp) and issued through encode_hotkey_sequence() -- the same 3-tick action-flag path a
// number key uses. Going through the action stream rather than poking ready_weapon() directly keeps
// weapon selection on one code path and leaves the door open for netplay later.
static bool  s_holsterGripPrev[2] = { false, false };

// Position of a body anchor in world space. Body space is metres from the head, +X right / +Y up /
// +Z forward, routed through the same stage->world mapping the hands use so anchors and hands agree.
static void vr_holster_world_pos(const view_data* view, int slot, float out[3])
{
	vr_body_frame bf;
	vr_get_body_frame(view, bf);
	vr_holster_anchor a;
	VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &a);
	vr_body_to_world(bf, a.x, a.y, a.z, out);
}

// True if the player is carrying this weapon (so its holster should show something).
static bool vr_player_has_weapon(short weapon_type)
{
	const short item = get_weapon_item_type(weapon_type);
	if (item == NONE) return false;
	const player_data* player = get_player_data(current_player_index);
	return player->items[item] > 0;
}

// Which weapon hangs on a slot, accounting for two-fisted pairs. A weapon the player is carrying
// TWO of (magnum, shotgun) also hangs on the mirrored anchor, so there is one on each hip and the
// hand you reach with is the hand that gets it. The mirror is only borrowed when it is genuinely
// free -- if another carried weapon is assigned there it keeps it, and the pair degrades to a
// single. Eight anchors cannot hold eight weapons plus mirrors; see docs/VR_PHYSICAL_WEAPONS.md.
static short vr_holster_occupant(int slot)
{
	const short direct = VR_GetHolsterSlotWeapon(slot);
	if (direct != NONE && vr_player_has_weapon(direct)) return direct;

	// Getting past the check above means this slot is genuinely free right now: either nothing is
	// assigned to it, or what is assigned is not being carried. Either way it can host the second
	// half of a two-fisted pair. (An earlier version bailed out whenever ANYTHING was assigned here,
	// which meant the second magnum never appeared -- the flamethrower nominally owns the other hip,
	// so the mirror was permanently blocked whether or not you had a flamethrower.)
	const int mirror = VR_GetMirrorHolsterSlot(slot);
	if (mirror == VR_HOLSTER_NONE) return NONE;
	const short paired = VR_GetHolsterSlotWeapon(mirror);
	if (paired == NONE || !weapon_is_twofisted(paired)) return NONE;
	const short item = get_weapon_item_type(paired);
	if (item == NONE) return NONE;
	const player_data* player = get_player_data(current_player_index);
	return (player->items[item] >= 2) ? paired : (short)NONE;   // only when actually carrying two
}

// Defined with the rest of the magazine code below; a grip that finds no weapon holster falls
// through to the ammo pouches, and that check lives here.
static bool vr_try_grab_magazine(const view_data* view, int hand, float hx, float hy, float hz);
static int  vr_nearest_dispensing_pouch(const view_data* view, int hand, float hx, float hy, float hz, float* d2Out);
static bool vr_primary_magwell(float W, int excludeHand, short* weapon, float well[3], float ins[3],
                               float* radius, float* angleTol, bool secondWell = false);
static void vr_loose_mag_pos(float W, const float well[3], const float ins[3], bool drops, float out[3]);
static bool vr_try_pull_loose_mag(float W, int hand, float hx, float hy, float hz);
static bool vr_hand_near_loose_mag(float W, int hand, const float pos[3]);
static bool vr_hand_world_pose(const vr_body_frame& bf, int hand,
                               float pos[3], float wr[3], float wu[3], float wf[3]);

// What the two hands are holding right now, in real weapon types (the dual-wield pseudo-types folded
// back to the weapon, with a count of two).
struct vr_hands_state
{
	int   heldHand;        // hand holding the primary weapon, -1 when empty-handed
	short primary;         // primary weapon type, NONE when empty-handed
	int   primaryCount;    // 2 while a two-fisted pair is up
	short offhand;         // the second hand's own weapon, NONE if none
};

static void vr_get_hands_state(vr_hands_state& hs)
{
	hs.heldHand = VR_GetHeldWeaponHand();
	short t = NONE, mode = 0;
	get_player_weapon_mode_and_type(current_player_index, &t, &mode);
	hs.primaryCount = 1;
	if (t == _weapon_doublefisted_pistols)  { t = _weapon_pistol;  hs.primaryCount = 2; }
	if (t == _weapon_doublefisted_shotguns) { t = _weapon_shotgun; hs.primaryCount = 2; }
	if (hs.heldHand < 0 || t == _weapon_fist) { t = NONE; hs.primaryCount = 0; }
	hs.primary = t;
	hs.offhand = vr_get_offhand_weapon(current_player_index);
}

// Which physical side of the body a slot is on for THIS player: 1 = right, 0 = left, -1 = centre line.
static int vr_holster_slot_side(int slot)
{
	if (VR_HolsterSlotIsCenter(slot)) return -1;
	vr_holster_anchor a;
	VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &a);
	return (a.x >= 0.f) ? 1 : 0;
}

// The weapon each slot is visibly holding right now: its occupant, minus whatever the hands have
// taken. A weapon in a hand empties the slot on THAT hand's side first, so drawing your left pistol
// empties your left hip and the right one stays put. Shared by the renderer and the grab test, so what
// you can grab is exactly what you can see.
static void vr_visible_holsters(short out[VR_HOLSTER_COUNT])
{
	vr_hands_state hs;
	vr_get_hands_state(hs);
	for (int slot = 0; slot < VR_HOLSTER_COUNT; ++slot)
		out[slot] = VR_HolsterSlotIsPouch(slot) ? (short)NONE : vr_holster_occupant(slot);

	auto takeFromBody = [&](short wt, int count, int hand) {
		for (int pass = 0; pass < 2 && count > 0; ++pass) {        // pass 0: that hand's side only
			for (int slot = 0; slot < VR_HOLSTER_COUNT && count > 0; ++slot) {
				if (out[slot] != wt) continue;
				if (pass == 0 && hand >= 0 && vr_holster_slot_side(slot) != hand) continue;
				out[slot] = NONE;
				--count;
			}
		}
	};
	if (hs.primary != NONE) takeFromBody(hs.primary, hs.primaryCount, hs.primaryCount >= 2 ? -1 : hs.heldHand);
	if (hs.offhand != NONE) takeFromBody(hs.offhand, 1, hs.heldHand >= 0 ? 1 - hs.heldHand : -1);
}

// ---- VR physical weapons: holster & pouch setup mode ---------------------------------------------
// Entered from the in-game menu (the exit button, shell.cpp). Every body anchor -- holsters and both
// ammo pouches -- shows as a box in its own orientation with its weapon hanging in it. Grip near one and it
// follows your hand like a picked-up object: position AND rotation, so turning your wrist turns the
// holster. Let go to leave it there. Weapon and magazine grabs are off while setting up. What you set is
// the player's OVERRIDE: saved in the preferences on leaving setup, and written to the log as MML --
// converted back to right-handed reference values -- so it can become the built-in defaults.
static bool  s_vrMagMustClear[2] = { false, false };   // a mag just pulled from a well must leave it first
static int   s_setupDragSlot[2]  = { -1, -1 };
static float s_setupGrabPos[2][3];    // the anchor's position in the grabbing hand's local frame, metres
static float s_setupGrabFwd[2][3];    // its barrel direction in that frame
static float s_setupGrabUp[2][3];     // and the direction the top of the gun faces
static bool  s_setupGripPrev[2]  = { false, false };
static bool  s_setupWasOn        = false;

// A hand's position relative to the head, in the current BODY frame (metres, +X right / +Y up / +Z fwd).
static bool vr_hand_body_coords(const vr_body_frame& bf, int hand, float out[3])
{
	float hp[3], ps[3], fs[3];
	if (!VR_GetHeadPosStage(hp) || !VR_GetAimPoseStage(hand, ps, fs)) return false;
	const float d[3] = { ps[0]-hp[0], ps[1]-hp[1], ps[2]-hp[2] };
	out[0] = d[0]*bf.right[0] + d[1]*bf.right[1] + d[2]*bf.right[2];
	out[1] = d[0]*bf.up[0]    + d[1]*bf.up[1]    + d[2]*bf.up[2];
	out[2] = d[0]*bf.fwd[0]   + d[1]*bf.fwd[1]   + d[2]*bf.fwd[2];
	return true;
}

static void vr_log_attach_mml(const char* why)
{
	const std::string mml = VR_DumpAttachPointsMML();
	logWarning("VRATTACH ---- %s ----", why);
	size_t start = 0;
	while (start < mml.size()) {
		size_t nl = mml.find('\n', start);
		if (nl == std::string::npos) nl = mml.size();
		if (nl > start) {
			const std::string line = mml.substr(start, nl - start);
			__android_log_print(ANDROID_LOG_INFO, "A1VR", "MML %s", line.c_str());
			logWarning("VRATTACH %s", line.c_str());
		}
		start = nl + 1;
	}
}

static inline float vr_dot3(const float a[3], const float b[3]) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }

// Once per frame. Returns true while setup mode is on (the caller then skips weapon/magazine grabs).
static bool vr_update_anchor_setup(const view_data* view)
{
	const bool on = VR_GetHolsterSetupMode();
	if (!on) {
		if (s_setupWasOn) {
			vr_log_attach_mml("holster setup");
			screen_printf("holster positions saved");
		}
		s_setupWasOn = false;
		s_setupDragSlot[0] = s_setupDragSlot[1] = -1;
		return false;
	}
	if (!s_setupWasOn) screen_printf("setup: grip a box to move and turn it");
	s_setupWasOn = true;

	vr_body_frame bf;
	vr_get_body_frame(view, bf);
	float head[3];
	if (!VR_GetHeadPosStage(head)) return true;

	for (int hand = 0; hand < 2; ++hand) {
		float grip = 0.f;
		if (!VR_GetRawHandInput(hand, nullptr, nullptr, nullptr, &grip, nullptr)) continue;
		const bool down = grip > 0.5f;
		const bool rising = down && !s_setupGripPrev[hand];
		s_setupGripPrev[hand] = down;

		// The hand's full pose in room space.
		float hpos[3], hf[3], hr[3], hu[3];
		if (!VR_GetAimPoseStage(hand, hpos, hf) || !VR_GetAimOrientStage(hand, hr, hu)) continue;
		// Square the hand's frame: the aim forward includes the aim-pitch adjustment, right and up do not,
		// so the three are not mutually perpendicular -- and anything carried in that frame stretched (the
		// first saved holsters had 1.29-long barrel vectors). forward = up x right in room space.
		hf[0] = hu[1]*hr[2] - hu[2]*hr[1]; hf[1] = hu[2]*hr[0] - hu[0]*hr[2]; hf[2] = hu[0]*hr[1] - hu[1]*hr[0];

		if (!down) {
			if (s_setupDragSlot[hand] >= 0) {
				screen_printf("%s placed", VR_HolsterSlotDisplayName(s_setupDragSlot[hand]));
				VR_Vibrate(hand, 30, 0.4f);
			}
			s_setupDragSlot[hand] = -1;
			continue;
		}

		if (rising) {
			float hb[3];
			if (!vr_hand_body_coords(bf, hand, hb)) continue;
			// Nearest anchor whose box the hand is in.
			int best = -1; float bestD2 = 0.f;
			for (int slot = 0; slot < VR_HOLSTER_COUNT; ++slot) {
				vr_holster_anchor a;
				VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &a);
				const float dx = hb[0]-a.x, dy = hb[1]-a.y, dz = hb[2]-a.z;
				const float d2 = dx*dx + dy*dy + dz*dz;
				if (d2 > a.radius * a.radius) continue;
				if (best < 0 || d2 < bestD2) { best = slot; bestD2 = d2; }
			}
			if (best >= 0 && best != s_setupDragSlot[1 - hand]) {
				vr_holster_anchor a;
				VR_GetHolsterAnchorForPlayer(best, VR_EyeHeightM(), &a);
				// The anchor's pose in room space...
				float pa[3], fa[3], ua[3];
				for (int k = 0; k < 3; ++k) {
					pa[k] = head[k] + a.x * bf.right[k] + a.y * bf.up[k] + a.z * bf.fwd[k];
					fa[k] = a.fwd[0] * bf.right[k] + a.fwd[1] * bf.up[k] + a.fwd[2] * bf.fwd[k];
					ua[k] = a.up[0]  * bf.right[k] + a.up[1]  * bf.up[k] + a.up[2]  * bf.fwd[k];
				}
				// ...captured in the hand's own frame, so from now on it moves and turns with the hand.
				const float rel[3] = { pa[0]-hpos[0], pa[1]-hpos[1], pa[2]-hpos[2] };
				s_setupGrabPos[hand][0] = vr_dot3(rel, hr); s_setupGrabPos[hand][1] = vr_dot3(rel, hu); s_setupGrabPos[hand][2] = vr_dot3(rel, hf);
				s_setupGrabFwd[hand][0] = vr_dot3(fa, hr);  s_setupGrabFwd[hand][1] = vr_dot3(fa, hu);  s_setupGrabFwd[hand][2] = vr_dot3(fa, hf);
				s_setupGrabUp[hand][0]  = vr_dot3(ua, hr);  s_setupGrabUp[hand][1]  = vr_dot3(ua, hu);  s_setupGrabUp[hand][2]  = vr_dot3(ua, hf);
				s_setupDragSlot[hand] = best;
				screen_printf("moving %s", VR_HolsterSlotDisplayName(best));
				VR_Vibrate(hand, 40, 0.6f);
			}
		}

		const int slot = s_setupDragSlot[hand];
		if (slot < 0) continue;
		// Rebuild the anchor from the hand's current pose, then express it in the body frame.
		float pa[3], fa[3], ua[3];
		for (int k = 0; k < 3; ++k) {
			pa[k] = hpos[k] + s_setupGrabPos[hand][0]*hr[k] + s_setupGrabPos[hand][1]*hu[k] + s_setupGrabPos[hand][2]*hf[k];
			fa[k] = s_setupGrabFwd[hand][0]*hr[k] + s_setupGrabFwd[hand][1]*hu[k] + s_setupGrabFwd[hand][2]*hf[k];
			ua[k] = s_setupGrabUp[hand][0]*hr[k]  + s_setupGrabUp[hand][1]*hu[k]  + s_setupGrabUp[hand][2]*hf[k];
		}
		const float d[3] = { pa[0]-head[0], pa[1]-head[1], pa[2]-head[2] };
		const float fb[3] = { vr_dot3(fa, bf.right), vr_dot3(fa, bf.up), vr_dot3(fa, bf.fwd) };
		const float ub[3] = { vr_dot3(ua, bf.right), vr_dot3(ua, bf.up), vr_dot3(ua, bf.fwd) };
		VR_SetHolsterAnchorFromPlayer(slot, VR_EyeHeightM(),
			vr_dot3(d, bf.right), vr_dot3(d, bf.up), vr_dot3(d, bf.fwd), fb, ub);
	}
	return true;
}

// How far a hand at (hx,hy,hz) is from a holster, the way grabbing measures it, and the grab radius. Back
// slots are reached over the shoulder: being above the anchor costs nothing, only horizontal distance and
// dropping below it count.
static void vr_holster_reach(const view_data* view, float W, int slot, bool haveHb, const float hb[3],
                             float hx, float hy, float hz, float* d2, float* r)
{
	vr_holster_anchor ha; VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &ha);
	const bool backSlot = slot == VR_HOLSTER_RIGHT_BACK || slot == VR_HOLSTER_LEFT_BACK ||
	                      slot == VR_HOLSTER_BACK_CENTER;
	if (backSlot && haveHb) {
		const float dx = hb[0] - ha.x, dz = hb[2] - ha.z;
		const float dy = (hb[1] < ha.y) ? (ha.y - hb[1]) : 0.f;
		*d2 = (dx*dx + dy*dy + dz*dz) * W * W;
		*r  = 0.40f * W;
	} else {
		float c[3]; vr_holster_world_pos(view, slot, c);
		const float dx = hx - c[0], dy = hy - c[1], dz = hz - c[2];
		*d2 = dx*dx + dy*dy + dz*dz;
		*r  = ha.radius * W;
	}
}

// The visible holster a grip at (hx,hy,hz) would draw from: the nearest one within its own radius, or -1.
static int vr_nearest_holster_slot(const view_data* view, float W, int hand, float hx, float hy, float hz,
                                   const short visible[VR_HOLSTER_COUNT], float* bestD2Out)
{
	vr_body_frame bfGrab;
	vr_get_body_frame(view, bfGrab);
	float hb[3];
	const bool haveHb = vr_hand_body_coords(bfGrab, hand, hb);
	int best = -1; float bestD2 = 0.f;
	for (int slot = 0; slot < VR_HOLSTER_COUNT; ++slot) {
		if (visible[slot] == NONE) continue;
		float d2, r;
		vr_holster_reach(view, W, slot, haveHb, hb, hx, hy, hz, &d2, &r);
		if (d2 > r * r) continue;
		if (best < 0 || d2 < bestD2) { best = slot; bestD2 = d2; }
	}
	if (bestD2Out) *bestD2Out = bestD2;
	return best;
}

// Whether a hand is free to draw a weapon: holding no weapon and no magazine.
static bool vr_hand_is_free(int hand)
{
	if (VR_GetHandMagazine(hand) != NONE) return false;
	vr_hands_state hs;
	vr_get_hands_state(hs);
	if (hs.heldHand < 0) return true;
	if (hs.primary != NONE && (hand == hs.heldHand || hs.primaryCount == 2)) return false;
	if (hs.offhand != NONE && hand != hs.heldHand) return false;
	return true;
}

// Once per frame: grip near an occupied holster draws that weapon.
static void vr_update_holster_grabs(const view_data* view)
{
	if (!VR_PhysicalWeaponsActive()) {
		s_holsterGripPrev[0] = s_holsterGripPrev[1] = false;
		return;
	}

	// Empty hands mean the fist. The engine always has SOME current weapon, and a new game, a loaded
	// save or a respawn comes up holding whatever it last selected -- a gun nobody grabbed, which you
	// then had to swap away. With no hand holding anything, put the fist up. Asked for at most every
	// half second or so, so a switch still in flight through the hotkey path is not re-requested
	// every frame.
	{
		static int s_fistRequestCooldown = 0;
		if (s_fistRequestCooldown > 0) --s_fistRequestCooldown;
		if (VR_GetHeldWeaponHand() < 0 && !PLAYER_IS_DEAD(get_player_data(current_player_index))) {
			if (vr_get_offhand_weapon(current_player_index) != NONE) vr_clear_offhand_weapon(current_player_index);
			short t = NONE, m = 0;
			get_player_weapon_mode_and_type(current_player_index, &t, &m);
			if (t != _weapon_fist && s_fistRequestCooldown == 0) {
				VR_SetHolsterGrabRequest(_weapon_fist);
				s_fistRequestCooldown = 45;
			}
		}
	}
	const float W = (float)VR_Settings()->worldScaleWUM;

	// Holster & pouch setup: grips drag anchors instead of grabbing weapons or magazines.
	const bool setupMode = vr_update_anchor_setup(view);

	// Facts the input layer needs about what each hand holds, pushed once per frame.
	{
		vr_hands_state hs0;
		vr_get_hands_state(hs0);
		for (int h = 0; h < 2; ++h) {
			short wt = NONE;
			if (hs0.heldHand >= 0) wt = (h == hs0.heldHand) ? hs0.primary : hs0.offhand;
			VR_SetHandChargeMode(h, wt != NONE && weapon_charges_on_secondary(wt));
		}
		VR_SetSupportSecondaryFire(hs0.primary != NONE && hs0.offhand == NONE &&
		                           weapon_has_distinct_secondary(hs0.primary));

		// Two-handing only engages with the support hand AT the weapon's two-handed grab spot.
		bool inRange = false;
		if (hs0.heldHand >= 0 && hs0.offhand == NONE && s_vrWeaponPose[hs0.heldHand].valid) {
			const vr_weapon_pose& wp = s_vrWeaponPose[hs0.heldHand];
			const vr_attach_point* fg = VR_GetForegripOrDefault(wp.weapon_type);
			float target[3];
			vr_weapon_local_to_world(wp, W, fg->x, fg->y, fg->z, target);
			vr_body_frame bf0;
			vr_get_body_frame(view, bf0);
			float hp0[3], r0[3], u0[3], f0[3];
			if (vr_hand_world_pose(bf0, 1 - hs0.heldHand, hp0, r0, u0, f0)) {
				const float dx = hp0[0]-target[0], dy = hp0[1]-target[1], dz = hp0[2]-target[2];
				inRange = (dx*dx + dy*dy + dz*dz) <= (fg->radius * W) * (fg->radius * W);
				// A loosened magazine within reach, or a magazine already in that hand, means the grip is
				// for the magazine -- the grenade well sits right by the rifle's grab spot.
				if (inRange && (VR_GetHandMagazine(1 - hs0.heldHand) != NONE ||
				                vr_hand_near_loose_mag(W, 1 - hs0.heldHand, hp0)))
					inRange = false;
			}
		}
		VR_SetTwoHandedGrabInRange(inRange);
	}

	for (int hand = 0; hand < 2; ++hand) {
		float grip = 0.f;
		if (!VR_GetRawHandInput(hand, nullptr, nullptr, nullptr, &grip, nullptr)) continue;
		const bool down = grip > 0.5f;
		const bool rising = down && !s_holsterGripPrev[hand];
		s_holsterGripPrev[hand] = down;
		if (!rising || setupMode) continue;

		float hp[3], ps[3], fs[3];
		if (!VR_GetHeadPosStage(hp) || !VR_GetAimPoseStage(hand, ps, fs)) continue;

		// Hand position in world, via the same transform the weapon anchor uses.
		const double yaw = view->virtual_yaw * (360.0 / (double(FIXED_ONE) * double(FULL_CIRCLE))) * (8.0 * atan(1.0) / 360.0);
		const double cyw = cos(yaw), syw = sin(yaw);
		float camx = (float)view->origin.x, camy = (float)view->origin.y, camz = (float)view->origin.z;
		float rcx, rcy, rcz;
		if (VR_GetRenderCamera(&rcx, &rcy, &rcz)) { camx = rcx; camy = rcy; camz = rcz + VR_GetEyeZOffset(); }
		const float qx = (ps[0] - hp[0]) * W, qsy = (ps[1] - hp[1]) * W, qz = (ps[2] - hp[2]) * W;
		const float rx = -qx, ry = -qz;
		const float hx = camx + (float)(rx * cyw - ry * syw);
		const float hy = camy + (float)(rx * syw + ry * cyw);
		const float hz = camz + qsy;

		// A LOOSENED magazine (either well): the free hand pulls it out, rounds and all. Checked first,
		// because the wells are on the gun and would otherwise read as a steadying grip.
		if (VR_GetHeldWeaponHand() >= 0 && VR_GetHeldWeaponHand() != hand &&
		    vr_get_offhand_weapon(current_player_index) == NONE && VR_GetHandMagazine(hand) == NONE &&
		    vr_try_pull_loose_mag(W, hand, hx, hy, hz))
			continue;

		// Nearest VISIBLE holster within its own radius wins -- a slot the hands have emptied has
		// nothing in it to grab.
		short visible[VR_HOLSTER_COUNT];
		vr_visible_holsters(visible);
		float bestD2 = 0.f;
		const int bestSlot = vr_nearest_holster_slot(view, W, hand, hx, hy, hz, visible, &bestD2);
		const short best = bestSlot >= 0 ? visible[bestSlot] : (short)NONE;
		// If this hand is reaching for the weapon the OTHER hand is holding, and that weapon is nearer
		// than any holster, it is a steadying grip -- leave it to the two-handed latch. Comparing
		// against the holster distance (not just a radius) keeps a hip draw working while the other
		// hand holds its gun down by your side.
		vr_hands_state hs;
		vr_get_hands_state(hs);
		if (hs.heldHand >= 0 && hs.heldHand != hand && hs.offhand == NONE &&
		    s_vrWeaponPose[hs.heldHand].valid) {
			const vr_weapon_pose& wp = s_vrWeaponPose[hs.heldHand];
			// Prefer the authored foregrip; fall back to the weapon anchor when none is set yet.
			const vr_attach_point* fg = VR_GetForegripOrDefault(wp.weapon_type);
			float target[3];
			vr_weapon_local_to_world(wp, W, fg->x, fg->y, fg->z, target);
			const float gx = hx-target[0], gy = hy-target[1], gz = hz-target[2];
			const float gd2 = gx*gx + gy*gy + gz*gz;
			const float gr = fg->radius * W;
			if (gd2 <= gr*gr && (best == NONE || gd2 < bestD2)) continue;   // steadying, not grabbing
		}

		// A pouch nearer than any holster wins. The pouches sit out by the off-hand hip, and reaching
		// into one must not pull the second magnum off that hip instead.
		if (hs.heldHand >= 0 && hs.heldHand != hand && hs.offhand == NONE && VR_GetHandMagazine(hand) == NONE) {
			float pd2 = 0.f;
			if (vr_nearest_dispensing_pouch(view, hand, hx, hy, hz, &pd2) >= 0 && (best == NONE || pd2 < bestD2)) {
				vr_try_grab_magazine(view, hand, hx, hy, hz);
				continue;
			}
		}

		if (best != NONE) {
			if (hs.heldHand < 0) {
				// Empty-handed: this becomes the primary weapon, in this hand.
				VR_SetHolsterGrabRequest(best);
				VR_SetHeldWeaponHand(hand);
				VR_Vibrate(hand, 40, 0.6f);
			} else if (hs.heldHand != hand && hs.offhand == NONE) {
				// The other hand already holds the primary. The same two-fisted weapon pairs up (dual
				// magnums); anything else goes into THIS hand as its own weapon, and the primary stays
				// exactly where it is.
				const bool took = (best == hs.primary && weapon_is_twofisted(best))
				                ? vr_raise_second_weapon(current_player_index)
				                : vr_set_offhand_weapon(current_player_index, best);
				if (took) VR_Vibrate(hand, 40, 0.6f);
			}
		} else if (hs.heldHand != hand && hs.offhand == NONE && VR_GetHandMagazine(hand) == NONE) {
			// Not on a weapon holster, and this hand is free: try the ammo pouches.
			vr_try_grab_magazine(view, hand, hx, hy, hz);
		}
	}

	// ---- release: a weapon nobody is holding goes back on the body -------------------------
	// No dropping and no throwing -- letting go simply re-holsters. If the OTHER hand still has a
	// grip closed we keep the weapon in play and hand ownership over to it: that hand becomes the
	// pivot the weapon hangs from, so a two-handed hold survives releasing the main handle and you
	// can re-grip it. Switching to the fist is how "holding nothing" is expressed to the engine,
	// which always has some current weapon.
	{
		vr_hands_state hs;
		vr_get_hands_state(hs);
		const int held = hs.heldHand;
		if (held >= 0) {
			float heldGrip = 0.f, otherGrip = 0.f;
			VR_GetRawHandInput(held,     nullptr, nullptr, nullptr, &heldGrip,  nullptr);
			VR_GetRawHandInput(1 - held, nullptr, nullptr, nullptr, &otherGrip, nullptr);
			const bool heldUp = heldGrip <= 0.5f, otherUp = otherGrip <= 0.5f;

			if (hs.offhand != NONE) {
				// Two different weapons. Each hand letting go puts ITS weapon back. If the primary
				// hand lets go but the other keeps hold, the other weapon becomes the primary (and the
				// one the HUD shows) in place, with no raise.
				if (otherUp) {
					vr_clear_offhand_weapon(current_player_index);
					VR_Vibrate(1 - held, 30, 0.4f);
				}
				if (heldUp) {
					if (!otherUp && vr_promote_offhand_weapon(current_player_index)) {
						VR_SetHeldWeaponHand(1 - held);
					} else {
						VR_SetHeldWeaponHand(-1);
						VR_SetHolsterGrabRequest(_weapon_fist);
					}
					VR_Vibrate(held, 30, 0.4f);
				}
			} else if (hs.primaryCount >= 2 && heldUp != otherUp) {
				// A dual-wielded pair lost one hand: the pistol in the releasing hand goes back on the
				// body and the other stays up. The engine always drops the SECOND copy, so ownership moves
				// to the hand still gripping -- the survivor is then the one the primary trigger fires.
				// When the PRIMARY hand is the one letting go, the gun staying up is physically trigger 1:
				// the two trade slots first, so each keeps its own rounds and magazine.
				if (heldUp) vr_swap_pair_triggers(current_player_index);
				vr_lower_second_weapon(current_player_index);
				if (heldUp) VR_SetHeldWeaponHand(1 - held);
				VR_Vibrate(heldUp ? held : 1 - held, 30, 0.4f);
			} else if (heldUp) {
				if (!otherUp) {
					VR_SetHeldWeaponHand(1 - held);      // pivot to the supporting hand
					VR_Vibrate(1 - held, 25, 0.4f);
				} else {
					VR_SetHeldWeaponHand(-1);
					VR_SetHolsterGrabRequest(_weapon_fist);
					VR_Vibrate(held, 30, 0.4f);
				}
			}
		}
	}
}

// Draw every carried weapon at its holster. The weapon currently in hand is skipped -- it is
// already being drawn on the controller by render_vr_weapon_sprites_3d.
// ---- holster display: outlines, solid on hover, a tick for what the grip would take ------------------
// A body full of guns is noise. Holstered weapons are NOT drawn at all, except: the one a free hand's grip
// would actually draw (the same pick as the grab) is SOLID with a thin bright rim, and that hand gets a
// tick as it arrives, so a draw can be done without looking; and looking down past kLookDownDeg shows
// every holstered weapon as an OUTLINE (see-through, a thin rim) so the layout can be checked.
// Without a stencil buffer the outline falls back to a see-through ghost (kHolsterGhostAlpha).
static const float kHolsterGhostAlpha = 0.22f;
static const float kRevealExtraM      = 0.20f;
static const float kOutlineWidthM     = 0.004f;
static const float kHoverOutlineWidthM = 0.003f;
static const float kLookDownOutlineWidthM = 0.0025f;
static const float kLookDownDeg       = 35.f, kLookDownFadeDeg = 10.f;
static float    s_holsterAlpha[VR_HOLSTER_COUNT];   // how SOLID each is (0 = outline only)
static float    s_holsterNear[VR_HOLSTER_COUNT];    // how near a hand is (outline brightness)
static int      s_hoverSlot[2] = { -1, -1 };
static uint32_t s_holsterDisplayFrame = 0;

static int vr_nearest_dispensing_pouch(const view_data* view, int hand, float hx, float hy, float hz, float* d2Out);

static void vr_update_holster_display(const view_data* view, const short visible[VR_HOLSTER_COUNT])
{
	if (s_holsterDisplayFrame == s_vrFrameNo) return;    // once per frame, not per eye
	s_holsterDisplayFrame = s_vrFrameNo;
	static uint64_t s_lastMs = 0;
	const uint64_t now = machine_tick_count();
	float dt = s_lastMs ? float(now - s_lastMs) / 1000.f : 0.f;
	if (dt < 0.f || dt > 0.1f) dt = 0.1f;
	s_lastMs = now;

	const float W = (float)VR_Settings()->worldScaleWUM;
	float reveal[VR_HOLSTER_COUNT] = {};    // solid
	float near[VR_HOLSTER_COUNT] = {};      // outline brightness
	int hover[2] = { -1, -1 };

	if (VR_GetHolsterSetupMode()) {
		for (int s = 0; s < VR_HOLSTER_COUNT; ++s) reveal[s] = 1.f;
	} else {
		// Looking down shows the lot.
		float hf[3];
		if (VR_GetHeadFwdStage(hf)) {
			const float y = hf[1] < -1.f ? -1.f : (hf[1] > 1.f ? 1.f : hf[1]);
			const float downDeg = -asinf(y) * 57.2957795f;
			float look = (downDeg - (kLookDownDeg - kLookDownFadeDeg)) / kLookDownFadeDeg;
			look = look < 0.f ? 0.f : (look > 1.f ? 1.f : look);
			for (int s = 0; s < VR_HOLSTER_COUNT; ++s) near[s] = look;   // brightens the outlines only
		}

		vr_body_frame bf;
		vr_get_body_frame(view, bf);
		vr_hands_state hs;
		vr_get_hands_state(hs);
		for (int hand = 0; hand < 2; ++hand) {
			float pos[3], wr[3], wu[3], wf[3], hb[3];
			if (!vr_hand_world_pose(bf, hand, pos, wr, wu, wf)) continue;
			(void)hb;
			// Outline brightens as the hand nears the holster itself (plain distance -- the generous
			// over-the-shoulder grab rule would light the back weapons whenever a hand is raised).
			for (int s = 0; s < VR_HOLSTER_COUNT; ++s) {
				if (visible[s] == NONE) continue;
				vr_holster_anchor ha; VR_GetHolsterAnchorForPlayer(s, VR_EyeHeightM(), &ha);
				float c[3]; vr_holster_world_pos(view, s, c);
				const float dx = pos[0]-c[0], dy = pos[1]-c[1], dz = pos[2]-c[2];
				float v = 1.f - (sqrtf(dx*dx + dy*dy + dz*dz) - ha.radius * W) / (kRevealExtraM * W);
				v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
				(void)v;   // proximity no longer shows anything: only hover and look-down do
			}
			// What this hand's grip would draw -- only a free hand draws, and a nearer steadying grip or
			// ammo pouch wins, exactly as in vr_update_holster_grabs.
			if (!vr_hand_is_free(hand)) continue;
			float bestD2 = 0.f;
			const int slot = vr_nearest_holster_slot(view, W, hand, pos[0], pos[1], pos[2], visible, &bestD2);
			if (slot < 0) continue;
			if (hs.heldHand >= 0 && hs.heldHand != hand && hs.offhand == NONE && s_vrWeaponPose[hs.heldHand].valid) {
				const vr_weapon_pose& wp = s_vrWeaponPose[hs.heldHand];
				const vr_attach_point* fg = VR_GetForegripOrDefault(wp.weapon_type);
				float target[3];
				vr_weapon_local_to_world(wp, W, fg->x, fg->y, fg->z, target);
				const float gx = pos[0]-target[0], gy = pos[1]-target[1], gz = pos[2]-target[2];
				const float gd2 = gx*gx + gy*gy + gz*gz, gr = fg->radius * W;
				if (gd2 <= gr*gr && gd2 < bestD2) continue;
				float pd2 = 0.f;
				if (vr_nearest_dispensing_pouch(view, hand, pos[0], pos[1], pos[2], &pd2) >= 0 && pd2 < bestD2) continue;
			}
			hover[hand] = slot;
		}
	}

	for (int hand = 0; hand < 2; ++hand) {
		if (hover[hand] >= 0 && hover[hand] != s_hoverSlot[hand]) VR_Vibrate(hand, 12, 0.35f);   // a tick
		s_hoverSlot[hand] = hover[hand];
	}
	// No fading: a weapon is shown or it is not (fading looked poor).
	(void)dt;
	for (int s = 0; s < VR_HOLSTER_COUNT; ++s) {
		s_holsterAlpha[s] = reveal[s];
		s_holsterNear[s]  = near[s];
	}
}

struct vr_model_parts;
static bool vr_gun_parts_now(const OGL_ModelData* mdl, short weapon, short copy,
                             std::vector<int>& F, std::vector<int>& C);

static void render_vr_holstered_weapons(view_data* view)
{
	if (!VR_PhysicalWeaponsActive()) return;
	if (!OGL_IsActive()) return;
	if (PLAYER_IS_DEAD(get_player_data(current_player_index))) return;

	const float W = (float)VR_Settings()->worldScaleWUM;
	short visible[VR_HOLSTER_COUNT];
	vr_visible_holsters(visible);
	vr_update_holster_display(view, visible);

	// Body frame (follows the head, see vr_get_body_frame), for anchor positions and how each weapon hangs.
	vr_body_frame body;
	vr_get_body_frame(view, body);
	float bRight[3], bUp[3];
	vr_room_dir_to_world(body, body.right, bRight);
	vr_room_dir_to_world(body, body.up,    bUp);

	for (int slot = 0; slot < VR_HOLSTER_COUNT; ++slot) {
		const short wt = visible[slot];     // already excludes pouches and whatever the hands hold
		if (wt == NONE) continue;

		short coll = NONE, shape = NONE, lowShape = NONE;
		if (!get_weapon_idle_shape(wt, &coll, &shape, &lowShape)) continue;

		shape_information_data* si = extended_get_shape_information(coll, lowShape);
		bitmap_definition* bmp = nullptr; void* shade = nullptr;
		extended_get_shape_bitmap_and_shading_table(coll, lowShape, &bmp, &shade, view->shading_mode);
		if (!si || !bmp) continue;

		float c[3]; vr_holster_world_pos(view, slot, c);

		// Just put back? Fly in from the hand that let go of it (see vr_apply_swing), and remember that
		// hand for mirroring.
		{
			const vr_pose_snap& hs = s_holsterSnap[slot];
			if (!(hs.wt == wt && s_vrFrameNo - hs.frame <= 1) && wt >= 0 && wt < MAXIMUM_NUMBER_OF_WEAPONS) {
				for (int h = 0; h < 2; ++h)
					if (vr_snap_just_left(s_handSnap[h][wt], wt)) {
						vr_swing_start(s_holsterSwing[slot], s_handSnap[h][wt]);
						s_holsterLastHand[slot] = h;
						break;
					}
			}
		}

		rectangle_definition rect;
		rect.ModelPtr        = nullptr;
		rect.Opacity         = 1;
		rect.ShapeDesc       = BUILD_DESCRIPTOR(coll, 0);
		rect.LowLevelShape   = lowShape;
		rect.texture         = bmp;
		rect.shading_tables  = shade;
		rect.transfer_mode   = _textured_transfer;
		rect.transfer_data   = 0;
		rect.flags           = 0;
		// Mirrored exactly as the hand that holds it would draw it (render_vr_weapon_sprites_3d: the shape's
		// own mirror bit, flipped for the left hand -- hand 0 -- unless the model is authored left-handed),
		// so nothing flips when it swings between holster and hand. The hand is the one that last put it
		// back here, else the one on this side of the body.
		{
			int drawHand = s_holsterLastHand[slot];
			if (drawHand < 0) {
				vr_holster_anchor pa;
				VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &pa);
				drawHand = pa.x < 0.f ? 0 : 1;
			}
			bool flip = (si->flags & _X_MIRRORED_BIT) != 0;
			if ((drawHand == 0) != VR_IsWeaponNaturallyLeftHanded(wt)) flip = !flip;
			rect.flip_horizontal = flip;
		}
		rect.flip_vertical   = false;
		rect.depth           = 0;
		rect.ambient_shade   = get_light_intensity(
			get_polygon_data(view->origin_polygon_index)->floor_lightsource_index);
		rect.ambient_shade   = MAX(si->minimum_light_intensity, rect.ambient_shade);
		if (view->shading_mode == _shading_infravision) rect.flags |= _SHADELESS_BIT;
		// takes a HIGH-level mode (_xfer_*). Passing _textured_transfer (3) here meant
		// _xfer_subtle_invisibility -> tinted -> silhouette colour table -> solid white.
		instantiate_rectangle_transfer_mode(view, &rect, _xfer_normal, 0);

		short modelSeq = NONE;
		vector<OGL_ModelData>* allModels = OGL_GetAllModels(coll, shape, modelSeq);
		OGL_ModelData* mdl = (allModels && !allModels->empty()) ? &(*allModels)[0] : nullptr;

		if (mdl) {
			float mR[3], mU[3], mF[3];
			vr_holster_model_basis(body, slot, mR, mU, mF);
			vr_snap_set(s_holsterSnap[slot], wt, c, mR, mU, mF);   // the true holster pose
			const bool swinging = vr_apply_swing(s_holsterSwing[slot], c, mR, mU, mF);
			// Solid (full opacity, snapped -- no fade) when it is the grab target or flying to/from a hand, with
			// a thin bright rim when hovered. Holster setup: every weapon as an outline, white while being
			// moved. Looking down: faint thin outlines. Otherwise: not drawn.
			const bool  hovered  = (slot == s_hoverSlot[0] || slot == s_hoverSlot[1]);
			const bool  setup    = VR_GetHolsterSetupMode();
			const bool  isStatic = rect.transfer_mode == _static_transfer;
			const bool  haveStencil = VR_EyeHasStencil();
			// A gun model with separate magazine parts hangs here without whatever has been taken out of it.
			std::vector<int> holsterPartF, holsterPartC;
			const bool holsterParts = vr_gun_parts_now(mdl, wt, 0, holsterPartF, holsterPartC);
			if (holsterParts) OGL_SetVRWeaponModelParts(holsterPartF.data(), holsterPartC.data(), (int)holsterPartF.size());
			auto outline = [&](float r, float g, float b, float oa, float width) {
				if (haveStencil &&
				    OGL_RenderVRWeaponModelOutline(rect, coll, 0, mdl, c[0], c[1], c[2], mR, mU, mF,
				                                   slot + 1, r, g, b, oa, width))
					return;
				// no stencil: a faint see-through ghost instead
				OGL_SetVRWeaponModelTint(0.55f, 0.75f, 1.f, kHolsterGhostAlpha, 0.f, 0.f, 0.f);
				OGL_RenderVRWeaponModel(rect, coll, 0, mdl, c[0], c[1], c[2], mR, mU, mF, isStatic, 0, 0.f, -1);
				OGL_SetVRWeaponModelTint(1.f, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f);
			};
			if (swinging || (hovered && !setup)) {
				if (hovered && !swinging) outline(0.75f, 0.95f, 1.f, 1.f, kHoverOutlineWidthM);
				OGL_RenderVRWeaponModel(rect, coll, 0, mdl, c[0], c[1], c[2], mR, mU, mF, isStatic, 0, 0.f, -1);
			} else if (setup) {
				const bool dragging = (slot == s_setupDragSlot[0] || slot == s_setupDragSlot[1]);
				if (dragging) outline(1.f, 1.f, 1.f, 1.f, kOutlineWidthM * 1.5f);
				else          outline(0.55f, 0.85f, 1.f, 0.9f, kOutlineWidthM);
			} else if (s_holsterNear[slot] > 0.5f) {
				outline(0.6f, 0.8f, 1.f, 0.4f, kLookDownOutlineWidthM);    // looking down: faint and thin
			}
			if (holsterParts) OGL_ClearVRWeaponModelParts();
		} else {
			// No 3D model for this weapon: fall back to a camera-facing sprite so the holster is
			// still visibly occupied rather than silently empty.
			const float wpx = fabsf(float(si->world_right - si->world_left));
			const float wpy = fabsf(float(si->world_bottom - si->world_top));
			const float hw = ((wpx > 4.f ? wpx : 512.f) / 1024.f) * 0.5f * W * 0.5f;
			const float hh = ((wpy > 4.f ? wpy : 512.f) / 1024.f) * 0.5f * W * 0.5f;
			float verts[4][3] = {
				{ c[0] - hw*bRight[0] + hh*bUp[0], c[1] - hw*bRight[1] + hh*bUp[1], c[2] - hw*bRight[2] + hh*bUp[2] },
				{ c[0] + hw*bRight[0] + hh*bUp[0], c[1] + hw*bRight[1] + hh*bUp[1], c[2] + hw*bRight[2] + hh*bUp[2] },
				{ c[0] + hw*bRight[0] - hh*bUp[0], c[1] + hw*bRight[1] - hh*bUp[1], c[2] + hw*bRight[2] - hh*bUp[2] },
				{ c[0] - hw*bRight[0] - hh*bUp[0], c[1] - hw*bRight[1] - hh*bUp[1], c[2] - hw*bRight[2] - hh*bUp[2] }
			};
			OGL_RenderVRWeaponQuad(rect, verts);
		}
	}
}

// ---- VR physical weapons: magazines ------------------------------------------------------------
// Reach into an ammo pouch and grip to get a magazine for whatever is in your other hand; bring it
// to that weapon's magwell to seat it. Nothing is deducted from the ammo pool at grab time -- the
// engine's own reload_weapon() does the accounting when the mag actually goes in, so grabbing and
// releasing a magazine costs nothing and cannot duplicate ammo.

// Which magazine a pouch hands you: the ammunition for the weapon in the OTHER hand. Left pouch
// gives its primary ammo, right its secondary (the assault rifle's grenades) -- so nine ammo types
// need only two pouches. NONE when there is nothing sensible to hand over.
// The ammunition a pouch is FOR right now: the weapon in the primary hand's primary ammo (primary
// pouch) or its second-trigger ammo when that differs (secondary pouch -- rifle grenades). Regardless
// of whether any is left; NONE when the pouch has no job for this weapon.
static short vr_pouch_ammo_type(int pouchSlot, int reachingHand)
{
	// The gun in the OTHER hand -- whichever gun it is (the primary, the second hand's, or one of a pair).
	if (reachingHand < 0 || reachingHand > 1) return NONE;
	const int weaponHand = 1 - reachingHand;
	short wt = NONE, copy = 0;
	if (!vr_hand_gun(current_player_index, weaponHand, &wt, &copy)) return NONE;
	if (!s_vrWeaponPose[weaponHand].valid) return NONE;
	if (wt == NONE || VR_WeaponUsesSpinReload(wt)) return NONE;   // shotgun reloads by spinning

	const short primaryAmmo = get_player_weapon_ammo_type(current_player_index, wt, _primary_weapon);
	if (pouchSlot != VR_HOLSTER_RIGHT_POUCH) return primaryAmmo;
	const short secondaryAmmo = get_player_weapon_ammo_type(current_player_index, wt, _secondary_weapon);
	return (secondaryAmmo == primaryAmmo) ? (short)NONE : secondaryAmmo;
}

// The magazine a pouch will actually hand over: its ammo, if there is any left.
static short vr_pouch_magazine_for(int pouchSlot, int reachingHand)
{
	const short item = vr_pouch_ammo_type(pouchSlot, reachingHand);
	if (item == NONE) return NONE;
	return get_player_data(current_player_index)->items[item] > 0 ? item : (short)NONE;
}

// Grip near a pouch takes a magazine out of it.
// The nearest pouch within reach that would actually hand this hand a magazine, or -1.
static int vr_nearest_dispensing_pouch(const view_data* view, int hand, float hx, float hy, float hz, float* d2Out)
{
	const float W = (float)VR_Settings()->worldScaleWUM;
	int best = -1; float bestD2 = 0.f;
	for (int slot = 0; slot < VR_HOLSTER_COUNT; ++slot) {
		if (!VR_HolsterSlotIsPouch(slot)) continue;
		if (vr_pouch_magazine_for(slot, hand) == NONE) continue;
		float c[3]; vr_holster_world_pos(view, slot, c);
		vr_holster_anchor ha; VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &ha);
		const float dx = hx - c[0], dy = hy - c[1], dz = hz - c[2];
		const float d2 = dx*dx + dy*dy + dz*dz;
		const float r = ha.radius * W;
		if (d2 > r * r) continue;
		if (best < 0 || d2 < bestD2) { best = slot; bestD2 = d2; }
	}
	if (d2Out) *d2Out = bestD2;
	return best;
}

static bool vr_try_grab_magazine(const view_data* view, int hand, float hx, float hy, float hz)
{
	const int slot = vr_nearest_dispensing_pouch(view, hand, hx, hy, hz, nullptr);
	if (slot < 0) return false;
	const short item = vr_pouch_magazine_for(slot, hand);
	VR_SetHandMagazine(hand, item);
	// Full magazines first. Once only part-used ones are left, hand one out: it leaves the inventory now
	// and is carried like any magazine with rounds in it (seating it costs nothing more).
	{
		const short have = get_player_data(current_player_index)->items[item];
		const int   haveN = (have == NONE || have < 0) ? 0 : have;
		short r = 0;
		while (VR_PartialMagCount(item) > haveN) VR_PopPartialMag(item, &r);   // stale entries (e.g. new game)
		if (haveN - VR_PartialMagCount(item) <= 0 && VR_PartialMagCount(item) > 0 &&
		    VR_PopPartialMag(item, &r) && vr_take_magazine_from_inventory(current_player_index, item))
			VR_SetHandMagazineRounds(hand, r);
	}
	s_vrMagMustClear[hand] = false;
	VR_Vibrate(hand, 35, 0.5f);
	return true;
}

static bool vr_hand_world_pose(const vr_body_frame& bf, int hand,
                               float pos[3], float wr[3], float wu[3], float wf[3]);   // with the hand visuals

// Each well says what size its magazine is (<vr_magwell mag_width= mag_length= mag_depth= mag_round=>).
struct vr_mag_dims
{
	float width, length, depth;   // metres: across the gun, along the insertion line, fore-aft
	bool  round;                  // a cylinder of diameter `width` along the insertion line (canisters)
	bool  drum;                   // a drum facing down the barrel: `depth` thick, `length` x `width` across
};

// ---- magazines falling away (visual only) ----------------------------------------------------
// A magazine you let go of, or one shaken loose, drops and lands. Purely a picture: nothing was deducted
// to get a fresh mag, and one that came out of a gun takes its rounds with it.
// Each tumbles as it goes: an orientation (r, u = toward its top, f) and a spin (world rad/s) -- the wrist
// roll that flicked it out, end-over-end from a throw, a small tip from a plain drop -- and lands lying flat.
struct vr_falling_mag {
	short item; float pos[3]; float vel[3]; float age; float floorZ; bool landed;
	float r[3], u[3], f[3];   // orientation; pos is the CENTRE of the magazine
	float om[3];              // angular velocity, world axes, radians per second
	short weapon; int which; int rounds; int gunHand;   // what it came out of, for drawing it from that model
};
static std::vector<vr_falling_mag> s_fallingMags;
static uint64_t s_fallingLastMs = 0;

static vr_mag_dims vr_mag_dims_for_ammo(short ammo);
static void vr_mag_to_pickup(const vr_falling_mag& m);

// top = where the magazine's top is now; r/u/f = its sides (u pointing to the top); om = its spin.
static bool vr_weapon_for_ammo(short ammo, int preferHand, short* weapon, int* which);

static void vr_spawn_falling_mag(const float top[3], const float vel[3], short item,
                                 const float r[3], const float u[3], const float f[3], const float om[3],
                                 int rounds, int gunHand)
{
	if (item == NONE) return;
	if (s_fallingMags.size() >= 8) {
		// make room: the oldest goes -- as its pickup if it still has rounds (never just lost)
		if (s_fallingMags.front().landed) vr_mag_to_pickup(s_fallingMags.front());
		s_fallingMags.erase(s_fallingMags.begin());
	}
	const float W = (float)VR_Settings()->worldScaleWUM;
	const vr_mag_dims md = vr_mag_dims_for_ammo(item);
	vr_falling_mag m;
	m.item = item; m.age = 0.f; m.landed = false;
	m.rounds = rounds; m.gunHand = gunHand; m.weapon = NONE; m.which = 0;
	vr_weapon_for_ammo(item, gunHand, &m.weapon, &m.which);
	// Lands at the player's feet: close enough for a magazine dropped or tossed nearby.
	m.floorZ = (float)get_player_data(current_player_index)->location.z;
	for (int k = 0; k < 3; ++k) {
		m.pos[k] = top[k] - u[k] * md.length * 0.5f * W;
		m.vel[k] = vel[k];
		m.r[k] = r[k]; m.u[k] = u[k]; m.f[k] = f[k]; m.om[k] = om[k];
	}
	s_fallingMags.push_back(m);
}

// A spin for a magazine let go of with this velocity (world units/s): end over end about the horizontal
// axis across the throw, faster the harder it goes, plus a little wobble so a plain drop still tips over.
static void vr_toss_spin(float W, const float vel[3], float om[3])
{
	const float up[3] = { 0.f, 0.f, 1.f };
	float ax[3] = { vel[1]*up[2] - vel[2]*up[1], vel[2]*up[0] - vel[0]*up[2], vel[0]*up[1] - vel[1]*up[0] };
	const float n = sqrtf(ax[0]*ax[0] + ax[1]*ax[1] + ax[2]*ax[2]);
	const float speed = sqrtf(vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2]) / W;   // m/s
	const float rate = 1.f + 3.f * speed;                                            // rad/s (toned down 2026-09-28)
	for (int k = 0; k < 3; ++k) om[k] = (n > 1e-4f ? ax[k] / n : 0.f) * rate;
	om[0] += ((rand() % 200) - 100) * 0.008f;
	om[1] += ((rand() % 200) - 100) * 0.008f;
	om[2] += ((rand() % 200) - 100) * 0.012f;
}

static float s_handOmega[2][3];   // each hand's rotation rate while it holds a magazine (world rad/s)

// A dropped magazine with rounds in it does not just vanish: where it lies it turns back into the ammo pickup
// it is, part-used ones remembering their rounds (picked up again, they go back into the inventory
// part-used). Empty ones are gone. A fresh one from a pouch never left the inventory until now -- it does
// as it becomes the pickup, so nothing is gained or lost.
static void vr_mag_to_pickup(const vr_falling_mag& m)
{
	if (m.item == NONE || m.rounds == 0 || !VR_PhysicalWeaponsActive()) return;
	const short full = vr_ammo_magazine_size(m.item);
	short rounds = m.rounds;
	if (rounds < 0) {
		if (!vr_take_magazine_from_inventory(current_player_index, m.item)) return;
		rounds = full;
	}
	player_data* player = get_player_data(current_player_index);
	object_data* pobj = get_object_data(get_monster_data(player->monster_index)->object_index);
	world_point2d from = { pobj->location.x, pobj->location.y };
	world_point2d to   = { (world_distance)m.pos[0], (world_distance)m.pos[1] };
	short poly = find_new_object_polygon(&from, &to, pobj->polygon);
	if (poly == NONE) { poly = pobj->polygon; to = from; }
	object_location loc;
	loc.p.x = to.x; loc.p.y = to.y; loc.p.z = 0;     // z is above the polygon's floor
	loc.polygon_index = poly;
	loc.yaw = 0; loc.pitch = 0; loc.flags = 0;
	const short obj = new_item(&loc, m.item);
	if (obj != NONE && full > 0 && rounds < full) VR_RegisterDroppedMag(obj, rounds);
}

static void vr_update_falling_mags(float W)
{
	const uint64_t now = machine_tick_count();
	float dt = s_fallingLastMs ? float(now - s_fallingLastMs) / 1000.f : 0.f;
	s_fallingLastMs = now;
	if (dt > 0.05f) dt = 0.05f;
	auto cross = [](const float a[3], const float b[3], float o[3]) {
		o[0] = a[1]*b[2] - a[2]*b[1]; o[1] = a[2]*b[0] - a[0]*b[2]; o[2] = a[0]*b[1] - a[1]*b[0];
	};
	auto norm = [](float v[3]) {
		const float n = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
		if (n > 1e-6f) { v[0] /= n; v[1] /= n; v[2] /= n; }
	};
	for (auto& m : s_fallingMags) {
		m.age += dt;
		if (m.landed) continue;
		m.vel[2] -= 9.8f * W * dt;                          // world Z is up; W = world units per metre
		for (int k = 0; k < 3; ++k) m.pos[k] += m.vel[k] * dt;
		// A little air drag on the spin, so it settles rather than whirling all the way down.
		{ const float drag = 1.f - 1.5f * dt; for (int k = 0; k < 3; ++k) m.om[k] *= (drag > 0.f ? drag : 0.f); }
		// Spin: rotate the sides by om*dt, then square them up again (keeping the handedness it started
		// with -- room->world is a reflection, so r is not simply u x f).
		const float handed = m.r[0]*(m.u[1]*m.f[2]-m.u[2]*m.f[1]) + m.r[1]*(m.u[2]*m.f[0]-m.u[0]*m.f[2])
		                   + m.r[2]*(m.u[0]*m.f[1]-m.u[1]*m.f[0]);
		float d[3];
		cross(m.om, m.u, d); for (int k = 0; k < 3; ++k) m.u[k] += d[k] * dt;
		cross(m.om, m.f, d); for (int k = 0; k < 3; ++k) m.f[k] += d[k] * dt;
		norm(m.u);
		const float fu = m.f[0]*m.u[0] + m.f[1]*m.u[1] + m.f[2]*m.u[2];
		for (int k = 0; k < 3; ++k) m.f[k] -= m.u[k] * fu;
		norm(m.f);
		cross(m.u, m.f, m.r);
		if (handed < 0.f) for (int k = 0; k < 3; ++k) m.r[k] = -m.r[k];

		// Lowest corner reaching the floor: it lands, lying flat on its broad side, long axis level.
		const vr_mag_dims md = vr_mag_dims_for_ammo(m.item);
		const float ext = (fabsf(m.u[2]) * md.length + fabsf(m.r[2]) * md.width + fabsf(m.f[2]) * md.depth) * 0.5f * W;
		if (m.pos[2] - ext <= m.floorZ + 0.005f * W) {
			float u[3] = { m.u[0], m.u[1], 0.f };
			if (u[0]*u[0] + u[1]*u[1] < 1e-4f) { u[0] = m.f[0]; u[1] = m.f[1]; }
			norm(u);
			const float up[3] = { 0.f, 0.f, 1.f };
			float side[3];
			cross(up, u, side);
			norm(side);
			// the thinner of its two sides goes vertical, so it rests on the broad one
			const bool depthThin = md.depth <= md.width;
			for (int k = 0; k < 3; ++k) {
				m.u[k] = u[k];
				m.f[k] = depthThin ? up[k] : side[k];
				m.r[k] = depthThin ? side[k] : up[k];
			}
			const float thick = depthThin ? md.depth : md.width;
			m.pos[2] = m.floorZ + 0.005f * W + thick * 0.5f * W;
			m.landed = true;
		}
	}
	// A magazine with rounds becomes its ammo pickup only once the player is OUT of pickup reach: the engine
	// takes any item within arm's reach (items.cpp MAXIMUM_ARM_REACH, 3/4 WU ~ 1.5 m), so one dropped at your
	// feet was picked straight back up the instant it turned into a pickup -- it flashed and vanished. Until
	// you move away it stays where it lies as the magazine; walk back over the pickup to take it again.
	const player_data* me = get_player_data(current_player_index);
	const float reach = 3.f * WORLD_ONE / 4.f * 1.15f;
	auto hasRounds = [](const vr_falling_mag& m) { return m.item != NONE && m.rounds != 0; };
	auto playerNear = [&](const vr_falling_mag& m) {
		const float dx = m.pos[0] - me->location.x, dy = m.pos[1] - me->location.y;
		return dx*dx + dy*dy <= reach * reach;
	};
	for (const auto& m : s_fallingMags)
		if (m.age > 3.0f && m.landed && hasRounds(m) && !playerNear(m)) vr_mag_to_pickup(m);
	s_fallingMags.erase(std::remove_if(s_fallingMags.begin(), s_fallingMags.end(),
		[&](const vr_falling_mag& m) {
			if (m.age <= 3.0f) return false;
			if (m.landed && hasRounds(m) && playerNear(m) && VR_PhysicalWeaponsActive()) return false;   // waits
			return true;
		}), s_fallingMags.end());
}

// ---- the magazine's shape ---------------------------------------------------------------------------
// A magazine in the hand is held by its base, sticking up out of the top of the fist, so its TOP -- the end
// that goes into the gun -- is what meets a well, and it can reach up into a pistol grip.
static const float kMagInFistM = 0.03f;     // how much of its base is inside the fist (at most)

static vr_mag_dims vr_mag_dims_of(const vr_attach_point* mw)
{
	vr_mag_dims d;
	d.width  = mw->mag_width  > 0.f ? mw->mag_width  : 0.035f;
	d.length = mw->mag_length > 0.f ? mw->mag_length : 0.13f;
	d.depth  = mw->mag_depth  > 0.f ? mw->mag_depth  : 0.06f;
	d.round  = mw->mag_round;
	d.drum   = mw->mag_drum;
	// Round faces are round: a drum's face (length x width) and a canister's cross-section (width x
	// depth) are always one diameter, whatever the numbers say.
	if (d.drum)       d.width = d.length = 0.5f * (d.width + d.length);
	else if (d.round) d.depth = d.width;
	return d;
}

static vr_mag_dims vr_mag_dims_for_well(short weapon, int which)
{
	return vr_mag_dims_of(which ? VR_GetMagwell2OrDefault(weapon) : VR_GetMagwellOrDefault(weapon));
}

// The size of a magazine of this ammo: the held gun's well for it if it has one, else the first weapon that
// feeds on it.
static vr_mag_dims vr_mag_dims_for_ammo(short ammo)
{
	const int held = VR_GetHeldWeaponHand();
	if (held >= 0 && s_vrWeaponPose[held].valid && s_vrWeaponPose[held].weapon_type != NONE) {
		const short wt = s_vrWeaponPose[held].weapon_type;
		for (int which = 0; which < 2; ++which)
			if (get_player_weapon_ammo_type(current_player_index, wt, which ? _secondary_weapon : _primary_weapon) == ammo)
				return vr_mag_dims_for_well(wt, which);
	}
	for (short wt = 0; wt < short(MAXIMUM_NUMBER_OF_WEAPONS); ++wt)
		for (int which = 0; which < 2; ++which)
			if (get_player_weapon_ammo_type(current_player_index, wt, which ? _secondary_weapon : _primary_weapon) == ammo)
				return vr_mag_dims_for_well(wt, which);
	return vr_mag_dims{ 0.035f, 0.13f, 0.06f, false, false };
}

// The top of a magazine in a hand at pos, pointing along the hand's up.
// How a DRUM sits in the hand -- MEASURED, not guessed (2026-09-28): Daniel pulled the M1 rifle grenade drum
// out of its well twice with his natural loading grip (right hand) and the VRMAGPOSE log gave its axes in the
// hand's own frame (x = hand right, y = hand up, z = hand forward), averaged and squared up here. The left
// hand is the mirror image (x negated). Offset = where the drum's top sat from the controller, for a drum
// kDrumHoldRefM across; smaller drums scale it down.
static const float kDrumTop[3]   = { -0.884f, 0.121f, 0.462f };   // toward its top (the end that goes in)
static const float kDrumRight[3] = { -0.362f, 0.478f, -0.800f };
static const float kDrumFace[3]  = {  0.320f, 0.870f, 0.375f };   // its round face
static const float kDrumTopOffM[3] = { -0.059f, 0.009f, 0.123f };
static const float kDrumHoldRefM = 0.20f;
static const float kDrumPalmGapM = 0.035f;   // palm to the drum's bottom edge, along its axis

// One of the drum-hold vectors for a hand, in hand-local coordinates (mirrored for the left hand).
static void vr_drum_local(const float v[3], int hand, float out[3])
{
	out[0] = (hand == 0) ? -v[0] : v[0];
	out[1] = v[1];
	out[2] = v[2];
}

// Hand-local -> world.
static void vr_hand_local_to_world(const float l[3], const float wr[3], const float wu[3], const float wf[3], float out[3])
{
	for (int k = 0; k < 3; ++k) out[k] = l[0]*wr[k] + l[1]*wu[k] + l[2]*wf[k];
}

// GRIP-RELATIVE HOLD (2026-10-04): a magazine taken OUT OF A GUN stays exactly where it was relative to the
// hand at that moment -- whatever grip took it (the rifle grenades come out palm-up, unlike a box magazine).
// The fixed holds below are only for magazines fresh from a pouch. Hand-local (x = hand right, y = up,
// z = forward); top in metres. Cleared when the hand lets go of the magazine.
static bool  s_holdValid[2] = { false, false };
static float s_holdTop[2][3], s_holdR[2][3], s_holdU[2][3], s_holdF[2][3];
static bool  s_holdPending[2] = { false, false };   // captured next frame, when the hand's axes are known
static float s_holdPendTop[2][3], s_holdPendR[2][3], s_holdPendU[2][3], s_holdPendF[2][3];

// Record the magazine's current world pose (top + sides, u toward its top) as this hand's hold.
static void vr_capture_hold(int hand, float W, const float pos[3], const float wr[3], const float wu[3],
                            const float wf[3], const float top[3], const float r[3], const float u[3], const float f[3])
{
	auto loc = [&](const float v[3], float out[3]) {
		out[0] = v[0]*wr[0] + v[1]*wr[1] + v[2]*wr[2];
		out[1] = v[0]*wu[0] + v[1]*wu[1] + v[2]*wu[2];
		out[2] = v[0]*wf[0] + v[1]*wf[1] + v[2]*wf[2];
	};
	const float d[3] = { (top[0]-pos[0]) / W, (top[1]-pos[1]) / W, (top[2]-pos[2]) / W };
	loc(d, s_holdTop[hand]); loc(r, s_holdR[hand]); loc(u, s_holdU[hand]); loc(f, s_holdF[hand]);
	s_holdValid[hand] = true;
	s_holdPending[hand] = false;
}

// The same, for a moment when the hand's axes are not to hand: done at the start of the next frame.
static void vr_capture_hold_later(int hand, const float top[3], const float r[3], const float u[3], const float f[3])
{
	for (int k = 0; k < 3; ++k) {
		s_holdPendTop[hand][k] = top[k]; s_holdPendR[hand][k] = r[k];
		s_holdPendU[hand][k] = u[k];     s_holdPendF[hand][k] = f[k];
	}
	s_holdPending[hand] = true;
	s_holdValid[hand] = false;
}

// The direction a magazine held in this hand goes in (world): its grip-relative hold if it came out of a gun,
// else the hand's up for a box magazine, the measured hold for a drum.
static void vr_hand_mag_axis(const vr_mag_dims& md, int hand, const float wr[3], const float wu[3],
                             const float wf[3], float out[3])
{
	if (hand >= 0 && hand < 2 && s_holdValid[hand]) { vr_hand_local_to_world(s_holdU[hand], wr, wu, wf, out); return; }
	if (!md.drum) { for (int k = 0; k < 3; ++k) out[k] = wu[k]; return; }
	float l[3];
	vr_drum_local(kDrumTop, hand, l);
	vr_hand_local_to_world(l, wr, wu, wf, out);
}

// The top of a magazine in a hand at pos.
static void vr_hand_mag_top(float W, const vr_mag_dims& md, int hand, const float pos[3], const float wr[3],
                            const float wu[3], const float wf[3], float out[3])
{
	if (hand >= 0 && hand < 2 && s_holdValid[hand]) {
		float w[3];
		vr_hand_local_to_world(s_holdTop[hand], wr, wu, wf, w);
		for (int k = 0; k < 3; ++k) out[k] = pos[k] + w[k] * W;
		return;
	}
	if (md.drum) {
		// Held by its BOTTOM, palm under it (Daniel's palm-up grenade pulls, 2026-10-04: the top sat a drum
		// length + ~3.5 cm from the hand along the drum's axis). Was anchored near its top (kDrumTopOffM),
		// which felt like the drum hanging from the hand.
		float l[3], w[3];
		vr_drum_local(kDrumTop, hand, l);
		vr_hand_local_to_world(l, wr, wu, wf, w);
		for (int k = 0; k < 3; ++k) out[k] = pos[k] + w[k] * (md.length + kDrumPalmGapM) * W;
		return;
	}
	const float inFist = md.length * 0.3f < kMagInFistM ? md.length * 0.3f : kMagInFistM;
	for (int k = 0; k < 3; ++k) out[k] = pos[k] + wu[k] * (md.length - inFist) * W;
}

// ---- the wells ----------------------------------------------------------------------------------
// A weapon has up to two wells: 0 = the main magwell, 1 = the second feed (the rifle's grenade launcher).
// Each is a SEATED point plus the direction a magazine travels going in; with a <vr_magwell travel=...>
// the magazine also has an ENTRY point that far back along that line. Between entry and seat it slides on
// the line -- pushed in, or pulled out -- and is only free of the gun beyond the entry point.
struct vr_well
{
	short weapon;
	int   which;                     // 0 main, 1 second
	float seat[3], entry[3], ins[3]; // world; ins = unit direction INTO the weapon
	float travel;                    // world units between entry and seat (0 = seats instantly)
	float radius;                    // world units
	float tol;                       // approach tolerance, degrees (180 = any)
	float right[3], up[3], fwd[3];   // the weapon's axes
	float magRight[3], magFwd[3];    // the magazine's sides once lined up: square to ins, turned by roll
	vr_mag_dims mag;                 // the size of the magazine this well takes
	int   hand;                      // the hand holding the gun
	int   copy;                      // which gun of a two-fisted pair (0 otherwise): magazine state is per gun
	short trigger;                   // the trigger a magazine here loads: the gun's own for a pair, else -1 (all)
	bool  drops;                     // the released magazine drops out a little (else it stays seated)
};

// The well of the gun in a given hand -- the primary's, the second hand's own weapon, or either of a pair.
// The geometry of a well on a gun at pose wp: seated point, insertion line, path, reach, and the magazine's
// sides. Shared by the live wells and by the magazine meshes (which need the seated pose in the gun's own
// frame to pivot from).
static void vr_fill_well_geometry(const vr_weapon_pose& wp, const vr_attach_point* mw, float W, vr_well& out)
{
	vr_weapon_local_to_world(wp, W, mw->x, mw->y, mw->z, out.seat);
	// Straight up into the weapon from below, swung by the authored angles.
	const float cp  = cosf(mw->pitch * 0.01745329f), sp  = sinf(mw->pitch * 0.01745329f);
	const float cy2 = cosf(mw->yaw   * 0.01745329f), sy2 = sinf(mw->yaw   * 0.01745329f);
	for (int k = 0; k < 3; ++k)
		out.ins[k] = wp.up[k]*cp - wp.fwd[k]*sp*cy2 - wp.right[k]*sp*sy2;
	out.travel = mw->travel * W;
	for (int k = 0; k < 3; ++k) {
		out.entry[k] = out.seat[k] - out.ins[k] * out.travel;
		out.right[k] = wp.right[k]; out.up[k] = wp.up[k]; out.fwd[k] = wp.fwd[k];
	}
	out.radius = mw->radius * W;
	out.tol    = mw->angle_tolerance;
	out.mag    = vr_mag_dims_of(mw);
	out.drops  = mw->drop_on_release;

	// The magazine's sides: the gun's right and forward squared up to the (possibly tilted) insertion line,
	// then turned about it by roll.
	auto dot3 = [](const float* a, const float* b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; };
	auto norm3 = [](float* a) {
		const float l = sqrtf(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
		if (l > 1e-6f) { a[0] /= l; a[1] /= l; a[2] /= l; }
	};
	float r[3], f[3];
	const float ri = dot3(wp.right, out.ins);
	for (int k = 0; k < 3; ++k) r[k] = wp.right[k] - out.ins[k] * ri;
	norm3(r);
	const float fi = dot3(wp.fwd, out.ins), fr = dot3(wp.fwd, r);
	for (int k = 0; k < 3; ++k) f[k] = wp.fwd[k] - out.ins[k] * fi - r[k] * fr;
	norm3(f);
	const float cr = cosf(mw->roll * 0.01745329f), sr = sinf(mw->roll * 0.01745329f);
	for (int k = 0; k < 3; ++k) {
		out.magRight[k] = r[k] * cr + f[k] * sr;
		out.magFwd[k]   = f[k] * cr - r[k] * sr;
	}
}

static bool vr_get_well_hand(float W, int gunHand, int which, vr_well& out)
{
	if (gunHand < 0 || gunHand > 1) return false;
	short w = NONE, copy = 0;
	if (!vr_hand_gun(current_player_index, gunHand, &w, &copy)) return false;
	const vr_weapon_pose& wp = s_vrWeaponPose[gunHand];
	if (!wp.valid || wp.weapon_type != w) return false;
	const vr_attach_point* mw = which ? VR_GetMagwell2OrDefault(wp.weapon_type)
	                                  : VR_GetMagwellOrDefault(wp.weapon_type);
	out.weapon  = wp.weapon_type;
	out.which   = which;
	out.hand    = gunHand;
	out.copy    = copy;
	out.trigger = weapon_is_twofisted(w) ? copy : short(-1);
	vr_fill_well_geometry(wp, mw, W, out);
	return true;
}

// The well a hand would load: the gun in the OTHER hand (excludeHand = the loading hand). With -1, the
// primary hand's gun.
static bool vr_get_well(float W, int excludeHand, int which, vr_well& out)
{
	const int gunHand = (excludeHand >= 0) ? 1 - excludeHand : VR_GetHeldWeaponHand();
	return vr_get_well_hand(W, gunHand, which, out);
}

// Kept for the flick code: the MAIN well's seated point and insertion direction.
static bool vr_primary_magwell(float W, int excludeHand, short* weapon, float well[3], float ins[3],
                               float* radius, float* angleTol, bool secondWell)
{
	vr_well wl;
	if (!vr_get_well(W, excludeHand, secondWell ? 1 : 0, wl)) return false;
	for (int k = 0; k < 3; ++k) { well[k] = wl.seat[k]; ins[k] = wl.ins[k]; }
	if (weapon)   *weapon   = wl.weapon;
	if (radius)   *radius   = wl.radius;
	if (angleTol) *angleTol = wl.tol;
	return true;
}

// Which well a magazine of this ammo belongs in, for this weapon: 0 main, 1 second, -1 neither.
static int vr_well_for_ammo(short weapon, short mag)
{
	if (mag == get_player_weapon_ammo_type(current_player_index, weapon, _primary_weapon)) return 0;
	if (mag == get_player_weapon_ammo_type(current_player_index, weapon, _secondary_weapon)) return 1;
	return -1;
}

// Whether that well will take a magazine right now. Normally the old one has to be OUT first -- released
// and then pulled or shaken free. A direct-load weapon (the rocket launcher) has nothing to take out: it
// takes a new load as soon as it is empty.
static bool vr_magwell_accepts(const vr_well& wl, short mag)
{
	const int which = vr_well_for_ammo(wl.weapon, mag);
	if (which < 0 || which != wl.which) return false;
	if (VR_WeaponDirectLoad(wl.weapon))
		return get_player_weapon_ammo_count(current_player_index, wl.weapon,
		                                    which ? _secondary_weapon : _primary_weapon) <= 0;
	return VR_GetMagState(wl.weapon, which, wl.copy) == VR_MAG_OUT;
}

// Where a loosened magazine hangs: a few centimetres out of the well, back along the insertion path.
// Only where the well says so (<vr_magwell drop_on_release="1">: pistols, the fusion pistol, M1's rifle) does
// the released magazine drop; everywhere else it stays seated where it is until it is pulled or flicked out.
static void vr_loose_mag_pos(float W, const float well[3], const float ins[3], bool drops, float out[3])
{
	const float d = drops ? 0.04f * W : 0.f;
	for (int k = 0; k < 3; ++k) out[k] = well[k] - ins[k] * d;
}

// ---- a magazine sliding on a well's path ------------------------------------------------------------
// Per hand: which well its magazine is engaged with (-1 = none), whether it is going in or coming out, and
// how far along the path it is (0 = entry, 1 = seated).
static int   s_slideWell[2]     = { -1, -1 };
static bool  s_slideRemoving[2] = { false, false };
static float s_slideT[2]        = { 0.f, 0.f };
static float s_slideTMax[2]     = { 1.f, 1.f };   // while pulling out: how far in it started
// The hand drives the slide by its motion ALONG the path relative to where it was when the magazine engaged,
// so however you happen to hold it, it moves with you and never jumps.
static float s_slideAlong0[2]   = { 0.f, 0.f };   // hand's along-path coordinate at t = 0
static float s_slideLat0[2][3]  = { { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f } };   // hand's offset off the line then
static bool  s_slideAuto[2]     = { false, false }; // let go on the path: it runs home on its own
static float s_slideMinT[2]     = { 1.f, 1.f };     // coming out: the furthest out it has been
static bool  s_vrLooseNoDrop[4] = { false, false, false, false }; // per gun hand x well: went loose by a hand letting go mid-pull,
                                                    // so it is not dropped out of the seat again
static const float kAutoSeatSeconds = 0.10f;       // the whole travel, when it runs home
static const float kSlideOutSeconds = 0.12f;       // the quickest a magazine can slide the whole path out

// Leaving the path, the magazine swings over from where it was on the line into its place in the hand
// over kHandoffSeconds, rather than jumping. Stored relative to the hand (its axes), so a moving hand
// carries the swing with it.
static const float kHandoffSeconds = 0.15f;
static float s_handoffT[2] = { 1.f, 1.f };          // 0 = just left the path, 1 = settled in the hand
static float s_handoffTop[2][3], s_handoffDown[2][3], s_handoffRight[2][3], s_handoffFwd[2][3];

static void vr_well_point(const vr_well& wl, float t, float out[3]);

// DIAGNOSTIC (2026-09-28): how a magazine on a well's path sits relative to the hand holding it, in the
// hand's own axes (x = hand right, y = hand up, z = hand forward). Pulling a drum out with a natural grip
// gives the in-hand orientation it should have. Logged as VRMAGPOSE (file log + A1VR).
static const bool kVRMagPoseDiag = false;
static void vr_log_mag_in_hand(const char* why, int hand, const vr_well& wl, float t, const float pos[3],
                               const float wr[3], const float wu[3], const float wf[3])
{
	if (!kVRMagPoseDiag) return;
	auto loc = [&](const float v[3], float out[3]) {
		out[0] = v[0]*wr[0] + v[1]*wr[1] + v[2]*wr[2];
		out[1] = v[0]*wu[0] + v[1]*wu[1] + v[2]*wu[2];
		out[2] = v[0]*wf[0] + v[1]*wf[1] + v[2]*wf[2];
	};
	float top[3], right[3], fwd[3], at[3], off[3];
	loc(wl.ins, top); loc(wl.magRight, right); loc(wl.magFwd, fwd);
	vr_well_point(wl, t, at);
	const float W = (float)VR_Settings()->worldScaleWUM;
	const float d[3] = { (at[0]-pos[0]) / W, (at[1]-pos[1]) / W, (at[2]-pos[2]) / W };
	loc(d, off);
	char line[400];
	snprintf(line, sizeof(line),
	         "VRMAGPOSE %s hand=%d weapon=%d well=%d drum=%d t=%.2f | mag TOP dir (%.2f,%.2f,%.2f) RIGHT (%.2f,%.2f,%.2f) "
	         "FACE/FWD (%.2f,%.2f,%.2f) | mag top from hand (cm) (%.1f,%.1f,%.1f)   [hand axes x=right y=up z=fwd]",
	         why, hand, (int)wl.weapon, wl.which, wl.mag.drum ? 1 : 0, t,
	         top[0], top[1], top[2], right[0], right[1], right[2], fwd[0], fwd[1], fwd[2],
	         off[0] * 100.f, off[1] * 100.f, off[2] * 100.f);
	logWarning("%s", line);
	__android_log_print(ANDROID_LOG_INFO, "A1VR", "%s", line);
}

static void vr_begin_mag_handoff(float W, int hand, const vr_well& wl, const float pos[3],
                                 const float wr[3], const float wu[3], const float wf[3], short mag)
{
	(void)mag;
	vr_log_mag_in_hand("LEAVE-PATH", hand, wl, s_slideT[hand], pos, wr, wu, wf);
	float at[3], handTop[3];
	vr_well_point(wl, s_slideT[hand], at);
	vr_hand_mag_top(W, wl.mag, hand, pos, wr, wu, wf, handTop);
	auto toLocal = [&](const float v[3], float out[3]) {
		out[0] = v[0]*wr[0] + v[1]*wr[1] + v[2]*wr[2];
		out[1] = v[0]*wu[0] + v[1]*wu[1] + v[2]*wu[2];
		out[2] = v[0]*wf[0] + v[1]*wf[1] + v[2]*wf[2];
	};
	const float d[3]    = { at[0]-handTop[0], at[1]-handTop[1], at[2]-handTop[2] };
	const float down[3] = { -wl.ins[0], -wl.ins[1], -wl.ins[2] };
	toLocal(d, s_handoffTop[hand]);
	toLocal(down, s_handoffDown[hand]);
	toLocal(wl.magRight, s_handoffRight[hand]);
	toLocal(wl.magFwd, s_handoffFwd[hand]);
	s_handoffT[hand] = 0.f;
}

// The magazine-in-hand pose, part way through a handoff: top (world) and axes.
static void vr_hand_mag_pose(float W, int hand, const vr_mag_dims& md, const float pos[3], const float wr[3],
                             const float wu[3], const float wf[3], float top[3], float right[3],
                             float down[3], float fwd[3])
{
	vr_hand_mag_top(W, md, hand, pos, wr, wu, wf, top);
	const float s0 = s_handoffT[hand] < 1.f ? s_handoffT[hand] : 1.f;
	const float s  = s0 * s0 * (3.f - 2.f * s0);   // smoothstep
	// How it sits in the hand (hand axes: right, up, forward). A box magazine points along the hand's up; a
	// drum sits the way it was measured (kDrumTop/Right/Face above, mirrored for the left hand).
	const float boxR[3] = { 1.f, 0.f, 0.f }, boxD[3] = { 0.f, -1.f, 0.f }, boxF[3] = { 0.f, 0.f, 1.f };
	float drumR[3], drumD[3], drumF[3], drumT[3];
	vr_drum_local(kDrumRight, hand, drumR);
	vr_drum_local(kDrumFace, hand, drumF);
	vr_drum_local(kDrumTop, hand, drumT);
	for (int k = 0; k < 3; ++k) drumD[k] = -drumT[k];
	const float* idR = md.drum ? drumR : boxR;
	const float* idD = md.drum ? drumD : boxD;
	const float* idF = md.drum ? drumF : boxF;
	float holdD[3];
	if (s_holdValid[hand]) {                             // taken out of a gun: exactly as it was gripped
		for (int k = 0; k < 3; ++k) holdD[k] = -s_holdU[hand][k];
		idR = s_holdR[hand]; idD = holdD; idF = s_holdF[hand];
	}
	auto toWorld = [&](const float l[3], float out[3]) {
		for (int k = 0; k < 3; ++k) out[k] = l[0]*wr[k] + l[1]*wu[k] + l[2]*wf[k];
	};
	auto blendAxis = [&](const float from[3], const float to[3], float out[3]) {
		float l[3];
		for (int k = 0; k < 3; ++k) l[k] = from[k] * (1.f - s) + to[k] * s;
		const float n = sqrtf(l[0]*l[0] + l[1]*l[1] + l[2]*l[2]);
		if (n > 1e-4f) for (int k = 0; k < 3; ++k) l[k] /= n;
		toWorld(l, out);
	};
	if (s >= 1.f) {
		toWorld(idR, right);
		toWorld(idD, down);
		toWorld(idF, fwd);
		return;
	}
	float off[3];
	const float l[3] = { s_handoffTop[hand][0] * (1.f - s), s_handoffTop[hand][1] * (1.f - s),
	                     s_handoffTop[hand][2] * (1.f - s) };
	toWorld(l, off);
	for (int k = 0; k < 3; ++k) top[k] += off[k];
	blendAxis(s_handoffRight[hand], idR, right);
	blendAxis(s_handoffDown[hand],  idD, down);
	blendAxis(s_handoffFwd[hand],   idF, fwd);
}

// A point on the path, t in [0,1].
static void vr_well_point(const vr_well& wl, float t, float out[3])
{
	for (int k = 0; k < 3; ++k) out[k] = wl.entry[k] + wl.ins[k] * wl.travel * t;
}

// Going onto the path, the magazine settles from where it was last drawn in the hand into line with the
// well over kEngageSettleMs (render only), so the snap reads as a quick motion rather than a jump.
static const float kEngageSettleMs = 80.f;
static bool     s_engageSettle[2] = { false, false };
static uint64_t s_engageStartMs[2] = { 0, 0 };
static float    s_magDrawn[2][4][3];               // last free-in-hand draw: top, right, down, fwd (world)
static uint32_t s_magDrawnFrame[2] = { 0, 0 };

// Engage a hand's magazine on a well's path at t, from the hand at pos.
static void vr_slide_engage(int hand, const vr_well& wl, const float pos[3], float t, bool removing)
{
	if (!removing && s_vrFrameNo - s_magDrawnFrame[hand] <= 2) {
		s_engageSettle[hand]  = true;
		s_engageStartMs[hand] = machine_tick_count();
	}
	const float rel[3] = { pos[0]-wl.entry[0], pos[1]-wl.entry[1], pos[2]-wl.entry[2] };
	const float along  = rel[0]*wl.ins[0] + rel[1]*wl.ins[1] + rel[2]*wl.ins[2];
	s_slideWell[hand]     = wl.which;
	s_slideRemoving[hand] = removing;
	s_slideAuto[hand]     = false;
	s_slideT[hand]        = s_slideTMax[hand] = t;
	s_slideMinT[hand]     = t;
	s_slideAlong0[hand]   = along - t * wl.travel;
	for (int k = 0; k < 3; ++k) s_slideLat0[hand][k] = rel[k] - wl.ins[k] * along;
}

// Whether the magazine in a hand is touching this well's mouth -- its TOP at the capture point (the entry of
// the path, else the seat), roughly pointing the right way. On contact it is snapped into line, so the
// angle only has to be in the ballpark. The ring turns green exactly when this is true.
static bool vr_mag_touches_well(const vr_well& wl, float W, int hand, const float pos[3], const float wr[3],
                                const float wu[3], const float wf[3],
                                float magTop[3], float* distSq)
{
	vr_hand_mag_top(W, wl.mag, hand, pos, wr, wu, wf, magTop);
	const float* cap = (wl.travel > 1e-3f) ? wl.entry : wl.seat;
	const float dx = magTop[0]-cap[0], dy = magTop[1]-cap[1], dz = magTop[2]-cap[2];
	const float d2 = dx*dx + dy*dy + dz*dz;
	if (distSq) *distSq = d2;
	// It has to actually reach the mouth: within kEngageReachM (or the well's own radius if smaller). The
	// radius was the engage distance too, and a generous one (the rifle grenades' 10 cm) read as the
	// magazine being snatched out of the hand from afar (2026-10-04). The ring still shows the radius as
	// the "bring it here" guide; it turns green only on contact.
	const float kEngageReachM = 0.04f;
	const float reach = (wl.radius < kEngageReachM * W) ? wl.radius : kEngageReachM * W;
	if (d2 > reach * reach) return false;
	// Roughly the right way round (it snaps into line from there): at least 70 deg of give, and never more
	// than 100 even on a well that takes "any angle" -- a magazine swung round from upside down also read
	// as being grabbed away.
	float tol = wl.tol > 70.f ? wl.tol : 70.f;
	if (tol > 100.f) tol = 100.f;
	float ax[3];
	vr_hand_mag_axis(wl.mag, hand, wr, wu, wf, ax);   // the magazine's insertion axis in the hand
	if (ax[0]*wl.ins[0] + ax[1]*wl.ins[1] + ax[2]*wl.ins[2] < cosf(tol * 0.01745329f)) return false;
	return true;
}

// The hand's magazine is home in the well: load it.
static void vr_seat_hand_mag(int hand, const vr_well& wl, short mag)
{
	const short magRounds = VR_GetHandMagazineRounds(hand);
	VR_SetHandMagazine(hand, NONE);
	s_slideWell[hand] = -1;
	s_slideAuto[hand] = false;
	VR_RequestManualReload(wl.weapon, mag, magRounds, wl.trigger);   // loaded + reload sound, next tick
	if (!VR_WeaponDirectLoad(wl.weapon)) VR_SetMagState(wl.weapon, VR_MAG_IN, wl.which, wl.copy);
	VR_Vibrate(hand, 60, 0.9f);
	VR_Vibrate(1 - hand, 45, 0.7f);
}

// Grip rising edge: take a LOOSENED magazine out of whichever well is loose and nearest. With a path the
// magazine engages it and has to be slid out; without one it comes straight into the hand.
// The loosened magazine nearest a hand at (hx,hy,hz) and within reach of it -- measured to the middle of the
// magazine's body, which is where you take hold of it -- or -1. `slack` widens the reach.
static int vr_nearest_loose_well(float W, int hand, float hx, float hy, float hz, float slack, vr_well& best)
{
	int bestWell = -1; float bestD2 = 0.f;
	for (int which = 0; which < 2; ++which) {
		vr_well wl;
		if (!vr_get_well(W, hand, which, wl)) continue;
		if (VR_GetMagState(wl.weapon, which, wl.copy) != VR_MAG_LOOSE) continue;
		float mp[3];
		vr_loose_mag_pos(W, wl.seat, wl.ins, wl.drops, mp);
		for (int k = 0; k < 3; ++k) mp[k] -= wl.ins[k] * wl.mag.length * 0.5f * W;
		const float dx = hx-mp[0], dy = hy-mp[1], dz = hz-mp[2];
		const float d2 = dx*dx + dy*dy + dz*dz;
		float rr = (wl.radius > 0.10f * W ? wl.radius : 0.10f * W);
		if (rr < wl.mag.length * 0.6f * W) rr = wl.mag.length * 0.6f * W;
		rr *= slack;
		if (d2 > rr * rr) continue;
		if (bestWell < 0 || d2 < bestD2) { bestWell = which; bestD2 = d2; best = wl; }
	}
	return bestWell;
}

// Whether a hand is close enough to a loosened magazine that gripping should take the magazine (with some
// slack, so the steadying grip never wins a near-tie).
static bool vr_hand_near_loose_mag(float W, int hand, const float pos[3])
{
	vr_well wl;
	return vr_nearest_loose_well(W, hand, pos[0], pos[1], pos[2], 1.25f, wl) >= 0;
}

static bool vr_try_pull_loose_mag(float W, int hand, float hx, float hy, float hz)
{
	vr_well best;
	const int bestWell = vr_nearest_loose_well(W, hand, hx, hy, hz, 1.0f, best);
	if (bestWell < 0) return false;

	const short ammo = get_player_weapon_ammo_type(current_player_index, best.weapon,
	                                               bestWell ? _secondary_weapon : _primary_weapon);
	VR_SetHandMagazine(hand, ammo);
	VR_SetHandMagazineRounds(hand, VR_GetLooseMagRounds(best.weapon, bestWell, best.copy));   // it keeps its rounds
	VR_SetMagState(best.weapon, VR_MAG_OUT, bestWell, best.copy);
	if (best.travel > 1e-3f) {
		// Engage the path where the loose magazine is sitting, and slide it out from there.
		float mp[3];
		vr_loose_mag_pos(W, best.seat, best.ins, best.drops, mp);
		float t = ((mp[0]-best.entry[0])*best.ins[0] + (mp[1]-best.entry[1])*best.ins[1] +
		           (mp[2]-best.entry[2])*best.ins[2]) / best.travel;
		t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
		const float hp[3] = { hx, hy, hz };
		vr_slide_engage(hand, best, hp, t, true);
	} else {
		s_vrMagMustClear[hand] = true;   // your hand is in the well: don't re-seat it on the spot
		VR_PlayMagSound(best.weapon, bestWell, VR_MAG_SOUND_REMOVE);
		{   // it stays where it was, relative to the grip that took it
			float mp[3];
			vr_loose_mag_pos(W, best.seat, best.ins, best.drops, mp);
			vr_capture_hold_later(hand, mp, best.magRight, best.ins, best.magFwd);
		}
	}
	VR_Vibrate(hand, 40, 0.6f);
	return true;
}

// Once per frame: seat magazines, slide them along wells, drop released ones, shake loose ones free.
static void vr_update_magazine_seating(view_data* view)
{
	if (!VR_PhysicalWeaponsActive() || VR_GetHolsterSetupMode()) return;
	vr_body_frame bf;
	vr_get_body_frame(view, bf);
	const float W = bf.W;
	vr_update_falling_mags(W);

	// A loose MAIN magazine comes free with a flick of the gun hand: a snap in the direction the magazine
	// comes OUT (the mag keeps going when the gun stops), or a sharp roll of the wrist around the barrel,
	// John Wick style. The rounds go with it -- it is on the floor. The second well never flicks out.
	{
		// Each gun hand on its own (a pair of magnums, or two different weapons, each shake their own).
		static float    s_prevUp[2][3];
		static bool     s_prevUpValid[2] = { false, false };
		static uint64_t s_prevUpMs[2] = { 0, 0 };
		const uint64_t now = machine_tick_count();
		for (int held = 0; held < 2; ++held) {
			// Roll rate of the gun hand around its own pointing direction, degrees per second.
			float rollRate = 0.f;
			float ps[3], f[3], r[3], u[3];
			vr_well wl;
			const bool haveGun = vr_get_well_hand(W, held, 0, wl);
			if (haveGun && VR_GetAimPoseStage(held, ps, f) && VR_GetAimOrientStage(held, r, u)) {
				if (s_prevUpValid[held] && now > s_prevUpMs[held]) {
					const float* p = s_prevUp[held];
					const float cx = p[1]*u[2] - p[2]*u[1], cy = p[2]*u[0] - p[0]*u[2], cz = p[0]*u[1] - p[1]*u[0];
					const float deg = atan2f(cx*f[0] + cy*f[1] + cz*f[2], p[0]*u[0] + p[1]*u[1] + p[2]*u[2]) * 57.2957795f;
					rollRate = deg / (float(now - s_prevUpMs[held]) / 1000.f);
				}
				s_prevUp[held][0] = u[0]; s_prevUp[held][1] = u[1]; s_prevUp[held][2] = u[2];
				s_prevUpValid[held] = true;
				s_prevUpMs[held] = now;
			} else {
				s_prevUpValid[held] = false;
			}

			// The gun hand's acceleration, from its velocity ~30 ms apart (m/s^2, world axes). A shake is a
			// JOLT: the gun stops or reverses and the loose magazine keeps going -- raise it and snap it down,
			// and it is the sudden stop that throws the magazine out, not the speed on the way.
			static float    s_shakeV[2][3];
			static uint64_t s_shakeVMs[2] = { 0, 0 };
			static float    s_shakeA[2][3];
			float hv[3], wv[3] = { 0.f, 0.f, 0.f };
			const bool haveV = haveGun && VR_GetHandVelocityStage(held, hv);
			if (haveV) {
				vr_room_dir_to_world(bf, hv, wv);            // m/s (a rotation/reflection keeps magnitudes)
				const float sdt = s_shakeVMs[held] ? float(now - s_shakeVMs[held]) / 1000.f : 0.f;
				if (s_shakeVMs[held] == 0 || sdt > 0.2f) {
					for (int k = 0; k < 3; ++k) { s_shakeV[held][k] = wv[k]; s_shakeA[held][k] = 0.f; }
					s_shakeVMs[held] = now;
				} else if (sdt >= 0.03f) {
					for (int k = 0; k < 3; ++k) {
						s_shakeA[held][k] = (wv[k] - s_shakeV[held][k]) / sdt;
						s_shakeV[held][k] = wv[k];
					}
					s_shakeVMs[held] = now;
				}
			} else {
				s_shakeVMs[held] = 0;
			}

			if (haveGun && VR_GetMagState(wl.weapon, 0, wl.copy) == VR_MAG_LOOSE) {
				const float outSpeed = -(wv[0]*wl.ins[0] + wv[1]*wl.ins[1] + wv[2]*wl.ins[2]);   // m/s, out of the well
				const float* a = s_shakeA[held];
				const float aAxis = fabsf(a[0]*wl.ins[0] + a[1]*wl.ins[1] + a[2]*wl.ins[2]);    // along the magazine
				const float aAll  = sqrtf(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
				// Out it comes on: a jolt along the magazine (kShakeAxis), any really violent jolt
				// (kShakeAny), a hard snap in the direction it comes out, or a hard wrist roll.
				const float kShakeAxis = 35.f, kShakeAny = 55.f;                                 // m/s^2
				if (aAxis > kShakeAxis || aAll > kShakeAny || outSpeed > 2.4f || fabsf(rollRate) > 900.f) {
					float p[3], v[3];
					vr_loose_mag_pos(W, wl.seat, wl.ins, wl.drops, p);
					for (int k = 0; k < 3; ++k) v[k] = (wv[k] - wl.ins[k] * 1.0f) * W;   // carried off by the flick
					// A wrist roll slings it off sideways: tangential velocity omega x r about the barrel axis.
					// Room->world is a reflection, and angular velocity is a pseudovector, so the axis flips
					// sign on the way across.
					float hpos[3], hr[3], hu[3], hf[3];
					if (vr_hand_world_pose(bf, held, hpos, hr, hu, hf)) {
						const float wz = -rollRate * 0.01745329f;
						const float om[3] = { hf[0]*wz, hf[1]*wz, hf[2]*wz };
						const float rr[3] = { p[0]-hpos[0], p[1]-hpos[1], p[2]-hpos[2] };
						v[0] += om[1]*rr[2] - om[2]*rr[1];
						v[1] += om[2]*rr[0] - om[0]*rr[2];
						v[2] += om[0]*rr[1] - om[1]*rr[0];
					}
					// Spin: the wrist roll that slung it (about the barrel), plus an end-over-end tumble.
					float om[3];
					vr_toss_spin(W, v, om);
					if (vr_hand_world_pose(bf, held, hpos, hr, hu, hf)) {
						const float wz = -rollRate * 0.01745329f * 0.4f;   // only part of the wrist roll carries over
						for (int k = 0; k < 3; ++k) om[k] += hf[k] * wz;
					}
					const float mu[3] = { wl.ins[0], wl.ins[1], wl.ins[2] };
					vr_spawn_falling_mag(p, v, get_player_weapon_ammo_type(current_player_index, wl.weapon, _primary_weapon),
					                     wl.magRight, mu, wl.magFwd, om,
					                     VR_GetLooseMagRounds(wl.weapon, 0, wl.copy), held);
					VR_SetMagState(wl.weapon, VR_MAG_OUT, 0, wl.copy);
					VR_PlayMagSound(wl.weapon, 0, VR_MAG_SOUND_REMOVE);
					VR_Vibrate(held, 45, 0.7f);
				}
			}
		}
	}

	// Frame time, for a magazine running home on its own.
	static uint64_t s_lastSeatMs = 0;
	const uint64_t nowMs = machine_tick_count();
	float dt = s_lastSeatMs ? float(nowMs - s_lastSeatMs) / 1000.f : 0.f;
	if (dt < 0.f || dt > 0.05f) dt = 0.05f;
	s_lastSeatMs = nowMs;

	for (int hand = 0; hand < 2; ++hand) {
		const short mag = VR_GetHandMagazine(hand);
		if (mag == NONE) {
			s_slideWell[hand] = -1; s_slideAuto[hand] = false; s_handoffT[hand] = 1.f;
			s_holdValid[hand] = s_holdPending[hand] = false;   // the next magazine starts in the default hold
			continue;
		}
		if (s_handoffT[hand] < 1.f) s_handoffT[hand] += dt / kHandoffSeconds;

		float pos[3], wr[3], wu[3], wf[3];
		if (!vr_hand_world_pose(bf, hand, pos, wr, wu, wf)) continue;
		if (s_holdPending[hand])
			vr_capture_hold(hand, W, pos, wr, wu, wf, s_holdPendTop[hand], s_holdPendR[hand],
			                s_holdPendU[hand], s_holdPendF[hand]);

		// The hand's actual rotation rate (world rad/s), from how its axes turned since last frame -- so a
		// thrown magazine carries the spin the wrist gave it. omega ~= sum(v_prev x v_now) / 2dt; smoothed a
		// little so one noisy frame does not decide the throw.
		{
			static float    s_prevAx[2][3][3];
			static uint64_t s_prevAxMs[2] = { 0, 0 };
			const float* ax[3] = { wr, wu, wf };
			const float hdt = s_prevAxMs[hand] ? float(nowMs - s_prevAxMs[hand]) / 1000.f : 0.f;
			if (hdt > 0.f && hdt < 0.1f) {
				float om[3] = { 0.f, 0.f, 0.f };
				for (int i = 0; i < 3; ++i) {
					const float* a = s_prevAx[hand][i]; const float* b = ax[i];
					om[0] += a[1]*b[2] - a[2]*b[1];
					om[1] += a[2]*b[0] - a[0]*b[2];
					om[2] += a[0]*b[1] - a[1]*b[0];
				}
				for (int k = 0; k < 3; ++k) s_handOmega[hand][k] += 0.5f * (om[k] / (2.f * hdt) - s_handOmega[hand][k]);
			} else {
				for (int k = 0; k < 3; ++k) s_handOmega[hand][k] = 0.f;
			}
			for (int i = 0; i < 3; ++i) for (int k = 0; k < 3; ++k) s_prevAx[hand][i][k] = ax[i][k];
			s_prevAxMs[hand] = nowMs;
		}
		float grip = 0.f;
		VR_GetRawHandInput(hand, nullptr, nullptr, nullptr, &grip, nullptr);

		// ---- engaged on a well's path ----------------------------------------------------------
		// Once the magazine's top meets the mouth of the well it is snapped into line with it and rides the
		// path: moving the hand along the insertion direction slides it in (or, coming out, back out).
		if (s_slideWell[hand] >= 0) {
			vr_well wl;
			if (!vr_get_well(W, hand, s_slideWell[hand], wl) || wl.travel <= 1e-3f) {
				s_slideWell[hand] = -1;
				s_slideAuto[hand] = false;
				continue;
			}
			// Let go on the way in: it runs the rest of the way home by itself, quickly.
			if (s_slideAuto[hand]) {
				s_slideT[hand] += dt / kAutoSeatSeconds;
				if (s_slideT[hand] >= 1.f) vr_seat_hand_mag(hand, wl, mag);
				continue;
			}
			if (grip <= 0.5f) {
				if (s_slideRemoving[hand]) {
					// Let go while pulling it out: it stays hanging loose in the gun.
					VR_SetMagState(wl.weapon, VR_MAG_LOOSE, wl.which, wl.copy);
					VR_SetLooseMagRounds(wl.weapon, VR_GetHandMagazineRounds(hand), wl.which, wl.copy);
					s_vrLooseNoDrop[wl.hand * 2 + wl.which] = true;
					VR_SetHandMagazine(hand, NONE);
					s_slideWell[hand] = -1;
				} else {
					s_slideAuto[hand] = true;
				}
				continue;
			}
			const float rel[3] = { pos[0]-wl.entry[0], pos[1]-wl.entry[1], pos[2]-wl.entry[2] };
			const float along  = rel[0]*wl.ins[0] + rel[1]*wl.ins[1] + rel[2]*wl.ins[2];
			float t = (along - s_slideAlong0[hand]) / wl.travel;
			if (s_slideRemoving[hand]) {
				// Coming out: follows the hand back along the path, and is free only once its top clears the
				// entry point. It never skips the slide: however fast or far the hand goes, the magazine runs
				// out along the line no faster than kSlideOutSeconds for the whole travel, and only then
				// swings over into the hand (vr_begin_mag_handoff) instead of jumping there.
				{
					static uint64_t s_poseLogMs = 0;
					if (nowMs - s_poseLogMs > 150) {
						s_poseLogMs = nowMs;
						vr_log_mag_in_hand("SLIDING-OUT", hand, wl, s_slideT[hand], pos, wr, wu, wf);
					}
				}
				// Pushed back in instead, it slides home and seats like any magazine -- on a short slide a
				// hair of jitter turns an insertion into a removal, and capping it here left it stuck part-way.
				// (Only once it has actually come part-way out: a magazine released without dropping starts
				// its pull-out fully seated, and would otherwise click straight back in.)
				if (t < s_slideMinT[hand]) s_slideMinT[hand] = t;
				if (t >= 0.97f && s_slideMinT[hand] < 0.85f) { vr_seat_hand_mag(hand, wl, mag); continue; }
				if (t > 1.f) t = 1.f;
				const float slowest = s_slideT[hand] - dt / kSlideOutSeconds;
				if (t < slowest) t = slowest;
				s_slideT[hand] = t < 0.f ? 0.f : t;
				if (t <= 0.f) {
					// Off the end of the path: from now on it stays exactly as this grip holds it.
					{
						float at[3];
						vr_well_point(wl, 0.f, at);
						vr_capture_hold(hand, W, pos, wr, wu, wf, at, wl.magRight, wl.ins, wl.magFwd);
					}
					vr_begin_mag_handoff(W, hand, wl, pos, wr, wu, wf, mag);
					VR_PlayMagSound(wl.weapon, wl.which, VR_MAG_SOUND_REMOVE);
					s_slideWell[hand] = -1;
					s_vrMagMustClear[hand] = true;       // just left the entry: don't grab it straight back
					VR_Vibrate(hand, 30, 0.5f);
				}
			} else {
				// Going in: slide it home. Once any of it is inside, it can only come back out along the path
				// -- pulled back, it turns into a removal and slides out (never skipping the slide). Only
				// while it is still at the mouth can it be pulled away or dragged off the line.
				if (t < 0.f && s_slideT[hand] > 0.02f) {
					s_slideRemoving[hand] = true;
					s_slideTMax[hand] = s_slideT[hand];
					continue;
				}
				float latD2 = 0.f;
				for (int k = 0; k < 3; ++k) {
					const float d = rel[k] - wl.ins[k] * along - s_slideLat0[hand][k];
					latD2 += d * d;
				}
				const float latMax = 0.10f * W;
				// Pulled back out of the mouth: an absolute distance, not a share of the slide -- on a short
				// slide (the rifle's 2 cm) 40% was 8 mm, and ordinary hand jitter knocked it off the path.
				const float backOut = t * wl.travel;                       // world units, negative = outside
				const float backMax = (0.4f * wl.travel > 0.04f * W) ? 0.4f * wl.travel : 0.04f * W;
				if (backOut < -backMax || latD2 > latMax * latMax) {
					vr_begin_mag_handoff(W, hand, wl, pos, wr, wu, wf, mag);
					s_slideWell[hand] = -1;
					continue;
				}
				s_slideT[hand] = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				if (t >= 0.97f) vr_seat_hand_mag(hand, wl, mag);
			}
			continue;
		}

		// ---- free in the hand ---------------------------------------------------------------------
		// Is its top touching the mouth of a well that will take it? (the green ring)
		vr_well wl;
		bool touching = false;
		float magTop[3], d2 = 0.f;
		const int gunHand = 1 - hand;           // the gun in the other hand, whichever it is
		const int which = s_vrWeaponPose[gunHand].valid
		                  ? vr_well_for_ammo(s_vrWeaponPose[gunHand].weapon_type, mag) : -1;
		if (which >= 0 && vr_get_well(W, hand, which, wl) && vr_magwell_accepts(wl, mag)) {
			touching = vr_mag_touches_well(wl, W, hand, pos, wr, wu, wf, magTop, &d2);
			// A magazine just taken out of this well starts next to it; take it clear first, or it would go
			// straight back in on the next frame.
			if (s_vrMagMustClear[hand]) {
				if (d2 > (wl.radius * 1.1f) * (wl.radius * 1.1f)) s_vrMagMustClear[hand] = false;   // just out of reach
				touching = false;
			}
		}

		if (grip <= 0.5f) {
			if (touching) {
				// Let go right at the well: it takes it, running the path home quickly.
				if (wl.travel > 1e-3f) { vr_slide_engage(hand, wl, pos, 0.f, false); s_slideAuto[hand] = true; }
				else                     vr_seat_hand_mag(hand, wl, mag);
				continue;
			}
			// Let go at a pouch that takes this ammunition: it goes back in (a part-used one keeps its rounds;
			// an empty one is simply gone). A fresh one from a pouch never left it.
			{
				int back = -1;
				for (int slot = 0; slot < VR_HOLSTER_COUNT && back < 0; ++slot) {
					if (!VR_HolsterSlotIsPouch(slot) || vr_pouch_ammo_type(slot, hand) != mag) continue;
					vr_holster_anchor pa;
					VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &pa);
					float c[3]; vr_holster_world_pos(view, slot, c);
					const float dx = pos[0]-c[0], dy = pos[1]-c[1], dz = pos[2]-c[2];
					const float r = pa.radius * 1.25f * W;
					if (dx*dx + dy*dy + dz*dz <= r*r) back = slot;
				}
				const short rounds = VR_GetHandMagazineRounds(hand);
				bool stowed = false;
				if (back >= 0) {
					if (rounds <= 0) stowed = true;                       // fresh: never left / empty: gone
					else if (vr_add_magazine_to_inventory(current_player_index, mag)) {
						const short full = vr_ammo_magazine_size(mag);
						if (rounds < full) VR_PushPartialMag(mag, rounds);
						stowed = true;
					}                                                     // inventory full: it drops instead
				}
				if (stowed) {
					VR_SetHandMagazine(hand, NONE);
					s_vrMagMustClear[hand] = false;
					VR_Vibrate(hand, 30, 0.5f);
					continue;
				}
			}
			// Letting go anywhere else throws it, with the hand's velocity, so a toss carries.
			float hv[3], wv[3] = { 0.f, 0.f, 0.f };
			if (VR_GetHandVelocityStage(hand, hv)) {
				vr_room_dir_to_world(bf, hv, wv);
				for (int k = 0; k < 3; ++k) wv[k] *= W;
			}
			{
				const vr_mag_dims md = vr_mag_dims_for_ammo(mag);
				float top[3], mr[3], mdn[3], mf[3], om[3];
				vr_hand_mag_pose(W, hand, md, pos, wr, wu, wf, top, mr, mdn, mf);
				const float mu[3] = { -mdn[0], -mdn[1], -mdn[2] };
				// A throw spins the way the hand was turning as it let go, plus a little toss tumble.
				vr_toss_spin(W, wv, om);
				for (int k = 0; k < 3; ++k) om[k] = s_handOmega[hand][k] + 0.5f * om[k];
				vr_spawn_falling_mag(top, wv, mag, mr, mu, mf, om, VR_GetHandMagazineRounds(hand), 1 - hand);
			}
			VR_SetHandMagazine(hand, NONE);
			s_vrMagMustClear[hand] = false;
			continue;
		}
		if (!touching) continue;

		if (wl.travel > 1e-3f) {
			// Onto the path, snapped into line: from here it slides in as you push.
			vr_slide_engage(hand, wl, pos, 0.f, false);
			VR_Vibrate(hand, 25, 0.5f);
			continue;
		}
		vr_seat_hand_mag(hand, wl, mag);
	}
}

// ---- VR physical weapons: flick reload (shotgun) --------------------------------------------------
// The shotgun reloads with an UPWARD WRIST FLICK -- the snap that cocks a lever gun one-handed: the muzzle
// whips up fast. Measured as the hand's pitch rate about its own right axis (muzzle-up positive; the
// controller's right = forward x up, so a positive turn about it carries forward toward up). A continuous
// upward swing faster than kFlickRate that covers kFlickDegrees reloads. Raising the gun to aim is never
// that fast; slowing, stopping or swinging down starts over. (MML still calls it <vr_spin_reload>.)
static const float kFlickRate    = 250.f;   // deg/s the muzzle must be rising at
static const float kFlickDegrees = 35.f;    // how far it must rise at that speed
static float    s_flickAccum[2]      = { 0.f, 0.f };
static float    s_flickPrevFwd[2][3];
static bool     s_flickPrevValid[2]  = { false, false };
static uint64_t s_flickPrevMs[2]     = { 0, 0 };
static uint64_t s_flickCooldownMs[2] = { 0, 0 };

static void vr_update_spin_reload(void)
{
	if (!VR_PhysicalWeaponsActive()) return;
	vr_hands_state hs;
	vr_get_hands_state(hs);
	const uint64_t now = machine_tick_count();

	for (int hand = 0; hand < 2; ++hand) {
		short w = NONE;
		if (hs.heldHand >= 0) {
			if (hand == hs.heldHand)          w = hs.primary;
			else if (hs.offhand != NONE)      w = hs.offhand;
			else if (hs.primaryCount >= 2)    w = hs.primary;     // a dual pair: both hands hold one
		}
		float ps[3], f[3], r[3], u[3];
		if (w == NONE || !VR_WeaponUsesSpinReload(w) ||
		    !VR_GetAimPoseStage(hand, ps, f) || !VR_GetAimOrientStage(hand, r, u)) {
			s_flickPrevValid[hand] = false;
			s_flickAccum[hand] = 0.f;
			continue;
		}

		if (s_flickPrevValid[hand] && now > s_flickPrevMs[hand]) {
			const float* p = s_flickPrevFwd[hand];
			const float cx = p[1]*f[2] - p[2]*f[1], cy = p[2]*f[0] - p[0]*f[2], cz = p[0]*f[1] - p[1]*f[0];
			const float dDeg = atan2f(cx*r[0] + cy*r[1] + cz*r[2], p[0]*f[0] + p[1]*f[1] + p[2]*f[2]) * 57.2957795f;
			const float rate = dDeg / (float(now - s_flickPrevMs[hand]) / 1000.f);
			if (rate > kFlickRate) s_flickAccum[hand] += dDeg;
			else                   s_flickAccum[hand] = 0.f;
			if (s_flickAccum[hand] >= kFlickDegrees && now >= s_flickCooldownMs[hand]) {
				// A dual pair is one weapon with a trigger per hand: trigger 0 is the gun in the primary hand,
				// trigger 1 the other. Name this hand's, so only the flicked shotgun reloads and animates.
				const short trig = (hs.primaryCount >= 2) ? short(hand == hs.heldHand ? _primary_weapon : _secondary_weapon)
				                                          : short(-1);
				VR_RequestManualReload(w, get_player_weapon_ammo_type(current_player_index, w, _primary_weapon), -1, trig);
				if (kVRReloadDiag) logWarning("VRRELOAD flick hand=%d weapon=%d trigger=%d (primaryHand=%d offhandWeapon=%d)",
				           hand, (int)w, (int)trig, hs.heldHand, (int)hs.offhand);
				VR_Vibrate(hand, 70, 1.0f);
				s_flickAccum[hand] = 0.f;
				s_flickCooldownMs[hand] = now + 400;
			}
		}
		s_flickPrevFwd[hand][0] = f[0]; s_flickPrevFwd[hand][1] = f[1]; s_flickPrevFwd[hand][2] = f[2];
		s_flickPrevValid[hand] = true;
		s_flickPrevMs[hand] = now;
	}
}

// ---- VR physical weapons: hands, ammo and reload cues ------------------------------------------
// Everything the player needs to SEE to use the physical weapons without the debug gizmos:
//   * a fist on each hand, always -- including a hand that is holding a gun
//   * the magazine that each ammo pouch will hand out, drawn in the pouch; an empty pouch shows nothing
//   * the magazine in your hand once you have taken one
//   * a target ring at the magwell while you carry a magazine, green once you are close enough to seat

// Which hands drew a fist through the engine's own weapon display this frame (empty-handed, the fist
// IS the current weapon); render_vr_hand_visuals only adds fists to the hands that did not.
static bool s_vrHandDrewFist[2] = { false, false };

// One shape -- a weapon's idle frame, a magazine pickup -- at a world position and basis: its 3D model
// when a plugin provides one, else its sprite as a quad in the right/up plane. weaponSized uses the
// in-hand weapon sprite scale; otherwise the sprite is drawn at its true world size.
static void vr_draw_shape_at(view_data* view, short coll, short seq, short lowShape,
                             const float pos[3], const float wr[3], const float wu[3], const float wf[3],
                             bool flipH, bool weaponSized, float sizeScale, float shadeScale = 1.0f)
{
	shape_information_data* si = extended_get_shape_information(coll, lowShape);
	bitmap_definition* bmp = nullptr; void* shade = nullptr;
	extended_get_shape_bitmap_and_shading_table(coll, lowShape, &bmp, &shade, view->shading_mode);
	if (!si || !bmp) return;

	rectangle_definition rect;
	rect.ModelPtr        = nullptr;
	rect.Opacity         = 1;
	rect.ShapeDesc       = BUILD_DESCRIPTOR(coll, 0);
	rect.LowLevelShape   = lowShape;
	rect.texture         = bmp;
	rect.shading_tables  = shade;
	rect.transfer_mode   = _textured_transfer;
	rect.transfer_data   = 0;
	rect.flags           = 0;
	rect.flip_horizontal = flipH;
	rect.flip_vertical   = false;
	rect.depth           = 0;
	rect.ambient_shade   = get_light_intensity(get_polygon_data(view->origin_polygon_index)->floor_lightsource_index);
	rect.ambient_shade   = MAX(si->minimum_light_intensity, rect.ambient_shade);
	rect.ambient_shade   = (_fixed)(rect.ambient_shade * shadeScale);   // < 1 = darkened (an empty pouch)
	if (view->shading_mode == _shading_infravision) rect.flags |= _SHADELESS_BIT;
	// takes a HIGH-level mode (_xfer_*). Passing _textured_transfer (3) here meant
		// _xfer_subtle_invisibility -> tinted -> silhouette colour table -> solid white.
		instantiate_rectangle_transfer_mode(view, &rect, _xfer_normal, 0);

	short modelSeq = NONE;
	vector<OGL_ModelData>* allModels = OGL_GetAllModels(coll, seq, modelSeq);
	OGL_ModelData* mdl = (allModels && !allModels->empty()) ? &(*allModels)[0] : nullptr;
	if (mdl) {
		OGL_RenderVRWeaponModel(rect, coll, 0, mdl, pos[0], pos[1], pos[2], wr, wu, wf,
		                        false, 0, 0.f, -1);
		return;
	}

	const float W  = (float)VR_Settings()->worldScaleWUM;
	float ww = fabsf(float(si->world_right - si->world_left));
	float wh = fabsf(float(si->world_bottom - si->world_top));
	if (ww < 4.f) ww = weaponSized ? 512.f : 128.f;
	if (wh < 4.f) wh = weaponSized ? 512.f : 128.f;
	const float k  = weaponSized ? (W / 4096.f) : 0.5f;    // weapon: same scale as the in-hand quad
	const float hw = ww * k * sizeScale, hh = wh * k * sizeScale;
	float verts[4][3];
	for (int i = 0; i < 3; ++i) {
		verts[0][i] = pos[i] - hw*wr[i] + hh*wu[i];
		verts[1][i] = pos[i] + hw*wr[i] + hh*wu[i];
		verts[2][i] = pos[i] + hw*wr[i] - hh*wu[i];
		verts[3][i] = pos[i] - hw*wr[i] - hh*wu[i];
	}
	OGL_RenderVRWeaponQuad(rect, verts, !weaponSized);   // items are world sprites, not in-hand art
}

// A magazine ITEM's picture, as an item appears in the world. Items are placed by their animation
// SEQUENCE, so the low-level frame comes from the sequence's first view.
static void vr_draw_item_at(view_data* view, short item,
                            const float pos[3], const float wr[3], const float wu[3], const float wf[3],
                            float sizeScale, float shadeScale = 1.0f)
{
	const shape_descriptor d = get_item_shape(item);
	if (d == UNONE) return;
	shape_animation_data* anim = get_shape_animation_data(d);
	if (!anim || anim->frames_per_view <= 0) return;
	vr_draw_shape_at(view, GET_DESCRIPTOR_COLLECTION(d), GET_DESCRIPTOR_SHAPE(d),
	                 anim->low_level_shape_indexes[0], pos, wr, wu, wf, false, false, sizeScale, shadeScale);
}

// A controller's position and orientation in world space: the same mapping the weapon anchor uses,
// with up rebuilt as cross(forward, right) exactly as the in-hand weapon does.
static bool vr_hand_world_pose(const vr_body_frame& bf, int hand,
                               float pos[3], float wr[3], float wu[3], float wf[3])
{
	float hp[3], ps[3], fs[3], sr[3], su[3];
	if (!VR_GetHeadPosStage(hp) || !VR_GetAimPoseStage(hand, ps, fs) || !VR_GetAimOrientStage(hand, sr, su))
		return false;
	const float d[3] = { ps[0]-hp[0], ps[1]-hp[1], ps[2]-hp[2] };
	float wd[3];
	vr_room_dir_to_world(bf, d, wd);
	for (int k = 0; k < 3; ++k) pos[k] = bf.origin[k] + wd[k] * bf.W;
	vr_room_dir_to_world(bf, sr, wr);
	vr_room_dir_to_world(bf, fs, wf);
	wu[0] = wf[1]*wr[2] - wf[2]*wr[1];
	wu[1] = wf[2]*wr[0] - wf[0]*wr[2];
	wu[2] = wf[0]*wr[1] - wf[1]*wr[0];
	return true;
}

static short vr_pouch_magazine_for(int pouchSlot, int reachingHand);   // with the magazine code below

// Blend a magazine pose in place: s = 0 gives the `from` pose, 1 leaves (top, r, d, f) as they are.
static void vr_blend_pose(float s, const float fTop[3], const float fR[3], const float fD[3], const float fF[3],
                          float top[3], float r[3], float d[3], float f[3])
{
	auto axis = [&](const float a[3], float b[3]) {
		for (int k = 0; k < 3; ++k) b[k] = a[k] + (b[k] - a[k]) * s;
		const float n = sqrtf(b[0]*b[0] + b[1]*b[1] + b[2]*b[2]);
		if (n > 1e-5f) for (int k = 0; k < 3; ++k) b[k] /= n;
	};
	for (int k = 0; k < 3; ++k) top[k] = fTop[k] + (top[k] - fTop[k]) * s;
	axis(fR, r); axis(fD, d); axis(fF, f);
}

// Approaching a well that will take it, a magazine in the hand eases into line: from ~2.2 x the capture
// radius out, its top is drawn increasingly onto the insertion line (outside the mouth) and its sides
// turn to match the well, up to 85% by 0.6 x the radius -- so by the time it engages (at the radius) the
// remaining snap is small, and kEngageSettleMs covers that. Render only; the logic still uses the hand.
// OFF (2026-10-04, Daniel): with a real magazine mesh the gradual alignment read as the magazine sliding
// around in the hand. It now stays exactly as held until it engages the well, then snaps into line over
// kEngageSettleMs. Kept for reference / re-enabling.
static const bool kVRMagApproachAssist = false;
static void vr_mag_assist(float W, int hand, short mag, float top[3], float r[3], float d[3], float f[3])
{
	if (!kVRMagApproachAssist) return;
	if (s_vrMagMustClear[hand]) return;
	const int gunHand = 1 - hand;               // the gun in the other hand
	if (!s_vrWeaponPose[gunHand].valid) return;
	const int which = vr_well_for_ammo(s_vrWeaponPose[gunHand].weapon_type, mag);
	vr_well wl;
	if (which < 0 || !vr_get_well(W, hand, which, wl) || !vr_magwell_accepts(wl, mag)) return;

	const float* cap = (wl.travel > 1e-3f) ? wl.entry : wl.seat;
	const float rel[3] = { top[0]-cap[0], top[1]-cap[1], top[2]-cap[2] };
	const float dist = sqrtf(rel[0]*rel[0] + rel[1]*rel[1] + rel[2]*rel[2]);
	float w = 1.f - (dist - 0.6f * wl.radius) / (1.6f * wl.radius);
	if (w <= 0.f) return;
	if (w > 1.f) w = 1.f;
	w = w * w * (3.f - 2.f * w) * 0.85f;

	// Where it is heading: the nearest point on the insertion line that is still outside the mouth.
	float along = rel[0]*wl.ins[0] + rel[1]*wl.ins[1] + rel[2]*wl.ins[2];
	if (along > 0.f) along = 0.f;
	float tTop[3], tR[3], tD[3], tF[3];
	// A magazine held back-to-front is the same box turned half round: aim for whichever way is nearer.
	const float sgn = (r[0]*wl.magRight[0] + r[1]*wl.magRight[1] + r[2]*wl.magRight[2]) < 0.f ? -1.f : 1.f;
	for (int k = 0; k < 3; ++k) {
		tTop[k] = cap[k] + wl.ins[k] * along;
		tR[k] = wl.magRight[k] * sgn; tD[k] = -wl.ins[k]; tF[k] = wl.magFwd[k] * sgn;
	}
	// Blend from the hand's pose toward the aligned one by w.
	float hTop[3], hR[3], hD[3], hF[3];
	for (int k = 0; k < 3; ++k) { hTop[k] = top[k]; hR[k] = r[k]; hD[k] = d[k]; hF[k] = f[k]; }
	for (int k = 0; k < 3; ++k) { top[k] = tTop[k]; r[k] = tR[k]; d[k] = tD[k]; f[k] = tF[k]; }
	vr_blend_pose(w, hTop, hR, hD, hF, top, r, d, f);
}

// TEMPORARY positioning aid (2026-09-27): draw magazines as an oriented wireframe box instead of the
// pickup sprite -- a flat picture shows nothing about which way a magazine is turned, and lining one up
// with a well is all about that. Flip to false to go back to the sprites. The pouches keep their sprites
// either way (they say WHICH ammo is in them).
static const bool s_vrMagAsBox = true;

// A magazine-shaped box (or, for a round one, a cylinder) of the given size. `top` is the end that goes
// into the gun; it runs from there `axis`-wards (pass the negated insertion direction to hang it below a
// well). right/fwd set its sides.
static void vr_draw_mag_box(float W, const vr_mag_dims& md, const float top[3], const float right[3],
                            const float axis[3], const float fwd[3], float r, float g, float b)
{
	const float hw = md.width * 0.5f * W, len = md.length * W, hd = md.depth * 0.5f * W;
	GLfloat v[48 * 2 * 3]; int n = 0;
	auto seg = [&](const float a[3], const float c[3]) {
		for (int k = 0; k < 3; ++k) v[n++] = a[k];
		for (int k = 0; k < 3; ++k) v[n++] = c[k];
	};
	float p0[3], p1[3];
	if (md.drum) {
		// A drum: its round faces look forward/back along the barrel (its axis = fwd). Two elliptical faces,
		// `length` tall along the insertion line and `width` across, `depth` apart, joined by 8 edges; its
		// top touches `top`.
		const int kSides = 16;
		const float ra = len * 0.5f;    // along the axis direction (insertion line)
		auto rim = [&](int i, int sx, float out[3]) {
			const float a = i * 6.2831853f / kSides, c = cosf(a), s = sinf(a);
			for (int k = 0; k < 3; ++k)
				out[k] = top[k] + axis[k] * (ra - c * ra) + right[k] * (s * hw) + fwd[k] * (sx * hd);
		};
		for (int sx = -1; sx <= 1; sx += 2)
			for (int i = 0; i < kSides; ++i) { rim(i, sx, p0); rim(i + 1, sx, p1); seg(p0, p1); }
		for (int i = 0; i < kSides; i += 2) { rim(i, -1, p0); rim(i, 1, p1); seg(p0, p1); }
	} else if (md.round) {
		// 16-sided rings at both ends, joined by 8 long edges; diameter = width.
		const int kSides = 16;
		auto rim = [&](int i, int sl, float out[3]) {
			const float a = i * 6.2831853f / kSides, c = cosf(a) * hw, s = sinf(a) * hw;
			for (int k = 0; k < 3; ++k) out[k] = top[k] + c * right[k] + s * fwd[k] + sl * len * axis[k];
		};
		for (int sl = 0; sl <= 1; ++sl)
			for (int i = 0; i < kSides; ++i) { rim(i, sl, p0); rim(i + 1, sl, p1); seg(p0, p1); }
		for (int i = 0; i < kSides; i += 2) { rim(i, 0, p0); rim(i, 1, p1); seg(p0, p1); }
	} else {
		auto corner = [&](int sx, int sl, int sd, float out[3]) {   // sl: 0 = top face, 1 = bottom face
			for (int k = 0; k < 3; ++k)
				out[k] = top[k] + sx * hw * right[k] + sl * len * axis[k] + sd * hd * fwd[k];
		};
		for (int sl = 0; sl <= 1; ++sl) {                          // the two end faces
			corner(-1, sl, -1, p0); corner( 1, sl, -1, p1); seg(p0, p1);
			corner( 1, sl, -1, p0); corner( 1, sl,  1, p1); seg(p0, p1);
			corner( 1, sl,  1, p0); corner(-1, sl,  1, p1); seg(p0, p1);
			corner(-1, sl,  1, p0); corner(-1, sl, -1, p1); seg(p0, p1);
		}
		for (int sx = -1; sx <= 1; sx += 2)                        // the four long edges
			for (int sd = -1; sd <= 1; sd += 2) { corner(sx, 0, sd, p0); corner(sx, 1, sd, p1); seg(p0, p1); }
	}

	glUseProgram(0);
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_DEPTH_TEST);   // seen through the gun: a magazine inside a pistol grip is the whole point
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(2.5f);
	glColor4f(r, g, b, 0.95f);
	glVertexPointer(3, GL_FLOAT, 0, v);
	glDrawArrays(GL_LINES, 0, n / 3);
	glLineWidth(1.0f);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

// ---- VR physical weapons: magazine MESHES -----------------------------------------------------------
// A gun model exported with one MD3 surface per Blender object ("Surfaces: From Objects") can carry its
// magazines and the top rounds as their own surfaces, named by suffix:
//   <name>_mag              the main magazine (well 0)
//   <name>_grenade / _mag2  the second feed's magazine (well 1, e.g. the rifle's grenades)
//   <name>_bullet1..3       the top rounds in the main magazine, topmost first
//   anything else           the gun itself
// In weapon handling mode the gun draws only the parts that are actually in it, and a magazine anywhere
// else -- on its path, hanging loose, in a hand, falling -- is drawn from the same model, pivoting about its
// SEATED pose (the <vr_magwell> seat + insertion line + roll): exactly where the debug box would be. The
// rounds show by count: 3+ = all three, then two, one, none. Models without such surfaces fall back to the box.
static const bool s_vrMagMeshes = true;

struct vr_model_parts {
	bool any = false;
	std::vector<int> gunF, gunC;
	std::vector<int> magF[2], magC[2];
	std::vector<int> bulF[3], bulC[3];
};

static const vr_model_parts& vr_get_model_parts(const OGL_ModelData* mdl)
{
	static std::map<const OGL_ModelData*, std::pair<size_t, vr_model_parts> > cache;
	const auto& surfs = mdl->Model.MD3Surfaces;
	auto it = cache.find(mdl);
	if (it != cache.end() && it->second.first == surfs.size()) return it->second.second;
	vr_model_parts p;
	for (const auto& sf : surfs) {
		std::string n = sf.name;
		for (auto& ch : n) ch = (char)tolower((unsigned char)ch);
		auto ends = [&](const char* suf) {
			const size_t l = strlen(suf);
			return n.size() >= l && n.compare(n.size() - l, l, suf) == 0;
		};
		const size_t b = n.find("_bullet");
		const int first = (int)sf.firstIndex, count = (int)sf.numIndices;
		if (ends("_mag")) {
			p.magF[0].push_back(first); p.magC[0].push_back(count); p.any = true;
		} else if (ends("_grenade") || ends("_mag2")) {
			p.magF[1].push_back(first); p.magC[1].push_back(count); p.any = true;
		} else if (b != std::string::npos && b + 7 < n.size() && isdigit((unsigned char)n[b + 7])) {
			const int k = atoi(n.c_str() + b + 7) - 1;
			if (k >= 0 && k < 3) { p.bulF[k].push_back(first); p.bulC[k].push_back(count); p.any = true; }
		} else {
			p.gunF.push_back(first); p.gunC.push_back(count);
		}
	}
	cache[mdl] = std::make_pair(surfs.size(), p);
	return cache[mdl].second;
}

// The slices to draw: the gun body, either magazine, and the top `bullets` rounds (0..3).
static void vr_part_ranges(const vr_model_parts& p, bool gun, bool mag0, bool mag1, int bullets,
                           std::vector<int>& F, std::vector<int>& C)
{
	F.clear(); C.clear();
	auto add = [&](const std::vector<int>& f, const std::vector<int>& c) {
		F.insert(F.end(), f.begin(), f.end()); C.insert(C.end(), c.begin(), c.end());
	};
	if (gun)  add(p.gunF, p.gunC);
	if (mag0) add(p.magF[0], p.magC[0]);
	if (mag1) add(p.magF[1], p.magC[1]);
	for (int k = 0; k < 3 && k < bullets; ++k) add(p.bulF[k], p.bulC[k]);
}

// Rounds in a magazine -> how many of the three top rounds show. -1 = a fresh (full) magazine.
static int vr_bullets_shown(int rounds) { return rounds < 0 ? 3 : (rounds > 3 ? 3 : rounds); }

// For a gun in a hand (or on the body): which parts are actually in it right now. copy = which gun of a
// pair. Returns false when the model has no separate parts (draw it whole).
static bool vr_gun_parts_now(const OGL_ModelData* mdl, short weapon, short copy,
                             std::vector<int>& F, std::vector<int>& C)
{
	if (!s_vrMagMeshes || !mdl || weapon == NONE || !VR_PhysicalWeaponsActive()) return false;
	const vr_model_parts& p = vr_get_model_parts(mdl);
	if (!p.any) return false;
	const bool in0 = VR_GetMagState(weapon, 0, copy) == VR_MAG_IN;
	const bool in1 = VR_GetMagState(weapon, 1, copy) == VR_MAG_IN;
	const short trig = weapon_is_twofisted(weapon) ? copy : short(_primary_weapon);
	short rounds = get_player_weapon_ammo_count(current_player_index, weapon, trig);
	if (rounds < 0) rounds = 0;
	vr_part_ranges(p, true, in0, in1, in0 ? vr_bullets_shown(rounds) : 0, F, C);
	return true;
}

// The weapon (and which of its wells) a magazine of this ammo belongs to: the gun in the given hand if it
// takes it, else the first weapon that does.
static bool vr_weapon_for_ammo(short ammo, int preferHand, short* weapon, int* which)
{
	auto check = [&](short wt) {
		for (int w = 0; w < 2; ++w)
			if (get_player_weapon_ammo_type(current_player_index, wt, w ? _secondary_weapon : _primary_weapon) == ammo) {
				*weapon = wt; *which = w; return true;
			}
		return false;
	};
	if (preferHand >= 0 && preferHand < 2 && s_vrWeaponPose[preferHand].valid &&
	    s_vrWeaponPose[preferHand].weapon_type != NONE && check(s_vrWeaponPose[preferHand].weapon_type))
		return true;
	for (short wt = 0; wt < short(MAXIMUM_NUMBER_OF_WEAPONS); ++wt)
		if (check(wt)) return true;
	return false;
}

// The model->world matrix that puts a magazine part (authored seated in the gun) at a magazine pose: top
// point + sides (up = toward its top). Worked in the gun's own frame, so it needs no live gun pose: a
// vertex goes through the gun's model matrix into gun space, into the SEATED magazine's frame, and back
// out through the magazine's actual pose.
static void vr_mag_part_matrix(short weapon, int which, const rectangle_definition& rect, const float top[3],
                               const float r[3], const float up[3], const float f[3], float out[16])
{
	const float W = (float)VR_Settings()->worldScaleWUM;
	vr_weapon_pose wp0;
	wp0.valid = true; wp0.weapon_type = weapon;
	for (int k = 0; k < 3; ++k) { wp0.anchor[k] = 0.f; wp0.right[k] = (k == 0); wp0.up[k] = (k == 1); wp0.fwd[k] = (k == 2); }
	const vr_attach_point* mw = which ? VR_GetMagwell2OrDefault(weapon) : VR_GetMagwellOrDefault(weapon);
	vr_well wl;
	vr_fill_well_geometry(wp0, mw, W, wl);
	float G[16];
	OGL_VRWeaponModelMatrix(rect, 0.f, 0.f, 0.f, wp0.right, wp0.up, wp0.fwd, G);
	auto toMag = [&](const float g[3], float q[3]) {   // gun space -> seated magazine frame
		q[0] = g[0]*wl.magRight[0] + g[1]*wl.magRight[1] + g[2]*wl.magRight[2];
		q[1] = g[0]*wl.ins[0]      + g[1]*wl.ins[1]      + g[2]*wl.ins[2];
		q[2] = g[0]*wl.magFwd[0]   + g[1]*wl.magFwd[1]   + g[2]*wl.magFwd[2];
	};
	for (int j = 0; j < 3; ++j) {
		float q[3];
		toMag(G + 4 * j, q);
		for (int k = 0; k < 3; ++k) out[4 * j + k] = q[0] * r[k] + q[1] * up[k] + q[2] * f[k];
		out[4 * j + 3] = 0.f;
	}
	const float t[3] = { G[12] - wl.seat[0], G[13] - wl.seat[1], G[14] - wl.seat[2] };
	float q[3];
	toMag(t, q);
	for (int k = 0; k < 3; ++k) out[12 + k] = top[k] + q[0] * r[k] + q[1] * up[k] + q[2] * f[k];
	out[15] = 1.f;
}

// Draw a magazine (and, for the main one, its top rounds) from its gun's model at a magazine pose -- the same
// top/right/down/fwd the debug box takes. gunHand = the hand whose mirroring to copy (-1: by default).
// False if this weapon's model has no such part (draw the box instead).
// Whether this weapon's model carries this well's magazine as a mesh (then the debug guides are hidden).
static bool vr_mag_has_mesh(short weapon, int which)
{
	if (!s_vrMagMeshes || weapon == NONE || which < 0 || which > 1) return false;
	short coll = NONE, shape = NONE, low = NONE;
	if (!get_weapon_idle_shape(weapon, &coll, &shape, &low)) return false;
	short seq = NONE;
	vector<OGL_ModelData>* all = OGL_GetAllModels(coll, shape, seq);
	if (!all || all->empty()) return false;
	return !vr_get_model_parts(&(*all)[0]).magF[which].empty();
}

static bool vr_draw_mag_mesh(view_data* view, short weapon, int which, int gunHand, int rounds,
                             const float top[3], const float right[3], const float down[3], const float fwd[3])
{
	if (!s_vrMagMeshes || weapon == NONE || which < 0 || which > 1) return false;
	short coll = NONE, shape = NONE, low = NONE;
	if (!get_weapon_idle_shape(weapon, &coll, &shape, &low)) return false;
	short seq = NONE;
	vector<OGL_ModelData>* all = OGL_GetAllModels(coll, shape, seq);
	if (!all || all->empty()) return false;
	OGL_ModelData* mdl = &(*all)[0];
	const vr_model_parts& p = vr_get_model_parts(mdl);
	if (!p.any || p.magF[which].empty()) return false;

	shape_information_data* si = extended_get_shape_information(coll, low);
	bitmap_definition* bmp = nullptr; void* shade = nullptr;
	extended_get_shape_bitmap_and_shading_table(coll, low, &bmp, &shade, view->shading_mode);
	if (!si || !bmp) return false;
	rectangle_definition rect;
	rect.ModelPtr        = nullptr;
	rect.Opacity         = 1;
	rect.ShapeDesc       = BUILD_DESCRIPTOR(coll, 0);
	rect.LowLevelShape   = low;
	rect.texture         = bmp;
	rect.shading_tables  = shade;
	rect.transfer_mode   = _textured_transfer;
	rect.transfer_data   = 0;
	rect.flags           = 0;
	bool flip = (si->flags & _X_MIRRORED_BIT) != 0;          // mirrored as the gun in that hand is
	if (((gunHand < 0 ? 1 : gunHand) == 0) != VR_IsWeaponNaturallyLeftHanded(weapon)) flip = !flip;
	rect.flip_horizontal = flip;
	rect.flip_vertical   = false;
	rect.depth           = 0;
	rect.ambient_shade   = get_light_intensity(get_polygon_data(view->origin_polygon_index)->floor_lightsource_index);
	rect.ambient_shade   = MAX(si->minimum_light_intensity, rect.ambient_shade);
	if (view->shading_mode == _shading_infravision) rect.flags |= _SHADELESS_BIT;
	instantiate_rectangle_transfer_mode(view, &rect, _xfer_normal, 0);

	std::vector<int> F, C;
	vr_part_ranges(p, false, which == 0, which == 1, which == 0 ? vr_bullets_shown(rounds) : 0, F, C);
	const float up[3] = { -down[0], -down[1], -down[2] };
	float M[16];
	vr_mag_part_matrix(weapon, which, rect, top, right, up, fwd, M);
	OGL_SetVRWeaponModelParts(F.data(), C.data(), (int)F.size());
	OGL_SetVRWeaponModelMatrix(M);
	OGL_RenderVRWeaponModel(rect, coll, 0, mdl, top[0], top[1], top[2], right, up, fwd, false, 0, 0.f, -1);
	OGL_SetVRWeaponModelMatrix(nullptr);
	OGL_ClearVRWeaponModelParts();
	return true;
}

static void render_vr_hand_visuals(view_data* view)
{
	if (!VR_PhysicalWeaponsActive() || !OGL_IsActive()) return;
	if (PLAYER_IS_DEAD(get_player_data(current_player_index))) return;

	vr_body_frame bf;
	vr_get_body_frame(view, bf);
	const int held = VR_GetHeldWeaponHand();

	// ---- fists on both hands --------------------------------------------------------------------
	short fColl = NONE, fSeq = NONE, fLow = NONE;
	const bool haveFist = get_weapon_idle_shape(_weapon_fist, &fColl, &fSeq, &fLow);
	for (int hand = 0; hand < 2; ++hand) {
		float pos[3], wr[3], wu[3], wf[3];
		if (!vr_hand_world_pose(bf, hand, pos, wr, wu, wf)) continue;

		if (haveFist && !s_vrHandDrewFist[hand]) {
			// The fist model is authored the opposite way round from the guns, so it mirrors on the RIGHT
			// hand where a gun mirrors on the left. The same fist.md3 ships in all three scenarios, so the
			// rule is the same everywhere -- Marathon 1's <left_handed_weapon name="fist"> is about the stock
			// 2D fist sprite and must NOT apply here (honouring it flipped M1's fists, 2026-09-28).
			// Confirmed on device for M2 (right hand mirrored, 2026-09-27).
			const bool flip = (hand == 1);
			vr_draw_shape_at(view, fColl, fSeq, fLow, pos, wr, wu, wf, flip, true, 1.0f);
		}

		// ---- a magazine in this hand -----------------------------------------------------------
		// Free, it sits just past the knuckles. Engaged on a well's path, it is drawn ON the path, lined
		// up with the gun -- sliding in or out as you push or pull.
		const short mag = VR_GetHandMagazine(hand);
		if (mag != NONE) {
			vr_well wl;
			if (s_slideWell[hand] >= 0 && vr_get_well(bf.W, hand, s_slideWell[hand], wl)) {
				float mp[3];
				vr_well_point(wl, s_slideT[hand], mp);
				if (s_vrMagAsBox) {
					// on the path: top on the line, hanging back down the insertion direction (green)
					float down[3] = { -wl.ins[0], -wl.ins[1], -wl.ins[2] };
					float mr[3] = { wl.magRight[0], wl.magRight[1], wl.magRight[2] };
					float mf[3] = { wl.magFwd[0], wl.magFwd[1], wl.magFwd[2] };
					// just engaged: settle in from where it was drawn in the hand
					if (s_engageSettle[hand]) {
						const float t = float(machine_tick_count() - s_engageStartMs[hand]) / kEngageSettleMs;
						if (t >= 1.f) s_engageSettle[hand] = false;
						else {
							const float s = t * t * (3.f - 2.f * t);
							// the box is symmetric: settle toward whichever way round is nearer
							const float* fr = s_magDrawn[hand][1];
							if (fr[0]*mr[0] + fr[1]*mr[1] + fr[2]*mr[2] < 0.f)
								for (int k = 0; k < 3; ++k) { mr[k] = -mr[k]; mf[k] = -mf[k]; }
							vr_blend_pose(s, s_magDrawn[hand][0], s_magDrawn[hand][1], s_magDrawn[hand][2],
							              s_magDrawn[hand][3], mp, mr, down, mf);
						}
					}
					if (!vr_draw_mag_mesh(view, wl.weapon, wl.which, wl.hand, VR_GetHandMagazineRounds(hand), mp, mr, down, mf))
						vr_draw_mag_box(bf.W, wl.mag, mp, mr, down, mf, 0.2f, 1.0f, 0.3f);
				} else {
					vr_draw_item_at(view, mag, mp, wl.right, wl.ins, wl.fwd, 0.6f);
				}
			} else if (s_vrMagAsBox) {
				// free in the hand: held by its base, sticking up out of the fist along the controller's up (cyan)
				const vr_mag_dims md = vr_mag_dims_for_ammo(mag);
				float top[3], mr[3], md3[3], mf[3];
				vr_hand_mag_pose(bf.W, hand, md, pos, wr, wu, wf, top, mr, md3, mf);
				vr_mag_assist(bf.W, hand, mag, top, mr, md3, mf);   // eases into line near the well
				for (int k = 0; k < 3; ++k) {
					s_magDrawn[hand][0][k] = top[k]; s_magDrawn[hand][1][k] = mr[k];
					s_magDrawn[hand][2][k] = md3[k]; s_magDrawn[hand][3][k] = mf[k];
				}
				s_magDrawnFrame[hand] = s_vrFrameNo;
				short magW = NONE; int magWhich = 0;
				if (!(vr_weapon_for_ammo(mag, 1 - hand, &magW, &magWhich) &&
				      vr_draw_mag_mesh(view, magW, magWhich, 1 - hand, VR_GetHandMagazineRounds(hand), top, mr, md3, mf)))
					vr_draw_mag_box(bf.W, md, top, mr, md3, mf, 0.2f, 0.9f, 1.0f);
			} else {
				float mp[3];
				for (int k = 0; k < 3; ++k) mp[k] = pos[k] + wf[k] * 0.05f * bf.W;   // just past the knuckles
				vr_draw_item_at(view, mag, mp, wr, wu, wf, 0.6f);
			}
		}
	}

	// ---- what each pouch will hand out -----------------------------------------------------------
	// Only shown when it would actually dispense something for the gun in your primary hand, so an
	// empty pouch (or one with nothing for this weapon) is simply empty.
	if (held >= 0) {
		float bR[3], bU[3], bF[3];
		vr_room_dir_to_world(bf, bf.right, bR);
		vr_room_dir_to_world(bf, bf.up,    bU);
		vr_room_dir_to_world(bf, bf.fwd,   bF);
		for (int slot = 0; slot < VR_HOLSTER_COUNT; ++slot) {
			if (!VR_HolsterSlotIsPouch(slot)) continue;
			// Shown whenever the pouch has a job for this gun -- dark when you are out, so you can see
			// the grenade pouch is there even with no grenades to draw.
			// For the gun in the other hand from whichever hand is free (both full: the primary's).
			const int reach = !vr_hand_gun(current_player_index, 0, nullptr, nullptr) ? 0
			                : !vr_hand_gun(current_player_index, 1, nullptr, nullptr) ? 1 : 1 - held;
			const short item = vr_pouch_ammo_type(slot, reach);
			if (item == NONE) continue;
			const bool empty = get_player_data(current_player_index)->items[item] <= 0;
			vr_holster_anchor a;
			VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &a);
			float c[3];
			vr_body_to_world(bf, a.x, a.y, a.z, c);
			// The gun's own magazine model, if it has one: standing in the pouch, turned the way the pouch is
			// set, dark when the pouch is empty.
			{
				short pw = NONE; int pwhich = 0;
				if (vr_weapon_for_ammo(item, 1 - reach, &pw, &pwhich) && vr_mag_has_mesh(pw, pwhich)) {
					float pr[3], pu[3], pf[3];
					vr_holster_model_basis(bf, slot, pr, pu, pf);
					const vr_mag_dims md = vr_mag_dims_for_well(pw, pwhich);
					float top[3];
					for (int k = 0; k < 3; ++k) top[k] = c[k] + pu[k] * md.length * 0.5f * bf.W;
					const float down[3] = { -pu[0], -pu[1], -pu[2] };
					if (empty) OGL_SetVRWeaponModelTint(0.25f, 0.25f, 0.25f, 1.f, 0.f, 0.f, 0.f);
					const bool drawn = vr_draw_mag_mesh(view, pw, pwhich, 1 - reach, -1, top, pr, down, pf);
					OGL_SetVRWeaponModelTint(1.f, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f);
					if (drawn) continue;
				}
			}
			// Billboarded toward your head, so you can see what is in the pouch as you reach for it.
			float t[3] = { c[0]-bf.origin[0], c[1]-bf.origin[1], c[2]-bf.origin[2] };
			const float tl = sqrtf(t[0]*t[0] + t[1]*t[1] + t[2]*t[2]);
			float br[3] = { -t[1], t[0], 0.f };
			const float brl = sqrtf(br[0]*br[0] + br[1]*br[1]);
			if (tl > 1e-3f && brl > 1e-3f) {
				for (int k = 0; k < 3; ++k) t[k] /= tl;
				br[0] /= brl; br[1] /= brl;
				float bu[3] = { t[1]*br[2] - t[2]*br[1], t[2]*br[0] - t[0]*br[2], t[0]*br[1] - t[1]*br[0] };
				vr_draw_item_at(view, item, c, br, bu, t, 0.525f, empty ? 0.2f : 1.0f);
			} else {
				vr_draw_item_at(view, item, c, bR, bU, bF, 0.525f, empty ? 0.2f : 1.0f);
			}
		}
	}

	// ---- loosened magazines hanging out of the gun, and magazines falling away ------------------------
	{
		float bR[3], bU[3], bF[3];
		vr_room_dir_to_world(bf, bf.right, bR);
		vr_room_dir_to_world(bf, bf.up,    bU);
		vr_room_dir_to_world(bf, bf.fwd,   bF);
		for (int slot = 0; slot < 4; ++slot) {          // each gun hand x each well
			const int gunHand = slot / 2, which = slot % 2;
			vr_well wl;
			// The release drops it out of the seat: it slides down to its loose position over kLooseDropMs
			// (unless it was left there by a hand letting go mid-pull -- then it is already there).
			static bool     s_wasLoose[4] = { false, false, false, false };
			static short    s_looseWeapon[4] = { NONE, NONE, NONE, NONE };
			static uint64_t s_looseSince[4] = { 0, 0, 0, 0 };
			const float kLooseDropMs = 90.f;
			if (!vr_get_well_hand(bf.W, gunHand, which, wl) || VR_GetMagState(wl.weapon, which, wl.copy) != VR_MAG_LOOSE) {
				s_wasLoose[slot] = false;
				continue;
			}
			if (!s_wasLoose[slot] || s_looseWeapon[slot] != wl.weapon) {
				s_looseSince[slot] = s_vrLooseNoDrop[slot] ? 0 : machine_tick_count();
				s_vrLooseNoDrop[slot] = false;
			}
			s_wasLoose[slot] = true;
			s_looseWeapon[slot] = wl.weapon;
			float mp[3];
			vr_loose_mag_pos(bf.W, wl.seat, wl.ins, wl.drops, mp);
			if (s_looseSince[slot]) {
				const float t = float(machine_tick_count() - s_looseSince[slot]) / kLooseDropMs;
				if (t < 1.f) {
					const float s = t * t * (3.f - 2.f * t);
					for (int k = 0; k < 3; ++k) mp[k] = wl.seat[k] + (mp[k] - wl.seat[k]) * s;
				}
			}
			if (s_vrMagAsBox) {
				const float down[3] = { -wl.ins[0], -wl.ins[1], -wl.ins[2] };
				if (!vr_draw_mag_mesh(view, wl.weapon, wl.which, wl.hand,
				                      VR_GetLooseMagRounds(wl.weapon, wl.which, wl.copy), mp, wl.magRight, down, wl.magFwd))
					vr_draw_mag_box(bf.W, wl.mag, mp, wl.magRight, down, wl.magFwd, 1.0f, 0.5f, 0.05f);   // orange: loose
			} else {
				vr_draw_item_at(view, get_player_weapon_ammo_type(current_player_index, wl.weapon,
				                which ? _secondary_weapon : _primary_weapon), mp, wl.right, wl.ins, wl.fwd, 0.6f);
			}
			// orange ring: this well needs clearing -- pull it out (or, the main mag, flick it free). A debug guide:
			// not drawn for a magazine shown as a real mesh.
			if (vr_mag_has_mesh(wl.weapon, which)) continue;
			static int lpulse = 0; ++lpulse;
			const float rr = 0.05f * bf.W * (0.85f + 0.15f * sinf(lpulse * 0.25f));
			GLfloat v[24 * 2 * 3]; int n = 0;
			for (int i = 0; i < 24; ++i) {
				const float t0 = i * 6.2831853f / 24, t1 = (i + 1) * 6.2831853f / 24;
				for (int k = 0; k < 3; ++k) v[n + k]     = mp[k] + rr * (cosf(t0) * wl.right[k] + sinf(t0) * wl.fwd[k]);
				for (int k = 0; k < 3; ++k) v[n + 3 + k] = mp[k] + rr * (cosf(t1) * wl.right[k] + sinf(t1) * wl.fwd[k]);
				n += 6;
			}
			glUseProgram(0);
			glDisable(GL_TEXTURE_2D);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glLineWidth(3.0f);
			if (which) glColor4f(1.0f, 0.3f, 0.05f, 0.95f);     // second well (grenades): deeper orange
			else       glColor4f(1.0f, 0.5f, 0.05f, 0.95f);
			glVertexPointer(3, GL_FLOAT, 0, v);
			glDrawArrays(GL_LINES, 0, n / 3);
			glLineWidth(1.0f);
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			glEnable(GL_TEXTURE_2D);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		for (const auto& fm : s_fallingMags) {
			if (s_vrMagAsBox) {
				const vr_mag_dims md = vr_mag_dims_for_ammo(fm.item);
				float top[3];
				for (int k = 0; k < 3; ++k) top[k] = fm.pos[k] + fm.u[k] * md.length * 0.5f * bf.W;
				const float down[3] = { -fm.u[0], -fm.u[1], -fm.u[2] };
				if (!vr_draw_mag_mesh(view, fm.weapon, fm.which, fm.gunHand, fm.rounds, top, fm.r, down, fm.f))
					vr_draw_mag_box(bf.W, md, top, fm.r, down, fm.f, 0.8f, 0.8f, 0.8f);   // grey: dropped
			} else {
				vr_draw_item_at(view, fm.item, fm.pos, fm.r, fm.u, fm.f, 0.6f);
			}
		}
	}

	// ---- where to put the magazine you are holding --------------------------------------------------
	// A ring at the capture point -- the path's entry if the well has one, else the seat -- with a line
	// from your magazine to it, and the path itself drawn from entry to seat.
	for (int hand = 0; hand < 2; ++hand) {
		const short mag = VR_GetHandMagazine(hand);
		if (mag == NONE || s_slideWell[hand] >= 0) continue;
		// The gun in the OTHER hand, whichever gun that is.
		const vr_weapon_pose& wp = s_vrWeaponPose[1 - hand];
		if (!wp.valid) continue;
		const int which = vr_well_for_ammo(wp.weapon_type, mag);
		vr_well wl;
		if (which < 0 || !vr_get_well(bf.W, hand, which, wl)) continue;
		if (vr_mag_has_mesh(wl.weapon, which)) continue;   // debug guide: not for a real magazine mesh
		const float* cap = (wl.travel > 1e-3f) ? wl.entry : wl.seat;

		float pos[3], wr[3], wu[3], wf[3];
		if (!vr_hand_world_pose(bf, hand, pos, wr, wu, wf)) continue;
		float magTop[3];
		const bool inRange = vr_mag_touches_well(wl, bf.W, hand, pos, wr, wu, wf, magTop, nullptr);

		static int pulse = 0; ++pulse;
		const float rr = wl.radius * (0.85f + 0.15f * sinf(pulse * 0.2f));
		GLfloat v[(24 + 2) * 2 * 3]; int n = 0;
		auto push = [&](const float p[3]) { v[n++] = p[0]; v[n++] = p[1]; v[n++] = p[2]; };
		for (int i = 0; i < 24; ++i) {
			const float t0 = i * 6.2831853f / 24, t1 = (i + 1) * 6.2831853f / 24;
			float a[3], b[3];
			for (int k = 0; k < 3; ++k) {
				a[k] = cap[k] + rr * (cosf(t0) * wl.right[k] + sinf(t0) * wl.fwd[k]);
				b[k] = cap[k] + rr * (cosf(t1) * wl.right[k] + sinf(t1) * wl.fwd[k]);
			}
			push(a); push(b);
		}
		push(magTop); push(cap);
		if (wl.travel > 1e-3f) { push(wl.entry); push(wl.seat); }

		glUseProgram(0);
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(3.0f);
		const bool accepts = vr_magwell_accepts(wl, mag);
		if (!accepts)     glColor4f(1.0f, 0.15f, 0.1f, 0.9f);   // red: the old one is still in
		else if (inRange) glColor4f(0.2f, 1.0f, 0.3f, 0.95f);   // green: it will take it
		else              glColor4f(1.0f, 0.85f, 0.1f, 0.85f);  // yellow: bring it here
		glVertexPointer(3, GL_FLOAT, 0, v);
		glDrawArrays(GL_LINES, 0, n / 3);
		glLineWidth(1.0f);
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glEnable(GL_TEXTURE_2D);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
}

// ---- VR physical weapons: in-headset attach-point tuner ----------------------------------------
// The model format carries no attachment points, so every grip / foregrip / magwell / muzzle and
// every body anchor has to be positioned by eye. Doing that from outside the headset means a
// build-deploy-"a bit left"-build round trip per number, and there are dozens of numbers. This
// lets them be dialled in live and dumped straight out as MML.
//
// Engaged by HOLDING THE OFF-HAND GRIP while the gizmos are on, so nothing is stolen from normal
// play the rest of the time:
//     off-hand stick        nudge the selected point along the two active axes
//     off-hand trigger      select the next target
//     off-hand stick click  cycle the active axis pair
//     dominant trigger      dump the whole tuned set to the log as <vr_weapons> MML
//
// Targets are the held weapon's five attach points followed by every body anchor. The selected one
// is drawn pulsing white by render_vr_attach_gizmos.
enum { VR_TUNE_AXES_XZ, VR_TUNE_AXES_Y_RADIUS, VR_TUNE_AXES_PITCH_YAW, VR_TUNE_AXES_COUNT };

static int   s_tuneTarget   = 0;      // index into the target list built by vr_tuner_target()
static int   s_tuneAxisMode = VR_TUNE_AXES_XZ;
static bool  s_tunePrevTrigger = false, s_tunePrevClick = false, s_tunePrevDomTrigger = false;

// The target list is [0 .. VR_ATTACH_COUNT-1] = attach points on the weapon in the dominant hand,
// then [VR_ATTACH_COUNT ..] = the body anchors.
static int vr_tuner_target_count(void) { return VR_ATTACH_COUNT + VR_HOLSTER_COUNT; }

// Resolve the current target to something editable. Exactly one of *pt / *anchor comes back set.
static void vr_tuner_resolve(short weapon_type, vr_attach_point** pt, vr_holster_anchor** anchor,
                             const char** label)
{
	*pt = nullptr; *anchor = nullptr; *label = "?";
	const int t = s_tuneTarget % vr_tuner_target_count();
	if (t < VR_ATTACH_COUNT) {
		if (weapon_type == NONE) return;          // nothing in hand: attach points aren't editable
		*pt    = VR_GetAttachPointMutable(weapon_type, t);
		*label = VR_AttachPointName(t);
	} else {
		const int slot = t - VR_ATTACH_COUNT;
		*anchor = VR_GetHolsterAnchorMutable(slot);
		*label  = VR_HolsterSlotName(slot);
	}
}

// Once per frame (NOT per eye -- the render pass runs twice and would double every edge).
static void vr_update_attach_tuner(void)
{
	if (!VR_IsActive() || !VR_Settings()->showAttachGizmos || !VR_PhysicalWeaponsActive()) return;

	const int domHand = VR_Settings()->dominantHand ? 0 : 1;
	const int offHand = 1 - domHand;

	float offStickX = 0.f, offStickY = 0.f, offTrigger = 0.f, offGrip = 0.f;
	bool  offClick = false;
	if (!VR_GetRawHandInput(offHand, &offStickX, &offStickY, &offTrigger, &offGrip, &offClick)) return;
	float domTrigger = 0.f;
	VR_GetRawHandInput(domHand, nullptr, nullptr, &domTrigger, nullptr, nullptr);

	const bool engaged = offGrip > 0.5f;
	if (!engaged) {
		s_tunePrevTrigger = s_tunePrevClick = s_tunePrevDomTrigger = false;
		return;
	}

	// Whatever the dominant hand is holding is what we're tuning attach points on.
	const short weapon_type = s_vrWeaponPose[domHand].valid ? s_vrWeaponPose[domHand].weapon_type
	                                                        : (short)NONE;
	vr_attach_point*   pt     = nullptr;
	vr_holster_anchor* anchor = nullptr;
	const char*        label  = "?";
	vr_tuner_resolve(weapon_type, &pt, &anchor, &label);

	const bool trigDown = offTrigger > 0.5f;
	const bool domDown  = domTrigger > 0.5f;

	// next target
	if (trigDown && !s_tunePrevTrigger) {
		s_tuneTarget = (s_tuneTarget + 1) % vr_tuner_target_count();
		vr_tuner_resolve(weapon_type, &pt, &anchor, &label);
		screen_printf("tune: %s", label);
	}
	// cycle axis pair -- pitch/yaw only means anything on the magwell
	if (offClick && !s_tunePrevClick) {
		s_tuneAxisMode = (s_tuneAxisMode + 1) % VR_TUNE_AXES_COUNT;
		static const char* kAxisNames[VR_TUNE_AXES_COUNT] = { "X/Z", "Y/radius", "pitch/yaw" };
		screen_printf("tune axes: %s", kAxisNames[s_tuneAxisMode]);
	}
	// dump
	if (domDown && !s_tunePrevDomTrigger) {
		const std::string mml = VR_DumpAttachPointsMML();
		// Split on newlines: logcat truncates long lines, and line-per-element is what you want to
		// paste anyway. Goes to both the A1VR tag and the on-device log file.
		size_t start = 0;
		while (start < mml.size()) {
			size_t nl = mml.find('\n', start);
			if (nl == std::string::npos) nl = mml.size();
			if (nl > start) {
				const std::string line = mml.substr(start, nl - start);
				__android_log_print(ANDROID_LOG_INFO, "A1VR", "MML %s", line.c_str());
				logWarning("VRATTACH %s", line.c_str());
			}
			start = nl + 1;
		}
		screen_printf("attach points dumped to log");
	}

	s_tunePrevTrigger    = trigDown;
	s_tunePrevClick      = offClick;
	s_tunePrevDomTrigger = domDown;

	// ---- nudge -------------------------------------------------------------------------------
	// Rate is per-frame and deliberately fine; a full stick deflection moves ~6 cm/sec at 72 Hz.
	const float dead = 0.2f;
	const float sx = (fabsf(offStickX) > dead) ? offStickX : 0.f;
	const float sy = (fabsf(offStickY) > dead) ? offStickY : 0.f;
	if (sx == 0.f && sy == 0.f) return;

	const float linStep = 0.0008f;   // metres per frame at full deflection
	const float angStep = 0.15f;     // degrees per frame at full deflection

	float* a = nullptr; float* b = nullptr; float stepA = linStep, stepB = linStep;
	if (pt) {
		switch (s_tuneAxisMode) {
			case VR_TUNE_AXES_XZ:       a = &pt->x;     b = &pt->z;      break;
			case VR_TUNE_AXES_Y_RADIUS: a = &pt->radius; b = &pt->y;     break;
			case VR_TUNE_AXES_PITCH_YAW:a = &pt->yaw;   b = &pt->pitch;
			                            stepA = stepB = angStep;         break;
		}
		pt->defined = true;   // touching a point authors it
	} else if (anchor) {
		switch (s_tuneAxisMode) {
			case VR_TUNE_AXES_XZ:       a = &anchor->x;      b = &anchor->z; break;
			case VR_TUNE_AXES_Y_RADIUS: a = &anchor->radius; b = &anchor->y; break;
			default: return;   // anchors carry no orientation
		}
	} else {
		return;
	}
	if (a) *a += sx * stepA;
	if (b) *b += sy * stepB;
	if (s_tuneAxisMode == VR_TUNE_AXES_Y_RADIUS && a && *a < 0.01f) *a = 0.01f;  // keep radius sane
}

// ---- VR physical weapons: MAGWELL TUNING mode ------------------------------------------------------
// Entered from the in-game menu (and remembered). The player stands still -- vbl.cpp hands the sticks over
// -- and edits the gun in the primary hand:
//   left stick    X = forward/back along the gun (right = toward the muzzle), Y = up/down
//   left click    next spot on this gun: main magwell -> second well (rifle grenades) -> two-handed grab spot
//   right stick   the selected page -- magwell: SIZE (X width, Y length) / DEPTH+ANGLE (Y depth, X pitch) /
//                 PATH+REACH (Y slide travel, X capture radius); grab spot: Y reach radius, X sideways
//   right click   next page
// Edits become the player's overrides (VR_SetAttachOverride), saved in the preferences a moment after the
// last change, and dumped to the log as pasteable MML when the mode is left.
extern void write_preferences(void);

enum { VR_TUNE_PAGE_SIZE, VR_TUNE_PAGE_DEPTH_ANGLE, VR_TUNE_PAGE_PATH_REACH, VR_TUNE_PAGE_COUNT };
static int  s_mwTuneSel   = 0;
static int  s_mwTunePage  = 0;
static bool s_mwTuneWasOn = false;
static bool s_mwTunePrevClick[2] = { false, false };

// The spots on this weapon that can be tuned, in cycling order. Returns how many.
static int vr_mw_tune_targets(short weapon, int out[3])
{
	int n = 0;
	if (weapon == NONE) return 0;
	if (VR_WeaponNeedsManualReload(weapon) && !VR_WeaponUsesSpinReload(weapon)) {
		out[n++] = VR_ATTACH_MAGWELL;
		const short a0 = get_player_weapon_ammo_type(current_player_index, weapon, _primary_weapon);
		const short a1 = get_player_weapon_ammo_type(current_player_index, weapon, _secondary_weapon);
		if (a1 != NONE && a1 != a0) out[n++] = VR_ATTACH_MAGWELL2;
	}
	if (!VR_IsWeaponTwoHandedDisabled(weapon)) out[n++] = VR_ATTACH_FOREGRIP;
	return n;
}

static const vr_attach_point* vr_mw_effective(short weapon, int which)
{
	if (which == VR_ATTACH_MAGWELL)  return VR_GetMagwellOrDefault(weapon);
	if (which == VR_ATTACH_MAGWELL2) return VR_GetMagwell2OrDefault(weapon);
	return VR_GetForegripOrDefault(weapon);
}

static const char* vr_mw_target_name(int which)
{
	return which == VR_ATTACH_MAGWELL ? "MAIN MAGWELL" : which == VR_ATTACH_MAGWELL2 ? "SECOND WELL" : "GRAB SPOT";
}

static void vr_mw_print(short weapon, int which)
{
	const vr_attach_point* p = vr_mw_effective(weapon, which);
	const char* wname = VR_WeaponMMLName(weapon);
	if (which == VR_ATTACH_FOREGRIP) {
		screen_printf("TUNE %s %s  pos y%+.3f z%+.3f x%+.3f  reach %.1fcm", wname ? wname : "?",
		              vr_mw_target_name(which), p->y, p->z, p->x, p->radius * 100.f);
		return;
	}
	static const char* kPages[VR_TUNE_PAGE_COUNT] = { "SIZE", "DEPTH+ANGLE", "PATH+REACH" };
	const vr_mag_dims md = vr_mag_dims_of(p);
	screen_printf("TUNE %s %s [%s]  pos y%+.3f z%+.3f  %.1fx%.1fx%.1fcm  pitch %.0f  slide %.1fcm  reach %.1fcm",
	              wname ? wname : "?", vr_mw_target_name(which), kPages[s_mwTunePage],
	              p->y, p->z, md.width * 100.f, md.length * 100.f, md.depth * 100.f, p->pitch,
	              p->travel * 100.f, p->radius * 100.f);
}

// Once per frame (eye 0).
static void vr_update_magwell_tuner(void)
{
	const bool on = VR_IsActive() && VR_GetMagwellTuneMode();
	static uint64_t s_lastMs = 0, s_lastEditMs = 0, s_lastPrintMs = 0;
	static bool s_dirty = false, s_editing = false;
	const uint64_t now = machine_tick_count();
	float dt = s_lastMs ? float(now - s_lastMs) / 1000.f : 0.f;
	if (dt < 0.f || dt > 0.05f) dt = 0.05f;
	s_lastMs = now;

	// Nothing is written or printed while a stick is moving (both cost frames on the Quest): the values
	// are printed once the sticks let go, and saved when the mode is left (or the game quit from the menu).
	(void)s_lastPrintMs;
	if (!on) {
		if (s_mwTuneWasOn) {
			if (s_dirty) { write_preferences(); s_dirty = false; }
			vr_log_attach_mml("magwell tuning");
			screen_printf("magwell tuning saved");
		}
		s_mwTuneWasOn = false;
		return;
	}
	if (!s_mwTuneWasOn)
		screen_printf("MAGWELL TUNING: L stick move, L click next spot, R stick resize, R click next page");
	s_mwTuneWasOn = true;

	const int held = VR_GetHeldWeaponHand();
	if (held < 0 || !s_vrWeaponPose[held].valid) return;
	const short weapon = s_vrWeaponPose[held].weapon_type;
	int targets[3];
	const int n = vr_mw_tune_targets(weapon, targets);
	if (n == 0) return;
	static short s_prevWeapon = NONE;
	bool announce = false;
	if (weapon != s_prevWeapon) { s_prevWeapon = weapon; s_mwTuneSel = 0; announce = true; }
	if (s_mwTuneSel >= n) s_mwTuneSel = 0;

	// hand 0 is the left controller, hand 1 the right
	float lx = 0.f, ly = 0.f, rx = 0.f, ry = 0.f;
	bool lc = false, rc = false;
	VR_GetRawHandInput(0, &lx, &ly, nullptr, nullptr, &lc);
	VR_GetRawHandInput(1, &rx, &ry, nullptr, nullptr, &rc);
	if (lc && !s_mwTunePrevClick[0]) { s_mwTuneSel = (s_mwTuneSel + 1) % n; announce = true; }
	if (rc && !s_mwTunePrevClick[1]) { s_mwTunePage = (s_mwTunePage + 1) % VR_TUNE_PAGE_COUNT; announce = true; }
	s_mwTunePrevClick[0] = lc; s_mwTunePrevClick[1] = rc;

	const int which = targets[s_mwTuneSel];
	// Squared, with a deadzone: fine control near the centre, quick at full deflection.
	auto shape = [](float v) { if (fabsf(v) < 0.15f) return 0.f; const float a = (fabsf(v) - 0.15f) / 0.85f; return v < 0 ? -a*a : a*a; };
	lx = shape(lx); ly = shape(ly); rx = shape(rx); ry = shape(ry);

	if (lx != 0.f || ly != 0.f || rx != 0.f || ry != 0.f) {
		vr_attach_point p = *vr_mw_effective(weapon, which);
		const float lin = 0.06f * dt;    // metres per second at full deflection
		const float ang = 30.f * dt;     // degrees per second
		p.z += lx * lin;
		p.y += ly * lin;
		if (which == VR_ATTACH_FOREGRIP) {
			p.radius += ry * lin;
			p.x      += rx * lin;
		} else {
			const vr_mag_dims md = vr_mag_dims_of(&p);   // start from the size actually in use
			p.mag_width = md.width; p.mag_length = md.length; p.mag_depth = md.depth;
			switch (s_mwTunePage) {
				case VR_TUNE_PAGE_SIZE:
					if (md.drum) {
						// a drum: Y = its diameter (both face dimensions together), X = its thickness
						p.mag_width += ry * lin; p.mag_length = p.mag_width; p.mag_depth += rx * lin;
					} else if (md.round) {
						// a canister: X = its diameter, Y = its length
						p.mag_width += rx * lin; p.mag_depth = p.mag_width; p.mag_length += ry * lin;
					} else {
						p.mag_width += rx * lin; p.mag_length += ry * lin;
					}
					break;
				case VR_TUNE_PAGE_DEPTH_ANGLE: p.pitch      += rx * ang; p.mag_depth  += ry * lin; break;
				case VR_TUNE_PAGE_PATH_REACH:  p.radius     += rx * lin; p.travel     += ry * lin; break;
			}
		}
		if (p.mag_width  < 0.01f) p.mag_width  = 0.01f;
		if (p.mag_length < 0.01f) p.mag_length = 0.01f;
		if (p.mag_depth  < 0.01f) p.mag_depth  = 0.01f;
		if (p.travel     < 0.f)   p.travel     = 0.f;
		if (p.radius     < 0.02f) p.radius     = 0.02f;
		if (p.pitch > 80.f) p.pitch = 80.f; else if (p.pitch < -80.f) p.pitch = -80.f;
		VR_SetAttachOverride(weapon, which, p);
		s_dirty = true;
		s_lastEditMs = now;
		s_editing = true;
	} else if (s_editing) {
		s_editing = false;
		announce = true;        // sticks let go: show where it ended up
	}
	if (announce) { vr_mw_print(weapon, which); s_lastPrintMs = now; }
}

// The tuned spots on the held gun: each magwell as its seated magazine, path and capture ring; the grab
// spot as a sphere of its reach. The selected one is bright and pulsing, the rest dim.
static void render_vr_magwell_tuner(view_data* view)
{
	if (!VR_IsActive() || !VR_GetMagwellTuneMode()) return;
	const int held = VR_GetHeldWeaponHand();
	if (held < 0 || !s_vrWeaponPose[held].valid) return;
	const vr_weapon_pose& wp = s_vrWeaponPose[held];
	int targets[3];
	const int n = vr_mw_tune_targets(wp.weapon_type, targets);
	if (n == 0) return;
	const float W = (float)VR_Settings()->worldScaleWUM;
	static int pulse = 0; ++pulse;
	const float amp = 0.7f + 0.3f * sinf(pulse * 0.2f);

	for (int i = 0; i < n; ++i) {
		const int  which = targets[i];
		const bool sel   = (i == s_mwTuneSel % n);
		const float cr = sel ? 1.0f * amp : 0.45f, cg = sel ? 0.25f * amp : 0.45f, cb = sel ? 1.0f * amp : 0.55f;

		std::vector<GLfloat> v;
		auto seg = [&](const float a[3], const float b[3]) {
			v.push_back(a[0]); v.push_back(a[1]); v.push_back(a[2]);
			v.push_back(b[0]); v.push_back(b[1]); v.push_back(b[2]);
		};
		auto ring = [&](const float c[3], const float e0[3], const float e1[3], float r) {
			for (int s = 0; s < 24; ++s) {
				const float t0 = s * 6.2831853f / 24, t1 = (s + 1) * 6.2831853f / 24;
				float a[3], b[3];
				for (int k = 0; k < 3; ++k) {
					a[k] = c[k] + r * (cosf(t0) * e0[k] + sinf(t0) * e1[k]);
					b[k] = c[k] + r * (cosf(t1) * e0[k] + sinf(t1) * e1[k]);
				}
				seg(a, b);
			}
		};

		if (which == VR_ATTACH_FOREGRIP) {
			const vr_attach_point* fg = VR_GetForegripOrDefault(wp.weapon_type);
			float c[3];
			vr_weapon_local_to_world(wp, W, fg->x, fg->y, fg->z, c);
			const float r = fg->radius * W;
			ring(c, wp.right, wp.up, r); ring(c, wp.up, wp.fwd, r); ring(c, wp.fwd, wp.right, r);
		} else {
			vr_well wl;
			if (!vr_get_well(W, -1, which == VR_ATTACH_MAGWELL2 ? 1 : 0, wl)) continue;
			// the seated magazine
			const float down[3] = { -wl.ins[0], -wl.ins[1], -wl.ins[2] };
			vr_draw_mag_box(W, wl.mag, wl.seat, wl.magRight, down, wl.magFwd, cr, cg, cb);
			// the slide path and the capture ring at its mouth
			const float* cap = (wl.travel > 1e-3f) ? wl.entry : wl.seat;
			if (wl.travel > 1e-3f) seg(wl.entry, wl.seat);
			ring(cap, wl.magRight, wl.magFwd, wl.radius);
		}

		glUseProgram(0);
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(sel ? 3.0f : 1.5f);
		glColor4f(cr, cg, cb, sel ? 0.95f : 0.6f);
		if (!v.empty()) {
			glVertexPointer(3, GL_FLOAT, 0, v.data());
			glDrawArrays(GL_LINES, 0, (GLsizei)(v.size() / 3));
		}
		glLineWidth(1.0f);
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glEnable(GL_TEXTURE_2D);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	(void)view;
}

// ---- VR physical weapons: attach-point gizmos --------------------------------------------------
// Aleph One's model loader has no MD3 tag support, so every point where a hand, a magazine or a
// muzzle meets a weapon must be authored by hand in MML (see docs/VR_PHYSICAL_WEAPONS.md). This
// draws those points in world space, on the live weapon, so they can actually be placed. Uses the
// pose captured by render_vr_weapon_sprites_3d, so a gizmo sits exactly where the mesh does.
//
// grip       white cross          foregrip  cyan sphere at its grab radius
// magwell    orange cross + arrow muzzle    red arrow down the barrel
// mag grip   yellow cross         holsters  wireframe boxes, magenta assigned / green unassigned
static void render_vr_attach_gizmos(view_data* view)
{
	if (!VR_IsActive() || !VR_PhysicalWeaponsActive()) return;
	const bool setupBoxes = VR_GetHolsterSetupMode();
	if (!VR_Settings()->showAttachGizmos && !setupBoxes) return;

	const float  W   = (float)VR_Settings()->worldScaleWUM;
	const double yaw = view->virtual_yaw * (360.0 / (double(FIXED_ONE) * double(FULL_CIRCLE))) * (8.0 * atan(1.0) / 360.0);
	const double cyw = cos(yaw), syw = sin(yaw);

	float camx = (float)view->origin.x, camy = (float)view->origin.y, camz = (float)view->origin.z;
	{
		float rcx, rcy, rcz;
		if (VR_GetRenderCamera(&rcx, &rcy, &rcz)) { camx = rcx; camy = rcy; camz = rcz + VR_GetEyeZOffset(); }
	}

	// A body-space offset in metres (+X right, +Y up, +Z forward) -> world WU, anchored at the head.
	// Routed through the same stage->world mapping the hands use, so holsters and hands agree.
	// Same body frame the holsters use, so a gizmo box sits exactly on the weapon it describes.
	vr_body_frame gizmoBody;
	vr_get_body_frame(view, gizmoBody);
	auto bodyToWorld = [&](float bx, float by, float bz, float out[3]) {
		vr_body_to_world(gizmoBody, bx, by, bz, out);
	};

	std::vector<GLfloat> v;
	auto seg = [&](const float a[3], const float b[3]) {
		v.push_back(a[0]); v.push_back(a[1]); v.push_back(a[2]);
		v.push_back(b[0]); v.push_back(b[1]); v.push_back(b[2]);
	};
	auto flush = [&](float r, float g, float b, float a, float width) {
		if (v.empty()) return;
		glColor4f(r, g, b, a);
		glLineWidth(width);
		glVertexPointer(3, GL_FLOAT, 0, v.data());
		glDrawArrays(GL_LINES, 0, (GLsizei)(v.size() / 3));
		v.clear();
	};
	auto cross = [&](const float p[3], float half) {
		for (int ax = 0; ax < 3; ++ax) {
			float a[3] = { p[0], p[1], p[2] }, b[3] = { p[0], p[1], p[2] };
			a[ax] -= half; b[ax] += half;
			seg(a, b);
		}
	};
	// A ring around p, spanned by e0/e1 (world vectors already scaled to the radius).
	auto ring = [&](const float p[3], const float e0[3], const float e1[3], int segs) {
		const float step = 6.2831853f / segs;
		for (int i = 0; i < segs; ++i) {
			const float t0 = i * step, t1 = (i + 1) * step;
			float a[3], b[3];
			for (int k = 0; k < 3; ++k) {
				a[k] = p[k] + e0[k] * cosf(t0) + e1[k] * sinf(t0);
				b[k] = p[k] + e0[k] * cosf(t1) + e1[k] * sinf(t1);
			}
			seg(a, b);
		}
	};
	auto arrow = [&](const float from[3], const float dir[3], float len, const float side[3]) {
		float tip[3], base[3];
		for (int k = 0; k < 3; ++k) { tip[k] = from[k] + dir[k] * len; base[k] = from[k]; }
		seg(base, tip);
		for (int sgn = -1; sgn <= 1; sgn += 2) {
			float h[3];
			for (int k = 0; k < 3; ++k)
				h[k] = tip[k] - dir[k] * (len * 0.25f) + side[k] * (len * 0.12f * sgn);
			seg(tip, h);
		}
	};

	glUseProgram(0);
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// ---- body holster anchors -------------------------------------------------------------
	// Only draw what earns its place. A holster with a weapon actually hanging in it is already
	// obvious from the weapon model, so its box is noise; ammo pouches have nothing to show and the
	// tuning target has to be visible to aim at. Everything else is hidden.
	const int selTarget = s_tuneTarget % (VR_ATTACH_COUNT + VR_HOLSTER_COUNT);
	const int selSlot   = (selTarget >= VR_ATTACH_COUNT) ? (selTarget - VR_ATTACH_COUNT) : -1;
	short shownWeapon[VR_HOLSTER_COUNT];
	vr_visible_holsters(shownWeapon);
	for (int slot = 0; slot < VR_HOLSTER_COUNT; ++slot) {
		const bool isPouch   = VR_HolsterSlotIsPouch(slot);
		const bool isSel     = (slot == selSlot);
		const bool hasWeapon = !isPouch && vr_holster_occupant(slot) != NONE;
		if (!setupBoxes && !isPouch && !isSel) continue;  // outside setup: only pouches + the tuner target
		// Setup: a slot with a weapon hanging in it is shown by that weapon's own outline
		// (render_vr_holstered_weapons), not a box.
		if (setupBoxes && !isPouch && !isSel && shownWeapon[slot] != NONE) continue;
		// Setup: a pouch is drawn as a magazine-shaped box turned the way the pouch is set, so its
		// rotation reads at a glance (white while being moved).
		if (setupBoxes && isPouch) {
			vr_holster_anchor pa;
			VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &pa);
			float pc[3]; bodyToWorld(pa.x, pa.y, pa.z, pc);
			float pr[3], pu[3], pf[3];
			vr_holster_model_basis(gizmoBody, slot, pr, pu, pf);
			const vr_mag_dims md = { 0.05f, 0.12f, 0.035f, false, false };
			float top[3];
			for (int k = 0; k < 3; ++k) top[k] = pc[k] + pu[k] * md.length * 0.5f * W;
			const float down[3] = { -pu[0], -pu[1], -pu[2] };
			const bool moving = (slot == s_setupDragSlot[0] || slot == s_setupDragSlot[1]);
			if (moving) vr_draw_mag_box(W, md, top, pr, down, pf, 1.f, 1.f, 1.f);
			else if (slot == VR_HOLSTER_RIGHT_POUCH) vr_draw_mag_box(W, md, top, pr, down, pf, 1.f, 0.25f, 0.1f);
			else vr_draw_mag_box(W, md, top, pr, down, pf, 1.f, 0.75f, 0.f);
			// vr_draw_mag_box leaves its own GL state; put back what this function's batches expect
			glUseProgram(0);
			glDisable(GL_TEXTURE_2D);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			continue;
		}
		vr_holster_anchor a;
		VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &a);
		float c[3]; bodyToWorld(a.x, a.y, a.z, c);
		const bool  pouch = VR_HolsterSlotIsPouch(slot);
		const float h = a.radius * W;
		// Oriented like the slot: the same basis the weapon hanging there is drawn with, so the box shows
		// the holster's rotation. A pouch is a squat box (thin along its up) with an X on top.
		float ar[3], au[3], af[3];
		vr_holster_model_basis(gizmoBody, slot, ar, au, af);
		const float ex = h, ey = pouch ? h * 0.45f : h, ez = pouch ? h * 0.85f : h;
		auto corner = [&](int sx, int sy, int sz, float out[3]) {
			for (int k = 0; k < 3; ++k)
				out[k] = c[k] + sx * ex * ar[k] + sy * ey * au[k] + sz * ez * af[k];
		};
		for (int axis = 0; axis < 3; ++axis) {
			for (int i = 0; i < 4; ++i) {
				int s0[3], s1[3];
				const int u = (axis + 1) % 3, w2 = (axis + 2) % 3;
				s0[u]  = s1[u]  = (i & 1) ? 1 : -1;
				s0[w2] = s1[w2] = (i & 2) ? 1 : -1;
				s0[axis] = -1; s1[axis] = 1;
				float p0[3], p1[3];
				corner(s0[0], s0[1], s0[2], p0);
				corner(s1[0], s1[1], s1[2], p1);
				seg(p0, p1);
			}
		}
		if (pouch) {
			float t0[3], t1[3], t2[3], t3[3];
			corner(-1, 1, -1, t0); corner(1, 1, 1, t1); corner(-1, 1, 1, t2); corner(1, 1, -1, t3);
			seg(t0, t1); seg(t2, t3);
		}
		if (slot == s_setupDragSlot[0] || slot == s_setupDragSlot[1])
			flush(1.0f, 1.0f, 1.0f, 1.0f, 3.5f);                      // white   = being moved
		else if (pouch)
			flush(slot == VR_HOLSTER_RIGHT_POUCH ? 1.0f : 1.0f,
			      slot == VR_HOLSTER_RIGHT_POUCH ? 0.25f : 0.75f,
			      slot == VR_HOLSTER_RIGHT_POUCH ? 0.1f : 0.0f, 1.0f, 3.5f);   // red / amber pouches
		else if (VR_GetHolsterSlotWeapon(slot) != NONE)
			flush(1.0f, 0.2f, 1.0f, 0.85f, 2.0f);                     // magenta = a weapon is assigned here
		else
			flush(0.2f, 0.9f, 0.3f, 0.45f, 1.0f);                     // green   = unassigned
	}

	// ---- per-weapon attach points ----------------------------------------------------------
	for (int hand = 0; hand < 2 && VR_Settings()->showAttachGizmos; ++hand) {
		const vr_weapon_pose& wp = s_vrWeaponPose[hand];
		if (!wp.valid) continue;

		auto localPt = [&](const vr_attach_point* pt, float out[3]) {
			vr_weapon_local_to_world(wp, W, pt->x, pt->y, pt->z, out);
		};

		// grip -- white cross. Drawn faint at the model origin when not yet authored, so there is
		// always something to aim the tuner at.
		const vr_attach_point* grip = VR_GetAttachPoint(wp.weapon_type, VR_ATTACH_GRIP);
		{
			float p[3];
			if (grip->defined) localPt(grip, p);
			else { p[0] = wp.anchor[0]; p[1] = wp.anchor[1]; p[2] = wp.anchor[2]; }
			cross(p, 0.05f * W);
			flush(1.0f, 1.0f, 1.0f, grip->defined ? 0.95f : 0.35f, 2.0f);
		}

		// foregrip -- cyan sphere (three orthogonal rings) at the grab radius
		const vr_attach_point* fore = VR_GetAttachPoint(wp.weapon_type, VR_ATTACH_FOREGRIP);
		if (fore->defined) {
			float p[3]; localPt(fore, p);
			const float r = fore->radius * W;
			float er[3], eu[3], ef[3];
			for (int k = 0; k < 3; ++k) { er[k] = wp.right[k] * r; eu[k] = wp.up[k] * r; ef[k] = wp.fwd[k] * r; }
			ring(p, er, eu, 20);
			ring(p, eu, ef, 20);
			ring(p, ef, er, 20);
			flush(0.2f, 0.9f, 1.0f, 0.8f, 2.0f);
		}

		// magwell -- orange cross + ring + an arrow along the insertion direction
		const vr_attach_point* mag = VR_GetAttachPoint(wp.weapon_type, VR_ATTACH_MAGWELL);
		if (mag->defined) {
			float p[3]; localPt(mag, p);
			cross(p, 0.03f * W);
			const float r = mag->radius * W;
			float er[3], ef[3];
			for (int k = 0; k < 3; ++k) { er[k] = wp.right[k] * r; ef[k] = wp.fwd[k] * r; }
			ring(p, er, ef, 16);
			// Insertion direction: straight down the weapon's -up, swung by the authored angles.
			const float cp  = cosf(mag->pitch * 0.01745329f), sp  = sinf(mag->pitch * 0.01745329f);
			const float cy2 = cosf(mag->yaw   * 0.01745329f), sy2 = sinf(mag->yaw   * 0.01745329f);
			float dir[3];
			for (int k = 0; k < 3; ++k)
				dir[k] = -wp.up[k] * cp + wp.fwd[k] * sp * cy2 + wp.right[k] * sp * sy2;
			arrow(p, dir, 0.12f * W, wp.right);
			flush(1.0f, 0.55f, 0.1f, 0.9f, 2.5f);
		}

		// second magwell (grenade launcher) -- dark orange cross + ring
		const vr_attach_point* mag2 = VR_GetAttachPoint(wp.weapon_type, VR_ATTACH_MAGWELL2);
		if (mag2->defined) {
			float p[3]; localPt(mag2, p);
			cross(p, 0.03f * W);
			const float r2 = mag2->radius * W;
			float er[3], ef[3];
			for (int k = 0; k < 3; ++k) { er[k] = wp.right[k] * r2; ef[k] = wp.fwd[k] * r2; }
			ring(p, er, ef, 16);
			flush(0.8f, 0.3f, 0.05f, 0.9f, 2.5f);
		}

		// muzzle -- red arrow down the barrel
		const vr_attach_point* muz = VR_GetAttachPoint(wp.weapon_type, VR_ATTACH_MUZZLE);
		if (muz->defined) {
			float p[3]; localPt(muz, p);
			arrow(p, wp.fwd, 0.15f * W, wp.right);
			flush(1.0f, 0.15f, 0.15f, 0.9f, 2.5f);
		}

		// magazine grip point -- yellow cross, drawn relative to the magwell so the two can be
		// eyeballed against each other while tuning.
		const vr_attach_point* mgrip = VR_GetAttachPoint(wp.weapon_type, VR_ATTACH_MAG_GRIP);
		if (mgrip->defined && mag->defined) {
			float base[3]; localPt(mag, base);
			float p[3];
			for (int k = 0; k < 3; ++k)
				p[k] = base[k] + (mgrip->x * wp.right[k] + mgrip->y * wp.up[k] + mgrip->z * wp.fwd[k]) * W;
			cross(p, 0.025f * W);
			flush(1.0f, 0.95f, 0.2f, 0.9f, 2.0f);
		}
	}

	// ---- selected tuning target: pulsing white box, so you can see what you are editing ----
	if (VR_Settings()->showAttachGizmos) {
		static int pulse = 0; ++pulse;
		const float amp = 0.75f + 0.25f * sinf(pulse * 0.25f);
		const int   t   = s_tuneTarget % (VR_ATTACH_COUNT + VR_HOLSTER_COUNT);
		float c[3]; bool have = false; float half = 0.05f * W;

		if (t < VR_ATTACH_COUNT) {
			const int domHand = VR_Settings()->dominantHand ? 0 : 1;
			const vr_weapon_pose& wp = s_vrWeaponPose[domHand];
			if (wp.valid) {
				const vr_attach_point* pt = VR_GetAttachPoint(wp.weapon_type, t);
				if (t == VR_ATTACH_MAG_GRIP) {
					// authored relative to the magwell, so show it there
					const vr_attach_point* mw = VR_GetAttachPoint(wp.weapon_type, VR_ATTACH_MAGWELL);
					float base[3];
					vr_weapon_local_to_world(wp, W, mw->x, mw->y, mw->z, base);
					for (int k = 0; k < 3; ++k)
						c[k] = base[k] + (pt->x * wp.right[k] + pt->y * wp.up[k] + pt->z * wp.fwd[k]) * W;
				} else {
					vr_weapon_local_to_world(wp, W, pt->x, pt->y, pt->z, c);
				}
				have = true;
			}
		} else {
			vr_holster_anchor a;
			VR_GetHolsterAnchorForPlayer(t - VR_ATTACH_COUNT, VR_EyeHeightM(), &a);
			bodyToWorld(a.x, a.y, a.z, c);
			half = a.radius * W * 1.15f;
			have = true;
		}

		if (have) {
			for (int axis = 0; axis < 3; ++axis) {
				for (int i = 0; i < 4; ++i) {
					const int u = (axis + 1) % 3, w2 = (axis + 2) % 3;
					float p0[3] = { c[0], c[1], c[2] }, p1[3] = { c[0], c[1], c[2] };
					p0[u]  += (i & 1) ? half : -half;  p1[u]  = p0[u];
					p0[w2] += (i & 2) ? half : -half;  p1[w2] = p0[w2];
					p0[axis] -= half; p1[axis] += half;
					seg(p0, p1);
				}
			}
			flush(1.0f, 1.0f, 1.0f, amp, 3.0f);
		}
	}

	glLineWidth(1.0f);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

// ---- VR multi-sector visibility sweep ----------------------------------------------------------
// Marathon's visibility cone cannot exceed ~162 deg: build_render_tree projects each endpoint onto a
// 1D screen line (x = half_screen_width + j*world_to_screen_x / i) which requires i > 0 -- in front
// of the view plane -- and precision dies as i approaches 0. That is fine for a flat player, whose
// pitch is limited and whose view therefore always fits one forward cone.
//
// A VR player can look straight down, and then the ground they can see spans the FULL 360 deg of
// azimuth. One cone can never cover that, which is why the floor renders as a forward wedge with
// hard black either side. So sweep the cone across several adjacent sectors and render each. This is
// pitch-driven: level play uses a single sector and pays nothing.
//
// Sector layout (angle units, 512 = full circle; half_cone must stay < 128):
//   sector 0 -> ALWAYS centred on the view, half_cone 115 (+/-81 deg)
//   extras   -> half_cone 86 centred at +/-171, which together with sector 0 closes the full circle
//
// Sector 0 MUST stay centred, because it is the only sector that places objects (see the eye loop):
// anything outside its cone gets no sprites. An earlier version centred the 2-sector case at
// yaw-86/+86, so sector 0 covered [yaw-172, yaw] -- the LEFT half of the view only -- and every
// sprite right of your facing direction silently vanished. That asymmetric tier is gone; sector 0
// now spans the same +/-115 it always did, so sprite coverage is exactly the pre-sweep behaviour.
static int vr_vis_sector_count(const view_data* view)
{
	// Signed pitch: NORMALIZE_ANGLE leaves "looking down" near the top of the 0..511 range.
	int p = (int)view->pitch;
	if (p > NUMBER_OF_ANGLES / 2) p -= NUMBER_OF_ANGLES;
	if (p < 0) p = -p;                        // looking up widens the footprint just as looking down
	// One cone spans +/-81 deg of azimuth. The azimuth a pitched frustum actually needs is
	// atan2(tan h, cos p - tan v * sin p), which passes 81 deg at roughly 36-40 deg of pitch for the
	// headset's ~45 deg vertical half-FOV. Switch at ~34 deg, a little early, and go straight to full
	// coverage -- there is no correct intermediate tier that keeps sector 0 centred.
	return (p < 48) ? 1 : 3;
}

static angle vr_vis_sector_offset(int sector)
{
	return (angle)(sector == 0 ? 0 : (sector == 1 ? 171 : -171));
}

// Sector 0 stays wide (and centred) so it places sprites across the whole forward view; the extras
// only need to be wide enough to close the circle around it.
static angle vr_vis_sector_half_cone(int sector)
{
	return (angle)(sector == 0 ? 115 : 86);
}

// ---- 3D weapon sprites anchored to VR controllers -------------------------------------------
// Replaces render_viewer_sprite_layer() in the VR eye loop. Renders each weapon sprite as a
// world-space quad at the matching controller's aim position. World projection + modelview from
// SetView() are still active (same GL state used by render_vr_aim_debug above).
// sprite 0 = primary weapon → dominant hand controller
// sprite 1 = secondary dual-wield weapon → non-dominant hand controller
static std::vector<int> s_cachedPartF, s_cachedPartC;   // the parts the cached hold redraws

static void render_vr_weapon_sprites_3d(view_data* view)
{
	// Reset before any early return, so a frame that draws no weapon cannot leave stale "this hand
	// already has a fist" flags behind for render_vr_hand_visuals.
	s_vrHandDrewFist[0] = s_vrHandDrewFist[1] = false;

	// Update input-routing flags unconditionally (before any early returns) so the game
	// tick always sees current weapon state regardless of whether weapons are being drawn.
	short weap_type = NONE, weap_mode = 0;
	get_player_weapon_mode_and_type(current_player_index, &weap_type, &weap_mode);
	// Mirror the engine's own dual-wield test (test_raise_double_weapon in weapons.cpp):
	// _twofisted_pistol_class OR (_melee_class AND !marathon_1). Marathon 2/Infinity fists are
	// alternating dual fists (routed to hands by on-screen sprite position below); Marathon 1's
	// fist is a single-hand melee whose idle sprite sits on the left of the screen — treating it
	// as dual would map it to the off-hand, so a lefty would punch with their non-dominant
	// controller. Excluding the M1 fist here sends it down the single-weapon path (hand = domHand).
	const bool weapon_is_dual = (weap_type == _weapon_doublefisted_pistols ||
	                              weap_type == _weapon_doublefisted_shotguns ||
	                              (weap_type == _weapon_fist &&
	                               !weapon_type_is_marathon_1(_weapon_fist)));
	VR_SetIsDualWield(weapon_is_dual);
	// Pistol-style one-handers (magnum, fusion pistol) opt out of two-handed grip via MML
	// (<no_two_handed_weapon>); keep them single-handed even when the hands come together.
	// ...and while the second hand holds a weapon of its own: that grip is holding a gun, not steadying
	// this one, so the two-handed latch must not engage when the hands happen to come close.
	const short vrOffhandWeapon = VR_PhysicalWeaponsActive() ? vr_get_offhand_weapon(current_player_index)
	                                                         : (short)NONE;
	VR_SetTwoHandedDisabled(VR_IsWeaponTwoHandedDisabled(weap_type) || vrOffhandWeapon != NONE);
	// Per-weapon two-handed angle offset (MML <two_handed_offset>): rotates BOTH the weapon model and
	// the firing aim while the two-handed grip is engaged, for grips that don't lie along the barrel
	// (pistol points up; the flamethrower's side handle swings the aim back onto the barrel). Pushed
	// every frame so a weapon with no entry resets it to 0. The VR layer mirrors the yaw for lefties.
	{
		float thPitch = 0.f, thYaw = 0.f;
		VR_GetWeaponTwoHandedOffset(weap_type, &thPitch, &thYaw);
		VR_SetTwoHandedOffset(thPitch, thYaw);
	}
	// Suppress grip-based secondary fire for weapons whose secondary is identical to primary.
	VR_SetGripAltFireEnabled(weap_type != _weapon_fist && weap_type != _weapon_pistol);

	if (!view->show_weapons_in_hand) return;
	if (!VR_IsActive() || !OGL_IsActive()) return;
	if (PLAYER_IS_DEAD(get_player_data(current_player_index))) return;

	const float W   = (float)VR_Settings()->worldScaleWUM;
	const double yaw = view->virtual_yaw
	                 * (360.0 / (double(FIXED_ONE) * double(FULL_CIRCLE)))
	                 * (8.0 * atan(1.0) / 360.0);
	const double cy = cos(yaw), sy = sin(yaw);
	// view->origin is int16-quantized (~1 WU steps) -- anchoring the continuously-tracked
	// controller offset on it reintroduces the same precision loss the wall/lean render-camera
	// fix (VR_GetRenderCamera, see Rasterizer_Shader::SetView) eliminated for world geometry.
	// Use that same continuous float camera here so the weapon doesn't step relative to the
	// (now-smooth) walls and hands as the body/head moves. Z needs the eye-height offset added
	// back (VR_SetRenderCamera publishes body Z only); VR_GetEyeZOffset() is the same latched
	// per-frame value apply_vr_view_offsets folds (as an int16) into origin.z.
	float camx = (float)view->origin.x;
	float camy = (float)view->origin.y;
	// Keep the true eye Z (including VR eye-height offset) so crouch/seated motion lowers hands.
	float camz = (float)view->origin.z;
	{
		float rcx, rcy, rcz;
		if (VR_GetRenderCamera(&rcx, &rcy, &rcz)) {
			camx = rcx;
			camy = rcy;
			camz = rcz + VR_GetEyeZOffset();
		}
	}

	// "domHand" here means the hand holding the PRIMARY weapon. Under physical weapons that is whichever
	// hand grabbed it, not the configured dominant hand; dual pistols, casings and the second hand's
	// weapon all key off it.
	int domHandTmp = VR_Settings()->dominantHand ? 0 : 1;
	const bool vrPhysical = VR_PhysicalWeaponsActive();
	if (vrPhysical && VR_GetHeldWeaponHand() >= 0) domHandTmp = VR_GetHeldWeaponHand();
	const int domHand = domHandTmp;
	const int offHand = 1 - domHand;

	float hp[3];
	if (!VR_GetHeadPosStage(hp)) return;

	// Determine handedness flip for this weapon.
	// dominantHand: 0 = right-handed, 1 = left-handed.
	bool player_left = (VR_Settings()->dominantHand == 1);
	bool naturally_left = VR_IsWeaponNaturallyLeftHanded(weap_type);
	bool should_flip = (player_left != naturally_left);

	// Invalidate last frame's captured weapon poses; each hand re-publishes below if it draws.
	s_vrWeaponPose[0].valid = s_vrWeaponPose[1].valid = false;

	// Per-weapon-sprite VR hand index (not counting shell casings)
	int vrWeaponIdx = 0;
	bool offHandRendered = false;
	short count = 0;
	weapon_display_information display_data;

	// Cache last successfully rendered 3D weapon so we can hold the pose during
	// brief frames where get_weapon_display_information returns 0 items (e.g. empty-click).
	static OGL_ModelData* s_cachedWpnModel    = nullptr;
	static short          s_cachedWpnColl     = NONE;
	static short          s_cachedWpnClut     = 0;
	static int            s_cachedWpnHand     = 0;
	static float          s_cachedWpnAmbient  = 0.5f;
	static int            s_cachedMd3Frame    = 0;
	static bool           s_cachedWpnIsStatic = false;
	static float          s_cachedHh      = 0.0f; // hh at last 3D render, for cached-weapon offset
	// Reload/holster slide fraction, one entry per hand (0=right, 1=left) so dual-wield weapons
	// don't bleed their slide state into each other. Reused by the cached-weapon path below.
	static float          s_lowerFrac[2] = {0.0f, 0.0f};
	bool loopHadItems  = false;
	bool any3DThisCall = false;

	while (get_weapon_display_information(&count, &display_data))
	{
		// Shell casings: render as a camera-facing (billboarded) world-space quad
		// ejecting from the dominant hand.  The 2D screen coords from the engine are
		// mapped to world offsets: horizontal → camera-right, vertical → world-Z (up).
		if ((display_data.interpolation_data & 0x0F) == 1) {
			shape_information_data* csi = extended_get_shape_information(
				display_data.collection, display_data.low_level_shape_index);
			bitmap_definition* cbmp = nullptr;
			void* cshade = nullptr;
			if (csi) extended_get_shape_bitmap_and_shading_table(
				display_data.collection, display_data.low_level_shape_index,
				&cbmp, &cshade, view->shading_mode);
			if (csi && cbmp) {
				float ps[3], fs[3], stage_rc[3], stage_uc[3];
				if (VR_GetAimPoseStage(domHand, ps, fs) &&
				    VR_GetAimOrientStage(domHand, stage_rc, stage_uc)) {
					const float qx  = (ps[0] - hp[0]) * W;
					const float qsy = (ps[1] - hp[1]) * W;
					const float qz  = (ps[2] - hp[2]) * W;
					const float rx  = -qx, ry = -qz;
					const float shx = camx + rx * cy - ry * sy;
					const float shy = camy + rx * sy + ry * cy;
					const float shz = camz + qsy;

					// Weapon right and forward in world space (same stageToWorldDir as main loop)
					const float zx_r = -stage_rc[0], zy_r = -stage_rc[2];
					const float wrx_x = zx_r * cy - zy_r * sy;
					const float wrx_y = zx_r * sy + zy_r * cy;
					const float zx_f = -fs[0], zy_f = -fs[2];
					const float wfwd_x = (float)(zx_f * cy - zy_f * sy);
					const float wfwd_y = (float)(zx_f * sy + zy_f * cy);
					const float wfwd_z = fs[1];

					// Per-weapon forward offset for the eject position (configured via MML vr_casing).
					const float fwd_off = VR_GetWeaponCasingFwdOffset(weap_type) * W;

					// xFrac sign encodes ejection side: +ve = right, −ve = left (reversed)
					// vertical_position = FIXED_ONE − casing_y; small vpos → casing high
					const float xFrac = float(display_data.horizontal_position) / float(FIXED_ONE) - 0.5f;
					const float yFrac = float(FIXED_ONE - display_data.vertical_position) / float(FIXED_ONE);
					// position: eject along weapon right, rise along world Z, plus weapon-forward offset
					const float ox = shx + xFrac * W * 0.5f * wrx_x + fwd_off * wfwd_x;
					const float oy = shy + xFrac * W * 0.5f * wrx_y + fwd_off * wfwd_y;
					const float oz = shz + yFrac * W * 0.4f          + fwd_off * wfwd_z;

					const float cworld_w = fabsf(float(csi->world_right - csi->world_left));
					const float cworld_h = fabsf(float(csi->world_bottom - csi->world_top));
					// Use bitmap pixel dims as fallback to preserve aspect ratio.
					const float raw_w = (cworld_w > 4.0f) ? cworld_w : float(cbmp->width  > 0 ? cbmp->width  : 32);
					const float raw_h = (cworld_h > 4.0f) ? cworld_h : float(cbmp->height > 0 ? cbmp->height : 32);
					const float chw = (raw_w / 1024.0f) * 0.25f * W;
					const float chh = (raw_h / 1024.0f) * 0.25f * W;

					// Spherical billboard: right/up axes from camera→casing vector so the quad
					// faces the eye correctly even with head pitch (common in VR).
					const float tx = ox - camx, ty = oy - camy, tz = oz - camz;
					const float tlen = sqrtf(tx*tx + ty*ty + tz*tz);
					// right = normalize(cross(world_up, T)) = normalize((-ty, tx, 0))
					float brx = -ty, bry = tx;
					const float brlen = sqrtf(brx*brx + bry*bry);
					if (brlen > 1e-4f) { brx /= brlen; bry /= brlen; }
					else               { brx = float(-cy); bry = float(-sy); }
					// up = normalize(cross(T_hat, right))
					const float tinv = tlen > 1e-4f ? 1.f/tlen : 1.f;
					const float tx_n = tx*tinv, ty_n = ty*tinv, tz_n = tz*tinv;
					const float bux = -tz_n*bry, buy = tz_n*brx, buz = tx_n*bry - ty_n*brx;
					const float bulen = sqrtf(bux*bux + buy*buy + buz*buz);
					const float bux_n = bulen > 1e-4f ? bux/bulen : 0.f;
					const float buy_n = bulen > 1e-4f ? buy/bulen : 0.f;
					const float buz_n = bulen > 1e-4f ? buz/bulen : 1.f;
					float cverts[4][3] = {
						{ ox - chw*brx + chh*bux_n, oy - chw*bry + chh*buy_n, oz + chh*buz_n },  // TL
						{ ox + chw*brx + chh*bux_n, oy + chw*bry + chh*buy_n, oz + chh*buz_n },  // TR
						{ ox + chw*brx - chh*bux_n, oy + chw*bry - chh*buy_n, oz - chh*buz_n },  // BR
						{ ox - chw*brx - chh*bux_n, oy - chw*bry - chh*buy_n, oz - chh*buz_n }   // BL
					};

					rectangle_definition crect;
					crect.ModelPtr        = nullptr;
					crect.Opacity         = 1;
					crect.ShapeDesc       = BUILD_DESCRIPTOR(display_data.collection, 0);
					crect.LowLevelShape   = display_data.low_level_shape_index;
					crect.texture         = cbmp;
					crect.shading_tables  = cshade;
					crect.transfer_mode   = display_data.transfer_mode;
					crect.transfer_data   = 0;
					crect.flags           = 0;
					crect.flip_horizontal = display_data.flip_horizontal;
					crect.flip_vertical   = display_data.flip_vertical;
					crect.depth           = 0;
					crect.ambient_shade   = get_light_intensity(
						get_polygon_data(view->origin_polygon_index)->floor_lightsource_index);
					crect.ambient_shade   = MAX(csi->minimum_light_intensity, crect.ambient_shade);
					if (view->shading_mode == _shading_infravision) crect.flags |= _SHADELESS_BIT;
					instantiate_rectangle_transfer_mode(view, &crect,
						display_data.transfer_mode, display_data.transfer_phase);
					OGL_RenderVRWeaponQuad(crect, cverts);
				}
			}
			continue;  // don't advance vrWeaponIdx for casings
		}
		loopHadItems = true;

		shape_information_data* si =
			extended_get_shape_information(display_data.collection,
			                               display_data.low_level_shape_index);
		if (!si) { vrWeaponIdx++; continue; }

		bitmap_definition* bmp = nullptr;
		void* shade_table = nullptr;
		extended_get_shape_bitmap_and_shading_table(
			display_data.collection, display_data.low_level_shape_index,
			&bmp, &shade_table, view->shading_mode);
		if (!bmp) { vrWeaponIdx++; continue; }

		// Dual pistols: use horizontal position — separation puts primary right-of-centre
		// (→ domHand) and secondary left-of-centre (→ offHand). Single pistol after one
		// runs dry: the engine repositions the survivor to idle_width (exact centre), so
		// position is useless; use trigger validity instead to decide which hand survived.
		// The weapon this item belongs to: the second hand's weapon for its items, else the primary
		// (with the dual-wield pseudo-types folded back to the real weapon for attach-point lookups).
		const bool itemOffhand = display_data.vr_offhand;
		short itemWeapon = itemOffhand ? display_data.vr_weapon_type : weap_type;
		if (itemWeapon == _weapon_doublefisted_pistols)  itemWeapon = _weapon_pistol;
		if (itemWeapon == _weapon_doublefisted_shotguns) itemWeapon = _weapon_shotgun;

		int hand;
		if (itemOffhand) {
			hand = offHand;                      // the second hand's own weapon
		} else if (weapon_is_dual) {
			bool sprite_is_left;
			if      (display_data.horizontal_positioning_mode == _position_low)
				sprite_is_left = true;
			else if (display_data.horizontal_positioning_mode == _position_high)
				sprite_is_left = false;
			else    // _position_center: fraction of screen; separation offsets are ±8192 from 32768
				sprite_is_left = (display_data.horizontal_position < (FIXED_ONE / 2));
			hand = sprite_is_left ? offHand : domHand;
		} else if (vrPhysical && VR_GetHeldWeaponHand() >= 0) {
			// Physical weapons: the weapon lives in whichever hand picked it up. Checked BEFORE the
			// lone-pistol heuristic below, which would otherwise put a pistol grabbed with the
			// non-dominant hand back in the dominant one.
			hand = VR_GetHeldWeaponHand();
		} else if (weap_type == _weapon_pistol) {
			// Check which trigger slot is still loaded.
			// Primary (trigger 0) = dominant hand; secondary (trigger 1) = off-hand.
			// If primary emptied and secondary is still alive, the off-hand pistol survived.
			const bool primary_valid   = get_player_weapon_drawn(current_player_index, _weapon_pistol, 0);
			const bool secondary_valid = get_player_weapon_drawn(current_player_index, _weapon_pistol, 1);
			hand = (!primary_valid && secondary_valid) ? offHand : domHand;
		} else {
			hand = domHand;
		}
		if (itemWeapon == _weapon_fist && (hand == 0 || hand == 1)) s_vrHandDrewFist[hand] = true;
		// Weapon handling mode draws BOTH fists itself (render_vr_hand_visuals), by the same per-hand rule
		// whether or not the other hand holds a gun. The engine's own empty-handed fists are routed by
		// screen side and mirrored by configured handedness -- the stock mapping -- and came out reversed.
		if (vrPhysical && itemWeapon == _weapon_fist) {
			s_vrHandDrewFist[hand] = false;
			vrWeaponIdx++;
			continue;
		}
		// Weapon handling: a hand draws only the gun it is actually holding. Letting go, the engine keeps the
		// gun as its current weapon until the switch to the fist lands a moment later -- drawing it through
		// that gap showed it in the hand AND back on the holster at once, and cancelled the swing back.
		if (vrPhysical) {
			short heldW = NONE, heldCopy = 0;
			if (!vr_hand_gun(current_player_index, hand, &heldW, &heldCopy) || heldW != itemWeapon) {
				vrWeaponIdx++;
				continue;
			}
		}

		float ps[3], fs[3];
		if (!VR_GetAimPoseStage(hand, ps, fs)) { vrWeaponIdx++; continue; }
		float stage_right[3], stage_up[3];
		if (!VR_GetAimOrientStage(hand, stage_right, stage_up)) { vrWeaponIdx++; continue; }

		// Track effective stage-space forward for 3D model rendering:
		// defaults to pitch-adjusted aim forward, overridden by two-handed direction below.
		float stage_fwd_eff[3] = { fs[0], fs[1], fs[2] };

		// Two-handed steadying: replace orientation with inter-hand vector + dominant roll.
		// Only for single-hand weapons (dual-wield keeps independent per-hand orientation).
		if (!weapon_is_dual && !itemOffhand && VR_IsTwoHandedActive()) {
			float th_fwd[3];
			if (VR_GetTwoHandedFwdStage(th_fwd)) {
				float new_right[3], new_up[3];
				vr_two_handed_model_basis(th_fwd, stage_right, stage_up, new_right, new_up);
				stage_right[0] = new_right[0]; stage_right[1] = new_right[1]; stage_right[2] = new_right[2];
				stage_up[0]    = new_up[0];    stage_up[1]    = new_up[1];    stage_up[2]    = new_up[2];
				stage_fwd_eff[0] = th_fwd[0];  stage_fwd_eff[1] = th_fwd[1];  stage_fwd_eff[2] = th_fwd[2];
			}
		}

		// Controller world position: stage delta -> world (Z-up, Marathon WU)
		// stage(metres, Y-up) -> world: (dx,dy,dz) -> (-dx,-dz,dy) then Rz(yaw)
		const float qx  = (ps[0] - hp[0]) * W;
		const float qsy = (ps[1] - hp[1]) * W;
		const float qz  = (ps[2] - hp[2]) * W;
		const float rx = -qx, ry = -qz, rz = qsy;
		// Non-const: the in-hand recoil flutter (fine_flutter) nudges this anchor along the weapon axes.
		float cwx = camx + (float)(rx * cy - ry * sy);
		float cwy = camy + (float)(rx * sy + ry * cy);
		float cwz = camz + rz;

		// Transform stage right/up direction vectors to Marathon world space.
		// Same mapping: (sx,sy,sz) -> (-sx,-sz,sy) then Rz(yaw)
		auto stageToWorldDir = [&](const float s[3], float w[3]) {
			const float zx = -s[0], zy = -s[2], zz = s[1];
			w[0] = (float)(zx * cy - zy * sy);
			w[1] = (float)(zx * sy + zy * cy);
			w[2] = zz;
		};
		float wrx[3], wup[3];
		stageToWorldDir(stage_right, wrx);
		stageToWorldDir(stage_up,    wup);

		// 3D model basis: forward from stage_fwd_eff (pitch-adjusted aim, or th_fwd for
		// two-handed). Recompute up from cross(wfwd, wrx) to guarantee orthonormality.
		float wfwd_mdl[3], wup_mdl[3];
		stageToWorldDir(stage_fwd_eff, wfwd_mdl);
		wup_mdl[0] = wfwd_mdl[1]*wrx[2] - wfwd_mdl[2]*wrx[1];
		wup_mdl[1] = wfwd_mdl[2]*wrx[0] - wfwd_mdl[0]*wrx[2];
		wup_mdl[2] = wfwd_mdl[0]*wrx[1] - wfwd_mdl[1]*wrx[0];

		// Sprite half-sizes in Marathon world units.
		const float spriteScaleM = 0.5f;
		float world_w = fabsf(float(si->world_right - si->world_left));
		float world_h = fabsf(float(si->world_bottom - si->world_top));
		if (world_w < 4.0f) world_w = 512.0f;
		if (world_h < 4.0f) world_h = 512.0f;
		const float hw = (world_w / 1024.0f) * spriteScaleM * W * 0.5f;
		const float hh = (world_h / 1024.0f) * spriteScaleM * W * 0.5f;

		// Physical weapons: a sprite faces whichever hand holds it. Drop the engine's own side-flip (it
		// marks the second dual pistol as "the left one", which is only true for a right-hander holding
		// them the stock way) and mirror purely by hand + the weapon's authored handedness. For the stock
		// cases this reproduces the old result exactly.
		if (vrPhysical) display_data.flip_horizontal = false;

		// Apply shape mirror flags
		if (si->flags & _X_MIRRORED_BIT) display_data.flip_horizontal = !display_data.flip_horizontal;
		if (si->flags & _Y_MIRRORED_BIT) display_data.flip_vertical   = !display_data.flip_vertical;

		if (vrPhysical) {
			if ((hand == 0) != VR_IsWeaponNaturallyLeftHanded(itemWeapon))
				display_data.flip_horizontal = !display_data.flip_horizontal;
		} else if (should_flip)
			display_data.flip_horizontal = !display_data.flip_horizontal;

		// Reload/holster slide: vpos > idle_height → descend; vpos >= kHideV → hidden. Now that the
		// view-elevation term is excluded from vertical_position in VR (see calculate_weapon_position_for_idle),
		// vpos only rises above idle_height during genuine reload/lower/raise weapon STATES, so this slide
		// tracks those without the spurious view-pitch drop. After un-hiding, vpos snaps back to idle_height
		// for one frame before the raise animation; s_lowerFrac is set to 1.0 while hidden and decays one
		// step per game tick so the weapon rises smoothly from below the hand regardless of the vpos pattern.
		float cwz_slid = cwz;
		{
			const float kIdleV = float(display_data.idle_height);
			const float kHideV = float(3 * FIXED_ONE / 2);
			const float vpos   = float(display_data.vertical_position);

			if (display_data.fine_flutter) {
				// In-hand recoil flutter (firing). The engine baked a small per-tick random shake into
				// horizontal/vertical_position (quarter-amplitude in VR, and lerped between ticks). Apply it
				// as a FINE world-space offset along BOTH weapon axes — unlike the 2D HUD we keep the
				// horizontal component, and float world units give far finer increments than the one-sided
				// slide. Firing is never a lowering state, so the slide stays disengaged.
				//
				// Scale is DERIVED from the 2D path, not the holster drop: position_sprite_axis draws a
				// _position_center weapon with its centre at screen_h*(vertical_position/FIXED_ONE) and a
				// sprite height of world_h*screen_h/WORLD_ONE, so a vertical_position delta D shifts the
				// weapon by D/FIXED_ONE of the screen — a fixed fraction of the weapon whatever its size.
				// Mapping that same fraction onto the model's world height (2*hh) cancels world_h and leaves
				// D * spriteScaleM * W / FIXED_ONE. This matches the 2D shake as a fraction of the gun for
				// every weapon (the old hh*4/(kHideV-kIdleV) scale grew with sprite size, so big sprites like
				// the assault rifle shook violently). The VR quarter-amplitude flutter then reads as ~1/4 of
				// the 2D recoil shake, consistently.
				const float flutterToWorld = spriteScaleM * W / float(FIXED_ONE);
				const float ofv = (vpos - kIdleV) * flutterToWorld;               // +vpos = lower on 2D → -up
				// The engine flutters width at 1/4 the height amplitude (>>6 vs >>4); faithful to the 2D
				// sprite but ~1mm in VR, so it looks like pure up/down. Boost horizontal to parity so the
				// recoil reads as a 3D shake. kHorizFlutterBoost=1 restores the native 2D ratio.
				const float kHorizFlutterBoost = 4.0f;
				const float ofh = weapon_is_dual ? 0.0f
				                : (float(display_data.horizontal_position) - float(display_data.idle_width)) * flutterToWorld * kHorizFlutterBoost;
				cwx += ofh * wrx[0] - ofv * wup_mdl[0];
				cwy += ofh * wrx[1] - ofv * wup_mdl[1];
				cwz += ofh * wrx[2] - ofv * wup_mdl[2];
				s_lowerFrac[hand] = 0.0f;
				cwz_slid = cwz;
			} else if (VR_PhysicalWeaponsActive()) {
				// Physical weapons: a weapon is either in your hand or on your body, never sliding
				// between the two. Pin the slide off so no stray lowering/raising frame can show --
				// belt and braces with the instant state transitions in lower_weapon /
				// change_to_desired_weapon, which would otherwise flash one near-stowed frame.
				s_lowerFrac[hand] = 0.0f;
				cwz_slid = cwz;
			} else {
				if (vpos >= kHideV) { s_lowerFrac[hand] = 1.0f; vrWeaponIdx++; continue; }  // fully stowed → hide
				// Track the engine's lowering directly: vertical_position ramps from idle_height (weapon at
				// the hand) up toward kHideV (holstered) during reload / weapon-switch, so descentFrac maps
				// straight to how far below the hand to draw the gun. No per-tick decay smoothing (the old
				// version stalled in VR: the idle bob is zeroed so vpos is perfectly constant at idle, and its
				// decay gate `vpos != prev` then never fired, freezing the gun below the hand). The engine
				// already ramps vertical_position smoothly, so direct tracking is smooth on its own.
				s_lowerFrac[hand] = (vpos > kIdleV) ? (vpos - kIdleV) / (kHideV - kIdleV) : 0.0f;
				cwz_slid = cwz - s_lowerFrac[hand] * hh * 4.0f;
			}
		}

		// Arm-drop rotation: as the weapon lowers, pitch the whole weapon forward/down about
		// its right axis (wrx) so it reads as the arm dropping at the wrist, not just a vertical
		// slide. Rotate the model basis (wfwd_mdl/wup_mdl) in place about the hand anchor; the
		// sprite quad below uses wup_mdl too, so it tilts to match. Scales with s_lowerFrac so it
		// eases in with the descent and unwinds as the weapon rises back to idle.
		if (s_lowerFrac[hand] > 0.0f) {
			const float kLowerPitchDeg = 50.0f;  // max forward pitch at full descent
			const float ang = s_lowerFrac[hand] * kLowerPitchDeg * 0.01745329252f;  // deg->rad
			const float cs = cosf(ang), sn = sinf(ang);
			float nf[3], nu[3];
			for (int i = 0; i < 3; ++i) {
				nf[i] = wfwd_mdl[i] * cs - wup_mdl[i] * sn;  // forward tilts toward -up (muzzle down)
				nu[i] = wfwd_mdl[i] * sn + wup_mdl[i] * cs;
			}
			wfwd_mdl[0] = nf[0]; wfwd_mdl[1] = nf[1]; wfwd_mdl[2] = nf[2];
			wup_mdl[0]  = nu[0]; wup_mdl[1]  = nu[1]; wup_mdl[2]  = nu[2];
		}

		// Publish this hand's final weapon pose for the attach-point gizmos / interaction tests.
		// Uses cwz_slid (not cwz) so model-local points follow the reload/holster slide, and the
		// post-arm-drop basis so they follow the wrist rotation too.
		if (hand >= 0 && hand < 2) {
			vr_weapon_pose& wp = s_vrWeaponPose[hand];
			wp.valid       = true;
			wp.weapon_type = itemWeapon;
			wp.anchor[0]   = cwx; wp.anchor[1] = cwy; wp.anchor[2] = cwz_slid;
			for (int i = 0; i < 3; ++i) {
				wp.right[i] = wrx[i];
				wp.up[i]    = wup_mdl[i];
				wp.fwd[i]   = wfwd_mdl[i];
			}

			// DIAGNOSTIC (2026-10-04, "jerky when examining a gun up close"): once per frame per hand, the raw
			// controller + head stage positions, the render camera and where the weapon was placed, with a
			// microsecond clock -- to tell tracking jitter, frame-timing jitter and our own transform apart.
			// logcat only (A1VRJIT), not the log file.
			if (kVRJitterDiag) {
				static uint32_t s_jitLogged[2] = { 0, 0 };
				if (s_jitLogged[hand] != s_vrFrameNo) {
					s_jitLogged[hand] = s_vrFrameNo;
					const long long us = (long long)std::chrono::duration_cast<std::chrono::microseconds>(
						std::chrono::steady_clock::now().time_since_epoch()).count();
					__android_log_print(ANDROID_LOG_INFO, "A1VRJIT",
						"f=%u t=%lld h=%d hand=(%.5f,%.5f,%.5f) head=(%.5f,%.5f,%.5f) cam=(%.3f,%.3f,%.3f) wpn=(%.3f,%.3f,%.3f) fwd=(%.4f,%.4f,%.4f)",
						s_vrFrameNo, us, hand, ps[0], ps[1], ps[2], hp[0], hp[1], hp[2], camx, camy, camz,
						cwx, cwy, cwz_slid, wfwd_mdl[0], wfwd_mdl[1], wfwd_mdl[2]);
				}
			}

			// Just drawn from a holster? Fly in from it (render only; wp above stays the real hand).
			if (vrPhysical && itemWeapon >= 0 && itemWeapon < MAXIMUM_NUMBER_OF_WEAPONS) {
				vr_pose_snap& snap = s_handSnap[hand][itemWeapon];
				if (!(snap.wt == itemWeapon && s_vrFrameNo - snap.frame <= 1)) {
					for (int slot = 0; slot < VR_HOLSTER_COUNT; ++slot)
						if (vr_snap_just_left(s_holsterSnap[slot], itemWeapon)) {
							vr_swing_start(s_handSwing[hand], s_holsterSnap[slot]);
							break;
						}
				}
				const float p[3] = { cwx, cwy, cwz_slid };
				vr_snap_set(snap, itemWeapon, p, wrx, wup_mdl, wfwd_mdl);
				float bp[3] = { cwx, cwy, cwz_slid };
				if (vr_apply_swing(s_handSwing[hand], bp, wrx, wup_mdl, wfwd_mdl)) {
					cwx = bp[0]; cwy = bp[1]; cwz_slid = bp[2];
				}
			}
		}

		// Quad corners in world space. Use wup_mdl (cross(wfwd_eff, wrx)) so the sprite
		// rotates with aimPitchAdjust, matching the 3D model orientation.
		float verts[4][3] = {
			{ cwx - hw*wrx[0] + hh*wup_mdl[0], cwy - hw*wrx[1] + hh*wup_mdl[1], cwz_slid - hw*wrx[2] + hh*wup_mdl[2] },  // TL
			{ cwx + hw*wrx[0] + hh*wup_mdl[0], cwy + hw*wrx[1] + hh*wup_mdl[1], cwz_slid + hw*wrx[2] + hh*wup_mdl[2] },  // TR
			{ cwx + hw*wrx[0] - hh*wup_mdl[0], cwy + hw*wrx[1] - hh*wup_mdl[1], cwz_slid + hw*wrx[2] - hh*wup_mdl[2] },  // BR
			{ cwx - hw*wrx[0] - hh*wup_mdl[0], cwy - hw*wrx[1] - hh*wup_mdl[1], cwz_slid - hw*wrx[2] - hh*wup_mdl[2] }   // BL
		};

		rectangle_definition rect;
		rect.ModelPtr      = nullptr;
		rect.Opacity       = 1;
		rect.ShapeDesc     = BUILD_DESCRIPTOR(display_data.collection, 0);
		rect.LowLevelShape = display_data.low_level_shape_index;
		rect.texture       = bmp;
		rect.shading_tables = shade_table;
		rect.transfer_mode = display_data.transfer_mode;
		rect.transfer_data = 0;
		rect.flags         = 0;
		rect.flip_horizontal = display_data.flip_horizontal;
		rect.flip_vertical   = display_data.flip_vertical;
		rect.depth = 0;
		rect.ambient_shade = get_light_intensity(
			get_polygon_data(view->origin_polygon_index)->floor_lightsource_index);
		rect.ambient_shade = MAX(si->minimum_light_intensity, rect.ambient_shade);
		if (view->shading_mode == _shading_infravision) rect.flags |= _SHADELESS_BIT;
		instantiate_rectangle_transfer_mode(view, &rect,
			display_data.transfer_mode, display_data.transfer_phase);

		const bool isStaticWpn = (rect.transfer_mode == _static_transfer);

		short modelSeq = NONE;
		vector<OGL_ModelData>* allModels = OGL_GetAllModels(
			display_data.collection, display_data.shape_index, modelSeq);
		OGL_ModelData* weaponMdl = (allModels && !allModels->empty()) ? &(*allModels)[0] : nullptr;
		if (weaponMdl && weaponMdl->SpecificFrame >= 0 &&
		    weaponMdl->SpecificFrame != display_data.Frame)
			weaponMdl = nullptr;
		auto md3Params = [&](OGL_ModelData* mdl, int& frame, float& mix, int& next) {
			int rawFrame = display_data.Frame;
			int rawNext  = display_data.NextFrame;
			if (mdl->MD3FrameCount > 0) {
				const int cap = mdl->MD3FrameCount - 1;
				rawFrame = rawFrame > cap ? cap : rawFrame;
				rawNext  = rawNext  > cap ? cap : rawNext;
			}
			frame = mdl->FirstMD3Frame + rawFrame;
			if (mdl->BlendMD3Frames && display_data.Ticks > 0 && rawNext > rawFrame) {
				mix  = float(display_data.Phase) / float(display_data.Ticks);
				next = mdl->FirstMD3Frame + rawNext;
			} else {
				mix  = 0.f;
				next = -1;
			}
		};
		// TEMP DIAGNOSTIC: draw a procedural long blue box instead of the weapon model/sprite,
		// using the identical anchor+basis. If the box glitches on the pistol but not the assault
		// rifle, the fault is in the transform inputs, not the .md3 asset. Flip to false to restore
		// normal weapon rendering.
		static const bool s_vrDebugWeaponBox = false;
		if (s_vrDebugWeaponBox) {
			OGL_RenderVRDebugBox(cwx, cwy, cwz_slid, wrx, wup_mdl, wfwd_mdl);
			if (weapon_is_dual && hand == offHand) offHandRendered = true;
			vrWeaponIdx++;
			continue;
		}

		int pFrame = 0; float pMix = 0.f; int pNext = -1;
		if (weaponMdl) md3Params(weaponMdl, pFrame, pMix, pNext);
		// Weapon handling: a gun model with separate magazine parts draws only what is in it right now.
		std::vector<int> gunPartF, gunPartC;
		bool gunParts = false;
		{
			short hw = NONE, hc = 0;
			if (vrPhysical && weaponMdl && vr_hand_gun(current_player_index, hand, &hw, &hc) && hw == itemWeapon)
				gunParts = vr_gun_parts_now(weaponMdl, hw, hc, gunPartF, gunPartC);
		}
		if (gunParts) OGL_SetVRWeaponModelParts(gunPartF.data(), gunPartC.data(), (int)gunPartF.size());
		bool renderedAs3D = weaponMdl &&
			OGL_RenderVRWeaponModel(rect, display_data.collection, 0 /*CLUT*/,
				weaponMdl, cwx, cwy, cwz_slid, wrx, wup_mdl, wfwd_mdl, isStaticWpn,
				pFrame, pMix, pNext);
		if (gunParts) OGL_ClearVRWeaponModelParts();
		s_cachedPartF = gunParts ? gunPartF : std::vector<int>();
		s_cachedPartC = gunParts ? gunPartC : std::vector<int>();
		if (renderedAs3D) {
			for (size_t mi = 1; allModels && mi < allModels->size(); mi++) {
				OGL_ModelData* ovl = &(*allModels)[mi];
				if (ovl->SpecificFrame >= 0 && ovl->SpecificFrame != display_data.Frame)
					continue;
				int oFrame; float oMix; int oNext;
				md3Params(ovl, oFrame, oMix, oNext);
				OGL_RenderVRWeaponModel(rect, display_data.collection, 0,
					ovl, cwx, cwy, cwz_slid, wrx, wup_mdl, wfwd_mdl, isStaticWpn,
					oFrame, oMix, oNext);
			}
			s_cachedWpnModel    = weaponMdl;
			s_cachedWpnColl     = display_data.collection;
			s_cachedWpnClut     = 0;
			s_cachedWpnHand     = hand;
			s_cachedWpnAmbient  = float(rect.ambient_shade) / float(FIXED_ONE);
			s_cachedMd3Frame    = pFrame;
			s_cachedWpnIsStatic = isStaticWpn;
			s_cachedHh          = hh;
			any3DThisCall = true;
		}
		if (!renderedAs3D)
			OGL_RenderVRWeaponQuad(rect, verts);
		if (weapon_is_dual && hand == offHand) offHandRendered = true;
		vrWeaponIdx++;
	}

	// If the game provided no display items this frame but we have a cached 3D weapon,
	// recalculate its world transform from current VR pose and hold it visible.
	if (!loopHadItems && s_cachedWpnModel) {
		float ps[3], fs[3], stage_right[3], stage_up[3];
		if (VR_GetAimPoseStage(s_cachedWpnHand, ps, fs) &&
		    VR_GetAimOrientStage(s_cachedWpnHand, stage_right, stage_up))
		{
			float stage_fwd_eff[3] = { fs[0], fs[1], fs[2] };
			if (!weapon_is_dual && VR_IsTwoHandedActive()) {
				float th_fwd[3];
				if (VR_GetTwoHandedFwdStage(th_fwd)) {
					float new_right[3], new_up[3];
					vr_two_handed_model_basis(th_fwd, stage_right, stage_up, new_right, new_up);
					stage_right[0]=new_right[0]; stage_right[1]=new_right[1]; stage_right[2]=new_right[2];
					stage_up[0]=new_up[0];       stage_up[1]=new_up[1];       stage_up[2]=new_up[2];
					stage_fwd_eff[0]=th_fwd[0];  stage_fwd_eff[1]=th_fwd[1];  stage_fwd_eff[2]=th_fwd[2];
				}
			}
			const float qx  = (ps[0] - hp[0]) * W;
			const float qsy = (ps[1] - hp[1]) * W;
			const float qz  = (ps[2] - hp[2]) * W;
			const float rx = -qx, ry = -qz, rz = qsy;
			const float cwx = camx + (float)(rx * cy - ry * sy);
			const float cwy = camy + (float)(rx * sy + ry * cy);
			const float cwz = camz + rz;
			auto stageToWorldDir = [&](const float s[3], float w[3]) {
				const float zx = -s[0], zy = -s[2], zz = s[1];
				w[0] = (float)(zx * cy - zy * sy);
				w[1] = (float)(zx * sy + zy * cy);
				w[2] = zz;
			};
			float wrx[3], wfwd_mdl[3], wup_mdl[3];
			stageToWorldDir(stage_right,   wrx);
			stageToWorldDir(stage_fwd_eff, wfwd_mdl);
			wup_mdl[0] = wfwd_mdl[1]*wrx[2] - wfwd_mdl[2]*wrx[1];
			wup_mdl[1] = wfwd_mdl[2]*wrx[0] - wfwd_mdl[0]*wrx[2];
			wup_mdl[2] = wfwd_mdl[0]*wrx[1] - wfwd_mdl[1]*wrx[0];
			// Match the arm-drop pitch from the live path so a cached hold doesn't pop upright.
			const float cachedLower = s_lowerFrac[s_cachedWpnHand];
			if (cachedLower > 0.0f) {
				const float ang = cachedLower * 50.0f * 0.01745329252f;
				const float cs = cosf(ang), sn = sinf(ang);
				float nf[3], nu[3];
				for (int i = 0; i < 3; ++i) {
					nf[i] = wfwd_mdl[i] * cs - wup_mdl[i] * sn;
					nu[i] = wfwd_mdl[i] * sn + wup_mdl[i] * cs;
				}
				wfwd_mdl[0]=nf[0]; wfwd_mdl[1]=nf[1]; wfwd_mdl[2]=nf[2];
				wup_mdl[0]=nu[0];  wup_mdl[1]=nu[1];  wup_mdl[2]=nu[2];
			}
			rectangle_definition rect;
			rect.ambient_shade = (short)(s_cachedWpnAmbient * float(FIXED_ONE));
			if (!s_cachedPartF.empty())
				OGL_SetVRWeaponModelParts(s_cachedPartF.data(), s_cachedPartC.data(), (int)s_cachedPartF.size());
			OGL_RenderVRWeaponModel(rect, s_cachedWpnColl, s_cachedWpnClut,
				s_cachedWpnModel, cwx, cwy, cwz - s_lowerFrac[s_cachedWpnHand] * s_cachedHh * 4.0f,
				wrx, wup_mdl, wfwd_mdl, s_cachedWpnIsStatic, s_cachedMd3Frame);
			OGL_ClearVRWeaponModelParts();
		}
	}

	// Tell VR whether the off-hand actually has a weapon (suppresses its trigger when absent).
	VR_SetOffHandHasWeapon(!weapon_is_dual || offHandRendered);

	// Restore GL state for subsequent passes (HUD layer, etc.)
	glUseProgram(0);  // engine shaders (e.g. S_Invincible from VR static mode) may still be bound
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_ALPHA_TEST);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}
#endif // __ANDROID__

#ifdef HAVE_OPENGL
// Toggle for the network VR pose debug lines below. OFF for release -- the wire is validated (bullets
// verified on 2 machines), so the head/gun debug rays are no longer drawn. Kept (not deleted) as a
// diagnostic for future VR netcode work; flip to true to see remote players' head/gun aim again.
bool debug_show_net_vr_pose = false;

// ---- Network players' VR pose debug lines ---------------------------------------------------
// Phase-1 correctness proof for the VR netcode extension (docs/VR_NETCODE.md). For every OTHER network
// player, draw world-space lines from their eye along their HEAD view direction and each GUN aim, taken
// straight from the VR block they sent over the wire (vr_net_get_player_block). If these track a remote
// VR player's real head/guns -- and a flat PC player's rays point along that player's facing (graceful
// fallback) -- the wire is delivering correct pose data. Depth test off so lines are always visible.
// Runs on BOTH the Quest (VR eye loop) and the flat PC (main view) whenever OpenGL is the renderer and
// the game is a VR-netcode netgame. Absolute world coords are correct because SetView() puts the camera
// translation into the GL_MODELVIEW stack (Rasterizer_Shader.cpp), so glUseProgram(0) fixed-function
// lines land in world space; on the Quest VR_BeginEye sets the per-eye matrices the same way.
static void render_net_vr_pose(view_data* /*view*/)
{
	if (!OGL_IsActive()) return;
	if (!debug_show_net_vr_pose) return;
	if (!game_is_networked || !vr_net_is_active()) return;

#if defined(__ANDROID__)
	const double W = VR_IsActive() ? VR_Settings()->worldScaleWUM : 1.0;
#else
	const double W = 1.0;   // flat PC: world units == render units
#endif
	const double TWO_PI = 8.0 * atan(1.0);

	glUseProgram(0);
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(2.0f);

	// Marathon angle (512-unit circle) -> unit world direction.
	auto angleToDir = [&](uint16 yaw, int16 pitch, double d[3]) {
		const double ya = (double)(yaw & (NUMBER_OF_ANGLES - 1)) * (TWO_PI / NUMBER_OF_ANGLES);
		const double pa = (double)pitch * (TWO_PI / NUMBER_OF_ANGLES);
		const double cp = cos(pa);
		d[0] = cos(ya) * cp; d[1] = sin(ya) * cp; d[2] = sin(pa);
	};
	auto drawRay = [&](const world_point3d& o, uint16 yaw, int16 pitch, float r, float g, float b) {
		double dir[3]; angleToDir(yaw, pitch, dir);
		const double L = 3.0 * WORLD_ONE * W;
		GLfloat v[6] = { (float)o.x, (float)o.y, (float)o.z,
		                 (float)(o.x + dir[0]*L), (float)(o.y + dir[1]*L), (float)(o.z + dir[2]*L) };
		glColor4f(r, g, b, 0.9f);
		glVertexPointer(3, GL_FLOAT, 0, v);
		glDrawArrays(GL_LINES, 0, 2);
	};

	for (short i = 0; i < dynamic_world->player_count; i++)
	{
		if (i == local_player_index) continue;
		vr_block blk;
		if (!vr_net_get_player_block(i, blk)) continue;

		// camera_location is the simulated eye position. Physical lean + crouch are render-only (not in
		// the sim), so they ride the wire block; add them here (they are already Marathon world units --
		// the getters bake in the sender's worldScale -- so no receiver-side scaling) to show the remote
		// player's TRUE physical head placement, incl. ducking. Folding these into the actual shot origin
		// is Phase 2; here it just makes the pose viz faithful.
		world_point3d head = get_player_data(i)->camera_location;
		head.x += blk.lean_x;
		head.y += blk.lean_y;
		head.z += blk.eye_z;

		drawRay(head, blk.head_yaw, blk.head_pitch, 0.1f, 1.0f, 0.2f); // head view = green
		drawRay(head, blk.dom_yaw,  blk.dom_pitch,  1.0f, 0.2f, 0.2f); // dominant gun = red
		drawRay(head, blk.off_yaw,  blk.off_pitch,  0.2f, 0.5f, 1.0f); // off-hand gun = blue

		// short vertical tick at the eye so a stationary player is still visible
		GLfloat vt[6] = { (float)head.x, (float)head.y, (float)(head.z - 0.15*WORLD_ONE*W),
		                  (float)head.x, (float)head.y, (float)(head.z + 0.15*WORLD_ONE*W) };
		glColor4f(1.0f, 1.0f, 0.1f, 0.9f);
		glVertexPointer(3, GL_FLOAT, 0, vt);
		glDrawArrays(GL_LINES, 0, 2);
	}

	glLineWidth(1.0f);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}
#endif // HAVE_OPENGL

/* origin,origin_polygon_index,yaw,pitch,roll,etc. have probably changed since last call */
void render_view(
	struct view_data *view,
	struct bitmap_definition *software_render_dest)
{
	update_view_data(view);

#if defined(__ANDROID__)
	if (VR_IsActive())
	{
		// Widen the visibility/sort cone so it covers both eyes' frusta plus head-rotation leeway
		// (each eye ~90 deg horizontal; stereo + parallax needs a generous cone). view->yaw/pitch were
		// already aimed at the HMD direction in update_world_view_camera. Keep half_cone < 128 (90 deg)
		// so the BSP cone walk stays well-defined, then rebuild the left/right edge vectors to match.
		angle theta;
		view->half_cone = 115;            // ~81 deg half-angle (162 deg total)
		view->half_vertical_cone = 110;   // ~77 deg
		theta = NORMALIZE_ANGLE(view->cone_yaw - view->half_cone);
		view->left_edge.i = cosine_table[theta]; view->left_edge.j = sine_table[theta];
		theta = NORMALIZE_ANGLE(view->cone_yaw + view->half_cone);
		view->right_edge.i = cosine_table[theta]; view->right_edge.j = sine_table[theta];
	}
#endif

	/* clear the render flags */
	objlist_clear(render_flags, RENDER_FLAGS_BUFFER_SIZE);

	ResetOverheadMap();
/*
#ifdef AUTOMAP_DEBUG
	memset(automap_lines, 0, (dynamic_world->line_count/8+((dynamic_world->line_count%8)?1:0)*sizeof(byte)));
	memset(automap_polygons, 0, (dynamic_world->polygon_count/8+((dynamic_world->polygon_count%8)?1:0)*sizeof(byte)));
#endif
*/
	
	if(view->terminal_mode_active)
	{
		/* Render the computer interface. */
		render_computer_interface(view);
	}
	else
	{
		// LP: the render objects have a pointer to the current view in them,
		// so that one can get rid of redundant references to it in them.
		
		// LP: now from the visibility-tree class
		/* build the render tree, regardless of map mode, so the automap updates while active */
		RenderVisTree.view = view;
#if defined(__ANDROID__)
		// VR rebuilds the tree, the sort and the object list PER EYE below, from each eye's true
		// position -- and starts by clearing render_flags, which throws away everything this pass
		// produced. Doing it here as well is pure waste: it was a third of the visibility CPU per
		// frame for a result nothing reads. Skipped, with two exceptions that still need it:
		//   * the opaque overhead map (the per-eye loop never runs, and this pass is what marks
		//     polygons explored for the automap), and
		//   * the level-load texture prewarm, which builds its own tree explicitly.
		const bool vr_rebuilds_per_eye =
			VR_IsActive() && (!view->overhead_map_active || map_is_translucent());
		if (!vr_rebuilds_per_eye)
#endif
		RenderVisTree.build_render_tree();
		
		/* do something complicated and difficult to explain */
		if (!view->overhead_map_active || map_is_translucent())
		{			
			// LP: now from the polygon-sorter class
			/* sort the render tree (so we have a depth-ordering of polygons) and accumulate
				clipping information for each polygon */
			RenderSortPoly.view = view;
			RenderPlaceObjs.view = view;
#if defined(__ANDROID__)
			if (!vr_rebuilds_per_eye)
#endif
			{
				RenderSortPoly.sort_render_tree();

				// LP: now from the object-placement class
				/* build the render object list by looking at the sorted render tree */
				RenderPlaceObjs.build_render_object_list();
			}
			
			// LP addition: set the current rasterizer to whichever is appropriate here
			RasterizerClass *RasPtr;
#ifdef HAVE_OPENGL
			if (OGL_IsActive())
				RasPtr = &Rasterizer_Shader;
			else
			{
#endif
				assert(software_render_dest);
				Rasterizer_SW.screen = software_render_dest;
				RasPtr = &Rasterizer_SW;
#ifdef HAVE_OPENGL
			}
#endif
			
			// LP: now from the clipping/rasterizer class
#ifdef HAVE_OPENGL
			RenderRasterizerClass *RenPtr = (graphics_preferences->screen_mode.acceleration == _opengl_acceleration) ? &Render_Shader : &Render_Classic;
#else
			RenderRasterizerClass *RenPtr = &Render_Classic;
#endif
			RenPtr->view = view;
			RenPtr->RasPtr = RasPtr;

#if defined(__ANDROID__)
			// VR: rebuild the scene tree per eye from the true eye world position so that
			// each eye's vis-tree and sprite clip planes are correct. Without this, polygons
			// visible only from the side eye snap in late when peeking around corners, and
			// sprite clips are computed from the wrong viewpoint.
			// view->origin_polygon_index is updated via find_new_object_polygon (same call
			// screen.cpp uses for room-scale leaning) so the BSP walk starts from the right
			// polygon. On NONE (eye inside a wall), we fall back to the head-centre polygon.
			// view->origin is restored to head-centre before SetView: VR_GetEyeViewMetres
			// already encodes the per-eye IPD, so the origin must not be pre-shifted.
			if (VR_IsActive())
			{
				// First render_view after a level load: prewarm the texture cache OUTSIDE an OpenXR
				// frame. The first world render otherwise lazy-uploads every visible sprite/landscape/
				// model texture (~2 s) INSIDE a begun frame with the eye swapchain acquired, so the
				// compositor shows the torn "one eye is a flat plane" image for that whole time. Here we
				// render the scene once into the scratch (screen-layer) FBO with no swapchain/no frame
				// begun, so a clean loading frame stays up during the (unavoidable, blocking) upload; the
				// next tick then renders both eyes fast. Skips the normal per-eye submit this tick.
				if (VR_ConsumeLevelWarmup())
				{
					VR_RenderLoadingFrame();   // clean both-eyes frame, held on screen during the warmup
					objlist_clear(render_flags, RENDER_FLAGS_BUFFER_SIZE);
					RenderVisTree.view = view;   RenderVisTree.build_render_tree();
					RenderSortPoly.view = view;  RenderSortPoly.sort_render_tree();
					RenderPlaceObjs.view = view; RenderPlaceObjs.build_render_object_list();
					VR_BeginWarmupEye(0);
					RasPtr->SetView(*view);
					RasPtr->Begin();
					RenPtr->render_tree();               // forces all visible world/sprite/landscape uploads
					render_vr_weapon_sprites_3d(view);   // warm the weapon model textures too
					RasPtr->End();
					VR_EndWarmup();
					VR_RenderLoadingFrame();   // textures warm; next tick draws both eyes fast
				}
				else {
				const bool render = VR_BeginFrame();
				if (render)
				{
					const world_point3d  base_origin = view->origin;
					const short          base_poly   = view->origin_polygon_index;
					// Discrete sprite-view (N/NE/E/...) selection uses this shared head-centre for BOTH
					// eyes, so a creature near a 45deg view boundary doesn't show a different sprite per
					// eye (get_object_shape_and_transfer_mode consults it). Cleared after the loop.
					VR_SetSpriteViewOrigin(&base_origin);
					// In-game console keyboard: place/hover/click once (this frame's head + controller
					// poses are latched by VR_BeginFrame above), then each eye draws it below.
					VR_UpdateInGameKeyboard();
					// The cone centre every sector is measured from (update_view_data set it = yaw).
					// view->yaw itself is never rotated: sprite-view selection and billboarding read
					// it and must keep following the real head azimuth.
					const angle base_cone_yaw = view->cone_yaw;
					for (int eye = 0; eye < 2; ++eye)
					{
						float ipd_wx = 0, ipd_wy = 0;
						VR_GetEyeIPDOffsetWU(eye, &ipd_wx, &ipd_wy);
						world_point3d eye_origin = base_origin;
						eye_origin.x += (world_distance)ipd_wx;
						eye_origin.y += (world_distance)ipd_wy;

						short eye_poly = find_new_object_polygon(
							(world_point2d*)&base_origin,
							(world_point2d*)&eye_origin,
							base_poly);
						if (eye_poly == NONE) eye_poly = base_poly;

						RenderVisTree.view  = view;
						RenderSortPoly.view = view;
						RenderPlaceObjs.view = view;

						// SetView needs head-centre: VR_GetEyeViewMetres encodes the IPD
						// offset in the OpenXR matrices, so the origin must not be pre-shifted.
						view->origin               = base_origin;
						view->origin_polygon_index = base_poly;

						VR_BeginEye(eye);
						RasPtr->SetView(*view);
						RasPtr->Begin();

						// Visibility sweep. Each sector rebuilds the tree around a rotated cone
						// and renders into the SAME eye buffer, so the sectors accumulate into one
						// image. Sector 0 is the primary view: it alone does the full render_tree
						// (per-frame shader uniforms + the glow/bloom pass) and it alone places
						// objects, so sprites are drawn once, in the sector that faces you. The
						// extra sectors add world geometry only -- they exist to fill in the floor
						// you can see beside and behind your feet when looking down.
						//
						// The flags MUST be cleared per sector: endpoint->transformed is cached and
						// gated by _endpoint_has_been_visited, and that cached transform is only
						// valid for the cone_yaw it was computed with.
						const int sectors = vr_vis_sector_count(view);
						for (int si = 0; si < sectors; ++si)
						{
							// Draw the PRIMARY sector LAST. Sprites go down with the depth test OFF (BSP painter's
							// order), so anything drawn after them simply covers them -- and the floor beneath your
							// feet lands in EVERY sector's tree, because directly below you spans every azimuth. With
							// the primary drawn first, the extra sectors repainted that floor straight over the ground
							// sprites the moment the sweep engaged (pitch past ~34 deg): the sprites were still being
							// placed and drawn every frame, just buried. Surfaces that happened not to land in an extra
							// sector's tree kept theirs, which is why they vanished patchily rather than all at once.
							//
							// The extras use render_tree_diffuse_only(), which skips the shader/uniform setup the full
							// render_tree() does, so they now inherit it from the OTHER eye's pass in the same frame
							// rather than this eye's. Those uniforms are frame-global (fog, brightness, bloom); the
							// per-draw matrices come through the GLES shim per draw call, not from here.
							const int sector = (si + 1 < sectors) ? (si + 1) : 0;
							view->cone_yaw  = NORMALIZE_ANGLE(base_cone_yaw + vr_vis_sector_offset(sector));
							view->half_cone = vr_vis_sector_half_cone(sector);
							angle th = NORMALIZE_ANGLE(view->cone_yaw - view->half_cone);
							view->left_edge.i  = cosine_table[th]; view->left_edge.j  = sine_table[th];
							th = NORMALIZE_ANGLE(view->cone_yaw + view->half_cone);
							view->right_edge.i = cosine_table[th]; view->right_edge.j = sine_table[th];

							objlist_clear(render_flags, RENDER_FLAGS_BUFFER_SIZE);

							view->origin               = eye_origin;
							view->origin_polygon_index = eye_poly;
							RenderVisTree.build_render_tree();
							RenderSortPoly.sort_render_tree();
							if (sector == 0)
								RenderPlaceObjs.build_render_object_list();

							// render_tree's CPU sprite clip planes want the true eye position; the
							// GPU matrices were set above and don't read view->origin.
							if (sector == 0) RenPtr->render_tree();
							else             RenPtr->render_tree_diffuse_only();
						}
						view->origin_polygon_index = base_poly;
						view->cone_yaw  = base_cone_yaw;
						view->half_cone = 115;   // restore the un-swept cone for anything downstream

						// Weapon and aim reticle build world-space quads using view->origin as
						// the camera world position; restore base_origin so the GPU matrices
						// (head-centre + OpenXR IPD) don't double-count the IPD offset.
						view->origin = base_origin;
						render_vr_aim_reticle(view);
						render_vr_aim_gizmos(view);
						render_net_vr_pose(view);
						render_vr_weapon_sprites_3d(view);
						// Tuner runs on the first eye only -- the eye loop runs twice a frame and
						// would otherwise double every button edge and every nudge step.
						if (eye == 0) { ++s_vrFrameNo; vr_update_torso();vr_update_attach_tuner(); vr_update_magwell_tuner(); vr_update_holster_grabs(view); vr_update_magazine_seating(view); vr_update_spin_reload(); }
						render_vr_holstered_weapons(view);
						render_vr_hand_visuals(view);   // fists, pouch ammo, mag in hand, magwell ring
						render_vr_attach_gizmos(view);   // physical-weapon attach-point debug overlay
						render_vr_magwell_tuner(view);   // magwell tuning mode overlay

						RasPtr->End();
						VR_PresentHudEye(eye);
						VR_PresentMapEye(eye);
						VR_DrawInGameKeyboardEye(eye);   // console keyboard (world-locked), over everything
						// VR screenshot promo feature DISABLED. Was: capture ONE eye's fully-composited
						// framebuffer to a clean PNG when VR_TakeScreenshotIfRequested(). Uncomment (with
						// the trigger/binding sites) to re-enable. See [[promo-temp-changes]].
						// if (eye == 0 && VR_TakeScreenshotIfRequested())
						// {
						// 	GLint vp[4] = {0,0,0,0};
						// 	glGetIntegerv(GL_VIEWPORT, vp);
						// 	dump_screen_region(vp[0], vp[1], vp[2], vp[3]);
						// }
						VR_FinishEye(eye);
					}
					// Clear so non-stereo passes (overhead map, etc.) use their own camera_location.
					VR_SetSpriteViewOrigin(nullptr);
				}
				VR_SubmitFrame();
				VR_MarkWorldFramePresented();
				}
			}
			else
#endif
			{
				// Set its view:
				RasPtr->SetView(*view);
				// Start rendering main view
				RasPtr->Begin();
				/* render the object list, back to front, doing clipping on each surface before
					passing it to the texture-mapping code */
				RenPtr->render_tree();
				// LP: won't put this into a separate class
				/* render the player’s weapons, etc. */
				if (!RenPtr->renders_viewer_sprites_in_tree()) {
					render_viewer_sprite_layer(view, RasPtr);
				}
#ifdef HAVE_OPENGL
				// Flat PC build: draw remote players' VR head/gun debug lines. This is the primary
				// validation for the VR netcode -- watch the PC screen while moving on the Quest.
				render_net_vr_pose(view);
#endif
				// Finish rendering main view
				RasPtr->End();
			}
		}

		if (view->overhead_map_active)
		{
			/* if the overhead map is active, render it */
			render_overhead_map(view);
		}
	}
}

void start_render_effect(
	struct view_data *view,
	short effect)
{
	view->effect= effect;
	view->effect_phase= NONE;
}

void check_m1_exploration(void)
{
	// Are we even on an exploration mission?
	if (!(static_world->mission_flags & _mission_exploration_m1))
		return;

	// Are there still polygons to explore?
	bool need_exploring = false;
	short polygon_index;
	struct polygon_data *polygon;
	for (polygon_index = 0, polygon = map_polygons;
	     polygon_index < dynamic_world->polygon_count;
	     ++polygon_index, ++polygon)
    {
		if (polygon->type == _polygon_must_be_explored)
		{
			need_exploring = true;
			break;
		}
	}
	if (!need_exploring)
		return;

	// All right, we need to do something.
	// First, make sure our data is set up.
	if (!explore_tree.view)
	{
		explore_view.overhead_map_active = false;
		explore_view.terminal_mode_active = false;
		explore_view.tunnel_vision_active = false;
		explore_view.effect = NONE;
		explore_view.horizontal_scale = 1;
		explore_view.vertical_scale = 1;
        
		// For cross-player stability, we don't leave any view settings
		// up to the preferences or MML.
		explore_view.field_of_view = explore_view.target_field_of_view = 80;
		explore_view.screen_width = explore_view.standard_screen_width = 640;
		explore_view.screen_height = 320;

		// We only need to initialize once, since nothing
		// that we use changes.
		initialize_view_data(&explore_view, true);

		explore_tree.view = &explore_view;
		explore_tree.add_to_automap = false;
		explore_tree.mark_as_explored = true;
		explore_tree.Resize(MAXIMUM_ENDPOINTS_PER_MAP, MAXIMUM_LINES_PER_MAP);
	}

	// Check the relevant players' views for exploration polygons.
	// We check every TICKS_PER_EXLORE ticks, staggered by index.
	for (int i = (dynamic_world->tick_count % TICKS_PER_EXPLORE);
	     i < dynamic_world->player_count;
	     i += TICKS_PER_EXPLORE)
	{
		struct player_data *explore_player = &players[i];
		explore_view.yaw = explore_player->facing;
		explore_view.pitch = explore_player->elevation;
		explore_view.origin = explore_player->camera_location;
		explore_view.origin_polygon_index = explore_player->camera_polygon_index;

		update_view_data(&explore_view);
		
		std::vector<uint16_t> saved_render_flags{RenderFlagList};
		objlist_clear(render_flags, RENDER_FLAGS_BUFFER_SIZE);
		
        // build_render_tree() actually marks the polygons
		explore_tree.build_render_tree();

		RenderFlagList = std::move(saved_render_flags);
	}
}


/* ---------- private code */

static void update_view_data(
	struct view_data *view)
{
	angle theta;

	// The cone follows the view unless a caller deliberately sweeps it (see view_data::cone_yaw).
	view->cone_yaw = view->yaw;

	// LP change: doing all the FOV changes here:
	View_AdjustFOV(view->field_of_view,view->target_field_of_view);
	
	if (view->effect==NONE)
	{
		view->world_to_screen_x= view->real_world_to_screen_x;
		view->world_to_screen_y= view->real_world_to_screen_y;
	}
	else
	{
		update_render_effect(view);
	}
	
	/* calculate world_to_screen_y*tan(pitch) */
	view->dtanpitch= (view->world_to_screen_y*sine_table[view->pitch])/cosine_table[view->pitch];

	/* calculate left cone vector */
	theta= NORMALIZE_ANGLE(view->cone_yaw-view->half_cone);
	view->left_edge.i= cosine_table[theta], view->left_edge.j= sine_table[theta];
	
	/* calculate right cone vector */
	theta= NORMALIZE_ANGLE(view->cone_yaw+view->half_cone);
	view->right_edge.i= cosine_table[theta], view->right_edge.j= sine_table[theta];
	
	/* if we’re sitting on one of the endpoints in our origin polygon, move us back slightly (±1) into
		that polygon.  when we split rays we’re assuming that we’ll never pass through a given
		vertex in different directions (because if we do the tree becomes a graph) but when
		we start on a vertex this can happen.  this is a destructive modification of the origin. */
	{
		short i;
		struct polygon_data *polygon= get_polygon_data(view->origin_polygon_index);
		
		for (i= 0;i<polygon->vertex_count;++i)
		{
			struct world_point2d *vertex= &get_endpoint_data(polygon->endpoint_indexes[i])->vertex;
			
			if (vertex->x == view->origin.x && vertex->y == view->origin.y)
			{
				world_point2d *ccw_vertex= &get_endpoint_data(polygon->endpoint_indexes[WRAP_LOW(i, polygon->vertex_count-1)])->vertex;
				world_point2d *cw_vertex= &get_endpoint_data(polygon->endpoint_indexes[WRAP_HIGH(i, polygon->vertex_count-1)])->vertex;
				world_vector2d inset_vector;
				
				inset_vector.i= (ccw_vertex->x-vertex->x) + (cw_vertex->x-vertex->x);
				inset_vector.j= (ccw_vertex->y-vertex->y) + (cw_vertex->y-vertex->y);
				
				if (inset_vector.i == 0 && inset_vector.j == 0)
				{
					// This happens when the CW and CCW vertices are equidistant from and collinear with the origin;
					// we switch tactics and just move directly toward one of them
					inset_vector.i = cw_vertex->x - vertex->x;
					inset_vector.j = cw_vertex->y - vertex->y;
				}
				
				view->origin.x+= SGN(inset_vector.i);
				view->origin.y+= SGN(inset_vector.j);
				
				break;
			}
		}
		
		// Also check adjacent polygons' vertices in case a degenerate polygon has a vertex under us (on a side)
		{
			// Local index of the first side that connects to such a polygon, or else NONE
			// (if non-NONE, we're on this side or one collinear with it)
			const int side_to_poly_with_vertex_on_origin = [&]() -> int
			{
				for (int i = 0; i < polygon->vertex_count; ++i)
				{
					const int16 adj_poly_index = polygon->adjacent_polygon_indexes[i];
					if (adj_poly_index != NONE)
					{
						const auto& adj_poly = *get_polygon_data(adj_poly_index);
						for (int k = 0; k < adj_poly.vertex_count; ++k)
						{
							const auto v = get_endpoint_data(adj_poly.endpoint_indexes[k])->vertex;
							if (v.x == view->origin.x && v.y == view->origin.y)
								return i;
						}
					}
				}
				return NONE;
			}();
			
			if (side_to_poly_with_vertex_on_origin != NONE)
			{
				// Scoot inward or along the side we're on (we're not on a corner because we handled that case already)
				const int vertex0_index = side_to_poly_with_vertex_on_origin;
				const int vertex1_index = WRAP_HIGH(side_to_poly_with_vertex_on_origin, polygon->vertex_count - 1);
				const world_distance vertex0_x = get_endpoint_data(polygon->endpoint_indexes[vertex0_index])->vertex.x;
				const world_distance vertex1_x = get_endpoint_data(polygon->endpoint_indexes[vertex1_index])->vertex.x;
				view->origin.y += (vertex1_x - vertex0_x >= 0) ? 1 : -1;
			}
		}
		
		/* determine whether we are under or over the media boundary of our polygon; we will see all
			other media boundaries from this orientation (above or below) or fail to draw them. */
		if (polygon->media_index==NONE)
		{
			view->under_media_boundary= false;
		}
		else
		{
			struct media_data *media= get_media_data(polygon->media_index);
			
			// LP change: idiot-proofing
			if (media)
			{
				view->under_media_boundary= UNDER_MEDIA(media, view->origin.z);
				view->under_media_index= polygon->media_index;
			} else {
				view->under_media_boundary= false;
			}
		}
	}
}

static void update_render_effect(
	struct view_data *view)
{
	short effect= view->effect;
	short phase= view->effect_phase==NONE ? 0 : (view->effect_phase+view->ticks_elapsed);
	short period;

	view->effect_phase= phase;

	switch (effect)
	{
		// LP change: suppressed all the FOV changes
		case _render_effect_fold_in: case _render_effect_fold_out: period= TICKS_PER_SECOND/2; break;
		case _render_effect_explosion: period= TICKS_PER_SECOND; break;
		default:
			assert(false);
			break;
	}
	
	if (phase>period)
	{
		view->effect= NONE;
	}
	else
	{
		float interpolated_phase = MAX(0, phase - 1 + view->heartbeat_fraction);
		switch (effect)
		{
			case _render_effect_explosion:
				shake_view_origin(view, EXPLOSION_EFFECT_RANGE - ((EXPLOSION_EFFECT_RANGE/2)*interpolated_phase)/period);
				break;
			
			case _render_effect_fold_in:
				interpolated_phase= period-interpolated_phase;
			case _render_effect_fold_out:
				/* calculate world_to_screen based on phase */
				view->world_to_screen_x= view->real_world_to_screen_x + (4*view->real_world_to_screen_x*interpolated_phase)/period;
				view->world_to_screen_y= view->real_world_to_screen_y - (view->real_world_to_screen_y*interpolated_phase)/(period+period/4);
				break;
		}
	}
}


/* ---------- transfer modes */

/* given a transfer mode and phase, cause whatever changes it should cause to a rectangle_definition
	structure */
void instantiate_rectangle_transfer_mode(
	view_data *view,
	rectangle_definition *rectangle,
	short transfer_mode,
	_fixed transfer_phase)
{
	// For the 3D-model code
	rectangle->HorizScale = 1;
	
	switch (transfer_mode)
	{
		case _xfer_invisibility:
		case _xfer_subtle_invisibility:
			if (view->shading_mode!=_shading_infravision)
			{
				rectangle->transfer_mode= _tinted_transfer;
				rectangle->shading_tables= get_global_shading_table();
				rectangle->transfer_data= (transfer_mode==_xfer_invisibility) ? 0x000f : 0x0018;
				break;
			}
			/* if we have infravision, fall through to _textured_transfer (i see you...) */
		case _xfer_normal:
			rectangle->transfer_mode= _textured_transfer;
			break;
		
		case _xfer_static:
		case _xfer_50percent_static:
			rectangle->transfer_mode= _static_transfer;
			rectangle->transfer_data= (transfer_mode==_xfer_static) ? 0x0000 : 0x8000;
			break;

		case _xfer_fade_out_static:
			rectangle->transfer_mode= _static_transfer;
			rectangle->transfer_data= transfer_phase;
			break;
			
		case _xfer_pulsating_static:
			rectangle->transfer_mode= _static_transfer;
			rectangle->transfer_data= 0x8000+((0x6000*sine_table[FIXED_INTEGERAL_PART(transfer_phase*NUMBER_OF_ANGLES)])>>TRIG_SHIFT);
			break;

		case _xfer_fold_in:
			transfer_phase= FIXED_ONE-transfer_phase; /* do everything backwards */
		case _xfer_fold_out:
			if (View_DoStaticEffect())
			{
				// Corrected the teleport shrinkage so that the sprite/object
				// shrinks to its object position and not to its sprite center
				short delta0 = FIXED_INTEGERAL_PART(int32(((1LL*rectangle->xc - rectangle->x0) - 1) * transfer_phase));
				short delta1 = FIXED_INTEGERAL_PART(int32(((1LL*rectangle->x1 - rectangle->xc) - 1) * transfer_phase));
				// short delta= FIXED_INTEGERAL_PART((((rectangle->x1-rectangle->x0)>>1)-1)*transfer_phase);
					
				rectangle->transfer_mode= _static_transfer;
				rectangle->transfer_data= (transfer_phase>>1);
				rectangle->x0+= delta0;
				rectangle->x1-= delta1;
				rectangle->HorizScale = 1 - float(transfer_phase)/float(FIXED_ONE);
			}
			else
				rectangle->transfer_mode= _textured_transfer;
			break;

#if 0		
		case _xfer_fade_out_to_black:
			rectangle->shading_tables= get_global_shading_table();
			if (transfer_phase<FIXED_ONE_HALF)
			{
				/* fade to black */
				rectangle->ambient_shade= (rectangle->ambient_shade*(transfer_phase-FIXED_ONE_HALF))>>(FIXED_FRACTIONAL_BITS-1);
				rectangle->transfer_mode= _textured_transfer;
			}
			else
			{
				/* vanish */
				rectangle->transfer_mode= _tinted_transfer;
				rectangle->transfer_data= 0x1f - ((0x1f*(FIXED_ONE_HALF-transfer_phase))>>(FIXED_FRACTIONAL_BITS-1));
			}
			break;
#endif
		
		// LP change: made an unrecognized mode act like normal
		default:
			rectangle->transfer_mode= _textured_transfer;
			break;
	}
}

/* given a transfer mode and phase, cause whatever changes it should cause to a polygon_definition
	structure (unfortunately we need to know whether this is a horizontal or vertical polygon) */
void instantiate_polygon_transfer_mode(
	struct view_data *view,
	struct polygon_definition *polygon,
	short transfer_mode,
	bool horizontal)
{
	world_distance x0, y0;
	world_distance vector_magnitude;
	short alternate_transfer_phase;
	short transfer_phase = view->tick_count;

	polygon->transfer_mode= _textured_transfer;
	switch (transfer_mode)
	{
		case _xfer_fast_horizontal_slide:
		case _xfer_horizontal_slide:
		case _xfer_vertical_slide:
		case _xfer_fast_vertical_slide:
		case _xfer_wander:
		case _xfer_fast_wander:
		case _xfer_reverse_horizontal_slide:
		case _xfer_reverse_fast_horizontal_slide:
		case _xfer_reverse_vertical_slide:
		case _xfer_reverse_fast_vertical_slide:
			x0= y0= 0;
			switch (transfer_mode)
			{
				case _xfer_fast_horizontal_slide: transfer_phase<<= 1;
				case _xfer_horizontal_slide: x0= (transfer_phase<<2)&(WORLD_ONE-1); break;
				
				case _xfer_fast_vertical_slide: transfer_phase<<= 1;
				case _xfer_vertical_slide: y0= (transfer_phase<<2)&(WORLD_ONE-1); break;
					
				case _xfer_reverse_fast_horizontal_slide: transfer_phase<<= 1;
				case _xfer_reverse_horizontal_slide: x0 = WORLD_ONE - (transfer_phase<<2)&(WORLD_ONE-1); break;
					
		        case _xfer_reverse_fast_vertical_slide: transfer_phase<<= 1;
				case _xfer_reverse_vertical_slide: y0 = WORLD_ONE - (transfer_phase<<2)&(WORLD_ONE-1); break;
					
				case _xfer_fast_wander: transfer_phase<<= 1;
				case _xfer_wander:
					alternate_transfer_phase= transfer_phase%(10*FULL_CIRCLE);
					transfer_phase= transfer_phase%(6*FULL_CIRCLE);
					x0= (cosine_table[NORMALIZE_ANGLE(alternate_transfer_phase)] +
						(cosine_table[NORMALIZE_ANGLE(2*alternate_transfer_phase)]>>1) +
						(cosine_table[NORMALIZE_ANGLE(5*alternate_transfer_phase)]>>1))>>(WORLD_FRACTIONAL_BITS-TRIG_SHIFT+2);
					y0= (sine_table[NORMALIZE_ANGLE(transfer_phase)] +
						(sine_table[NORMALIZE_ANGLE(2*transfer_phase)]>>1) +
						(sine_table[NORMALIZE_ANGLE(3*transfer_phase)]>>1))>>(WORLD_FRACTIONAL_BITS-TRIG_SHIFT+2);
					break;
			}
			if (horizontal)
			{
				polygon->origin.x+= x0;
				polygon->origin.y+= y0;
			}
			else
			{
				vector_magnitude= isqrt(polygon->vector.i*polygon->vector.i + polygon->vector.j*polygon->vector.j);
				polygon->origin.x+= (polygon->vector.i*x0)/vector_magnitude;
				polygon->origin.y+= (polygon->vector.j*x0)/vector_magnitude;
				polygon->origin.z-= y0;
			}
			break;
		
		case _xfer_pulsate:
		case _xfer_wobble:
		case _xfer_fast_wobble:
			if (transfer_mode==_xfer_fast_wobble) transfer_phase*= 15;
			transfer_phase&= WORLD_ONE/16-1;
			transfer_phase= (transfer_phase>=WORLD_ONE/32) ? (WORLD_ONE/32+WORLD_ONE/64 - transfer_phase) : (transfer_phase - WORLD_ONE/64);
			if (horizontal)
			{
				polygon->origin.z+= transfer_phase;
			}
			else
			{
				if (transfer_mode==_xfer_pulsate) /* translate .origin perpendicular to .vector */
				{
					world_vector2d offset;
					world_distance vector_magnitude= isqrt(polygon->vector.i*polygon->vector.i + polygon->vector.j*polygon->vector.j);
	
					offset.i= (polygon->vector.j*transfer_phase)/vector_magnitude;
					offset.j= (polygon->vector.i*transfer_phase)/vector_magnitude;
	
					polygon->origin.x+= offset.i;
					polygon->origin.y+= offset.j;
				}
				else /* ==_xfer_wobble, wobble .vector */
				{
					polygon->vector.i+= transfer_phase;
					polygon->vector.j+= transfer_phase;
				}
			}
			break;

		case _xfer_normal:
			break;
		
		case _xfer_smear:
			polygon->transfer_mode= _solid_transfer;
			break;
			
		case _xfer_static:
			polygon->transfer_mode= _static_transfer;
			polygon->transfer_data= 0x0000;
			break;
		
		case _xfer_landscape:
			polygon->transfer_mode= _big_landscaped_transfer;
			break;
//		case _xfer_big_landscape:
//			polygon->transfer_mode= _big_landscaped_transfer;
//			break;
			
		default:
			// LP change: made an unrecognized mode act like normal
			break;
	}
}

/* ---------- viewer sprite layer (i.e., weapons) */

static void render_viewer_sprite_layer(view_data *view, RasterizerClass *RasPtr)
{
	rectangle_definition textured_rectangle;
	weapon_display_information display_data;
	shape_information_data *shape_information;
	short count;

	// LP change: bug out if weapons-in-hand are not to be displayed
	if (!view->show_weapons_in_hand) return;
	
	// Need to set this...
	RasPtr->SetForeground();
	
	// No models here, and completely opaque
	textured_rectangle.ModelPtr = NULL;
	textured_rectangle.Opacity = 1;

	/* get_weapon_display_information() returns true if there is a weapon to be drawn.  it
		should initially be passed a count of zero.  it returns the weapon’s texture and
		enough information to draw it correctly. */
	count= 0;
	while (get_weapon_display_information(&count, &display_data))
	{
		/* fetch relevant shape data */
		// LP: model-setup code is cribbed from 
		// RenderPlaceObjsClass::build_render_object() in RenderPlaceObjs.cpp
#ifdef HAVE_OPENGL
		// Find which 3D model will take the place of this sprite, if any
		short ModelSequence;
		OGL_ModelData *ModelPtr =
			OGL_GetModelData(GET_COLLECTION(display_data.collection),display_data.shape_index,ModelSequence);
#endif
		shape_information= extended_get_shape_information(display_data.collection, display_data.low_level_shape_index);
		// Nonexistent frame: skip
		if (!shape_information) continue;
		// No need for a fake sprite rectangle, since models are foreground objects
		
		// LP change: for the convenience of the OpenGL renderer
		textured_rectangle.ShapeDesc = BUILD_DESCRIPTOR(display_data.collection,0);
		textured_rectangle.LowLevelShape = display_data.low_level_shape_index;
#ifdef HAVE_OPENGL
		textured_rectangle.ModelPtr = ModelPtr;
		if (ModelPtr)
		{
			textured_rectangle.ModelSequence = ModelSequence;
			textured_rectangle.ModelFrame = display_data.Frame;
			textured_rectangle.NextModelFrame = display_data.NextFrame;
			textured_rectangle.MixFrac = display_data.Ticks > 0 ?
				float(display_data.Phase)/float(display_data.Ticks) : 0;
			const world_point3d Zero = {0, 0, 0};
			textured_rectangle.Position = Zero;
			textured_rectangle.Azimuth = 0;
			textured_rectangle.Scale = 1;
			textured_rectangle.LightDepth = 0;
			const GLfloat LightDirection[3] = {0, 1, 0};	// y is forward
			objlist_copy(textured_rectangle.LightDirection,LightDirection,3);
			RasPtr->SetForegroundView(display_data.flip_horizontal);
		}
#endif
		
		if (shape_information->flags&_X_MIRRORED_BIT) display_data.flip_horizontal= !display_data.flip_horizontal;
		if (shape_information->flags&_Y_MIRRORED_BIT) display_data.flip_vertical= !display_data.flip_vertical;

		/* calculate shape rectangle */
		position_sprite_axis(&textured_rectangle.x0, &textured_rectangle.x1, view->screen_height, view->screen_width, display_data.horizontal_positioning_mode,
			display_data.horizontal_position, display_data.flip_horizontal, shape_information->world_left, shape_information->world_right);
		position_sprite_axis(&textured_rectangle.y0, &textured_rectangle.y1, view->screen_height, view->screen_height, display_data.vertical_positioning_mode,
			display_data.vertical_position, display_data.flip_vertical, -shape_information->world_top, -shape_information->world_bottom);
		
		/* set rectangle bitmap and shading table */
		extended_get_shape_bitmap_and_shading_table(display_data.collection, display_data.low_level_shape_index, &textured_rectangle.texture, &textured_rectangle.shading_tables, view->shading_mode);
		if (!textured_rectangle.texture) continue;
		
		textured_rectangle.flags= 0;

		/* initialize clipping window to full screen */
		textured_rectangle.clip_left= 0;
		textured_rectangle.clip_right= view->screen_width;
		textured_rectangle.clip_top= 0;
		textured_rectangle.clip_bottom= view->screen_height;

		/* copy mirror flags */
		textured_rectangle.flip_horizontal= display_data.flip_horizontal;
		textured_rectangle.flip_vertical= display_data.flip_vertical;
		
		/* lighting: depth of zero in the camera’s polygon index */
		textured_rectangle.depth= 0;
		textured_rectangle.ambient_shade= get_light_intensity(get_polygon_data(view->origin_polygon_index)->floor_lightsource_index);
		textured_rectangle.ambient_shade= MAX(shape_information->minimum_light_intensity, textured_rectangle.ambient_shade);
		if (view->shading_mode==_shading_infravision) textured_rectangle.flags|= _SHADELESS_BIT;

		// Calculate the object's horizontal position
		// for the convenience of doing teleport-in/teleport-out
		textured_rectangle.xc = (textured_rectangle.x0 + textured_rectangle.x1) >> 1;
		
		/* make the weapon reflect the owner’s transfer mode */
		instantiate_rectangle_transfer_mode(view, &textured_rectangle, display_data.transfer_mode, display_data.transfer_phase);
		
		/* and draw it */
		// LP: added OpenGL support
		RasPtr->texture_rectangle(textured_rectangle);
	}
}

void position_sprite_axis(
	short *x0,
	short *x1,
	short scale_width,
	short screen_width,
	short positioning_mode,
	_fixed position,
	bool flip,
	world_distance world_left,
	world_distance world_right)
{
	short origin;

	/* if this shape is mirrored, reverse the left/right world coordinates */
	if (flip)
	{
		world_distance swap= world_left;
		world_left= -world_right, world_right= -swap;
	}
	
	switch (positioning_mode)
	{
		case _position_center:
			/* origin is the screen coordinate where the logical center of the shape will be drawn */
			origin= (screen_width*position)>>FIXED_FRACTIONAL_BITS;
			break;
		case _position_low:
		case _position_high:
			/* origin is in [0,WORLD_ONE] and represents the amount of the weapon visible off the side */
			origin= ((world_right-world_left)*position)>>FIXED_FRACTIONAL_BITS;
			break;
		
		default:
			assert(false);
			break;
	}
	
	switch (positioning_mode)
	{
		case _position_high:
			*x0= screen_width - ((origin*scale_width)>>WORLD_FRACTIONAL_BITS);
			*x1= *x0 + (((world_right-world_left)*scale_width)>>WORLD_FRACTIONAL_BITS);
			break;
		case _position_low:
			*x1= ((origin*scale_width)>>WORLD_FRACTIONAL_BITS);
			*x0= *x1 - (((world_right-world_left)*scale_width)>>WORLD_FRACTIONAL_BITS);
			break;
		
		case _position_center:
			*x0= origin + ((world_left*scale_width)>>WORLD_FRACTIONAL_BITS);
			*x1= origin + ((world_right*scale_width)>>WORLD_FRACTIONAL_BITS);
			break;
		
		default:
			assert(false);
			break;
	}
}

static void shake_view_origin(
	struct view_data *view,
	world_distance delta)
{
	world_point3d new_origin= view->origin;
	short half_delta= delta>>1;
	
	new_origin.x+= half_delta - ((delta*sine_table[NORMALIZE_ANGLE((view->tick_count&~3)*(7*FULL_CIRCLE))])>>TRIG_SHIFT);
	new_origin.y+= half_delta - ((delta*sine_table[NORMALIZE_ANGLE(((view->tick_count+5*TICKS_PER_SECOND)&~3)*(7*FULL_CIRCLE))])>>TRIG_SHIFT);
	new_origin.z+= half_delta - ((delta*sine_table[NORMALIZE_ANGLE(((view->tick_count+7*TICKS_PER_SECOND)&~3)*(7*FULL_CIRCLE))])>>TRIG_SHIFT);

	/* only use the new origin if we didn’t cross a polygon boundary */
	if (find_line_crossed_leaving_polygon(view->origin_polygon_index, (world_point2d *) &view->origin,
		(world_point2d *) &new_origin)==NONE)
	{
		view->origin= new_origin;
	}
}
