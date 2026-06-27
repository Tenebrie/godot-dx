/**************************************************************************/
/*  editor_asset_map.cpp                                                   */
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

#include "editor_asset_map.h"

#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/main/asset.h"
#include "scene/resources/packed_scene.h"

EditorAssetMap *EditorAssetMap::singleton = nullptr;

void EditorAssetMap::_scan_dir(EditorFileSystemDirectory *p_dir, HashMap<String, Vector<String>> &r_map) const {
	if (!p_dir) {
		return;
	}

	for (int i = 0; i < p_dir->get_file_count(); i++) {
		if (p_dir->get_file_type(i) != SNAME("PackedScene")) {
			continue;
		}
		const String scene_path = p_dir->get_file_path(i);
		Ref<PackedScene> packed = ResourceLoader::load(scene_path, "PackedScene");
		if (packed.is_null()) {
			continue;
		}
		const String script_path = Asset::get_canonical_script_for_scene(packed);
		if (script_path.is_empty()) {
			continue;
		}
		r_map[script_path].push_back(scene_path);
	}

	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_scan_dir(p_dir->get_subdir(i), r_map);
	}
}

void EditorAssetMap::build_map(HashMap<String, Vector<String>> &r_map) const {
	r_map.clear();
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	if (!efs) {
		return;
	}
	_scan_dir(efs->get_filesystem(), r_map);
}

void EditorAssetMap::rebuild_and_persist() {
	HashMap<String, Vector<String>> map;
	build_map(map);

	Asset *asset = Asset::get_singleton();
	if (asset) {
		asset->set_map(map);
		asset->save_map(Asset::MAP_PATH);
	}
}

void EditorAssetMap::_on_filesystem_changed() {
	rebuild_and_persist();
}

void EditorAssetMap::connect_filesystem_signals() {
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	ERR_FAIL_NULL(efs);
	efs->connect("filesystem_changed", callable_mp(this, &EditorAssetMap::_on_filesystem_changed));

	// Load the previous session's cached map immediately so the GDScript analyzer
	// has data to validate against from the first frame, before the first
	// filesystem scan completes and triggers a full rebuild.
	Asset *asset = Asset::get_singleton();
	if (asset) {
		asset->load_map(Asset::MAP_PATH);
	}
}

EditorAssetMap::EditorAssetMap() {
	singleton = this;
}

EditorAssetMap::~EditorAssetMap() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
