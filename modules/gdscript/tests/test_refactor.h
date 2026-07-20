/**************************************************************************/
/*  test_refactor.h                                                       */
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

#ifdef TOOLS_ENABLED

#include "../gdscript.h"
#include "../gdscript_refactor.h"
#include "gdscript_test_runner.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "tests/test_macros.h"

namespace GDScriptTests {

static PackedStringArray format_occurrences(const Vector<GDScriptRefactor::Occurrence> &p_occurrences, const String &p_case_prefix) {
	PackedStringArray formatted;
	for (const GDScriptRefactor::Occurrence &occ : p_occurrences) {
		formatted.push_back(vformat("%s:%d:%d", occ.path.trim_prefix(p_case_prefix), occ.line, occ.start_column));
	}
	formatted.sort();
	return formatted;
}

static void test_refactor_case(const String &p_case_dir) {
	Error err = OK;
	Ref<DirAccess> dir = DirAccess::open(p_case_dir, &err);
	if (err != OK) {
		FAIL("Invalid refactor test case directory.");
		return;
	}

	const String localized_case_dir = ProjectSettings::get_singleton()->localize_path(p_case_dir);

	Vector<String> files;
	dir->list_dir_begin();
	String next = dir->get_next();
	String cfg_path;
	while (!next.is_empty()) {
		if (!dir->current_is_dir()) {
			if (next.ends_with(".gd")) {
				files.push_back(localized_case_dir.path_join(next));
			} else if (next.ends_with(".cfg")) {
				cfg_path = p_case_dir.path_join(next);
			}
		}
		next = dir->get_next();
	}

	if (cfg_path.is_empty()) {
		FAIL("No config file found in refactor test case '", p_case_dir, "'.");
		return;
	}

	ConfigFile conf;
	REQUIRE(conf.load(cfg_path) == OK);

	const String origin_file = conf.get_value("input", "file", "");
	const String symbol = conf.get_value("input", "symbol", "");
	const int line = conf.get_value("input", "line", 0);
	const int column = conf.get_value("input", "column", 0);
	REQUIRE(!origin_file.is_empty());
	REQUIRE(!symbol.is_empty());

	const String case_prefix = localized_case_dir.path_join("");

	GDScriptRefactor::Result result;
	const Error refactor_err = GDScriptRefactor::find_references(symbol, localized_case_dir.path_join(origin_file), line, column, HashMap<String, String>(), result, files);

	if (conf.get_value("output", "error", false)) {
		CHECK_MESSAGE(refactor_err != OK, "Expected failure, but lookup succeeded in '", p_case_dir, "'.");
		return;
	}

	CHECK_MESSAGE(refactor_err == OK, "find_references failed for '", p_case_dir, "'.");
	if (refactor_err != OK) {
		return;
	}

	PackedStringArray expected_refs = conf.get_value("output", "references", PackedStringArray());
	expected_refs.sort();
	const PackedStringArray actual_refs = format_occurrences(result.references, case_prefix);
	CHECK_MESSAGE(expected_refs == actual_refs, "Wrong references for '", p_case_dir, "'. Expected [", String(", ").join(expected_refs), "] got [", String(", ").join(actual_refs), "]. Origin key: ", result.origin_key);

	if (conf.has_section_key("output", "unverified")) {
		PackedStringArray expected_unverified = conf.get_value("output", "unverified", PackedStringArray());
		expected_unverified.sort();
		const PackedStringArray actual_unverified = format_occurrences(result.unverified, case_prefix);
		CHECK_MESSAGE(expected_unverified == actual_unverified, "Wrong unverified list for '", p_case_dir, "'. Expected [", String(", ").join(expected_unverified), "] got [", String(", ").join(actual_unverified), "].");
	}

	if (conf.has_section_key("output", "renamable")) {
		const bool expected_renamable = conf.get_value("output", "renamable", true);
		CHECK_MESSAGE(expected_renamable == result.origin_renamable, "Wrong renamable flag for '", p_case_dir, "'. Origin key: ", result.origin_key);
	}
}

static void refactor_setup_global_classes(const String &p_dir, Vector<StringName> &r_registered) {
	Error err = OK;
	Ref<DirAccess> dir = DirAccess::open(p_dir, &err);
	if (err != OK) {
		return;
	}
	String path = dir->get_current_dir();
	dir->list_dir_begin();
	String next = dir->get_next();
	while (!next.is_empty()) {
		if (dir->current_is_dir() && next != "." && next != "..") {
			refactor_setup_global_classes(path.path_join(next), r_registered);
		} else if (next.ends_with(".gd")) {
			const String source_file = ProjectSettings::get_singleton()->localize_path(path.path_join(next));
			String base_type;
			bool is_abstract = false;
			bool is_tool = false;
			const String class_name = GDScriptLanguage::get_singleton()->get_global_class_name(source_file, &base_type, nullptr, &is_abstract, &is_tool);
			if (!class_name.is_empty() && !ScriptServer::is_global_class(class_name)) {
				ScriptServer::add_global_class(class_name, base_type, GDScriptLanguage::get_singleton()->get_name(), source_file, is_abstract, is_tool);
				r_registered.push_back(class_name);
			}
		}
		next = dir->get_next();
	}
}

static void test_refactor_directory(const String &p_dir) {
	Error err = OK;
	Ref<DirAccess> dir = DirAccess::open(p_dir, &err);
	if (err != OK) {
		FAIL("Invalid refactor test directory.");
		return;
	}

	String path = dir->get_current_dir();
	dir->list_dir_begin();
	String next = dir->get_next();
	while (!next.is_empty()) {
		if (dir->current_is_dir() && next != "." && next != "..") {
			test_refactor_case(path.path_join(next));
		}
		next = dir->get_next();
	}
}

TEST_SUITE("[Modules][GDScript][Refactor]") {
	TEST_CASE("[Editor] Find all references") {
		init_language("modules/gdscript/tests/scripts");
		Vector<StringName> registered;
		refactor_setup_global_classes("modules/gdscript/tests/scripts/refactor", registered);
		test_refactor_directory("modules/gdscript/tests/scripts/refactor");

		// Pre-flight resolution used by the rename dialog.
		{
			const String member_path = ProjectSettings::get_singleton()->localize_path("modules/gdscript/tests/scripts/refactor/member_basic/main.notest.gd");
			const String native_path = ProjectSettings::get_singleton()->localize_path("modules/gdscript/tests/scripts/refactor/native_override/main.notest.gd");
			bool renamable = false;
			CHECK(GDScriptRefactor::resolve_symbol_at("health", member_path, 3, 4, String(), renamable) == OK);
			CHECK(renamable);
			CHECK(GDScriptRefactor::resolve_symbol_at("_ready", native_path, 3, 5, String(), renamable) == OK);
			CHECK_FALSE(renamable);
			CHECK(GDScriptRefactor::resolve_symbol_at("var", member_path, 3, 0, String(), renamable) != OK);
			CHECK(GDScriptRefactor::resolve_symbol_at("Node", native_path, 1, 8, String(), renamable) == OK);
			CHECK_FALSE(renamable);
		}

		for (const StringName &name : registered) {
			ScriptServer::remove_global_class(name);
		}
		finish_language();
	}
}
} // namespace GDScriptTests

#endif // TOOLS_ENABLED
