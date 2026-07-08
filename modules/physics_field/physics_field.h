#pragma once

#include "core/math/rect2.h"
#include "core/math/transform_2d.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"

// ---------- Shapes ----------

class PhysicsFieldShape : public RefCounted {
	GDCLASS(PhysicsFieldShape, RefCounted);

protected:
	static void _bind_methods() {}

public:
	PhysicsFieldShape() {}
};

class PhysicsFieldShapeRect : public PhysicsFieldShape {
	GDCLASS(PhysicsFieldShapeRect, PhysicsFieldShape);

	Vector2 size;

protected:
	static void _bind_methods();

public:
	void set_size(const Vector2 &p_size) { size = p_size; }
	Vector2 get_size() const { return size; }

	_FORCE_INLINE_ const Vector2 &size_ref() const { return size; }
};

class PhysicsFieldShapeCircle : public PhysicsFieldShape {
	GDCLASS(PhysicsFieldShapeCircle, PhysicsFieldShape);

	real_t radius = 0.0;

protected:
	static void _bind_methods();

public:
	void set_radius(real_t p_radius) { radius = p_radius; }
	real_t get_radius() const { return radius; }

	_FORCE_INLINE_ real_t radius_ref() const { return radius; }
};

// ---------- Obstacle / Segment ----------

class PhysicsFieldObstacleSegment : public RefCounted {
	GDCLASS(PhysicsFieldObstacleSegment, RefCounted);

	Ref<PhysicsFieldShape> shape;
	Transform2D transform;
	Transform2D inverse_field_transform;

protected:
	static void _bind_methods();

public:
	void set_shape(const Ref<PhysicsFieldShape> &p_shape) { shape = p_shape; }
	Ref<PhysicsFieldShape> get_shape() const { return shape; }

	void set_transform(const Transform2D &p_transform) { transform = p_transform; }
	Transform2D get_transform() const { return transform; }

	void set_inverse_field_transform(const Transform2D &p_transform) { inverse_field_transform = p_transform; }
	Transform2D get_inverse_field_transform() const { return inverse_field_transform; }

	_FORCE_INLINE_ PhysicsFieldShape *shape_ptr() const { return shape.ptr(); }
	_FORCE_INLINE_ const Transform2D &transform_ref() const { return transform; }
	_FORCE_INLINE_ const Transform2D &inverse_field_transform_ref() const { return inverse_field_transform; }
};

class PhysicsFieldObstacle : public RefCounted {
	GDCLASS(PhysicsFieldObstacle, RefCounted);

	int collision_layer = 1;
	Rect2 aabb;
	TypedArray<PhysicsFieldObstacleSegment> segments;
	Transform2D transform;

protected:
	static void _bind_methods();

public:
	void set_collision_layer(int p_layer) { collision_layer = p_layer; }
	int get_collision_layer() const { return collision_layer; }

	void set_aabb(const Rect2 &p_aabb) { aabb = p_aabb; }
	Rect2 get_aabb() const { return aabb; }

	void set_segments(const TypedArray<PhysicsFieldObstacleSegment> &p_segments) { segments = p_segments; }
	TypedArray<PhysicsFieldObstacleSegment> get_segments() const { return segments; }

	void set_transform(const Transform2D &p_transform) { transform = p_transform; }
	Transform2D get_transform() const { return transform; }

	_FORCE_INLINE_ int collision_layer_ref() const { return collision_layer; }
	_FORCE_INLINE_ const Rect2 &aabb_ref() const { return aabb; }
	_FORCE_INLINE_ const TypedArray<PhysicsFieldObstacleSegment> &segments_ref() const { return segments; }
	_FORCE_INLINE_ const Transform2D &transform_ref() const { return transform; }
};

// ---------- Raycast query / result ----------

class PhysicsFieldRaycastQuery : public RefCounted {
	GDCLASS(PhysicsFieldRaycastQuery, RefCounted);

	Vector3 origin;
	Vector3 target;
	int collision_mask = 0x7FFFFFFF;
	real_t width = 0.0;
	TypedArray<PhysicsFieldObstacle> exclude;

protected:
	static void _bind_methods();

public:
	void set_origin(const Vector3 &p_origin) { origin = p_origin; }
	Vector3 get_origin() const { return origin; }

	void set_target(const Vector3 &p_target) { target = p_target; }
	Vector3 get_target() const { return target; }

	void set_collision_mask(int p_mask) { collision_mask = p_mask; }
	int get_collision_mask() const { return collision_mask; }

	void set_width(real_t p_width) { width = p_width; }
	real_t get_width() const { return width; }

	void set_exclude(const TypedArray<PhysicsFieldObstacle> &p_exclude) { exclude = p_exclude; }
	TypedArray<PhysicsFieldObstacle> get_exclude() const { return exclude; }

	_FORCE_INLINE_ int collision_mask_ref() const { return collision_mask; }
	_FORCE_INLINE_ real_t width_ref() const { return width; }
	_FORCE_INLINE_ const TypedArray<PhysicsFieldObstacle> &exclude_ref() const { return exclude; }
};

class PhysicsFieldRaycastResultHit : public RefCounted {
	GDCLASS(PhysicsFieldRaycastResultHit, RefCounted);

	real_t fraction = 0.0;
	Vector3 position;

protected:
	static void _bind_methods();

public:
	void set_fraction(real_t p_fraction) { fraction = p_fraction; }
	real_t get_fraction() const { return fraction; }

	void set_position(const Vector3 &p_position) { position = p_position; }
	Vector3 get_position() const { return position; }
};

class PhysicsFieldRaycastResult : public RefCounted {
	GDCLASS(PhysicsFieldRaycastResult, RefCounted);

	LocalVector<real_t> fractions;
	LocalVector<Vector3> positions;

protected:
	static void _bind_methods();

public:
	_FORCE_INLINE_ int get_hit_count() const { return int(fractions.size()); }
	_FORCE_INLINE_ bool get_has_hits() const { return !fractions.is_empty(); }
	real_t get_hit_fraction(int p_index) const;
	Vector3 get_hit_position(int p_index) const;
	Ref<PhysicsFieldRaycastResultHit> get_hit(int p_index) const;
	void clear();

	_FORCE_INLINE_ void _add_hit(real_t p_fraction, const Vector3 &p_position) {
		fractions.push_back(p_fraction);
		positions.push_back(p_position);
	}
	_FORCE_INLINE_ void _reserve(uint32_t p_capacity) {
		if (fractions.get_capacity() < p_capacity) {
			fractions.reserve(p_capacity);
			positions.reserve(p_capacity);
		}
	}
};

// ---------- Field ----------

class PhysicsField : public RefCounted {
	GDCLASS(PhysicsField, RefCounted);

	Rect2 aabb;
	TypedArray<PhysicsFieldObstacle> obstacles;

protected:
	static void _bind_methods();

public:
	Rect2 get_aabb() const { return aabb; }

	void set_obstacles(const TypedArray<PhysicsFieldObstacle> &p_obstacles);
	TypedArray<PhysicsFieldObstacle> get_obstacles() const { return obstacles; }

	void update_bounding_box();
	void update_transforms();

	Ref<PhysicsFieldRaycastResult> raycast_query(PhysicsFieldRaycastQuery *p_query);
	void raycast_query_into(PhysicsFieldRaycastQuery *p_query, PhysicsFieldRaycastResult *p_result);
};
