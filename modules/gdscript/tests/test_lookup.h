/**************************************************************************/
/*  test_lookup.h                                                         */
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
/* included in all copies or substantial portions of the Software.       */
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
#include "gdscript_test_runner.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/script_language.h"
#include "tests/test_macros.h"

namespace GDScriptTests {

static String lookup_result_type_to_string(ScriptLanguage::LookupResultType p_type) {
	switch (p_type) {
		case ScriptLanguage::LOOKUP_RESULT_SCRIPT_LOCATION:
			return "SCRIPT_LOCATION";
		case ScriptLanguage::LOOKUP_RESULT_CLASS:
			return "CLASS";
		case ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT:
			return "CLASS_CONSTANT";
		case ScriptLanguage::LOOKUP_RESULT_CLASS_PROPERTY:
			return "CLASS_PROPERTY";
		case ScriptLanguage::LOOKUP_RESULT_CLASS_METHOD:
			return "CLASS_METHOD";
		case ScriptLanguage::LOOKUP_RESULT_CLASS_SIGNAL:
			return "CLASS_SIGNAL";
		case ScriptLanguage::LOOKUP_RESULT_CLASS_ENUM:
			return "CLASS_ENUM";
		case ScriptLanguage::LOOKUP_RESULT_CLASS_TBD_GLOBALSCOPE:
			return "CLASS_TBD_GLOBALSCOPE";
		case ScriptLanguage::LOOKUP_RESULT_CLASS_ANNOTATION:
			return "CLASS_ANNOTATION";
		case ScriptLanguage::LOOKUP_RESULT_LOCAL_CONSTANT:
			return "LOCAL_CONSTANT";
		case ScriptLanguage::LOOKUP_RESULT_LOCAL_VARIABLE:
			return "LOCAL_VARIABLE";
		case ScriptLanguage::LOOKUP_RESULT_MAX:
			break;
	}
	return "UNKNOWN";
}

static void test_lookup_directory(const String &p_dir) {
	Error err = OK;
	Ref<DirAccess> dir = DirAccess::open(p_dir, &err);

	if (err != OK) {
		FAIL("Invalid lookup test directory.");
		return;
	}

	String path = dir->get_current_dir();

	dir->list_dir_begin();
	String next = dir->get_next();

	while (!next.is_empty()) {
		if (dir->current_is_dir()) {
			if (next == "." || next == "..") {
				next = dir->get_next();
				continue;
			}
			test_lookup_directory(path.path_join(next));
		} else if (next.ends_with(".gd") && !next.ends_with(".notest.gd")) {
			Ref<FileAccess> acc = FileAccess::open(path.path_join(next), FileAccess::READ, &err);

			if (err != OK) {
				next = dir->get_next();
				continue;
			}

			String code = acc->get_as_utf8_string();
			// For ease of reading ➡ (0x27A1) acts as sentinel char instead of 0xFFFF in the files.
			code = code.replace_first(String::chr(0x27A1), String::chr(0xFFFF));
			// Require pointer sentinel char in scripts.
			int location = code.find_char(0xFFFF);
			CHECK(location != -1);

			ConfigFile conf;
			if (conf.load(path.path_join(next.get_basename() + ".cfg")) != OK) {
				FAIL("No config file found.");
			}

			const String symbol = conf.get_value("input", "symbol", "");
			CHECK_MESSAGE(!symbol.is_empty(), "No [input] symbol in '", path.path_join(next), "'.");

			// `lookup_code` loads the script at the given path for go-to-definition info, and
			// the real test file contains the cursor sentinel (invalid GDScript) — give it a
			// sanitized copy instead. `.notest.gd` keeps it out of every runner's scan.
			const String sanitized_disk_path = path.path_join(next.get_basename() + ".tmp.notest.gd");
			{
				Ref<FileAccess> sanitized = FileAccess::open(sanitized_disk_path, FileAccess::WRITE, &err);
				CHECK_MESSAGE(err == OK, "Could not write sanitized lookup script for '", path.path_join(next), "'.");
				if (err == OK) {
					sanitized->store_string(code.replace(String::chr(0xFFFF), ""));
				}
			}

			ScriptLanguage::LookupResult result;
			const String res_path = ProjectSettings::get_singleton()->localize_path(sanitized_disk_path);
			const Error lookup_err = GDScriptLanguage::get_singleton()->lookup_code(code, symbol, res_path, nullptr, result);

			DirAccess::remove_absolute(sanitized_disk_path);

			CHECK_MESSAGE(lookup_err == OK, "Lookup failed for '", path.path_join(next), "'.");
			if (lookup_err != OK) {
				next = dir->get_next();
				continue;
			}

			if (conf.has_section_key("output", "type")) {
				const String expected_type = conf.get_value("output", "type");
				CHECK_MESSAGE(expected_type == lookup_result_type_to_string(result.type), "Wrong lookup result type for '", path.path_join(next), "'.");
			}
			if (conf.has_section_key("output", "class_name")) {
				const String expected_class = conf.get_value("output", "class_name");
				CHECK_MESSAGE(expected_class == result.class_name, "Wrong lookup class_name for '", path.path_join(next), "': got '", result.class_name, "'.");
			}
			if (conf.has_section_key("output", "class_member")) {
				const String expected_member = conf.get_value("output", "class_member");
				CHECK_MESSAGE(expected_member == result.class_member, "Wrong lookup class_member for '", path.path_join(next), "': got '", result.class_member, "'.");
			}
			if (conf.has_section_key("output", "doc_type")) {
				const String expected_doc_type = conf.get_value("output", "doc_type");
				CHECK_MESSAGE(expected_doc_type == result.doc_type, "Wrong lookup doc_type for '", path.path_join(next), "': got '", result.doc_type, "'.");
			}
			if (conf.has_section_key("output", "method_return_type_override")) {
				const String expected_return = conf.get_value("output", "method_return_type_override");
				CHECK_MESSAGE(expected_return == result.method_return_type_override, "Wrong method_return_type_override for '", path.path_join(next), "': got '", result.method_return_type_override, "'.");
			}
			if (conf.has_section_key("output", "method_arg_type_overrides")) {
				const PackedStringArray expected_args = conf.get_value("output", "method_arg_type_overrides");
				CHECK_MESSAGE(expected_args == result.method_arg_type_overrides, "Wrong method_arg_type_overrides for '", path.path_join(next), "': got '", String(", ").join(result.method_arg_type_overrides), "'.");
			}
		}
		next = dir->get_next();
	}
}

TEST_SUITE("[Modules][GDScript][Lookup]") {
	TEST_CASE("[Editor] Check symbol lookup") {
		init_language("modules/gdscript/tests/scripts");
		test_lookup_directory("modules/gdscript/tests/scripts/lookup");
		finish_language();
	}
}
} // namespace GDScriptTests

#endif
