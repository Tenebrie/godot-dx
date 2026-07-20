/**************************************************************************/
/*  gdscript_refactor.h                                                   */
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

#include "core/error/error_list.h"
#include "core/object/script_language.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

// Finds all references to a GDScript symbol by parsing and analyzing every
// candidate script once, then walking the annotated syntax trees. Occurrences
// are matched by a declaration key (defining class fqcn + member name, or
// declaration position for locals), so results do not depend on line numbers
// staying in sync between editor buffers and disk.
//
// The editor reaches this through the `ScriptLanguage::find_symbol_references`
// virtual (implemented on `GDScriptLanguage`), never by linking against this
// class directly: `libeditor` precedes the module archives on the link line,
// so a direct editor -> module symbol reference would not resolve.
class GDScriptRefactor {
public:
	using Occurrence = ScriptLanguage::SymbolReference;

	struct Result {
		// Occurrences proven to resolve to the origin symbol's declaration(s).
		Vector<Occurrence> references;
		// Same-word occurrences that could not be proven or refuted: identifiers
		// whose base type could not be resolved, and matches inside string
		// literals. Never auto-renamed.
		Vector<Occurrence> unverified;
		String origin_key;
		// False when the origin symbol resolves to an engine class/member or an
		// autoload name: findable, but not renamable from a script refactor.
		bool origin_renamable = false;
		// Keys treated as the same symbol (override closure for methods).
		Vector<String> matched_keys;
	};

	// p_buffer_overrides maps res:// paths to unsaved editor content; files not
	// present are read from disk. p_origin_column is a 0-based character index
	// anywhere within (or at the end of) the symbol occurrence. p_files limits
	// the search to an explicit file set; empty means scan the whole project.
	static Error find_references(const String &p_symbol, const String &p_origin_path, int p_origin_line, int p_origin_column, const HashMap<String, String> &p_buffer_overrides, Result &r_result, const Vector<String> &p_files = Vector<String>());

	// Cheap single-file resolution for pre-flight checks (e.g. whether a rename
	// dialog should open at all). Returns ERR_CANT_RESOLVE when the position
	// holds no resolvable symbol; r_renamable is false for engine symbols and
	// script overrides of engine virtuals.
	static Error resolve_symbol_at(const String &p_symbol, const String &p_path, int p_line, int p_column, const String &p_content, bool &r_renamable);
};

#endif // TOOLS_ENABLED
