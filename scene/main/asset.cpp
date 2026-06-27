/**************************************************************************/
/*  asset.cpp                                                             */
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

#include "asset.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "scene/resources/packed_scene.h"

Asset *Asset::singleton = nullptr;

const char *Asset::MAP_PATH = "res://.godot/asset_map.json";

String Asset::get_canonical_script_for_scene(const Ref<PackedScene> &p_scene) {
	if (p_scene.is_null()) {
		return String();
	}
	Ref<SceneState> state = p_scene->get_state();
	if (state.is_null()) {
		return String();
	}

	// Inherited scenes do not count: only the base scene that originally
	// declares the script is the canonical implementation.
	if (state->get_base_scene_state().is_valid()) {
		return String();
	}

	if (state->get_node_count() == 0) {
		return String();
	}

	// The root must not be an instance of another (sub-)scene.
	if (state->get_node_instance(0).is_valid()) {
		return String();
	}

	// Read the script declared locally on the root node (index 0) without
	// instantiating the scene.
	for (int i = 0; i < state->get_node_property_count(0); i++) {
		if (state->get_node_property_name(0, i) != SNAME("script")) {
			continue;
		}
		Variant value = state->get_node_property_value(0, i);
		Ref<Script> script = value;
		if (script.is_valid()) {
			return script->get_path();
		}
		return String();
	}

	return String();
}

void Asset::clear() {
	script_to_scenes.clear();
}

void Asset::set_scenes_for_script(const String &p_script_path, const Vector<String> &p_scenes) {
	if (p_scenes.is_empty()) {
		script_to_scenes.erase(p_script_path);
	} else {
		script_to_scenes[p_script_path] = p_scenes;
	}
}

void Asset::set_map(const HashMap<String, Vector<String>> &p_map) {
	script_to_scenes = p_map;
	map_loaded = true;
}

String Asset::encode_map_json() const {
	Dictionary dict;
	for (const KeyValue<String, Vector<String>> &E : script_to_scenes) {
		Array scenes;
		for (const String &s : E.value) {
			scenes.push_back(s);
		}
		dict[E.key] = scenes;
	}
	return JSON::stringify(dict, "\t");
}

Error Asset::load_from_json(const String &p_json_text) {
	JSON json;
	Error err = json.parse(p_json_text);
	if (err != OK) {
		ERR_PRINT(vformat("Asset: failed to parse asset map JSON at line %d: %s", json.get_error_line(), json.get_error_message()));
		return err;
	}
	Variant data = json.get_data();
	if (data.get_type() != Variant::DICTIONARY) {
		return ERR_PARSE_ERROR;
	}
	Dictionary dict = data;
	script_to_scenes.clear();
	for (const KeyValue<Variant, Variant> &E : dict) {
		String script_path = E.key;
		Vector<String> scenes;
		if (E.value.get_type() == Variant::ARRAY) {
			Array arr = E.value;
			for (int i = 0; i < arr.size(); i++) {
				scenes.push_back(arr[i]);
			}
		}
		if (!scenes.is_empty()) {
			script_to_scenes[script_path] = scenes;
		}
	}
	map_loaded = true;
	return OK;
}

Error Asset::load_map(const String &p_path) {
	if (!FileAccess::exists(p_path)) {
		// No map shipped/built yet; treat as empty rather than an error so the
		// engine still boots. Instantiate() reports a clear error per-call.
		map_loaded = true;
		return ERR_FILE_NOT_FOUND;
	}
	String text = FileAccess::get_file_as_string(p_path);
	return load_from_json(text);
}

Error Asset::save_map(const String &p_path) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, vformat("Asset: cannot open '%s' for writing.", p_path));
	f->store_string(encode_map_json());
	return OK;
}

void Asset::_ensure_runtime_map_loaded() {
	if (map_loaded) {
		return;
	}
	load_map(MAP_PATH);
}

int Asset::get_scene_count_for_script(const String &p_script_path) const {
	HashMap<String, Vector<String>>::ConstIterator it = script_to_scenes.find(p_script_path);
	if (!it) {
		return 0;
	}
	return it->value.size();
}

Vector<String> Asset::get_scenes_for_script(const String &p_script_path) const {
	HashMap<String, Vector<String>>::ConstIterator it = script_to_scenes.find(p_script_path);
	if (!it) {
		return Vector<String>();
	}
	return it->value;
}

String Asset::get_scene_for_script(const String &p_script_path) const {
	HashMap<String, Vector<String>>::ConstIterator it = script_to_scenes.find(p_script_path);
	if (!it || it->value.size() != 1) {
		return String();
	}
	return it->value[0];
}

Object *Asset::instantiate(const Ref<Script> &p_script) {
	ERR_FAIL_COND_V_MSG(p_script.is_null(), nullptr, "Asset.Instantiate: the provided script is null.");

	_ensure_runtime_map_loaded();

	const String script_path = p_script->get_path();
	ERR_FAIL_COND_V_MSG(script_path.is_empty(), nullptr, "Asset.Instantiate: the provided script has no resource path.");

	const Vector<String> scenes = get_scenes_for_script(script_path);
	ERR_FAIL_COND_V_MSG(scenes.is_empty(), nullptr,
			vformat("Asset.Instantiate: no scene uses '%s' as its canonical (top-level, non-inherited) root script.", script_path));
	ERR_FAIL_COND_V_MSG(scenes.size() > 1, nullptr,
			vformat("Asset.Instantiate: '%s' is used as the canonical root script by %d scenes (%s); it must be exactly one.",
					script_path, scenes.size(), String(", ").join(scenes)));

	Ref<PackedScene> packed = ResourceLoader::load(scenes[0]);
	ERR_FAIL_COND_V_MSG(packed.is_null(), nullptr, vformat("Asset.Instantiate: failed to load scene '%s'.", scenes[0]));

	return packed->instantiate();
}

void Asset::_bind_methods() {
	ClassDB::bind_method(D_METHOD("Instantiate", "script"), &Asset::instantiate);
	ClassDB::bind_method(D_METHOD("get_scene_for_script", "script_path"), &Asset::get_scene_for_script);
	ClassDB::bind_method(D_METHOD("get_scene_count_for_script", "script_path"), &Asset::get_scene_count_for_script);
}

Asset::Asset() {
	singleton = this;
}

Asset::~Asset() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
