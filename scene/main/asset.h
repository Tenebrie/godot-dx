/**************************************************************************/
/*  asset.h                                                               */
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

#pragma once

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/object/script_language.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

class PackedScene;

// `Asset` is a scripting singleton that maps a GDScript class to the single
// scene that uses it as its canonical (top-level, non-inherited, non-instanced)
// root script, allowing `Asset.Instantiate(MyClass)` to return a fully built
// instance of `MyClass`'s scene.
//
// The map is built by the editor (see editor/file_system/editor_asset_map.*)
// and persisted to `res://.godot/asset_map.json`, which is loaded at runtime.
class Asset : public Object {
	GDCLASS(Asset, Object);

	static Asset *singleton;

	// Path of the script (res://...) -> list of scene paths that use it as the
	// canonical root script. A well-formed entry has exactly one scene; zero or
	// more than one is an ambiguity that makes Instantiate() fail.
	HashMap<String, Vector<String>> script_to_scenes;

	bool map_loaded = false;

	void _ensure_runtime_map_loaded();

protected:
	static void _bind_methods();

public:
	static const char *MAP_PATH; // res://.godot/asset_map.json

	static Asset *get_singleton() { return singleton; }

	// Determines the canonical script path for a scene, or an empty string when
	// the scene is inherited, has an instanced root, or its root declares no
	// local script. Does not instantiate the scene.
	static String get_canonical_script_for_scene(const Ref<PackedScene> &p_scene);

	// Map mutation (used by the editor-side builder).
	void clear();
	void set_scenes_for_script(const String &p_script_path, const Vector<String> &p_scenes);
	void set_map(const HashMap<String, Vector<String>> &p_map);

	// Serialization. `save_map` writes the current in-memory map as JSON;
	// `encode_map_json` / `load_from_json` allow the export pipeline to reuse it.
	Error load_map(const String &p_path);
	Error save_map(const String &p_path) const;
	String encode_map_json() const;
	Error load_from_json(const String &p_json_text);

	// Query API (also used by the GDScript analyzer for instant editor errors).
	bool is_map_loaded() const { return map_loaded; }
	int get_scene_count_for_script(const String &p_script_path) const;
	Vector<String> get_scenes_for_script(const String &p_script_path) const;
	String get_scene_for_script(const String &p_script_path) const; // unambiguous only

	// Scripting entry points.
	Object *instantiate(const Ref<Script> &p_script);

	Asset();
	~Asset();
};
