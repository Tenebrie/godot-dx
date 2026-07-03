/**************************************************************************/
/*  nav_mesh_generator_3d.cpp                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "nav_mesh_generator_3d.h"

#include "core/config/project_settings.h"
#include "core/os/thread.h"
#include "scene/3d/node_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/3d/navigation_mesh_source_geometry_data_3d.h"
#include "scene/resources/navigation_mesh.h"

#include <Recast.h>

#ifdef CLIPPER2_ENABLED
#include <thirdparty/clipper2/include/clipper2/clipper.h>
#include <thirdparty/misc/polypartition.h>
#include <poly2tri/poly2tri.h>
#endif // CLIPPER2_ENABLED

NavMeshGenerator3D *NavMeshGenerator3D::singleton = nullptr;
Mutex NavMeshGenerator3D::baking_navmesh_mutex;
Mutex NavMeshGenerator3D::generator_task_mutex;
RWLock NavMeshGenerator3D::generator_parsers_rwlock;
bool NavMeshGenerator3D::use_threads = true;
bool NavMeshGenerator3D::baking_use_multiple_threads = true;
bool NavMeshGenerator3D::baking_use_high_priority_threads = true;
HashMap<Ref<NavigationMesh>, NavMeshGenerator3D::NavMeshGeneratorTask3D *> NavMeshGenerator3D::baking_navmeshes;
HashMap<WorkerThreadPool::TaskID, NavMeshGenerator3D::NavMeshGeneratorTask3D *> NavMeshGenerator3D::generator_tasks;
LocalVector<NavMeshGeometryParser3D *> NavMeshGenerator3D::generator_parsers;

static const char *_navmesh_bake_state_msgs[(size_t)NavMeshGenerator3D::NavMeshBakeState::BAKE_STATE_MAX] = {
	"",
	"Setting up configuration...",
	"Calculating grid size...",
	"Creating heightfield...",
	"Marking walkable triangles...",
	"Constructing compact heightfield...", // step 5
	"Eroding walkable area...",
	"Sample partitioning...",
	"Creating contours...",
	"Creating polymesh...",
	"Converting to native navigation mesh...", // step 10
	"Baking cleanup...",
	"Baking finished.",
};

NavMeshGenerator3D *NavMeshGenerator3D::get_singleton() {
	return singleton;
}

NavMeshGenerator3D::NavMeshGenerator3D() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;

	baking_use_multiple_threads = GLOBAL_GET("navigation/baking/thread_model/baking_use_multiple_threads");
	baking_use_high_priority_threads = GLOBAL_GET("navigation/baking/thread_model/baking_use_high_priority_threads");

	// Using threads might cause problems on certain exports or with the Editor on certain devices.
	// This is the main switch to turn threaded navmesh baking off should the need arise.
	use_threads = baking_use_multiple_threads;
}

NavMeshGenerator3D::~NavMeshGenerator3D() {
	cleanup();
}

void NavMeshGenerator3D::sync() {
	if (generator_tasks.is_empty()) {
		return;
	}

	MutexLock baking_navmesh_lock(baking_navmesh_mutex);
	{
		MutexLock generator_task_lock(generator_task_mutex);

		LocalVector<WorkerThreadPool::TaskID> finished_task_ids;

		for (KeyValue<WorkerThreadPool::TaskID, NavMeshGeneratorTask3D *> &E : generator_tasks) {
			if (WorkerThreadPool::get_singleton()->is_task_completed(E.key)) {
				WorkerThreadPool::get_singleton()->wait_for_task_completion(E.key);
				finished_task_ids.push_back(E.key);

				NavMeshGeneratorTask3D *generator_task = E.value;
				DEV_ASSERT(generator_task->status == NavMeshGeneratorTask3D::TaskStatus::BAKING_FINISHED);

				baking_navmeshes.erase(generator_task->navigation_mesh);
				if (generator_task->callback.is_valid()) {
					generator_emit_callback(generator_task->callback);
				}
				generator_task->navigation_mesh->emit_changed();
				memdelete(generator_task);
			}
		}

		for (WorkerThreadPool::TaskID finished_task_id : finished_task_ids) {
			generator_tasks.erase(finished_task_id);
		}
	}
}

void NavMeshGenerator3D::cleanup() {
	MutexLock baking_navmesh_lock(baking_navmesh_mutex);
	{
		MutexLock generator_task_lock(generator_task_mutex);

		baking_navmeshes.clear();

		for (KeyValue<WorkerThreadPool::TaskID, NavMeshGeneratorTask3D *> &E : generator_tasks) {
			WorkerThreadPool::get_singleton()->wait_for_task_completion(E.key);
			NavMeshGeneratorTask3D *generator_task = E.value;
			memdelete(generator_task);
		}
		generator_tasks.clear();

		generator_parsers_rwlock.write_lock();
		generator_parsers.clear();
		generator_parsers_rwlock.write_unlock();
	}
}

void NavMeshGenerator3D::finish() {
	cleanup();
}

void NavMeshGenerator3D::parse_source_geometry_data(Ref<NavigationMesh> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_root_node, const Callable &p_callback) {
	ERR_FAIL_COND(!Thread::is_main_thread());
	ERR_FAIL_COND(p_navigation_mesh.is_null());
	ERR_FAIL_NULL(p_root_node);
	ERR_FAIL_COND(!p_root_node->is_inside_tree());
	ERR_FAIL_COND(p_source_geometry_data.is_null());

	generator_parse_source_geometry_data(p_navigation_mesh, p_source_geometry_data, p_root_node);

	if (p_callback.is_valid()) {
		generator_emit_callback(p_callback);
	}
}

void NavMeshGenerator3D::bake_from_source_geometry_data(Ref<NavigationMesh> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, const Callable &p_callback) {
	ERR_FAIL_COND(p_navigation_mesh.is_null());
	ERR_FAIL_COND(p_source_geometry_data.is_null());

	if (!p_source_geometry_data->has_data()) {
		p_navigation_mesh->clear();
		if (p_callback.is_valid()) {
			generator_emit_callback(p_callback);
		}
		p_navigation_mesh->emit_changed();
		return;
	}

	if (is_baking(p_navigation_mesh)) {
		ERR_FAIL_MSG("NavigationMesh is already baking. Wait for current bake to finish.");
	}
	baking_navmesh_mutex.lock();
	NavMeshGeneratorTask3D generator_task;
	baking_navmeshes.insert(p_navigation_mesh, &generator_task);
	baking_navmesh_mutex.unlock();

	generator_task.navigation_mesh = p_navigation_mesh;
	generator_task.source_geometry_data = p_source_geometry_data;
	generator_task.status = NavMeshGeneratorTask3D::TaskStatus::BAKING_STARTED;

	generator_bake_from_source_geometry_data(&generator_task);

	baking_navmesh_mutex.lock();
	baking_navmeshes.erase(p_navigation_mesh);
	baking_navmesh_mutex.unlock();

	if (p_callback.is_valid()) {
		generator_emit_callback(p_callback);
	}

	p_navigation_mesh->emit_changed();
}

void NavMeshGenerator3D::bake_from_source_geometry_data_async(Ref<NavigationMesh> p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, const Callable &p_callback) {
	ERR_FAIL_COND(p_navigation_mesh.is_null());
	ERR_FAIL_COND(p_source_geometry_data.is_null());

	if (!p_source_geometry_data->has_data()) {
		p_navigation_mesh->clear();
		if (p_callback.is_valid()) {
			generator_emit_callback(p_callback);
		}
		p_navigation_mesh->emit_changed();
		return;
	}

	if (!use_threads) {
		bake_from_source_geometry_data(p_navigation_mesh, p_source_geometry_data, p_callback);
		return;
	}

	if (is_baking(p_navigation_mesh)) {
		ERR_FAIL_MSG("NavigationMesh is already baking. Wait for current bake to finish.");
		return;
	}
	baking_navmesh_mutex.lock();
	NavMeshGeneratorTask3D *generator_task = memnew(NavMeshGeneratorTask3D);
	baking_navmeshes.insert(p_navigation_mesh, generator_task);
	baking_navmesh_mutex.unlock();

	generator_task->navigation_mesh = p_navigation_mesh;
	generator_task->source_geometry_data = p_source_geometry_data;
	generator_task->callback = p_callback;
	generator_task->status = NavMeshGeneratorTask3D::TaskStatus::BAKING_STARTED;
	generator_task->thread_task_id = WorkerThreadPool::get_singleton()->add_native_task(&NavMeshGenerator3D::generator_thread_bake, generator_task, NavMeshGenerator3D::baking_use_high_priority_threads, SNAME("NavMeshGeneratorBake3D"));
	MutexLock generator_task_lock(generator_task_mutex);
	generator_tasks.insert(generator_task->thread_task_id, generator_task);
}

bool NavMeshGenerator3D::is_baking(Ref<NavigationMesh> p_navigation_mesh) {
	MutexLock baking_navmesh_lock(baking_navmesh_mutex);
	return baking_navmeshes.has(p_navigation_mesh);
}

String NavMeshGenerator3D::get_baking_state_msg(Ref<NavigationMesh> p_navigation_mesh) {
	String bake_state_msg;
	MutexLock baking_navmesh_lock(baking_navmesh_mutex);
	if (baking_navmeshes.has(p_navigation_mesh)) {
		bake_state_msg = _navmesh_bake_state_msgs[baking_navmeshes[p_navigation_mesh]->bake_state];
	} else {
		bake_state_msg = _navmesh_bake_state_msgs[NavMeshBakeState::BAKE_STATE_NONE];
	}
	return bake_state_msg;
}

void NavMeshGenerator3D::generator_thread_bake(void *p_arg) {
	NavMeshGeneratorTask3D *generator_task = static_cast<NavMeshGeneratorTask3D *>(p_arg);

	generator_bake_from_source_geometry_data(generator_task);

	generator_task->status = NavMeshGeneratorTask3D::TaskStatus::BAKING_FINISHED;
}

void NavMeshGenerator3D::generator_parse_geometry_node(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node, bool p_recurse_children) {
	generator_parsers_rwlock.read_lock();
	for (const NavMeshGeometryParser3D *parser : generator_parsers) {
		if (!parser->callback.is_valid()) {
			continue;
		}
		parser->callback.call(p_navigation_mesh, p_source_geometry_data, p_node);
	}
	generator_parsers_rwlock.read_unlock();

	if (p_recurse_children) {
		for (int i = 0; i < p_node->get_child_count(); i++) {
			generator_parse_geometry_node(p_navigation_mesh, p_source_geometry_data, p_node->get_child(i), p_recurse_children);
		}
	}
}

void NavMeshGenerator3D::set_generator_parsers(const LocalVector<NavMeshGeometryParser3D *> &p_parsers) {
	RWLockWrite write_lock(generator_parsers_rwlock);
	generator_parsers = p_parsers;
}

void NavMeshGenerator3D::generator_parse_source_geometry_data(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_root_node) {
	Vector<Node *> parse_nodes;

	if (p_navigation_mesh->get_source_geometry_mode() == NavigationMesh::SOURCE_GEOMETRY_ROOT_NODE_CHILDREN) {
		parse_nodes.push_back(p_root_node);
	} else {
		parse_nodes = p_root_node->get_tree()->get_nodes_in_group(p_navigation_mesh->get_source_group_name());
	}

	Transform3D root_node_transform = Transform3D();
	if (Object::cast_to<Node3D>(p_root_node)) {
		root_node_transform = Object::cast_to<Node3D>(p_root_node)->get_global_transform().affine_inverse();
	}

	p_source_geometry_data->clear();
	p_source_geometry_data->root_node_transform = root_node_transform;

	bool recurse_children = p_navigation_mesh->get_source_geometry_mode() != NavigationMesh::SOURCE_GEOMETRY_GROUPS_EXPLICIT;

	for (Node *parse_node : parse_nodes) {
		generator_parse_geometry_node(p_navigation_mesh, p_source_geometry_data, parse_node, recurse_children);
	}
}

#ifdef CLIPPER2_ENABLED
// Triangulate a single "outer" polytree node (an island of walkable area) with its immediate hole
// children using poly2tri's constrained Delaunay triangulation. Recurses into island-in-hole nesting.
static void generator_flat_triangulate_outer(
		const Clipper2Lib::PolyPathD *p_outer_node,
		Vector<Vector3> &r_nav_vertices,
		Vector<Vector<int>> &r_nav_polygons,
		HashMap<Vector3, int> &r_point_to_index,
		const Vector3 &p_plane_origin,
		const Vector3 &p_basis_u,
		const Vector3 &p_basis_v) {
	using namespace Clipper2Lib;

	// Own all p2t::Point allocations so poly2tri can hold raw pointers into them.
	std::vector<p2t::Point *> owned_points;

	auto build_ring = [&owned_points](const PolyPathD *p_ring_node, std::vector<p2t::Point *> &r_out) {
		r_out.reserve(p_ring_node->Polygon().size());
		for (const PointD &polypath_point : p_ring_node->Polygon()) {
			p2t::Point *p = new p2t::Point(polypath_point.x, polypath_point.y);
			owned_points.push_back(p);
			r_out.push_back(p);
		}
	};

	std::vector<p2t::Point *> outer;
	build_ring(p_outer_node, outer);

	// A ring with fewer than 3 vertices cannot be triangulated; skip but still recurse into any islands.
	if (outer.size() >= 3) {
		p2t::CDT cdt(outer);

		for (size_t i = 0; i < p_outer_node->Count(); i++) {
			const PolyPathD *hole_node = p_outer_node->Child(i);
			std::vector<p2t::Point *> hole;
			build_ring(hole_node, hole);
			if (hole.size() >= 3) {
				cdt.AddHole(hole);
			}
		}

		cdt.Triangulate();
		const std::vector<p2t::Triangle *> triangles = cdt.GetTriangles();
		for (p2t::Triangle *tri : triangles) {
			Vector<int> nav_polygon;
			nav_polygon.resize(3);
			for (int i = 0; i < 3; i++) {
				const p2t::Point *pt = tri->GetPoint(i);
				const Vector3 world_vertex = p_plane_origin + p_basis_u * (real_t)pt->x + p_basis_v * (real_t)pt->y;
				HashMap<Vector3, int>::Iterator E = r_point_to_index.find(world_vertex);
				if (!E) {
					E = r_point_to_index.insert(world_vertex, r_nav_vertices.size());
					r_nav_vertices.push_back(world_vertex);
				}
				// Reverse the winding: poly2tri emits CCW-in-(u,v) triangles, but Godot's navigation
				// mesh expects the opposite winding so the polygons face along the plane normal
				// (correct surface normal for the navigation map and backface-culled debug render).
				nav_polygon.write[2 - i] = E->value;
			}
			r_nav_polygons.push_back(nav_polygon);
		}
	}

	for (p2t::Point *p : owned_points) {
		delete p;
	}

	// Islands-in-holes: each direct hole child may contain further outer polygons that need their own CDT.
	for (size_t i = 0; i < p_outer_node->Count(); i++) {
		const PolyPathD *hole_node = p_outer_node->Child(i);
		for (size_t j = 0; j < hole_node->Count(); j++) {
			generator_flat_triangulate_outer(
					hole_node->Child(j),
					r_nav_vertices, r_nav_polygons, r_point_to_index,
					p_plane_origin, p_basis_u, p_basis_v);
		}
	}
}

// Flatten a polytree into polypartition's List<TPPLPoly> representation (holes marked with the
// SetHole flag), then ear-clip. Kept as a legacy option: it's the fastest triangulator available
// but produces sliver triangles that destabilize corridor A*.
static void generator_flat_flatten_polytree_tppl(List<TPPLPoly> &r_polys, const Clipper2Lib::PolyPathD *p_node) {
	using namespace Clipper2Lib;

	TPPLPoly tp;
	int size = p_node->Polygon().size();
	tp.Init(size);
	int j = 0;
	for (const PointD &polypath_point : p_node->Polygon()) {
		tp[j] = Vector2(static_cast<real_t>(polypath_point.x), static_cast<real_t>(polypath_point.y));
		++j;
	}
	if (p_node->IsHole()) {
		tp.SetOrientation(TPPL_ORIENTATION_CW);
		tp.SetHole(true);
	} else {
		tp.SetOrientation(TPPL_ORIENTATION_CCW);
	}
	r_polys.push_back(tp);

	for (size_t i = 0; i < p_node->Count(); i++) {
		generator_flat_flatten_polytree_tppl(r_polys, p_node->Child(i));
	}
}

static bool generator_flat_triangulate_polypartition(
		const Clipper2Lib::PolyTreeD &p_polytree,
		Vector<Vector3> &r_nav_vertices,
		Vector<Vector<int>> &r_nav_polygons,
		HashMap<Vector3, int> &r_point_to_index,
		const Vector3 &p_plane_origin,
		const Vector3 &p_basis_u,
		const Vector3 &p_basis_v) {
	List<TPPLPoly> tppl_in_polygon, tppl_out_polygon;
	for (size_t i = 0; i < p_polytree.Count(); i++) {
		generator_flat_flatten_polytree_tppl(tppl_in_polygon, p_polytree[i]);
	}
	TPPLPartition tpart;
	if (tpart.Triangulate_EC(&tppl_in_polygon, &tppl_out_polygon) == 0) {
		return false;
	}
	for (const TPPLPoly &tp : tppl_out_polygon) {
		const int point_count = tp.GetNumPoints();
		Vector<int> nav_polygon;
		nav_polygon.resize(point_count);
		for (int64_t i = 0; i < point_count; i++) {
			const Vector2 &point = tp[i];
			const Vector3 world_vertex = p_plane_origin + p_basis_u * (real_t)point.x + p_basis_v * (real_t)point.y;
			HashMap<Vector3, int>::Iterator E = r_point_to_index.find(world_vertex);
			if (!E) {
				E = r_point_to_index.insert(world_vertex, r_nav_vertices.size());
				r_nav_vertices.push_back(world_vertex);
			}
			nav_polygon.write[point_count - 1 - i] = E->value;
		}
		r_nav_polygons.push_back(nav_polygon);
	}
	return true;
}

// Builds a Clipper2 path set from projected obstructions, optionally filtered by carve flag.
static void _flat_collect_projected_obstructions(Clipper2Lib::PathsD &r_paths, const Vector<NavigationMeshSourceGeometryData3D::ProjectedObstruction> &p_projected_obstructions, const Vector3 &p_basis_u, const Vector3 &p_basis_v, bool p_want_carve) {
	using namespace Clipper2Lib;
	for (const NavigationMeshSourceGeometryData3D::ProjectedObstruction &projected_obstruction : p_projected_obstructions) {
		if (projected_obstruction.carve != p_want_carve) {
			continue;
		}
		if (projected_obstruction.vertices.is_empty() || projected_obstruction.vertices.size() % 3 != 0) {
			continue;
		}
		const int obstruction_vertex_count = projected_obstruction.vertices.size() / 3;
		PathD clip_path;
		clip_path.reserve(obstruction_vertex_count);
		for (int i = 0; i < obstruction_vertex_count; i++) {
			const Vector3 obstruction_vertex(projected_obstruction.vertices[i * 3 + 0], projected_obstruction.vertices[i * 3 + 1], projected_obstruction.vertices[i * 3 + 2]);
			clip_path.emplace_back(obstruction_vertex.dot(p_basis_u), obstruction_vertex.dot(p_basis_v));
		}
		if (!IsPositive(clip_path)) {
			std::reverse(clip_path.begin(), clip_path.end());
		}
		r_paths.push_back(std::move(clip_path));
	}
}
#endif // CLIPPER2_ENABLED

// Flat baking: generate the navigation mesh by 2D polygon clipping on a single
// plane instead of voxelizing the source geometry with Recast. Source geometry
// triangles that rise above the walkable band (agent_max_climb) are treated as
// solid obstacle footprints, triangles within the band as walkable floor.
static void generator_bake_flat_from_source_geometry_data(Ref<NavigationMesh> p_navigation_mesh, const Vector<float> &p_vertices, const Vector<int> &p_indices, const Vector<NavigationMeshSourceGeometryData3D::ProjectedObstruction> &p_projected_obstructions) {
#ifndef CLIPPER2_ENABLED
	ERR_FAIL_MSG("NavigationMesh flat baking requires the Clipper2 library which is not available in this build.");
#else
	using namespace Clipper2Lib;

	const Plane plane = p_navigation_mesh->get_baking_flat_plane();
	Vector3 plane_normal = plane.normal;
	ERR_FAIL_COND_MSG(plane_normal.is_zero_approx(), "NavigationMesh flat baking plane has a zero-length normal.");
	plane_normal.normalize();

	// Build an orthonormal basis (u, v) spanning the baking plane. Note u.cross(v) == plane_normal,
	// so a CCW winding in (u, v) coordinates yields a polygon facing along the plane normal.
	const Vector3 basis_helper = Math::abs(plane_normal.x) < 0.9f ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
	const Vector3 basis_u = plane_normal.cross(basis_helper).normalized();
	const Vector3 basis_v = plane_normal.cross(basis_u).normalized();
	const Vector3 plane_origin = plane_normal * plane.d;

	const float agent_radius = p_navigation_mesh->get_agent_radius();
	// Geometry rising more than this above the plane is an obstacle (walls, obstacle tops);
	// geometry within the band around the plane is walkable floor.
	const float obstacle_threshold = MAX(p_navigation_mesh->get_agent_max_climb(), (float)CMP_EPSILON);
	// Only triangles facing the plane within this slope count as walkable floor. This excludes
	// obstacle undersides and near-vertical faces, and (together with forcing a consistent winding
	// before the union below) keeps the floor from self-cancelling under the NonZero fill rule.
	const float cos_max_slope = Math::cos(Math::deg_to_rad(p_navigation_mesh->get_agent_max_slope()));

	const float *verts = p_vertices.ptr();
	const int *tris = p_indices.ptr();
	const int ntris = p_indices.size() / 3;

	PathsD traversable_polygon_paths;
	PathsD obstruction_polygon_paths;

	for (int i = 0; i < ntris; i++) {
		const int vi0 = tris[i * 3 + 0];
		const int vi1 = tris[i * 3 + 1];
		const int vi2 = tris[i * 3 + 2];
		const Vector3 p0(verts[vi0 * 3 + 0], verts[vi0 * 3 + 1], verts[vi0 * 3 + 2]);
		const Vector3 p1(verts[vi1 * 3 + 0], verts[vi1 * 3 + 1], verts[vi1 * 3 + 2]);
		const Vector3 p2(verts[vi2 * 3 + 0], verts[vi2 * 3 + 1], verts[vi2 * 3 + 2]);

		const float d0 = plane_normal.dot(p0) - plane.d;
		const float d1 = plane_normal.dot(p1) - plane.d;
		const float d2 = plane_normal.dot(p2) - plane.d;
		const float tri_max = MAX(d0, MAX(d1, d2));
		const float tri_min = MIN(d0, MIN(d1, d2));

		PathD tri_path;
		tri_path.reserve(3);
		tri_path.emplace_back(p0.dot(basis_u), p0.dot(basis_v));
		tri_path.emplace_back(p1.dot(basis_u), p1.dot(basis_v));
		tri_path.emplace_back(p2.dot(basis_u), p2.dot(basis_v));

		if (tri_max > obstacle_threshold) {
			// Rises above the walkable band: carve as a solid obstacle footprint.
			if (!IsPositive(tri_path)) {
				std::reverse(tri_path.begin(), tri_path.end());
			}
			obstruction_polygon_paths.push_back(std::move(tri_path));
			continue;
		}

		if (tri_min < -obstacle_threshold) {
			// Below the walkable band, e.g. a lower floor level: not part of this plane's layer.
			continue;
		}

		// Within the band around the plane. Only count triangles roughly parallel to the plane as
		// floor so near-vertical faces don't pollute the traversable area. Use the absolute alignment
		// so floor geometry is accepted regardless of its source winding (consistency is enforced
		// below). Obstacle undersides also land here but get carved back out by their tops above.
		Vector3 tri_normal = (p1 - p0).cross(p2 - p0);
		const float tri_double_area = tri_normal.length();
		if (tri_double_area <= (float)CMP_EPSILON) {
			continue; // Degenerate or projects to a line.
		}
		tri_normal /= tri_double_area;
		if (Math::abs(tri_normal.dot(plane_normal)) < cos_max_slope) {
			continue; // Too steep relative to the plane.
		}

		// Force a consistent winding so coplanar floor triangles merge instead of cancelling.
		if (!IsPositive(tri_path)) {
			std::reverse(tri_path.begin(), tri_path.end());
		}
		traversable_polygon_paths.push_back(std::move(tri_path));
	}

	// Explicit projected obstructions (e.g. from NavigationObstacle3D) that are affected by agent radius.
	_flat_collect_projected_obstructions(obstruction_polygon_paths, p_projected_obstructions, basis_u, basis_v, false);

	// Optional baking bounds: project the AABB corners onto the plane and clip to their 2D extent.
	const AABB baking_aabb = p_navigation_mesh->get_filter_baking_aabb();
	if (baking_aabb.has_volume()) {
		const Vector3 aabb_offset = p_navigation_mesh->get_filter_baking_aabb_offset();
		const Vector3 aabb_position = baking_aabb.position + aabb_offset;
		double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0;
		for (int corner = 0; corner < 8; corner++) {
			const Vector3 world_corner = aabb_position + Vector3(
														 (corner & 1) ? baking_aabb.size.x : 0.0f,
														 (corner & 2) ? baking_aabb.size.y : 0.0f,
														 (corner & 4) ? baking_aabb.size.z : 0.0f);
			const double cx = world_corner.dot(basis_u);
			const double cy = world_corner.dot(basis_v);
			if (corner == 0) {
				min_x = max_x = cx;
				min_y = max_y = cy;
			} else {
				min_x = MIN(min_x, cx);
				max_x = MAX(max_x, cx);
				min_y = MIN(min_y, cy);
				max_y = MAX(max_y, cy);
			}
		}
		const RectD clipper_rect = RectD(min_x, min_y, max_x, max_y);
		traversable_polygon_paths = RectClip(clipper_rect, traversable_polygon_paths);
		obstruction_polygon_paths = RectClip(clipper_rect, obstruction_polygon_paths);
	}

	if (traversable_polygon_paths.empty()) {
		p_navigation_mesh->clear();
		WARN_PRINT("NavigationMesh flat baking found no walkable floor near the baking plane. Check that 'baking_flat_plane' matches the floor height and orientation, and that 'agent_max_slope' / 'agent_max_climb' are not too restrictive.");
		return;
	}

	const PathsD dummy_clip_path;
	// Merge floor polygons into solid traversable areas.
	traversable_polygon_paths = Union(traversable_polygon_paths, dummy_clip_path, FillRule::NonZero);
	// Merge obstacle footprints into solid areas (no interior holes), giving them a true "inside".
	obstruction_polygon_paths = Union(obstruction_polygon_paths, dummy_clip_path, FillRule::NonZero);

	PathsD path_solution = Difference(traversable_polygon_paths, obstruction_polygon_paths, FillRule::NonZero);

	if (agent_radius > 0.0) {
		path_solution = InflatePaths(path_solution, -agent_radius, JoinType::Miter, EndType::Polygon);
	}

	// Carve obstructions that are not affected by agent radius, applied after erosion.
	PathsD carve_obstruction_paths;
	_flat_collect_projected_obstructions(carve_obstruction_paths, p_projected_obstructions, basis_u, basis_v, true);
	if (!carve_obstruction_paths.empty()) {
		carve_obstruction_paths = Union(carve_obstruction_paths, dummy_clip_path, FillRule::NonZero);
		path_solution = Difference(path_solution, carve_obstruction_paths, FillRule::NonZero);
	}

	if (path_solution.empty()) {
		p_navigation_mesh->clear();
		return;
	}

	// Build a proper polytree from the clipped path so outer polygons and their holes are nested
	// (rather than being a flat list of paths) — this is required by poly2tri (one CDT call per
	// outer, holes added via AddHole()) and equally useful as input to polypartition.
	PolyTreeD polytree;
	ClipperD clipper_D;
	clipper_D.AddSubject(path_solution);
	clipper_D.Execute(ClipType::Union, FillRule::NonZero, polytree);

	Vector<Vector3> nav_vertices;
	Vector<Vector<int>> nav_polygons;
	HashMap<Vector3, int> point_to_index;

	// Branch on the user's chosen triangulation algorithm. Everything above (boolean clipping,
	// obstacle carving, erosion) is shared; only the final triangulation step differs.
	// See NavigationMesh::FlatTriangulationAlgorithm for the semantic tradeoffs.
	switch (p_navigation_mesh->get_baking_flat_triangulation_algorithm()) {
		case NavigationMesh::FLAT_TRIANGULATION_EAR_CLIPPING: {
			if (!generator_flat_triangulate_polypartition(polytree, nav_vertices, nav_polygons, point_to_index, plane_origin, basis_u, basis_v)) {
				p_navigation_mesh->clear();
				ERR_FAIL_MSG("NavigationMesh flat triangulation failed. Unable to create a valid navigation mesh polygon layout from the provided source geometry.");
			}
		} break;
		case NavigationMesh::FLAT_TRIANGULATION_CDT:
		default: {
			for (size_t i = 0; i < polytree.Count(); i++) {
				generator_flat_triangulate_outer(polytree[i], nav_vertices, nav_polygons, point_to_index, plane_origin, basis_u, basis_v);
			}
		} break;
	}

	p_navigation_mesh->set_data(nav_vertices, nav_polygons);
#endif // CLIPPER2_ENABLED
}

void NavMeshGenerator3D::generator_bake_from_source_geometry_data(NavMeshGeneratorTask3D *p_generator_task) {
	Ref<NavigationMesh> p_navigation_mesh = p_generator_task->navigation_mesh;
	const Ref<NavigationMeshSourceGeometryData3D> &p_source_geometry_data = p_generator_task->source_geometry_data;

	if (p_navigation_mesh.is_null() || p_source_geometry_data.is_null()) {
		return;
	}

	Vector<float> source_geometry_vertices;
	Vector<int> source_geometry_indices;
	Vector<NavigationMeshSourceGeometryData3D::ProjectedObstruction> projected_obstructions;

	p_source_geometry_data->get_data(
			source_geometry_vertices,
			source_geometry_indices,
			projected_obstructions);

	if (source_geometry_vertices.size() < 3 || source_geometry_indices.size() < 3) {
		return;
	}

	if (p_navigation_mesh->get_baking_flat_enabled()) {
		generator_bake_flat_from_source_geometry_data(p_navigation_mesh, source_geometry_vertices, source_geometry_indices, projected_obstructions);
		p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_BAKE_FINISHED;
		return;
	}

	rcHeightfield *hf = nullptr;
	rcCompactHeightfield *chf = nullptr;
	rcContourSet *cset = nullptr;
	rcPolyMesh *poly_mesh = nullptr;
	rcPolyMeshDetail *detail_mesh = nullptr;
	rcContext ctx;

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_CONFIGURATION; // step #1

	const float *verts = source_geometry_vertices.ptr();
	const int nverts = source_geometry_vertices.size() / 3;
	const int *tris = source_geometry_indices.ptr();
	const int ntris = source_geometry_indices.size() / 3;

	float bmin[3], bmax[3];
	rcCalcBounds(verts, nverts, bmin, bmax);

	rcConfig cfg;
	memset(&cfg, 0, sizeof(cfg));

	cfg.cs = p_navigation_mesh->get_cell_size();
	cfg.ch = p_navigation_mesh->get_cell_height();
	if (p_navigation_mesh->get_border_size() > 0.0) {
		cfg.borderSize = (int)Math::ceil(p_navigation_mesh->get_border_size() / cfg.cs);
	}
	cfg.walkableSlopeAngle = p_navigation_mesh->get_agent_max_slope();
	cfg.walkableHeight = (int)Math::ceil(p_navigation_mesh->get_agent_height() / cfg.ch);
	cfg.walkableClimb = (int)Math::floor(p_navigation_mesh->get_agent_max_climb() / cfg.ch);
	cfg.walkableRadius = (int)Math::ceil(p_navigation_mesh->get_agent_radius() / cfg.cs);
	cfg.maxEdgeLen = (int)(p_navigation_mesh->get_edge_max_length() / p_navigation_mesh->get_cell_size());
	cfg.maxSimplificationError = p_navigation_mesh->get_edge_max_error();
	cfg.minRegionArea = (int)(p_navigation_mesh->get_region_min_size() * p_navigation_mesh->get_region_min_size());
	cfg.mergeRegionArea = (int)(p_navigation_mesh->get_region_merge_size() * p_navigation_mesh->get_region_merge_size());
	cfg.maxVertsPerPoly = (int)p_navigation_mesh->get_vertices_per_polygon();
	cfg.detailSampleDist = MAX(p_navigation_mesh->get_cell_size() * p_navigation_mesh->get_detail_sample_distance(), 0.1f);
	cfg.detailSampleMaxError = p_navigation_mesh->get_cell_height() * p_navigation_mesh->get_detail_sample_max_error();

	if (p_navigation_mesh->get_border_size() > 0.0 && !Math::is_zero_approx(Math::fmod(p_navigation_mesh->get_border_size(), p_navigation_mesh->get_cell_size()))) {
		WARN_PRINT("Property border_size is ceiled to cell_size voxel units and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.walkableHeight * cfg.ch, p_navigation_mesh->get_agent_height())) {
		WARN_PRINT("Property agent_height is ceiled to cell_height voxel units and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.walkableClimb * cfg.ch, p_navigation_mesh->get_agent_max_climb())) {
		WARN_PRINT("Property agent_max_climb is floored to cell_height voxel units and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.walkableRadius * cfg.cs, p_navigation_mesh->get_agent_radius())) {
		WARN_PRINT("Property agent_radius is ceiled to cell_size voxel units and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.maxEdgeLen * cfg.cs, p_navigation_mesh->get_edge_max_length())) {
		WARN_PRINT("Property edge_max_length is rounded to cell_size voxel units and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.minRegionArea, p_navigation_mesh->get_region_min_size() * p_navigation_mesh->get_region_min_size())) {
		WARN_PRINT("Property region_min_size is converted to int and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.mergeRegionArea, p_navigation_mesh->get_region_merge_size() * p_navigation_mesh->get_region_merge_size())) {
		WARN_PRINT("Property region_merge_size is converted to int and loses precision.");
	}
	if (!Math::is_equal_approx((float)cfg.maxVertsPerPoly, p_navigation_mesh->get_vertices_per_polygon())) {
		WARN_PRINT("Property vertices_per_polygon is converted to int and loses precision.");
	}
	if (p_navigation_mesh->get_cell_size() * p_navigation_mesh->get_detail_sample_distance() < 0.1f) {
		WARN_PRINT("Property detail_sample_distance is clamped to 0.1 world units as the resulting value from multiplying with cell_size is too low.");
	}

	cfg.bmin[0] = bmin[0];
	cfg.bmin[1] = bmin[1];
	cfg.bmin[2] = bmin[2];
	cfg.bmax[0] = bmax[0];
	cfg.bmax[1] = bmax[1];
	cfg.bmax[2] = bmax[2];

	AABB baking_aabb = p_navigation_mesh->get_filter_baking_aabb();
	if (baking_aabb.has_volume()) {
		Vector3 baking_aabb_offset = p_navigation_mesh->get_filter_baking_aabb_offset();
		cfg.bmin[0] = baking_aabb.position[0] + baking_aabb_offset.x;
		cfg.bmin[1] = baking_aabb.position[1] + baking_aabb_offset.y;
		cfg.bmin[2] = baking_aabb.position[2] + baking_aabb_offset.z;
		cfg.bmax[0] = cfg.bmin[0] + baking_aabb.size[0];
		cfg.bmax[1] = cfg.bmin[1] + baking_aabb.size[1];
		cfg.bmax[2] = cfg.bmin[2] + baking_aabb.size[2];
	}

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_CALC_GRID_SIZE; // step #2
	rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

	// ~30000000 seems to be around sweetspot where Editor baking breaks
	if ((cfg.width * cfg.height) > 30000000 && GLOBAL_GET("navigation/baking/use_crash_prevention_checks")) {
		ERR_FAIL_MSG("Baking interrupted."
					 "\nNavigationMesh baking process would likely crash the engine."
					 "\nSource geometry is suspiciously big for the current Cell Size and Cell Height in the NavMesh Resource bake settings."
					 "\nIf baking does not crash the engine or fail, the resulting NavigationMesh will create serious pathfinding performance issues."
					 "\nIt is advised to increase Cell Size and/or Cell Height in the NavMesh Resource bake settings or reduce the size / scale of the source geometry."
					 "\nIf you would like to try baking anyway, disable the 'navigation/baking/use_crash_prevention_checks' project setting.");
		return;
	}

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_CREATE_HEIGHTFIELD; // step #3
	hf = rcAllocHeightfield();

	ERR_FAIL_NULL(hf);
	ERR_FAIL_COND(!rcCreateHeightfield(&ctx, *hf, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch));

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_MARK_WALKABLE_TRIANGLES; // step #4
	{
		Vector<unsigned char> tri_areas;
		tri_areas.resize(ntris);

		ERR_FAIL_COND(tri_areas.is_empty());

		memset(tri_areas.ptrw(), 0, ntris * sizeof(unsigned char));
		rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, verts, nverts, tris, ntris, tri_areas.ptrw());

		ERR_FAIL_COND(!rcRasterizeTriangles(&ctx, verts, nverts, tris, tri_areas.ptr(), ntris, *hf, cfg.walkableClimb));
	}

	if (p_navigation_mesh->get_filter_low_hanging_obstacles()) {
		rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *hf);
	}
	if (p_navigation_mesh->get_filter_ledge_spans()) {
		rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf);
	}
	if (p_navigation_mesh->get_filter_walkable_low_height_spans()) {
		rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *hf);
	}

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_CONSTRUCT_COMPACT_HEIGHTFIELD; // step #5

	chf = rcAllocCompactHeightfield();

	ERR_FAIL_NULL(chf);
	ERR_FAIL_COND(!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf, *chf));

	rcFreeHeightField(hf);
	hf = nullptr;

	// Add obstacles to the source geometry. Those will be affected by e.g. agent_radius.
	if (!projected_obstructions.is_empty()) {
		for (const NavigationMeshSourceGeometryData3D::ProjectedObstruction &projected_obstruction : projected_obstructions) {
			if (projected_obstruction.carve) {
				continue;
			}
			if (projected_obstruction.vertices.is_empty() || projected_obstruction.vertices.size() % 3 != 0) {
				continue;
			}

			const float *projected_obstruction_verts = projected_obstruction.vertices.ptr();
			const int projected_obstruction_nverts = projected_obstruction.vertices.size() / 3;

			rcMarkConvexPolyArea(&ctx, projected_obstruction_verts, projected_obstruction_nverts, projected_obstruction.elevation, projected_obstruction.elevation + projected_obstruction.height, RC_NULL_AREA, *chf);
		}
	}

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_ERODE_WALKABLE_AREA; // step #6

	ERR_FAIL_COND(!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf));

	// Carve obstacles to the eroded geometry. Those will NOT be affected by e.g. agent_radius because that step is already done.
	if (!projected_obstructions.is_empty()) {
		for (const NavigationMeshSourceGeometryData3D::ProjectedObstruction &projected_obstruction : projected_obstructions) {
			if (!projected_obstruction.carve) {
				continue;
			}
			if (projected_obstruction.vertices.is_empty() || projected_obstruction.vertices.size() % 3 != 0) {
				continue;
			}

			const float *projected_obstruction_verts = projected_obstruction.vertices.ptr();
			const int projected_obstruction_nverts = projected_obstruction.vertices.size() / 3;

			rcMarkConvexPolyArea(&ctx, projected_obstruction_verts, projected_obstruction_nverts, projected_obstruction.elevation, projected_obstruction.elevation + projected_obstruction.height, RC_NULL_AREA, *chf);
		}
	}

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_SAMPLE_PARTITIONING; // step #7

	if (p_navigation_mesh->get_sample_partition_type() == NavigationMesh::SAMPLE_PARTITION_WATERSHED) {
		ERR_FAIL_COND(!rcBuildDistanceField(&ctx, *chf));
		ERR_FAIL_COND(!rcBuildRegions(&ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea));
	} else if (p_navigation_mesh->get_sample_partition_type() == NavigationMesh::SAMPLE_PARTITION_MONOTONE) {
		ERR_FAIL_COND(!rcBuildRegionsMonotone(&ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea));
	} else {
		ERR_FAIL_COND(!rcBuildLayerRegions(&ctx, *chf, cfg.borderSize, cfg.minRegionArea));
	}

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_CREATING_CONTOURS; // step #8

	cset = rcAllocContourSet();

	ERR_FAIL_NULL(cset);
	ERR_FAIL_COND(!rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset));

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_CREATING_POLYMESH; // step #9

	poly_mesh = rcAllocPolyMesh();
	ERR_FAIL_NULL(poly_mesh);
	ERR_FAIL_COND(!rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *poly_mesh));

	detail_mesh = rcAllocPolyMeshDetail();
	ERR_FAIL_NULL(detail_mesh);
	ERR_FAIL_COND(!rcBuildPolyMeshDetail(&ctx, *poly_mesh, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *detail_mesh));

	rcFreeCompactHeightfield(chf);
	chf = nullptr;
	rcFreeContourSet(cset);
	cset = nullptr;

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_CONVERTING_NATIVE_NAVMESH; // step #10

	Vector<Vector3> nav_vertices;
	Vector<Vector<int>> nav_polygons;

	HashMap<Vector3, int> recast_vertex_to_native_index;
	LocalVector<int> recast_index_to_native_index;
	recast_index_to_native_index.resize(detail_mesh->nverts);

	for (int i = 0; i < detail_mesh->nverts; i++) {
		const float *v = &detail_mesh->verts[i * 3];
		const Vector3 vertex = Vector3(v[0], v[1], v[2]);
		int *existing_index_ptr = recast_vertex_to_native_index.getptr(vertex);
		if (!existing_index_ptr) {
			int new_index = recast_vertex_to_native_index.size();
			recast_index_to_native_index[i] = new_index;
			recast_vertex_to_native_index[vertex] = new_index;
			nav_vertices.push_back(vertex);
		} else {
			recast_index_to_native_index[i] = *existing_index_ptr;
		}
	}

	for (int i = 0; i < detail_mesh->nmeshes; i++) {
		const unsigned int *detail_mesh_m = &detail_mesh->meshes[i * 4];
		const unsigned int detail_mesh_bverts = detail_mesh_m[0];
		const unsigned int detail_mesh_m_btris = detail_mesh_m[2];
		const unsigned int detail_mesh_ntris = detail_mesh_m[3];
		const unsigned char *detail_mesh_tris = &detail_mesh->tris[detail_mesh_m_btris * 4];
		for (unsigned int j = 0; j < detail_mesh_ntris; j++) {
			Vector<int> nav_indices;
			nav_indices.resize(3);
			// Polygon order in recast is opposite than godot's
			int index1 = ((int)(detail_mesh_bverts + detail_mesh_tris[j * 4 + 0]));
			int index2 = ((int)(detail_mesh_bverts + detail_mesh_tris[j * 4 + 2]));
			int index3 = ((int)(detail_mesh_bverts + detail_mesh_tris[j * 4 + 1]));

			nav_indices.write[0] = recast_index_to_native_index[index1];
			nav_indices.write[1] = recast_index_to_native_index[index2];
			nav_indices.write[2] = recast_index_to_native_index[index3];

			nav_polygons.push_back(nav_indices);
		}
	}

	p_navigation_mesh->set_data(nav_vertices, nav_polygons);

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_BAKE_CLEANUP; // step #11

	rcFreePolyMesh(poly_mesh);
	poly_mesh = nullptr;
	rcFreePolyMeshDetail(detail_mesh);
	detail_mesh = nullptr;

	p_generator_task->bake_state = NavMeshBakeState::BAKE_STATE_BAKE_FINISHED; // step #12
}

bool NavMeshGenerator3D::generator_emit_callback(const Callable &p_callback) {
	ERR_FAIL_COND_V(!p_callback.is_valid(), false);

	Callable::CallError ce;
	Variant result;
	p_callback.callp(nullptr, 0, result, ce);

	return ce.error == Callable::CallError::CALL_OK;
}
