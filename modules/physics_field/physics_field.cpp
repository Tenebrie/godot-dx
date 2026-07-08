#include "physics_field.h"

#include "core/math/math_funcs.h"
#include "core/templates/local_vector.h"
#include "core/templates/sort_array.h"

// ---------- Shape bindings ----------

void PhysicsFieldShapeRect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &PhysicsFieldShapeRect::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &PhysicsFieldShapeRect::get_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size"), "set_size", "get_size");
}

void PhysicsFieldShapeCircle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &PhysicsFieldShapeCircle::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &PhysicsFieldShapeCircle::get_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius"), "set_radius", "get_radius");
}

// ---------- Segment / Obstacle bindings ----------

void PhysicsFieldObstacleSegment::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &PhysicsFieldObstacleSegment::set_shape);
	ClassDB::bind_method(D_METHOD("get_shape"), &PhysicsFieldObstacleSegment::get_shape);
	ClassDB::bind_method(D_METHOD("set_transform", "transform"), &PhysicsFieldObstacleSegment::set_transform);
	ClassDB::bind_method(D_METHOD("get_transform"), &PhysicsFieldObstacleSegment::get_transform);
	ClassDB::bind_method(D_METHOD("set_inverse_field_transform", "transform"), &PhysicsFieldObstacleSegment::set_inverse_field_transform);
	ClassDB::bind_method(D_METHOD("get_inverse_field_transform"), &PhysicsFieldObstacleSegment::get_inverse_field_transform);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shape", PROPERTY_HINT_RESOURCE_TYPE, "PhysicsFieldShape"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "transform"), "set_transform", "get_transform");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "inverse_field_transform"), "set_inverse_field_transform", "get_inverse_field_transform");
}

void PhysicsFieldObstacle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &PhysicsFieldObstacle::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &PhysicsFieldObstacle::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_aabb", "aabb"), &PhysicsFieldObstacle::set_aabb);
	ClassDB::bind_method(D_METHOD("get_aabb"), &PhysicsFieldObstacle::get_aabb);
	ClassDB::bind_method(D_METHOD("set_segments", "segments"), &PhysicsFieldObstacle::set_segments);
	ClassDB::bind_method(D_METHOD("get_segments"), &PhysicsFieldObstacle::get_segments);
	ClassDB::bind_method(D_METHOD("set_transform", "transform"), &PhysicsFieldObstacle::set_transform);
	ClassDB::bind_method(D_METHOD("get_transform"), &PhysicsFieldObstacle::get_transform);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer"), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "aabb"), "set_aabb", "get_aabb");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "segments"), "set_segments", "get_segments");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "transform"), "set_transform", "get_transform");
}

// ---------- Field ----------

void PhysicsField::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_aabb"), &PhysicsField::get_aabb);
	ClassDB::bind_method(D_METHOD("set_obstacles", "obstacles"), &PhysicsField::set_obstacles);
	ClassDB::bind_method(D_METHOD("get_obstacles"), &PhysicsField::get_obstacles);
	ClassDB::bind_method(D_METHOD("update_bounding_box"), &PhysicsField::update_bounding_box);
	ClassDB::bind_method(D_METHOD("update_transforms"), &PhysicsField::update_transforms);
	ClassDB::bind_method(D_METHOD("raycast_query", "query"), &PhysicsField::raycast_query);
	ClassDB::bind_method(D_METHOD("raycast_query_into", "query", "result"), &PhysicsField::raycast_query_into);

	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "aabb", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_aabb");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "obstacles"), "set_obstacles", "get_obstacles");
}

void PhysicsField::set_obstacles(const TypedArray<PhysicsFieldObstacle> &p_obstacles) {
	obstacles = p_obstacles;
	update_bounding_box();
	update_transforms();
}

void PhysicsField::update_bounding_box() {
	const int count = obstacles.size();
	if (count == 0) {
		aabb = Rect2();
		return;
	}
	PhysicsFieldObstacle *first = Object::cast_to<PhysicsFieldObstacle>(obstacles[0]);
	if (!first) {
		aabb = Rect2();
		return;
	}
	Rect2 merged = first->aabb_ref();
	for (int i = 1; i < count; i++) {
		PhysicsFieldObstacle *obstacle = Object::cast_to<PhysicsFieldObstacle>(obstacles[i]);
		if (!obstacle) {
			continue;
		}
		merged = merged.merge(obstacle->aabb_ref());
	}
	aabb = merged;
}

void PhysicsField::update_transforms() {
	const int count = obstacles.size();
	for (int i = 0; i < count; i++) {
		PhysicsFieldObstacle *obstacle = Object::cast_to<PhysicsFieldObstacle>(obstacles[i]);
		if (!obstacle) {
			continue;
		}
		const Transform2D &obstacle_xform = obstacle->transform_ref();
		const TypedArray<PhysicsFieldObstacleSegment> &segments = obstacle->segments_ref();
		const int seg_count = segments.size();
		for (int j = 0; j < seg_count; j++) {
			PhysicsFieldObstacleSegment *segment = Object::cast_to<PhysicsFieldObstacleSegment>(segments[j]);
			if (!segment) {
				continue;
			}
			segment->set_inverse_field_transform((obstacle_xform * segment->transform_ref()).affine_inverse());
		}
	}
}

// ---------- Raycast bindings ----------

void PhysicsFieldRaycastQuery::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_origin", "origin"), &PhysicsFieldRaycastQuery::set_origin);
	ClassDB::bind_method(D_METHOD("get_origin"), &PhysicsFieldRaycastQuery::get_origin);
	ClassDB::bind_method(D_METHOD("set_target", "target"), &PhysicsFieldRaycastQuery::set_target);
	ClassDB::bind_method(D_METHOD("get_target"), &PhysicsFieldRaycastQuery::get_target);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &PhysicsFieldRaycastQuery::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &PhysicsFieldRaycastQuery::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_width", "width"), &PhysicsFieldRaycastQuery::set_width);
	ClassDB::bind_method(D_METHOD("get_width"), &PhysicsFieldRaycastQuery::get_width);
	ClassDB::bind_method(D_METHOD("set_exclude", "exclude"), &PhysicsFieldRaycastQuery::set_exclude);
	ClassDB::bind_method(D_METHOD("get_exclude"), &PhysicsFieldRaycastQuery::get_exclude);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "origin"), "set_origin", "get_origin");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "target"), "set_target", "get_target");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask"), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width"), "set_width", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exclude"), "set_exclude", "get_exclude");
}

void PhysicsFieldRaycastResultHit::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_fraction", "fraction"), &PhysicsFieldRaycastResultHit::set_fraction);
	ClassDB::bind_method(D_METHOD("get_fraction"), &PhysicsFieldRaycastResultHit::get_fraction);
	ClassDB::bind_method(D_METHOD("set_position", "position"), &PhysicsFieldRaycastResultHit::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &PhysicsFieldRaycastResultHit::get_position);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fraction"), "set_fraction", "get_fraction");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position"), "set_position", "get_position");
}

void PhysicsFieldRaycastResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_hit_count"), &PhysicsFieldRaycastResult::get_hit_count);
	ClassDB::bind_method(D_METHOD("get_has_hits"), &PhysicsFieldRaycastResult::get_has_hits);
	ClassDB::bind_method(D_METHOD("get_hit_fraction", "index"), &PhysicsFieldRaycastResult::get_hit_fraction);
	ClassDB::bind_method(D_METHOD("get_hit_position", "index"), &PhysicsFieldRaycastResult::get_hit_position);
	ClassDB::bind_method(D_METHOD("get_hit", "index"), &PhysicsFieldRaycastResult::get_hit);
	ClassDB::bind_method(D_METHOD("clear"), &PhysicsFieldRaycastResult::clear);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "hit_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_hit_count");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "has_hits", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_has_hits");
}

real_t PhysicsFieldRaycastResult::get_hit_fraction(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, int(fractions.size()), 0.0);
	return fractions[p_index];
}

Vector3 PhysicsFieldRaycastResult::get_hit_position(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, int(positions.size()), Vector3());
	return positions[p_index];
}

Ref<PhysicsFieldRaycastResultHit> PhysicsFieldRaycastResult::get_hit(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, int(fractions.size()), Ref<PhysicsFieldRaycastResultHit>());
	Ref<PhysicsFieldRaycastResultHit> hit;
	hit.instantiate();
	hit->set_fraction(fractions[p_index]);
	hit->set_position(positions[p_index]);
	return hit;
}

void PhysicsFieldRaycastResult::clear() {
	fractions.clear();
	positions.clear();
}

// ---------- Math helpers ----------

static constexpr real_t PARALLEL_EPSILON = 0.00001;

static _FORCE_INLINE_ bool raycast_hits_aabb(const Vector2 &p_origin, const Vector2 &p_target, const Rect2 &p_aabb) {
	const Vector2 box_min = p_aabb.position;
	const Vector2 box_max = p_aabb.position + p_aabb.size;

	real_t entry_fraction = 0.0;
	real_t exit_fraction = 1.0;

	for (int axis = 0; axis < 2; axis++) {
		const real_t start = p_origin[axis];
		const real_t travel = p_target[axis] - start;

		const real_t band_min = box_min[axis];
		const real_t band_max = box_max[axis];

		if (Math::abs(travel) < PARALLEL_EPSILON) {
			if (start < band_min || start > band_max) {
				return false;
			}
		} else {
			real_t to_min = (band_min - start) / travel;
			real_t to_max = (band_max - start) / travel;
			if (to_min > to_max) {
				SWAP(to_min, to_max);
			}
			entry_fraction = MAX(entry_fraction, to_min);
			exit_fraction = MIN(exit_fraction, to_max);
			if (entry_fraction > exit_fraction) {
				return false;
			}
		}
	}
	return true;
}

static _FORCE_INLINE_ real_t raycast_against_local_rect(const Vector2 &p_origin, const Vector2 &p_target, const Vector2 &p_half_size) {
	real_t entry_fraction = 0.0;
	real_t exit_fraction = 1.0;

	for (int axis = 0; axis < 2; axis++) {
		const real_t start = p_origin[axis];
		const real_t travel = p_target[axis] - start;

		const real_t band_min = -p_half_size[axis];
		const real_t band_max = p_half_size[axis];

		if (Math::abs(travel) < PARALLEL_EPSILON) {
			if (start < band_min || start > band_max) {
				return -1.0;
			}
		} else {
			real_t to_min = (band_min - start) / travel;
			real_t to_max = (band_max - start) / travel;
			if (to_min > to_max) {
				SWAP(to_min, to_max);
			}
			entry_fraction = MAX(entry_fraction, to_min);
			exit_fraction = MIN(exit_fraction, to_max);
			if (entry_fraction > exit_fraction) {
				return -1.0;
			}
		}
	}
	return entry_fraction;
}

static _FORCE_INLINE_ real_t raycast_against_local_circle(const Vector2 &p_origin, const Vector2 &p_target, real_t p_radius) {
	const Vector2 d = p_target - p_origin;
	const Vector2 f = p_origin;

	const real_t a = d.dot(d);
	const real_t b = 2.0 * f.dot(d);
	const real_t c = f.dot(f) - p_radius * p_radius;

	if (a < PARALLEL_EPSILON) {
		return (c <= 0.0) ? real_t(0.0) : real_t(-1.0);
	}

	const real_t discriminant = b * b - 4.0 * a * c;
	if (discriminant < 0.0) {
		return -1.0;
	}

	const real_t sqrt_disc = Math::sqrt(discriminant);
	const real_t inv_denom = 1.0 / (2.0 * a);
	const real_t f1 = (-b - sqrt_disc) * inv_denom;
	const real_t f2 = (-b + sqrt_disc) * inv_denom;

	if (f1 >= 0.0 && f1 <= 1.0) {
		return f1;
	}
	if (f2 >= 0.0 && f2 <= 1.0) {
		return f2;
	}
	return -1.0;
}

// Swept "segment of width w perpendicular to sweep, zero length along sweep" vs local rect.
// Inflation is anisotropic: obstacle grows by half_width only across the sweep direction,
// so the sweep endpoint stays exactly at target instead of extending by half_width.
static _FORCE_INLINE_ real_t raycast_swept_rect_local(
		const Vector2 &p_origin, const Vector2 &p_target,
		const Vector2 &p_half_size, real_t p_half_width) {
	const Vector2 delta = p_target - p_origin;
	const real_t L = delta.length();
	if (L < PARALLEL_EPSILON) {
		return raycast_against_local_rect(p_origin, p_target,
				p_half_size + Vector2(p_half_width, p_half_width));
	}

	const Vector2 dir = delta / L;
	const Vector2 perp(-dir.y, dir.x);

	const Vector2 corners[4] = {
		Vector2(-p_half_size.x, -p_half_size.y),
		Vector2(p_half_size.x, -p_half_size.y),
		Vector2(p_half_size.x, p_half_size.y),
		Vector2(-p_half_size.x, p_half_size.y),
	};

	Vector2 sc[4];
	for (int i = 0; i < 4; i++) {
		const Vector2 rel = corners[i] - p_origin;
		sc[i].x = rel.dot(dir);
		sc[i].y = rel.dot(perp);
	}

	const real_t half_w = p_half_width;
	real_t min_x = Math::INF;
	real_t max_x = -Math::INF;

	for (int i = 0; i < 4; i++) {
		const Vector2 &a = sc[i];
		const Vector2 &b = sc[(i + 1) & 3];

		if (Math::abs(a.y) <= half_w) {
			if (a.x < min_x) {
				min_x = a.x;
			}
			if (a.x > max_x) {
				max_x = a.x;
			}
		}

		const real_t dy = b.y - a.y;
		if (Math::abs(dy) > PARALLEL_EPSILON) {
			for (int s = 0; s < 2; s++) {
				const real_t boundary = (s == 0) ? -half_w : half_w;
				const real_t t = (boundary - a.y) / dy;
				if (t >= 0.0 && t <= 1.0) {
					const real_t x_cross = a.x + t * (b.x - a.x);
					if (x_cross < min_x) {
						min_x = x_cross;
					}
					if (x_cross > max_x) {
						max_x = x_cross;
					}
				}
			}
		}
	}

	if (min_x > max_x || max_x < 0.0 || min_x > L) {
		return -1.0;
	}
	return MAX((real_t)0.0, min_x) / L;
}

// Swept "segment of width w perpendicular to sweep" vs local circle centered at origin.
// Rounded on the sides (perpendicular), flat at the sweep endpoints.
static _FORCE_INLINE_ real_t raycast_swept_circle_local(
		const Vector2 &p_origin, const Vector2 &p_target,
		real_t p_radius, real_t p_half_width) {
	const Vector2 delta = p_target - p_origin;
	const real_t L = delta.length();
	if (L < PARALLEL_EPSILON) {
		return raycast_against_local_circle(p_origin, p_target, p_radius + p_half_width);
	}

	const Vector2 dir = delta / L;
	const Vector2 perp(-dir.y, dir.x);

	const Vector2 rel = -p_origin;
	const real_t cx = rel.dot(dir);
	const real_t cy = rel.dot(perp);

	const real_t abs_cy = Math::abs(cy);
	const real_t half_w = p_half_width;

	if (abs_cy > p_radius + half_w) {
		return -1.0;
	}

	real_t leftmost, rightmost;
	if (abs_cy <= half_w) {
		leftmost = cx - p_radius;
		rightmost = cx + p_radius;
	} else {
		const real_t dy_edge = abs_cy - half_w;
		const real_t dx = Math::sqrt(p_radius * p_radius - dy_edge * dy_edge);
		leftmost = cx - dx;
		rightmost = cx + dx;
	}

	if (rightmost < 0.0 || leftmost > L) {
		return -1.0;
	}
	return MAX((real_t)0.0, leftmost) / L;
}

// ---------- Raycast on Field ----------

namespace {
struct HitData {
	real_t fraction;
	Vector3 position;
};

struct HitDataLess {
	_FORCE_INLINE_ bool operator()(const HitData &a, const HitData &b) const {
		return a.fraction < b.fraction;
	}
};
} // namespace

Ref<PhysicsFieldRaycastResult> PhysicsField::raycast_query(PhysicsFieldRaycastQuery *p_query) {
	Ref<PhysicsFieldRaycastResult> result;
	result.instantiate();
	raycast_query_into(p_query, result.ptr());
	return result;
}

// Linear scan of excludes; typical list is 0-few entries, so this is faster than
// any hash structure and touches only one cache line for small lists.
static _FORCE_INLINE_ bool is_excluded(const Object *const *p_ptrs, uint32_t p_count, const Object *p_obstacle) {
	for (uint32_t k = 0; k < p_count; k++) {
		if (p_ptrs[k] == p_obstacle) {
			return true;
		}
	}
	return false;
}

// Templated inner loop. WITH_WIDTH=false generates the exact ray path; WITH_WIDTH=true
// models the moving shape as a segment of length 0 along sweep and width `width`
// perpendicular — i.e. the swept region is a rectangle from origin to target with flat
// ends. Per-segment tests inflate perpendicular to the sweep direction only; the AABB
// broadphase still uses isotropic grow (over-approximation, filtered per-segment).
template <bool WITH_WIDTH>
static _FORCE_INLINE_ void run_raycast_pass(
		const TypedArray<PhysicsFieldObstacle> &p_obstacles,
		const Object *const *p_exclude_ptrs, uint32_t p_exclude_count,
		const Vector2 &p_ray_origin, const Vector2 &p_ray_target,
		const Vector3 &p_origin3, const Vector3 &p_delta,
		real_t p_half_width, int p_mask,
		LocalVector<HitData> &r_hits) {
	const int obstacle_count = p_obstacles.size();
	for (int i = 0; i < obstacle_count; i++) {
		PhysicsFieldObstacle *obstacle = Object::cast_to<PhysicsFieldObstacle>(p_obstacles[i]);
		if (!obstacle) {
			continue;
		}
		if ((obstacle->collision_layer_ref() & p_mask) == 0) {
			continue;
		}
		if (p_exclude_count && is_excluded(p_exclude_ptrs, p_exclude_count, obstacle)) {
			continue;
		}
		if constexpr (WITH_WIDTH) {
			if (!raycast_hits_aabb(p_ray_origin, p_ray_target, obstacle->aabb_ref().grow(p_half_width))) {
				continue;
			}
		} else {
			if (!raycast_hits_aabb(p_ray_origin, p_ray_target, obstacle->aabb_ref())) {
				continue;
			}
		}

		const TypedArray<PhysicsFieldObstacleSegment> &segments = obstacle->segments_ref();
		const int seg_count = segments.size();
		for (int j = 0; j < seg_count; j++) {
			PhysicsFieldObstacleSegment *segment = Object::cast_to<PhysicsFieldObstacleSegment>(segments[j]);
			if (!segment) {
				continue;
			}
			PhysicsFieldShape *shape = segment->shape_ptr();
			if (!shape) {
				continue;
			}
			const Transform2D &inv = segment->inverse_field_transform_ref();
			const Vector2 local_origin = inv.xform(p_ray_origin);
			const Vector2 local_target = inv.xform(p_ray_target);

			real_t fraction = -1.0;
			if (PhysicsFieldShapeRect *rect = Object::cast_to<PhysicsFieldShapeRect>(shape)) {
				if constexpr (WITH_WIDTH) {
					fraction = raycast_swept_rect_local(local_origin, local_target, rect->size_ref() * 0.5, p_half_width);
				} else {
					fraction = raycast_against_local_rect(local_origin, local_target, rect->size_ref() * 0.5);
				}
			} else if (PhysicsFieldShapeCircle *circle = Object::cast_to<PhysicsFieldShapeCircle>(shape)) {
				if constexpr (WITH_WIDTH) {
					fraction = raycast_swept_circle_local(local_origin, local_target, circle->radius_ref(), p_half_width);
				} else {
					fraction = raycast_against_local_circle(local_origin, local_target, circle->radius_ref());
				}
			}
			if (fraction < 0.0) {
				continue;
			}

			HitData hd;
			hd.fraction = fraction;
			hd.position = p_origin3 + p_delta * fraction;
			r_hits.push_back(hd);
		}
	}
}

void PhysicsField::raycast_query_into(PhysicsFieldRaycastQuery *p_query, PhysicsFieldRaycastResult *p_result) {
	ERR_FAIL_NULL(p_query);
	ERR_FAIL_NULL(p_result);
	p_result->clear();

	const Vector3 origin3 = p_query->get_origin();
	const Vector3 target3 = p_query->get_target();
	const Vector2 ray_origin(origin3.x, origin3.z);
	const Vector2 ray_target(target3.x, target3.z);
	const Vector3 delta = target3 - origin3;
	const int mask = p_query->collision_mask_ref();
	const real_t half_width = p_query->width_ref() * 0.5;

	// Per-thread scratch buffers; capacity persists across calls, so after warmup
	// these allocate zero times per query.
	static thread_local LocalVector<HitData> hits;
	static thread_local LocalVector<const Object *> exclude_ptrs;
	hits.clear();
	exclude_ptrs.clear();

	{
		const TypedArray<PhysicsFieldObstacle> &exclude = p_query->exclude_ref();
		const int exclude_count = exclude.size();
		for (int i = 0; i < exclude_count; i++) {
			if (const Object *o = Object::cast_to<Object>(exclude[i])) {
				exclude_ptrs.push_back(o);
			}
		}
	}
	const Object *const *exclude_data = exclude_ptrs.ptr();
	const uint32_t exclude_count = exclude_ptrs.size();

	if (half_width > 0.0) {
		run_raycast_pass<true>(obstacles, exclude_data, exclude_count,
				ray_origin, ray_target, origin3, delta, half_width, mask, hits);
	} else {
		run_raycast_pass<false>(obstacles, exclude_data, exclude_count,
				ray_origin, ray_target, origin3, delta, half_width, mask, hits);
	}

	if (hits.size() > 1) {
		SortArray<HitData, HitDataLess> sorter;
		sorter.sort(hits.ptr(), hits.size());
	}

	p_result->_reserve(hits.size());
	for (uint32_t i = 0; i < hits.size(); i++) {
		p_result->_add_hit(hits[i].fraction, hits[i].position);
	}
}
