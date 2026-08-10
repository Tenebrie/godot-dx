/**************************************************************************/
/*  test_gdscript_reload.h                                                */
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

#include "../gdscript.h"
#include "../gdscript_cache.h"
#include "gdscript_test_runner.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "tests/test_macros.h"

namespace GDScriptTests {

static void write_source_file(const String &p_path, const String &p_content) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(p_content);
}

TEST_SUITE("[Modules][GDScript]") {
	TEST_CASE("[GDScriptCache] Base script gains an inner class while only shallow-cached") {
		// A cached base script keeps the subclass map built from the source it
		// had when it was last prepared. If the file gains an inner class while
		// the cached script is invalid (e.g. its own reload failed before the
		// compiler ran), recompiling a derived script used to pass a null
		// subclass into GDScriptCompiler::_prepare_compilation and crash.
		const String fixture_dir = "modules/gdscript/tests/scripts/reload_stale_base";
		Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		REQUIRE(dir->make_dir_recursive(fixture_dir) == OK);
		write_source_file(fixture_dir + "/base.notest.gd", "func ok():\n\tpass\n");
		write_source_file(fixture_dir + "/derived.notest.gd", "extends \"res://reload_stale_base/base.notest.gd\"\n\nfunc foo():\n\tpass\n");

		init_language("modules/gdscript/tests/scripts");

		const String base_res_path = "res://reload_stale_base/base.notest.gd";
		Error err = OK;
		Ref<GDScript> derived = GDScriptCache::get_full_script("res://reload_stale_base/derived.notest.gd", err);
		REQUIRE(err == OK);
		REQUIRE(derived.is_valid());
		CHECK(derived->is_valid());

		// The base gains an inner class together with a (temporary) analysis
		// error, and is reloaded from disk — as happens when saving a
		// work-in-progress base script in the editor. The failed reload leaves
		// the cached script object invalid, with a subclass map that still
		// predates the new inner class.
		write_source_file(fixture_dir + "/base.notest.gd", "class Storage:\n\tvar y = 2\n\nfunc ok():\n\tundeclared_call()\n");
		Ref<GDScript> base = GDScriptCache::get_full_script(base_res_path, err, String(), true);
		REQUIRE(base.is_valid());
		CHECK(err != OK);
		CHECK_FALSE(base->is_valid());

		// The error is fixed (the inner class stays), but the base script's own
		// reload has not run yet.
		const String fixed_base_source = "class Storage:\n\tvar y = 2\n\nfunc ok():\n\tpass\n";
		write_source_file(fixture_dir + "/base.notest.gd", fixed_base_source);
		base->set_source_code(fixed_base_source);
		GDScriptCache::remove_parser(base_res_path);

		// Recompiling the derived script walks the base's fresh parse tree
		// against the stale cached subclass map. This used to crash.
		err = derived->reload(true);
		CHECK(err == OK);

		GDScriptCache::remove_script("res://reload_stale_base/derived.notest.gd");
		GDScriptCache::remove_script(base_res_path);
		dir->remove(fixture_dir + "/base.notest.gd");
		dir->remove(fixture_dir + "/derived.notest.gd");
		dir->remove(fixture_dir);
		finish_language();
	}
}
} // namespace GDScriptTests
