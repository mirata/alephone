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
#include "items.h"         // physical weapons: magazine item pictures (get_item_shape)
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
static void vr_loose_mag_pos(float W, const float well[3], const float ins[3], float out[3]);
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

		// A LOOSENED magazine: the free hand pulls it out, rounds and all.
		// Checked first, because the magwell is on the gun and would otherwise read as a steadying grip.
		if (VR_GetHeldWeaponHand() >= 0 && VR_GetHeldWeaponHand() != hand &&
		    vr_get_offhand_weapon(current_player_index) == NONE && VR_GetHandMagazine(hand) == NONE) {
			short lw = NONE; float lwell[3], lins[3], lr = 0.f;
			if (vr_primary_magwell(W, hand, &lw, lwell, lins, &lr, nullptr) && VR_GetMagState(lw) == VR_MAG_LOOSE) {
				float mp[3];
				vr_loose_mag_pos(W, lwell, lins, mp);
				const float dx = hx-mp[0], dy = hy-mp[1], dz = hz-mp[2];
				const float rr = (lr > 0.10f * W ? lr : 0.10f * W);
				if (dx*dx + dy*dy + dz*dz <= rr * rr) {
					VR_SetHandMagazine(hand, get_player_weapon_ammo_type(current_player_index, lw, _primary_weapon));
					VR_SetHandMagazineRounds(hand, VR_GetLooseMagRounds(lw));   // it keeps its rounds
					s_vrMagMustClear[hand] = true;   // your hand is in the well: don't re-seat it on the spot
					VR_SetMagState(lw, VR_MAG_OUT);
					VR_Vibrate(hand, 40, 0.6f);
					continue;
				}
			}
		}

		// Nearest VISIBLE holster within its own radius wins -- a slot the hands have emptied has
		// nothing in it to grab.
		short visible[VR_HOLSTER_COUNT];
		vr_visible_holsters(visible);
		vr_body_frame bfGrab;
		vr_get_body_frame(view, bfGrab);
		float hb[3];
		const bool haveHb = vr_hand_body_coords(bfGrab, hand, hb);
		short best = NONE; float bestD2 = 0.f;
		for (int slot = 0; slot < VR_HOLSTER_COUNT; ++slot) {
			const short wt = visible[slot];
			if (wt == NONE) continue;
			vr_holster_anchor ha; VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &ha);
			float d2, r;
			const bool backSlot = slot == VR_HOLSTER_RIGHT_BACK || slot == VR_HOLSTER_LEFT_BACK ||
			                      slot == VR_HOLSTER_BACK_CENTER;
			if (backSlot && haveHb) {
				// Over-the-shoulder weapons: reaching ANYWHERE up and over your head gets them. Being above
				// the anchor costs nothing; only the horizontal distance (and dropping below it) counts, so
				// the side you reach over picks which of the back weapons you take.
				const float dx = hb[0] - ha.x, dz = hb[2] - ha.z;
				const float dy = (hb[1] < ha.y) ? (ha.y - hb[1]) : 0.f;
				d2 = (dx*dx + dy*dy + dz*dz) * W * W;
				r  = 0.40f * W;
			} else {
				float c[3]; vr_holster_world_pos(view, slot, c);
				const float dx = hx - c[0], dy = hy - c[1], dz = hz - c[2];
				d2 = dx*dx + dy*dy + dz*dz;
				r  = ha.radius * W;
			}
			if (d2 > r * r) continue;
			if (best == NONE || d2 < bestD2) { best = wt; bestD2 = d2; }
		}
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
static void render_vr_holstered_weapons(view_data* view)
{
	if (!VR_PhysicalWeaponsActive()) return;
	if (!OGL_IsActive()) return;
	if (PLAYER_IS_DEAD(get_player_data(current_player_index))) return;

	const float W = (float)VR_Settings()->worldScaleWUM;
	short visible[VR_HOLSTER_COUNT];
	vr_visible_holsters(visible);

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
		rect.flip_horizontal = false;
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
			OGL_RenderVRWeaponModel(rect, coll, 0, mdl, c[0], c[1], c[2], mR, mU, mF,
			                        rect.transfer_mode == _static_transfer, 0, 0.f, -1);
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
	const int weaponHand = VR_GetHeldWeaponHand();
	if (weaponHand < 0 || weaponHand == reachingHand) return NONE;
	if (!s_vrWeaponPose[weaponHand].valid) return NONE;
	const short wt = s_vrWeaponPose[weaponHand].weapon_type;
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
	VR_SetHandMagazine(hand, vr_pouch_magazine_for(slot, hand));
	s_vrMagMustClear[hand] = false;
	VR_Vibrate(hand, 35, 0.5f);
	return true;
}

static bool vr_hand_world_pose(const vr_body_frame& bf, int hand,
                               float pos[3], float wr[3], float wu[3], float wf[3]);   // with the hand visuals

// ---- magazines falling away (visual only) ----------------------------------------------------
// A magazine you let go of, or one shaken loose, drops and fades. Purely a picture: nothing was
// deducted to get a fresh mag, and a spent one has already been emptied.
struct vr_falling_mag { short item; float pos[3]; float vel[3]; float age; float floorZ; bool landed; };
static std::vector<vr_falling_mag> s_fallingMags;
static uint64_t s_fallingLastMs = 0;

static void vr_spawn_falling_mag(const float pos[3], const float vel[3], short item)
{
	if (item == NONE) return;
	if (s_fallingMags.size() >= 8) s_fallingMags.erase(s_fallingMags.begin());
	vr_falling_mag m;
	m.item = item; m.age = 0.f; m.landed = false;
	// Lands at the player's feet: close enough for a magazine dropped or tossed nearby.
	m.floorZ = (float)get_player_data(current_player_index)->location.z;
	for (int k = 0; k < 3; ++k) { m.pos[k] = pos[k]; m.vel[k] = vel[k]; }
	s_fallingMags.push_back(m);
}

static void vr_update_falling_mags(float W)
{
	const uint64_t now = machine_tick_count();
	float dt = s_fallingLastMs ? float(now - s_fallingLastMs) / 1000.f : 0.f;
	s_fallingLastMs = now;
	if (dt > 0.05f) dt = 0.05f;
	for (auto& m : s_fallingMags) {
		m.age += dt;
		if (m.landed) continue;
		m.vel[2] -= 9.8f * W * dt;                          // world Z is up; W = world units per metre
		for (int k = 0; k < 3; ++k) m.pos[k] += m.vel[k] * dt;
		// Rest a centimetre above the floor so it isn't half-swallowed by it now that it depth-tests.
		if (m.pos[2] <= m.floorZ + 0.01f * W) { m.pos[2] = m.floorZ + 0.01f * W; m.landed = true; }
	}
	s_fallingMags.erase(std::remove_if(s_fallingMags.begin(), s_fallingMags.end(),
		[](const vr_falling_mag& m) { return m.age > 3.0f; }), s_fallingMags.end());
}

// The magwell of the weapon in the PRIMARY hand, in world space: position, insertion direction (the
// way a magazine travels going IN), and its seat tolerances. excludeHand = the hand that would be
// doing the loading, which cannot also be the hand holding the gun.
static bool vr_primary_magwell(float W, int excludeHand, short* weapon, float well[3], float ins[3],
                               float* radius, float* angleTol, bool secondWell)
{
	const int held = VR_GetHeldWeaponHand();
	if (held < 0 || held == excludeHand) return false;
	const vr_weapon_pose& wp = s_vrWeaponPose[held];
	if (!wp.valid || wp.weapon_type == NONE) return false;
	const vr_attach_point* mw = secondWell ? VR_GetMagwell2OrDefault(wp.weapon_type)
	                                       : VR_GetMagwellOrDefault(wp.weapon_type);
	vr_weapon_local_to_world(wp, W, mw->x, mw->y, mw->z, well);
	// Straight up into the weapon from below, swung by the authored angles.
	const float cp  = cosf(mw->pitch * 0.01745329f), sp  = sinf(mw->pitch * 0.01745329f);
	const float cy2 = cosf(mw->yaw   * 0.01745329f), sy2 = sinf(mw->yaw   * 0.01745329f);
	for (int k = 0; k < 3; ++k)
		ins[k] = wp.up[k]*cp - wp.fwd[k]*sp*cy2 - wp.right[k]*sp*sy2;
	if (weapon)   *weapon   = wp.weapon_type;
	if (radius)   *radius   = mw->radius * W;
	if (angleTol) *angleTol = mw->angle_tolerance;
	return true;
}

// Whether a magazine of this ammo can go into this weapon right now. The main magazine needs the old
// one OUT first -- ejected and then pulled or shaken free. A second-trigger magazine (the rifle's
// grenades) has no eject of its own, so it goes in once that trigger has fired its last round.
static bool vr_magwell_accepts(short weapon, short mag)
{
	const short primaryAmmo = get_player_weapon_ammo_type(current_player_index, weapon, _primary_weapon);
	if (mag == primaryAmmo) return VR_GetMagState(weapon) == VR_MAG_OUT;
	return get_player_weapon_ammo_count(current_player_index, weapon, _secondary_weapon) <= 0;
}

// Where a loosened magazine hangs: a few centimetres out of the magwell, back along the insertion path.
static void vr_loose_mag_pos(float W, const float well[3], const float ins[3], float out[3])
{
	for (int k = 0; k < 3; ++k) out[k] = well[k] - ins[k] * 0.04f * W;
}

// Once per frame: seat magazines, drop released ones, shake loose ones free.
static void vr_update_magazine_seating(view_data* view)
{
	if (!VR_PhysicalWeaponsActive() || VR_GetHolsterSetupMode()) return;
	vr_body_frame bf;
	vr_get_body_frame(view, bf);
	const float W = bf.W;
	vr_update_falling_mags(W);

	// A loose magazine comes free with a flick of the gun hand: a snap in the direction the magazine
	// comes OUT (the mag keeps going when the gun stops), or a sharp roll of the wrist around the barrel,
	// John Wick style. The rounds go with it -- it is on the floor.
	{
		static float    s_prevUp[3];
		static bool     s_prevUpValid = false;
		static uint64_t s_prevUpMs = 0;
		const int held = VR_GetHeldWeaponHand();
		const uint64_t now = machine_tick_count();

		// Roll rate of the gun hand around its own pointing direction, degrees per second.
		float rollRate = 0.f;
		float ps[3], f[3], r[3], u[3];
		if (held >= 0 && VR_GetAimPoseStage(held, ps, f) && VR_GetAimOrientStage(held, r, u)) {
			if (s_prevUpValid && now > s_prevUpMs) {
				const float* p = s_prevUp;
				const float cx = p[1]*u[2] - p[2]*u[1], cy = p[2]*u[0] - p[0]*u[2], cz = p[0]*u[1] - p[1]*u[0];
				const float deg = atan2f(cx*f[0] + cy*f[1] + cz*f[2], p[0]*u[0] + p[1]*u[1] + p[2]*u[2]) * 57.2957795f;
				rollRate = deg / (float(now - s_prevUpMs) / 1000.f);
			}
			s_prevUp[0] = u[0]; s_prevUp[1] = u[1]; s_prevUp[2] = u[2];
			s_prevUpValid = true;
			s_prevUpMs = now;
		} else {
			s_prevUpValid = false;
		}

		short w = NONE; float well[3], ins[3];
		if (held >= 0 && vr_primary_magwell(W, -1, &w, well, ins, nullptr, nullptr) &&
		    VR_GetMagState(w) == VR_MAG_LOOSE) {
			float hv[3], wv[3] = { 0.f, 0.f, 0.f };
			if (VR_GetHandVelocityStage(held, hv)) vr_room_dir_to_world(bf, hv, wv);
			const float outSpeed = -(wv[0]*ins[0] + wv[1]*ins[1] + wv[2]*ins[2]);   // m/s, out of the well
			if (outSpeed > 1.1f || fabsf(rollRate) > 450.f) {
				float p[3], v[3];
				vr_loose_mag_pos(W, well, ins, p);
				for (int k = 0; k < 3; ++k) v[k] = (wv[k] - ins[k] * 1.0f) * W;   // carried off by the flick
				// A wrist roll slings it off sideways: tangential velocity omega x r about the barrel axis.
				// Room->world is a reflection, and angular velocity is a pseudovector, so the axis flips
				// sign on the way across.
				float hpos[3], hr[3], hu[3], hf[3];
				if (vr_hand_world_pose(bf, held, hpos, hr, hu, hf)) {
					const float wz = -rollRate * 0.01745329f;             // rad/s about the world barrel axis
					const float om[3] = { hf[0]*wz, hf[1]*wz, hf[2]*wz };
					const float rr[3] = { p[0]-hpos[0], p[1]-hpos[1], p[2]-hpos[2] };   // world units
					v[0] += om[1]*rr[2] - om[2]*rr[1];
					v[1] += om[2]*rr[0] - om[0]*rr[2];
					v[2] += om[0]*rr[1] - om[1]*rr[0];
				}
				vr_spawn_falling_mag(p, v, get_player_weapon_ammo_type(current_player_index, w, _primary_weapon));
				VR_SetMagState(w, VR_MAG_OUT);
				VR_Vibrate(held, 45, 0.7f);
			}
		}
	}

	for (int hand = 0; hand < 2; ++hand) {
		const short mag = VR_GetHandMagazine(hand);
		if (mag == NONE) continue;

		float pos[3], wr[3], wu[3], wf[3];
		if (!vr_hand_world_pose(bf, hand, pos, wr, wu, wf)) continue;

		// Letting go drops it -- a fresh mag costs nothing until it is seated, a spent one is empty.
		float grip = 0.f;
		VR_GetRawHandInput(hand, nullptr, nullptr, nullptr, &grip, nullptr);
		if (grip <= 0.5f) {
			// Thrown with the hand's velocity at the moment it lets go, so a toss carries.
			float hv[3], wv[3] = { 0.f, 0.f, 0.f };
			if (VR_GetHandVelocityStage(hand, hv)) {
				vr_room_dir_to_world(bf, hv, wv);
				for (int k = 0; k < 3; ++k) wv[k] *= W;
			}
			vr_spawn_falling_mag(pos, wv, mag);
			VR_SetHandMagazine(hand, NONE);
			s_vrMagMustClear[hand] = false;
			continue;
		}

		// Main-ammo magazines go in the main magwell; second-trigger ammo (grenades) in the second one.
		const int gunHand = VR_GetHeldWeaponHand();
		const short gunType = (gunHand >= 0 && s_vrWeaponPose[gunHand].valid) ? s_vrWeaponPose[gunHand].weapon_type : (short)NONE;
		const bool secondWell = gunType != NONE &&
			mag != get_player_weapon_ammo_type(current_player_index, gunType, _primary_weapon);
		short w = NONE; float well[3], ins[3], r = 0.f, tol = 180.f;
		if (!vr_primary_magwell(W, hand, &w, well, ins, &r, &tol, secondWell)) continue;
		if (!vr_magwell_accepts(w, mag)) continue;

		const float dx = pos[0]-well[0], dy = pos[1]-well[1], dz = pos[2]-well[2];
		const float d2 = dx*dx + dy*dy + dz*dz;
		// A magazine just pulled out of this well starts INSIDE its radius. It has to be taken clear of
		// the well first; otherwise pulling it would re-seat it on the very next frame.
		if (s_vrMagMustClear[hand]) {
			if (d2 > (r * 1.6f) * (r * 1.6f)) s_vrMagMustClear[hand] = false;
			continue;
		}
		if (d2 > r * r) continue;
		// The magazine should be coming in along the insertion path, from outside the well -- not waved
		// past sideways. angle_tolerance widens how sloppy that approach may be.
		if (d2 > 1e-6f && tol < 179.f) {
			const float inv = 1.0f / sqrtf(d2);
			const float dot = -(dx*ins[0] + dy*ins[1] + dz*ins[2]) * inv;
			if (dot < cosf(tol * 0.01745329f)) continue;
		}

		const short magRounds = VR_GetHandMagazineRounds(hand);
		VR_SetHandMagazine(hand, NONE);
		VR_RequestManualReload(w, mag, magRounds);                // loaded + reload sound, next tick
		if (mag == get_player_weapon_ammo_type(current_player_index, w, _primary_weapon))
			VR_SetMagState(w, VR_MAG_IN);
		VR_Vibrate(hand, 60, 0.9f);
		VR_Vibrate(1 - hand, 45, 0.7f);
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
				logWarning("VRRELOAD flick hand=%d weapon=%d trigger=%d (primaryHand=%d offhandWeapon=%d)",
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
			// hand where a gun mirrors on the left. The same fist.md3 ships in all three scenarios, but
			// Marathon 1 marks the fist <left_handed_weapon> in its vr_weapons.mml and M2/Infinity do not --
			// that flag is the per-scenario difference, so honour it, inverted relative to the guns' rule.
			// Confirmed on device for M2 (right hand mirrored, 2026-09-27); M1 follows from the flag.
			const bool flip = (hand == 1) != VR_IsWeaponNaturallyLeftHanded(_weapon_fist);
			vr_draw_shape_at(view, fColl, fSeq, fLow, pos, wr, wu, wf, flip, true, 1.0f);
		}

		// ---- a magazine in this hand -----------------------------------------------------------
		const short mag = VR_GetHandMagazine(hand);
		if (mag != NONE) {
			float mp[3];
			for (int k = 0; k < 3; ++k) mp[k] = pos[k] + wf[k] * 0.05f * bf.W;   // just past the knuckles
			vr_draw_item_at(view, mag, mp, wr, wu, wf, 0.6f);
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
			const short item = vr_pouch_ammo_type(slot, 1 - held);
			if (item == NONE) continue;
			const bool empty = get_player_data(current_player_index)->items[item] <= 0;
			vr_holster_anchor a;
			VR_GetHolsterAnchorForPlayer(slot, VR_EyeHeightM(), &a);
			float c[3];
			vr_body_to_world(bf, a.x, a.y, a.z, c);
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

	// ---- a loosened magazine hanging out of the gun, and magazines falling away ---------------------
	{
		float bR[3], bU[3], bF[3];
		vr_room_dir_to_world(bf, bf.right, bR);
		vr_room_dir_to_world(bf, bf.up,    bU);
		vr_room_dir_to_world(bf, bf.fwd,   bF);
		short lw = NONE; float lwell[3], lins[3];
		if (vr_primary_magwell(bf.W, -1, &lw, lwell, lins, nullptr, nullptr) && VR_GetMagState(lw) == VR_MAG_LOOSE) {
			const vr_weapon_pose& wp = s_vrWeaponPose[held];
			float mp[3];
			vr_loose_mag_pos(bf.W, lwell, lins, mp);
			vr_draw_item_at(view, get_player_weapon_ammo_type(current_player_index, lw, _primary_weapon),
			                mp, wp.right, wp.up, wp.fwd, 0.6f);
			// orange ring: this magwell needs clearing -- pull the mag out or flick it free
			static int lpulse = 0; ++lpulse;
			const float rr = 0.05f * bf.W * (0.85f + 0.15f * sinf(lpulse * 0.25f));
			GLfloat v[24 * 2 * 3]; int n = 0;
			for (int i = 0; i < 24; ++i) {
				const float t0 = i * 6.2831853f / 24, t1 = (i + 1) * 6.2831853f / 24;
				for (int k = 0; k < 3; ++k) v[n + k]     = mp[k] + rr * (cosf(t0) * wp.right[k] + sinf(t0) * wp.fwd[k]);
				for (int k = 0; k < 3; ++k) v[n + 3 + k] = mp[k] + rr * (cosf(t1) * wp.right[k] + sinf(t1) * wp.fwd[k]);
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
			glColor4f(1.0f, 0.5f, 0.05f, 0.95f);
			glVertexPointer(3, GL_FLOAT, 0, v);
			glDrawArrays(GL_LINES, 0, n / 3);
			glLineWidth(1.0f);
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			glEnable(GL_TEXTURE_2D);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		for (const auto& fm : s_fallingMags) vr_draw_item_at(view, fm.item, fm.pos, bR, bU, bF, 0.6f);
	}

	// ---- the magwell to aim for while you hold a magazine ------------------------------------------
	for (int hand = 0; hand < 2; ++hand) {
		if (VR_GetHandMagazine(hand) == NONE || held < 0 || held == hand) continue;
		const vr_weapon_pose& wp = s_vrWeaponPose[held];
		if (!wp.valid) continue;
		const bool secondWell = VR_GetHandMagazine(hand) !=
			get_player_weapon_ammo_type(current_player_index, wp.weapon_type, _primary_weapon);
		const vr_attach_point* mw = secondWell ? VR_GetMagwell2OrDefault(wp.weapon_type)
		                                       : VR_GetMagwellOrDefault(wp.weapon_type);

		float well[3];
		vr_weapon_local_to_world(wp, bf.W, mw->x, mw->y, mw->z, well);
		float pos[3], wr[3], wu[3], wf[3];
		if (!vr_hand_world_pose(bf, hand, pos, wr, wu, wf)) continue;
		const float dx = pos[0]-well[0], dy = pos[1]-well[1], dz = pos[2]-well[2];
		const float r  = mw->radius * bf.W;
		const bool  inRange = (dx*dx + dy*dy + dz*dz) <= r*r;

		// Ring in the plane across the magwell opening (perpendicular to the weapon's up axis, which is
		// the default insertion direction), plus a line from the magazine to it.
		static int pulse = 0; ++pulse;
		const float rr = r * (0.85f + 0.15f * sinf(pulse * 0.2f));
		GLfloat v[(24 + 1) * 2 * 3]; int n = 0;
		auto push = [&](const float p[3]) { v[n++] = p[0]; v[n++] = p[1]; v[n++] = p[2]; };
		for (int i = 0; i < 24; ++i) {
			const float t0 = i * 6.2831853f / 24, t1 = (i + 1) * 6.2831853f / 24;
			float a[3], b[3];
			for (int k = 0; k < 3; ++k) {
				a[k] = well[k] + rr * (cosf(t0) * wp.right[k] + sinf(t0) * wp.fwd[k]);
				b[k] = well[k] + rr * (cosf(t1) * wp.right[k] + sinf(t1) * wp.fwd[k]);
			}
			push(a); push(b);
		}
		push(pos); push(well);

		glUseProgram(0);
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(3.0f);
		const bool accepts = vr_magwell_accepts(wp.weapon_type, VR_GetHandMagazine(hand));
		if (!accepts)     glColor4f(1.0f, 0.15f, 0.1f, 0.9f);   // red: the old mag is still in
		else if (inRange) glColor4f(0.2f, 1.0f, 0.3f, 0.95f);   // green: let it seat
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
	for (int slot = 0; slot < VR_HOLSTER_COUNT; ++slot) {
		const bool isPouch   = VR_HolsterSlotIsPouch(slot);
		const bool isSel     = (slot == selSlot);
		const bool hasWeapon = !isPouch && vr_holster_occupant(slot) != NONE;
		if (!setupBoxes && !isPouch && !isSel) continue;  // outside setup: only pouches + the tuner target
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
		bool renderedAs3D = weaponMdl &&
			OGL_RenderVRWeaponModel(rect, display_data.collection, 0 /*CLUT*/,
				weaponMdl, cwx, cwy, cwz_slid, wrx, wup_mdl, wfwd_mdl, isStaticWpn,
				pFrame, pMix, pNext);
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
			OGL_RenderVRWeaponModel(rect, s_cachedWpnColl, s_cachedWpnClut,
				s_cachedWpnModel, cwx, cwy, cwz - s_lowerFrac[s_cachedWpnHand] * s_cachedHh * 4.0f,
				wrx, wup_mdl, wfwd_mdl, s_cachedWpnIsStatic, s_cachedMd3Frame);
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
						if (eye == 0) { vr_update_torso(); vr_update_attach_tuner(); vr_update_holster_grabs(view); vr_update_magazine_seating(view); vr_update_spin_reload(); }
						render_vr_holstered_weapons(view);
						render_vr_hand_visuals(view);   // fists, pouch ammo, mag in hand, magwell ring
						render_vr_attach_gizmos(view);   // physical-weapon attach-point debug overlay

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
