/**************************************************************************/
/*  test_docgen.h                                                         */
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

#include "core/doc_data.h"
#include "tests/test_macros.h"

namespace GDScriptTests {

static String find_member_doc_type(const Vector<DocData::ClassDoc> &p_docs, const String &p_member) {
	for (const DocData::ClassDoc &class_doc : p_docs) {
		for (const DocData::PropertyDoc &prop : class_doc.properties) {
			if (prop.name == p_member) {
				return prop.type;
			}
		}
	}
	return "<not found>";
}

TEST_SUITE("[Modules][GDScript][DocGen]") {
	TEST_CASE("[Editor] Weak member types are documented with their known type") {
		init_language("modules/gdscript/tests/scripts");

		Ref<GDScript> script;
		script.instantiate();
		script->set_path("res://__weak_member_docs_test.gd");
		script->set_source_code(R"GD(
var member_untyped = 1
var member_from_map = ([1, 2] as Array[int]).map(func (n) -> bool: return n > 1)
var member_unknown = unknown_call()

func unknown_call():
	return 1

func test():
	pass
)GD");
		script->reload();

		const Vector<DocData::ClassDoc> docs = script->get_documentation();
		CHECK(find_member_doc_type(docs, "member_untyped") == "int");
		CHECK(find_member_doc_type(docs, "member_from_map") == "bool[]");
		// An untyped function's return is genuinely unknown — stays Variant.
		CHECK(find_member_doc_type(docs, "member_unknown") == "Variant");

		finish_language();
	}
}
} // namespace GDScriptTests

#endif
