/**************************************************************************/
/*  editor_asset_map.h                                                     */
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
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

class EditorFileSystemDirectory;

// Editor-only builder for the `Asset` script -> scene map. It scans every
// PackedScene in the project, determines each scene's canonical root script,
// pushes the resulting map into the runtime `Asset` singleton, and persists it
// to `res://.godot/asset_map.json` so a project run from the editor (a separate
// process without editor singletons) can read it.
class EditorAssetMap : public Object {
	GDCLASS(EditorAssetMap, Object);

	static EditorAssetMap *singleton;

	void _scan_dir(EditorFileSystemDirectory *p_dir, HashMap<String, Vector<String>> &r_map) const;
	void _on_filesystem_changed();

public:
	static EditorAssetMap *get_singleton() { return singleton; }

	// Builds the full script-path -> scene-paths map from the editor filesystem.
	void build_map(HashMap<String, Vector<String>> &r_map) const;

	// Rebuilds the map, updates the runtime Asset singleton, and writes the
	// JSON cache to disk.
	void rebuild_and_persist();

	// Hooks this builder up to EditorFileSystem change signals.
	void connect_filesystem_signals();

	EditorAssetMap();
	~EditorAssetMap();
};
