/**************************************************************************/
/*  gdscript_refactor.cpp                                                 */
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

#include "gdscript_refactor.h"

#ifdef TOOLS_ENABLED

#include "gdscript.h"
#include "gdscript_analyzer.h"
#include "gdscript_parser.h"
#include "gdscript_utility_functions.h"

#include "core/config/project_settings.h"
#include "core/core_constants.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/templates/hash_set.h"

namespace {

using ClassNode = GDScriptParser::ClassNode;
using DataType = GDScriptParser::DataType;
using IdentifierNode = GDScriptParser::IdentifierNode;
using MemberKind = GDScriptParser::ClassNode::Member::Type;

struct Collected {
	GDScriptRefactor::Occurrence occ;
	String key; // Empty for unverified occurrences.
	MemberKind member_kind = ClassNode::Member::UNDEFINED;
	bool native_rooted = false; // Function overriding an engine virtual.
};

struct MemberMatch {
	String key;
	MemberKind kind = ClassNode::Member::UNDEFINED;
	DataType datatype;
	bool native_rooted = false;
};

String script_fqcn(const Ref<Script> &p_script) {
	const StringName global_name = p_script->get_global_name();
	if (global_name != StringName()) {
		return String(global_name);
	}
	return GDScript::canonicalize_path(p_script->get_path());
}

// `ClassDB::has_method` misses GDVIRTUAL-only methods (`_ready`, `_process`, ...).
bool native_has_method(const StringName &p_class, const StringName &p_member, bool p_no_inheritance) {
	if (ClassDB::has_method(p_class, p_member, p_no_inheritance)) {
		return true;
	}
	List<MethodInfo> virtuals;
	ClassDB::get_virtual_methods(p_class, &virtuals, p_no_inheritance);
	for (const MethodInfo &mi : virtuals) {
		if (mi.name == p_member) {
			return true;
		}
	}
	return false;
}

bool native_declares(const StringName &p_class, const StringName &p_member) {
	return native_has_method(p_class, p_member, true) ||
			ClassDB::has_property(p_class, p_member, true) ||
			ClassDB::has_signal(p_class, p_member, true) ||
			ClassDB::has_enum(p_class, p_member, true) ||
			ClassDB::has_integer_constant(p_class, p_member, true);
}

// Base-most native class declaring the member, so keys are stable no matter
// which subclass the lookup started from.
StringName native_defining_class(const StringName &p_class, const StringName &p_member) {
	StringName found;
	StringName c = p_class;
	while (c != StringName()) {
		if (native_declares(c, p_member)) {
			found = c;
		}
		c = ClassDB::get_parent_class_nocheck(c);
	}
	return found;
}

// Whether some engine class up the inheritance chain declares a method with
// this name — script methods overriding engine virtuals can't be renamed.
bool method_native_rooted(const DataType &p_base_start, const StringName &p_name) {
	DataType cur = p_base_start;
	for (int depth = 0; depth < 64; depth++) {
		switch (cur.kind) {
			case DataType::CLASS: {
				if (cur.class_type == nullptr) {
					return false;
				}
				cur = cur.class_type->base_type;
			} break;
			case DataType::SCRIPT: {
				Ref<Script> scr = cur.script_type;
				StringName native_base;
				while (scr.is_valid()) {
					native_base = scr->get_instance_base_type();
					scr = scr->get_base_script();
				}
				if (native_base == StringName()) {
					return false;
				}
				cur = DataType();
				cur.kind = DataType::NATIVE;
				cur.native_type = native_base;
			} break;
			case DataType::NATIVE: {
				const StringName defining = native_defining_class(cur.native_type, p_name);
				return defining != StringName() && native_has_method(defining, p_name, true);
			} break;
			default:
				return false;
		}
	}
	return false;
}

bool resolve_member_walk(const DataType &p_start, const StringName &p_name, MemberMatch &r_match) {
	DataType cur = p_start;
	for (int depth = 0; depth < 64; depth++) {
		switch (cur.kind) {
			case DataType::CLASS: {
				ClassNode *cn = cur.class_type;
				if (cn == nullptr) {
					return false;
				}
				if (cn->has_member(p_name)) {
					const ClassNode::Member member = cn->get_member(p_name);
					r_match.kind = member.type;
					r_match.datatype = member.get_datatype();
					if (member.type == ClassNode::Member::CLASS) {
						r_match.key = "class:" + member.m_class->fqcn;
					} else {
						r_match.key = "member:" + cn->fqcn + "." + String(p_name);
					}
					if (member.type == ClassNode::Member::FUNCTION) {
						r_match.native_rooted = method_native_rooted(cn->base_type, p_name);
					}
					return true;
				}
				cur = cn->base_type;
			} break;
			case DataType::SCRIPT: {
				Ref<Script> scr = cur.script_type;
				StringName native_base;
				while (scr.is_valid()) {
					native_base = scr->get_instance_base_type();
					if (scr->get_member_line(p_name) >= 0) {
						r_match.kind = scr->has_method(p_name) ? ClassNode::Member::FUNCTION : ClassNode::Member::UNDEFINED;
						r_match.key = "member:" + script_fqcn(scr) + "." + String(p_name);
						if (r_match.kind == ClassNode::Member::FUNCTION) {
							DataType base_dt;
							base_dt.kind = DataType::NATIVE;
							base_dt.native_type = native_base;
							Ref<Script> base_scr = scr->get_base_script();
							if (base_scr.is_valid()) {
								base_dt.kind = DataType::SCRIPT;
								base_dt.script_type = base_scr;
							}
							r_match.native_rooted = method_native_rooted(base_dt, p_name);
						}
						return true;
					}
					scr = scr->get_base_script();
				}
				if (native_base == StringName()) {
					return false;
				}
				cur = DataType();
				cur.kind = DataType::NATIVE;
				cur.native_type = native_base;
			} break;
			case DataType::NATIVE: {
				const StringName defining = native_defining_class(cur.native_type, p_name);
				if (defining == StringName()) {
					return false;
				}
				r_match.kind = native_has_method(defining, p_name, true) ? ClassNode::Member::FUNCTION : ClassNode::Member::UNDEFINED;
				r_match.key = "nativemember:" + String(defining) + "." + String(p_name);
				return true;
			} break;
			case DataType::ENUM: {
				if (!cur.is_meta_type) {
					return false;
				}
				if (!cur.enum_values.has(p_name)) {
					return false;
				}
				r_match.kind = ClassNode::Member::ENUM_VALUE;
				if (cur.class_type != nullptr || !cur.script_path.is_empty()) {
					r_match.key = "enumval:" + String(cur.native_type) + "." + String(p_name);
				} else {
					r_match.key = "nativeenumval:" + String(cur.native_type) + "." + String(p_name);
				}
				return true;
			} break;
			case DataType::BUILTIN: {
				r_match.kind = ClassNode::Member::UNDEFINED;
				r_match.key = "builtinmember:" + String(Variant::get_type_name(cur.builtin_type)) + "." + String(p_name);
				return true;
			} break;
			default:
				return false;
		}
	}
	return false;
}

struct FileCollector {
	String symbol;
	StringName symbol_name;
	String path;
	Vector<String> source_lines;
	Vector<ClassNode *> class_stack;

	Vector<Collected> out;
	HashMap<uint64_t, int> position_index;

	// Ancestry info for the override closure, keyed by class fqcn.
	HashMap<String, Vector<String>> *class_script_ancestors = nullptr;
	HashMap<String, StringName> *class_native_base = nullptr;
	HashMap<String, bool> *class_declares_symbol_fn = nullptr;

	static uint64_t pos_key(int p_line, int p_col) {
		return (uint64_t(uint32_t(p_line)) << 32) | uint64_t(uint32_t(p_col));
	}

	bool word_boundaries_clean(const String &p_line_text, int p_start, int p_end) const {
		if (p_start > 0 && is_unicode_identifier_continue(p_line_text[p_start - 1])) {
			return false;
		}
		if (p_end < p_line_text.length() && is_unicode_identifier_continue(p_line_text[p_end])) {
			return false;
		}
		return true;
	}

	// Parser columns are 1-based, but verify against the source text and never
	// emit a range whose text isn't exactly the symbol.
	bool locate_in_line(int p_line, int p_reported_column, int &r_start) const {
		if (p_line < 1 || p_line > source_lines.size()) {
			return false;
		}
		const String &line_text = source_lines[p_line - 1];
		const int len = symbol.length();
		for (int candidate : { p_reported_column - 1, p_reported_column }) {
			if (candidate >= 0 && candidate + len <= line_text.length() && line_text.substr(candidate, len) == symbol && word_boundaries_clean(line_text, candidate, candidate + len)) {
				r_start = candidate;
				return true;
			}
		}
		return false;
	}

	void emit_at(int p_line, int p_start_col, const String &p_key, MemberKind p_kind, bool p_is_declaration, bool p_in_string, bool p_native_rooted = false) {
		const uint64_t pk = pos_key(p_line, p_start_col);
		if (position_index.has(pk)) {
			Collected &existing = out.write[position_index[pk]];
			if (existing.key.is_empty() && !p_key.is_empty()) {
				existing.key = p_key;
				existing.member_kind = p_kind;
				existing.native_rooted = p_native_rooted;
				existing.occ.is_declaration = p_is_declaration;
				existing.occ.in_string = p_in_string;
			}
			return;
		}
		Collected c;
		c.occ.path = path;
		c.occ.line = p_line;
		c.occ.start_column = p_start_col;
		c.occ.end_column = p_start_col + symbol.length();
		c.occ.is_declaration = p_is_declaration;
		c.occ.in_string = p_in_string;
		c.occ.line_text = source_lines[p_line - 1];
		c.key = p_key;
		c.member_kind = p_kind;
		c.native_rooted = p_native_rooted;
		position_index[pk] = out.size();
		out.push_back(c);
	}

	void emit_node(const GDScriptParser::Node *p_node, const String &p_key, MemberKind p_kind, bool p_is_declaration, bool p_native_rooted = false) {
		if (p_node == nullptr) {
			return;
		}
		int start_col = 0;
		if (locate_in_line(p_node->start_line, p_node->start_column, start_col)) {
			emit_at(p_node->start_line, start_col, p_key, p_kind, p_is_declaration, false, p_native_rooted);
		}
	}

	String local_key(const GDScriptParser::Node *p_decl_node) const {
		return vformat("local:%s:%d:%d", path, p_decl_node->start_line, p_decl_node->start_column);
	}

	ClassNode *current_class() const {
		return class_stack.is_empty() ? nullptr : class_stack[class_stack.size() - 1];
	}

	static DataType class_meta(ClassNode *p_class) {
		DataType dt;
		dt.kind = DataType::CLASS;
		dt.class_type = p_class;
		return dt;
	}

	bool resolve_member_in_scope(const StringName &p_name, MemberMatch &r_match) const {
		for (int i = class_stack.size() - 1; i >= 0; i--) {
			if (resolve_member_walk(class_meta(class_stack[i]), p_name, r_match)) {
				return true;
			}
		}
		return false;
	}

	bool resolve_global(const StringName &p_name, MemberMatch &r_match) const {
		if (ScriptServer::is_global_class(p_name)) {
			r_match.key = "class:" + String(p_name);
			r_match.kind = ClassNode::Member::CLASS;
			return true;
		}
		if (ClassDB::class_exists(p_name)) {
			r_match.key = "nativeclass:" + String(p_name);
			return true;
		}
		if (ProjectSettings::get_singleton()->has_autoload(p_name)) {
			r_match.key = "autoload:" + String(p_name);
			return true;
		}
		if (GDScriptUtilityFunctions::function_exists(p_name)) {
			r_match.key = "nativemember:@GDScript." + String(p_name);
			return true;
		}
		if (Variant::has_utility_function(p_name)) {
			r_match.key = "nativemember:@GlobalScope." + String(p_name);
			return true;
		}
		if (CoreConstants::is_global_constant(p_name) || CoreConstants::is_global_enum(p_name)) {
			r_match.key = "nativemember:@GlobalScope." + String(p_name);
			return true;
		}
		if (Variant::get_type_by_name(p_name) < Variant::VARIANT_MAX) {
			r_match.key = "nativeclass:" + String(p_name);
			return true;
		}
		return false;
	}

	void resolve_local_from_source(const IdentifierNode *p_id, MemberMatch &r_match) const {
		const GDScriptParser::Node *decl = nullptr;
		switch (p_id->source) {
			case IdentifierNode::FUNCTION_PARAMETER:
				decl = p_id->parameter_source != nullptr ? (GDScriptParser::Node *)p_id->parameter_source->identifier : nullptr;
				break;
			case IdentifierNode::LOCAL_VARIABLE:
				decl = p_id->variable_source != nullptr ? (GDScriptParser::Node *)p_id->variable_source->identifier : nullptr;
				break;
			case IdentifierNode::LOCAL_CONSTANT:
				decl = p_id->constant_source != nullptr ? (GDScriptParser::Node *)p_id->constant_source->identifier : nullptr;
				break;
			case IdentifierNode::LOCAL_ITERATOR:
			case IdentifierNode::LOCAL_BIND:
				decl = p_id->bind_source;
				break;
			default:
				break;
		}
		if (decl != nullptr) {
			r_match.key = local_key(decl);
		}
	}

	bool resolve_local_from_suite(const IdentifierNode *p_id, MemberMatch &r_match) const {
		const GDScriptParser::SuiteNode *suite = p_id->suite;
		while (suite != nullptr) {
			if (suite->has_local(symbol_name)) {
				const GDScriptParser::SuiteNode::Local &local = suite->get_local(symbol_name);
				const GDScriptParser::Node *decl = nullptr;
				switch (local.type) {
					case GDScriptParser::SuiteNode::Local::CONSTANT:
						decl = local.constant->identifier;
						break;
					case GDScriptParser::SuiteNode::Local::VARIABLE:
						decl = local.variable->identifier;
						break;
					case GDScriptParser::SuiteNode::Local::PARAMETER:
						decl = local.parameter->identifier;
						break;
					case GDScriptParser::SuiteNode::Local::FOR_VARIABLE:
					case GDScriptParser::SuiteNode::Local::PATTERN_BIND:
						decl = local.bind;
						break;
					default:
						break;
				}
				if (decl != nullptr) {
					r_match.key = local_key(decl);
					return true;
				}
			}
			suite = suite->parent_block;
		}
		return false;
	}

	// Bare (non-attribute) identifier use.
	void visit_identifier_use(const IdentifierNode *p_id) {
		if (p_id == nullptr || p_id->name != symbol_name) {
			return;
		}
		MemberMatch match;
		switch (p_id->source) {
			case IdentifierNode::FUNCTION_PARAMETER:
			case IdentifierNode::LOCAL_VARIABLE:
			case IdentifierNode::LOCAL_CONSTANT:
			case IdentifierNode::LOCAL_ITERATOR:
			case IdentifierNode::LOCAL_BIND:
				resolve_local_from_source(p_id, match);
				break;
			case IdentifierNode::NATIVE_CLASS:
				match.key = "nativeclass:" + String(p_id->name);
				break;
			default: {
				if (resolve_local_from_suite(p_id, match)) {
					break;
				}
				if (resolve_member_in_scope(p_id->name, match)) {
					break;
				}
				resolve_global(p_id->name, match);
			} break;
		}
		emit_node(p_id, match.key, match.kind, false, match.native_rooted);
	}

	void visit_attribute(const GDScriptParser::SubscriptNode *p_subscript) {
		const IdentifierNode *attr = p_subscript->attribute;
		if (attr == nullptr || attr->name != symbol_name) {
			return;
		}
		MemberMatch match;
		if (p_subscript->base != nullptr) {
			resolve_member_walk(p_subscript->base->get_datatype(), attr->name, match);
		}
		emit_node(attr, match.key, match.kind, false, match.native_rooted);
	}

	// A member-function reference of the current class that appears without the
	// analyzer having annotated it (setget pointers, super calls).
	void visit_function_reference(const IdentifierNode *p_id, const DataType &p_base) {
		if (p_id == nullptr || p_id->name != symbol_name) {
			return;
		}
		MemberMatch match;
		resolve_member_walk(p_base, p_id->name, match);
		emit_node(p_id, match.key, match.kind, false, match.native_rooted);
	}

	void scan_string_literal(const GDScriptParser::Node *p_node) {
		const int first_line = MAX(1, p_node->start_line);
		const int last_line = MIN(source_lines.size(), MAX(p_node->end_line, p_node->start_line));
		for (int line = first_line; line <= last_line; line++) {
			const String &line_text = source_lines[line - 1];
			int from = line == p_node->start_line ? MAX(0, p_node->start_column - 1) : 0;
			const int to = line == p_node->end_line ? MIN(line_text.length(), p_node->end_column) : line_text.length();
			while (true) {
				const int pos = line_text.find(symbol, from);
				if (pos < 0 || pos >= to) {
					break;
				}
				from = pos + symbol.length();
				if (word_boundaries_clean(line_text, pos, pos + symbol.length())) {
					emit_at(line, pos, String(), ClassNode::Member::UNDEFINED, false, true);
				}
			}
		}
	}

	void walk_type(const GDScriptParser::TypeNode *p_type) {
		if (p_type == nullptr) {
			return;
		}
		for (const GDScriptParser::TypeNode *container : p_type->container_types) {
			walk_type(container);
		}
		walk_type(p_type->func_return_type);

		bool relevant = false;
		for (const IdentifierNode *id : p_type->type_chain) {
			if (id != nullptr && id->name == symbol_name) {
				relevant = true;
			}
		}
		if (!relevant) {
			return;
		}

		// Resolve the chain progressively from the enclosing scope.
		MemberMatch chain_match;
		bool resolved = false;
		for (int i = 0; i < p_type->type_chain.size(); i++) {
			const IdentifierNode *id = p_type->type_chain[i];
			if (id == nullptr) {
				return;
			}
			MemberMatch next;
			bool ok = false;
			if (i == 0) {
				ok = resolve_member_in_scope(id->name, next) || resolve_global(id->name, next);
			} else if (resolved) {
				ok = resolve_member_walk(chain_match.datatype, id->name, next);
			}
			if (ok && next.kind == ClassNode::Member::CLASS && next.datatype.kind == DataType::UNRESOLVED) {
				// Global classes resolved by name only: continue the chain through
				// the loaded script when possible.
				if (ScriptServer::is_global_class(id->name)) {
					const Ref<Script> scr = ResourceLoader::load(ScriptServer::get_global_class_path(id->name));
					if (scr.is_valid()) {
						next.datatype.kind = DataType::SCRIPT;
						next.datatype.script_type = scr;
					}
				}
			}
			if (id->name == symbol_name) {
				emit_node(id, ok ? next.key : String(), ok ? next.kind : ClassNode::Member::UNDEFINED, false);
			}
			chain_match = next;
			resolved = ok;
			if (!resolved && i + 1 < p_type->type_chain.size()) {
				// Later chain elements can't be resolved; emit them unverified.
				for (int j = i + 1; j < p_type->type_chain.size(); j++) {
					if (p_type->type_chain[j] != nullptr && p_type->type_chain[j]->name == symbol_name) {
						emit_node(p_type->type_chain[j], String(), ClassNode::Member::UNDEFINED, false);
					}
				}
				return;
			}
		}
	}

	void walk_annotations(const GDScriptParser::Node *p_node) {
		for (const GDScriptParser::AnnotationNode *annotation : p_node->annotations) {
			for (GDScriptParser::ExpressionNode *argument : annotation->arguments) {
				walk_expression(argument);
			}
		}
	}

	void walk_expression(GDScriptParser::ExpressionNode *p_expr) {
		if (p_expr == nullptr) {
			return;
		}
		switch (p_expr->type) {
			case GDScriptParser::Node::IDENTIFIER: {
				visit_identifier_use(static_cast<IdentifierNode *>(p_expr));
			} break;
			case GDScriptParser::Node::SUBSCRIPT: {
				GDScriptParser::SubscriptNode *subscript = static_cast<GDScriptParser::SubscriptNode *>(p_expr);
				walk_expression(subscript->base);
				if (subscript->is_attribute) {
					visit_attribute(subscript);
				} else {
					walk_expression(subscript->index);
				}
			} break;
			case GDScriptParser::Node::CALL: {
				GDScriptParser::CallNode *call = static_cast<GDScriptParser::CallNode *>(p_expr);
				if (call->is_super) {
					if (call->callee != nullptr && call->callee->type == GDScriptParser::Node::IDENTIFIER && current_class() != nullptr) {
						visit_function_reference(static_cast<IdentifierNode *>(call->callee), current_class()->base_type);
					}
				} else {
					walk_expression(call->callee);
				}
				for (GDScriptParser::ExpressionNode *argument : call->arguments) {
					walk_expression(argument);
				}
			} break;
			case GDScriptParser::Node::LITERAL: {
				GDScriptParser::LiteralNode *literal = static_cast<GDScriptParser::LiteralNode *>(p_expr);
				const Variant::Type value_type = literal->value.get_type();
				if (value_type == Variant::STRING || value_type == Variant::STRING_NAME || value_type == Variant::NODE_PATH) {
					if (String(literal->value).contains(symbol)) {
						scan_string_literal(literal);
					}
				}
			} break;
			case GDScriptParser::Node::ARRAY: {
				for (GDScriptParser::ExpressionNode *element : static_cast<GDScriptParser::ArrayNode *>(p_expr)->elements) {
					walk_expression(element);
				}
			} break;
			case GDScriptParser::Node::DICTIONARY: {
				GDScriptParser::DictionaryNode *dictionary = static_cast<GDScriptParser::DictionaryNode *>(p_expr);
				for (const GDScriptParser::DictionaryNode::Pair &pair : dictionary->elements) {
					if (dictionary->style == GDScriptParser::DictionaryNode::LUA_TABLE && pair.key != nullptr && pair.key->type == GDScriptParser::Node::IDENTIFIER) {
						// Lua-style keys are string keys, not symbol references.
						const IdentifierNode *key_id = static_cast<IdentifierNode *>(pair.key);
						if (key_id->name == symbol_name) {
							emit_node(key_id, String(), ClassNode::Member::UNDEFINED, false);
						}
					} else {
						walk_expression(pair.key);
					}
					walk_expression(pair.value);
				}
			} break;
			case GDScriptParser::Node::ASSIGNMENT: {
				GDScriptParser::AssignmentNode *assignment = static_cast<GDScriptParser::AssignmentNode *>(p_expr);
				walk_expression(assignment->assignee);
				walk_expression(assignment->assigned_value);
			} break;
			case GDScriptParser::Node::BINARY_OPERATOR: {
				GDScriptParser::BinaryOpNode *op = static_cast<GDScriptParser::BinaryOpNode *>(p_expr);
				walk_expression(op->left_operand);
				walk_expression(op->right_operand);
			} break;
			case GDScriptParser::Node::UNARY_OPERATOR: {
				walk_expression(static_cast<GDScriptParser::UnaryOpNode *>(p_expr)->operand);
			} break;
			case GDScriptParser::Node::TERNARY_OPERATOR: {
				GDScriptParser::TernaryOpNode *op = static_cast<GDScriptParser::TernaryOpNode *>(p_expr);
				walk_expression(op->condition);
				walk_expression(op->true_expr);
				walk_expression(op->false_expr);
			} break;
			case GDScriptParser::Node::AWAIT: {
				walk_expression(static_cast<GDScriptParser::AwaitNode *>(p_expr)->to_await);
			} break;
			case GDScriptParser::Node::CAST: {
				GDScriptParser::CastNode *cast = static_cast<GDScriptParser::CastNode *>(p_expr);
				walk_expression(cast->operand);
				walk_type(cast->cast_type);
			} break;
			case GDScriptParser::Node::TYPE_TEST: {
				GDScriptParser::TypeTestNode *test = static_cast<GDScriptParser::TypeTestNode *>(p_expr);
				walk_expression(test->operand);
				walk_type(test->test_type);
			} break;
			case GDScriptParser::Node::LAMBDA: {
				GDScriptParser::LambdaNode *lambda = static_cast<GDScriptParser::LambdaNode *>(p_expr);
				walk_function(lambda->function, false);
			} break;
			case GDScriptParser::Node::PRELOAD: {
				walk_expression(static_cast<GDScriptParser::PreloadNode *>(p_expr)->path);
			} break;
			default:
				break;
		}
	}

	void walk_pattern(GDScriptParser::PatternNode *p_pattern) {
		if (p_pattern == nullptr) {
			return;
		}
		switch (p_pattern->pattern_type) {
			case GDScriptParser::PatternNode::PT_LITERAL: {
				walk_expression(p_pattern->literal);
			} break;
			case GDScriptParser::PatternNode::PT_EXPRESSION: {
				walk_expression(p_pattern->expression);
			} break;
			case GDScriptParser::PatternNode::PT_BIND: {
				if (p_pattern->bind != nullptr && p_pattern->bind->name == symbol_name) {
					emit_node(p_pattern->bind, local_key(p_pattern->bind), ClassNode::Member::UNDEFINED, true);
				}
			} break;
			default:
				break;
		}
		for (GDScriptParser::PatternNode *sub : p_pattern->array) {
			walk_pattern(sub);
		}
		for (const GDScriptParser::PatternNode::Pair &pair : p_pattern->dictionary) {
			walk_expression(pair.key);
			walk_pattern(pair.value_pattern);
		}
	}

	void walk_suite(GDScriptParser::SuiteNode *p_suite) {
		if (p_suite == nullptr) {
			return;
		}
		for (GDScriptParser::Node *statement : p_suite->statements) {
			walk_annotations(statement);
			switch (statement->type) {
				case GDScriptParser::Node::VARIABLE: {
					GDScriptParser::VariableNode *variable = static_cast<GDScriptParser::VariableNode *>(statement);
					if (variable->identifier != nullptr && variable->identifier->name == symbol_name) {
						emit_node(variable->identifier, local_key(variable->identifier), ClassNode::Member::UNDEFINED, true);
					}
					walk_type(variable->datatype_specifier);
					walk_expression(variable->initializer);
				} break;
				case GDScriptParser::Node::CONSTANT: {
					GDScriptParser::ConstantNode *constant = static_cast<GDScriptParser::ConstantNode *>(statement);
					if (constant->identifier != nullptr && constant->identifier->name == symbol_name) {
						emit_node(constant->identifier, local_key(constant->identifier), ClassNode::Member::UNDEFINED, true);
					}
					walk_type(constant->datatype_specifier);
					walk_expression(constant->initializer);
				} break;
				case GDScriptParser::Node::IF: {
					GDScriptParser::IfNode *if_node = static_cast<GDScriptParser::IfNode *>(statement);
					walk_expression(if_node->condition);
					walk_suite(if_node->true_block);
					walk_suite(if_node->false_block);
				} break;
				case GDScriptParser::Node::FOR: {
					GDScriptParser::ForNode *for_node = static_cast<GDScriptParser::ForNode *>(statement);
					if (for_node->variable != nullptr && for_node->variable->name == symbol_name) {
						emit_node(for_node->variable, local_key(for_node->variable), ClassNode::Member::UNDEFINED, true);
					}
					walk_type(for_node->datatype_specifier);
					walk_expression(for_node->list);
					walk_suite(for_node->loop);
				} break;
				case GDScriptParser::Node::WHILE: {
					GDScriptParser::WhileNode *while_node = static_cast<GDScriptParser::WhileNode *>(statement);
					walk_expression(while_node->condition);
					walk_suite(while_node->loop);
				} break;
				case GDScriptParser::Node::MATCH: {
					GDScriptParser::MatchNode *match = static_cast<GDScriptParser::MatchNode *>(statement);
					walk_expression(match->test);
					for (GDScriptParser::MatchBranchNode *branch : match->branches) {
						for (GDScriptParser::PatternNode *pattern : branch->patterns) {
							walk_pattern(pattern);
						}
						walk_suite(branch->guard_body);
						walk_suite(branch->block);
					}
				} break;
				case GDScriptParser::Node::RETURN: {
					walk_expression(static_cast<GDScriptParser::ReturnNode *>(statement)->return_value);
				} break;
				case GDScriptParser::Node::ASSERT: {
					GDScriptParser::AssertNode *assert_node = static_cast<GDScriptParser::AssertNode *>(statement);
					walk_expression(assert_node->condition);
					walk_expression(assert_node->message);
				} break;
				default: {
					if (statement->is_expression()) {
						walk_expression(static_cast<GDScriptParser::ExpressionNode *>(statement));
					}
				} break;
			}
		}
	}

	void walk_function(GDScriptParser::FunctionNode *p_function, bool p_emit_identifier) {
		if (p_function == nullptr) {
			return;
		}
		if (p_emit_identifier && p_function->identifier != nullptr && p_function->identifier->name == symbol_name && current_class() != nullptr) {
			const bool overrides_native = method_native_rooted(current_class()->base_type, symbol_name);
			emit_node(p_function->identifier, "member:" + current_class()->fqcn + "." + symbol, ClassNode::Member::FUNCTION, true, overrides_native);
		}
		for (GDScriptParser::ParameterNode *parameter : p_function->parameters) {
			if (parameter->identifier != nullptr && parameter->identifier->name == symbol_name) {
				emit_node(parameter->identifier, local_key(parameter->identifier), ClassNode::Member::UNDEFINED, true);
			}
			walk_type(parameter->datatype_specifier);
			walk_expression(parameter->initializer);
		}
		if (p_function->rest_parameter != nullptr && p_function->rest_parameter->identifier != nullptr && p_function->rest_parameter->identifier->name == symbol_name) {
			emit_node(p_function->rest_parameter->identifier, local_key(p_function->rest_parameter->identifier), ClassNode::Member::UNDEFINED, true);
		}
		walk_type(p_function->return_type);
		walk_suite(p_function->body);
	}

	void walk_class(ClassNode *p_class) {
		class_stack.push_back(p_class);

		if (p_class->identifier != nullptr && p_class->identifier->name == symbol_name) {
			emit_node(p_class->identifier, "class:" + p_class->fqcn, ClassNode::Member::CLASS, true);
		}
		for (const IdentifierNode *extends_id : p_class->extends) {
			// `extends A.B` chains resolve like type chains from the outer scope.
			if (extends_id != nullptr && extends_id->name == symbol_name) {
				MemberMatch match;
				bool ok = false;
				class_stack.resize(class_stack.size() - 1);
				ok = resolve_member_in_scope(extends_id->name, match) || resolve_global(extends_id->name, match);
				class_stack.push_back(p_class);
				emit_node(extends_id, ok ? match.key : String(), ok ? match.kind : ClassNode::Member::UNDEFINED, false);
			}
		}

		if (class_declares_symbol_fn != nullptr) {
			(*class_declares_symbol_fn)[p_class->fqcn] = p_class->has_member(symbol_name) && p_class->get_member(symbol_name).type == ClassNode::Member::FUNCTION;

			Vector<String> ancestors;
			StringName native_base;
			DataType cur = p_class->base_type;
			for (int depth = 0; depth < 64; depth++) {
				if (cur.kind == DataType::CLASS && cur.class_type != nullptr) {
					ancestors.push_back(cur.class_type->fqcn);
					cur = cur.class_type->base_type;
				} else if (cur.kind == DataType::SCRIPT && cur.script_type.is_valid()) {
					Ref<Script> scr = cur.script_type;
					while (scr.is_valid()) {
						ancestors.push_back(script_fqcn(scr));
						native_base = scr->get_instance_base_type();
						scr = scr->get_base_script();
					}
					break;
				} else if (cur.kind == DataType::NATIVE) {
					native_base = cur.native_type;
					break;
				} else {
					break;
				}
			}
			(*class_script_ancestors)[p_class->fqcn] = ancestors;
			(*class_native_base)[p_class->fqcn] = native_base;
		}

		for (int i = 0; i < p_class->members.size(); i++) {
			const ClassNode::Member &member = p_class->members[i];
			switch (member.type) {
				case ClassNode::Member::VARIABLE: {
					GDScriptParser::VariableNode *variable = member.variable;
					walk_annotations(variable);
					if (variable->identifier != nullptr && variable->identifier->name == symbol_name) {
						emit_node(variable->identifier, "member:" + p_class->fqcn + "." + symbol, ClassNode::Member::VARIABLE, true);
					}
					walk_type(variable->datatype_specifier);
					walk_expression(variable->initializer);
					if (variable->property == GDScriptParser::VariableNode::PROP_INLINE) {
						walk_function(variable->setter, false);
						walk_function(variable->getter, false);
					} else if (variable->property == GDScriptParser::VariableNode::PROP_SETGET) {
						visit_function_reference(variable->setter_pointer, class_meta(p_class));
						visit_function_reference(variable->getter_pointer, class_meta(p_class));
					}
				} break;
				case ClassNode::Member::CONSTANT: {
					GDScriptParser::ConstantNode *constant = member.constant;
					walk_annotations(constant);
					if (constant->identifier != nullptr && constant->identifier->name == symbol_name) {
						emit_node(constant->identifier, "member:" + p_class->fqcn + "." + symbol, ClassNode::Member::CONSTANT, true);
					}
					walk_type(constant->datatype_specifier);
					walk_expression(constant->initializer);
				} break;
				case ClassNode::Member::FUNCTION: {
					walk_annotations(member.function);
					walk_function(member.function, true);
				} break;
				case ClassNode::Member::SIGNAL: {
					GDScriptParser::SignalNode *signal = member.signal;
					walk_annotations(signal);
					if (signal->identifier != nullptr && signal->identifier->name == symbol_name) {
						emit_node(signal->identifier, "member:" + p_class->fqcn + "." + symbol, ClassNode::Member::SIGNAL, true);
					}
					for (GDScriptParser::ParameterNode *parameter : signal->parameters) {
						if (parameter->identifier != nullptr && parameter->identifier->name == symbol_name) {
							emit_node(parameter->identifier, local_key(parameter->identifier), ClassNode::Member::UNDEFINED, true);
						}
						walk_type(parameter->datatype_specifier);
					}
				} break;
				case ClassNode::Member::ENUM: {
					GDScriptParser::EnumNode *enum_node = member.m_enum;
					walk_annotations(enum_node);
					const bool named = enum_node->identifier != nullptr;
					if (named && enum_node->identifier->name == symbol_name) {
						emit_node(enum_node->identifier, "member:" + p_class->fqcn + "." + symbol, ClassNode::Member::ENUM, true);
					}
					for (const GDScriptParser::EnumNode::Value &value : enum_node->values) {
						if (value.identifier != nullptr && value.identifier->name == symbol_name) {
							String key;
							if (named) {
								// Matches `make_enum_type()` (fqcn + ENUM_SEPARATOR + enum name).
								key = "enumval:" + p_class->fqcn + "." + String(enum_node->identifier->name) + "." + symbol;
							} else {
								key = "member:" + p_class->fqcn + "." + symbol;
							}
							emit_node(value.identifier, key, ClassNode::Member::ENUM_VALUE, true);
						}
						walk_expression(value.custom_value);
					}
				} break;
				case ClassNode::Member::CLASS: {
					walk_annotations(member.m_class);
					walk_class(member.m_class);
				} break;
				case ClassNode::Member::ENUM_VALUE: {
					// Loose (unnamed-enum) values are also registered as ENUM members;
					// handled there.
				} break;
				default:
					break;
			}
		}

		class_stack.resize(class_stack.size() - 1);
	}
};

void collect_gd_files(const String &p_dir, Vector<String> &r_files) {
	Ref<DirAccess> dir = DirAccess::open(p_dir);
	if (dir.is_null()) {
		return;
	}
	if (FileAccess::exists(p_dir.path_join(".gdignore"))) {
		return;
	}
	const String project_data_dir_name = ProjectSettings::get_singleton()->get_project_data_dir_name();
	dir->list_dir_begin();
	String file = dir->get_next();
	while (!file.is_empty()) {
		if (!file.begins_with(".") && file != project_data_dir_name) {
			if (dir->current_is_dir()) {
				collect_gd_files(p_dir.path_join(file), r_files);
			} else if (file.get_extension() == "gd") {
				r_files.push_back(p_dir.path_join(file));
			}
		}
		file = dir->get_next();
	}
}

bool contains_whole_word(const String &p_text, const String &p_word) {
	int from = 0;
	while (true) {
		const int pos = p_text.find(p_word, from);
		if (pos < 0) {
			return false;
		}
		from = pos + p_word.length();
		const bool left_ok = pos == 0 || !is_unicode_identifier_continue(p_text[pos - 1]);
		const int end = pos + p_word.length();
		const bool right_ok = end >= p_text.length() || !is_unicode_identifier_continue(p_text[end]);
		if (left_ok && right_ok) {
			return true;
		}
	}
}

} // namespace

Error GDScriptRefactor::find_references(const String &p_symbol, const String &p_origin_path, int p_origin_line, int p_origin_column, const HashMap<String, String> &p_buffer_overrides, Result &r_result, const Vector<String> &p_files) {
	ERR_FAIL_COND_V(p_symbol.is_empty(), ERR_INVALID_PARAMETER);

	const String origin_path = GDScript::canonicalize_path(p_origin_path);

	Vector<String> files = p_files;
	if (files.is_empty()) {
		collect_gd_files("res://", files);
	}
	if (!files.has(origin_path)) {
		files.push_back(origin_path);
	}

	HashMap<String, Vector<Collected>> per_file;
	HashMap<String, Vector<String>> class_script_ancestors;
	HashMap<String, StringName> class_native_base;
	HashMap<String, bool> class_declares_symbol_fn;

	for (const String &file : files) {
		String content;
		if (p_buffer_overrides.has(file)) {
			content = p_buffer_overrides[file];
		} else {
			content = FileAccess::get_file_as_string(file);
		}
		if (!contains_whole_word(content, p_symbol)) {
			continue;
		}

		GDScriptParser parser;
		parser.parse(content, file, false);
		GDScriptAnalyzer analyzer(&parser);
		analyzer.analyze();

		if (parser.get_tree() == nullptr) {
			continue;
		}

		FileCollector collector;
		collector.symbol = p_symbol;
		collector.symbol_name = StringName(p_symbol);
		collector.path = file;
		collector.source_lines = content.split("\n");
		collector.class_script_ancestors = &class_script_ancestors;
		collector.class_native_base = &class_native_base;
		collector.class_declares_symbol_fn = &class_declares_symbol_fn;
		collector.walk_class(parser.get_tree());

		per_file[file] = collector.out;
	}

	if (!per_file.has(origin_path)) {
		return ERR_CANT_RESOLVE;
	}

	// Locate the origin occurrence under the caret.
	const Collected *origin = nullptr;
	for (const Collected &c : per_file[origin_path]) {
		if (c.occ.line == p_origin_line && p_origin_column >= c.occ.start_column && p_origin_column <= c.occ.end_column) {
			origin = &c;
			break;
		}
	}
	if (origin == nullptr || origin->occ.in_string) {
		return ERR_CANT_RESOLVE;
	}
	if (origin->key.is_empty()) {
		return ERR_CANT_RESOLVE;
	}

	r_result.origin_key = origin->key;

	// Compute the set of keys treated as the same symbol. For functions this is
	// the override closure: the base-most declaration plus every override.
	HashSet<String> matched_keys;
	matched_keys.insert(origin->key);
	bool native_rooted = false;

	if (origin->member_kind == ClassNode::Member::FUNCTION && origin->key.begins_with("member:")) {
		const String origin_body = origin->key.trim_prefix("member:");
		const int name_pos = origin_body.rfind_char('.');
		if (name_pos > 0) {
			const String origin_fqcn = origin_body.substr(0, name_pos);
			const StringName fn_name = StringName(p_symbol);

			String top_fqcn = origin_fqcn;
			if (class_script_ancestors.has(origin_fqcn)) {
				for (const String &ancestor : class_script_ancestors[origin_fqcn]) {
					if (class_declares_symbol_fn.has(ancestor) && class_declares_symbol_fn[ancestor]) {
						top_fqcn = ancestor;
					}
				}
			}
			StringName native_base;
			if (class_native_base.has(top_fqcn)) {
				native_base = class_native_base[top_fqcn];
			}
			StringName native_declarer;
			if (native_base != StringName()) {
				native_declarer = native_defining_class(native_base, fn_name);
			}
			if (native_declarer != StringName() && native_has_method(native_declarer, fn_name, true)) {
				native_rooted = true;
				matched_keys.insert("nativemember:" + String(native_declarer) + "." + p_symbol);
				// Every script method overriding the native one is the same symbol.
				for (const KeyValue<String, bool> &E : class_declares_symbol_fn) {
					if (!E.value) {
						continue;
					}
					StringName base = class_native_base.has(E.key) ? class_native_base[E.key] : StringName();
					if (base != StringName() && native_defining_class(base, fn_name) == native_declarer) {
						matched_keys.insert("member:" + E.key + "." + p_symbol);
					}
				}
			} else {
				matched_keys.insert("member:" + top_fqcn + "." + p_symbol);
				for (const KeyValue<String, bool> &E : class_declares_symbol_fn) {
					if (!E.value || E.key == top_fqcn) {
						continue;
					}
					if (class_script_ancestors.has(E.key) && class_script_ancestors[E.key].has(top_fqcn)) {
						matched_keys.insert("member:" + E.key + "." + p_symbol);
					}
				}
			}
		}
	}

	r_result.origin_renamable = !native_rooted && !origin->native_rooted &&
			(origin->key.begins_with("member:") || origin->key.begins_with("local:") || origin->key.begins_with("enumval:") || origin->key.begins_with("class:"));

	for (const String &key : matched_keys) {
		r_result.matched_keys.push_back(key);
	}

	// Locals never leave their file; skip the rest.
	const bool is_local = origin->key.begins_with("local:");

	for (const KeyValue<String, Vector<Collected>> &E : per_file) {
		if (is_local && E.key != origin_path) {
			continue;
		}
		for (const Collected &c : E.value) {
			if (!c.key.is_empty() && matched_keys.has(c.key)) {
				r_result.references.push_back(c.occ);
			} else if (c.key.is_empty()) {
				r_result.unverified.push_back(c.occ);
			}
		}
	}

	return OK;
}

Error GDScriptRefactor::resolve_symbol_at(const String &p_symbol, const String &p_path, int p_line, int p_column, const String &p_content, bool &r_renamable) {
	r_renamable = false;
	ERR_FAIL_COND_V(p_symbol.is_empty(), ERR_INVALID_PARAMETER);

	const String path = GDScript::canonicalize_path(p_path);
	String content = p_content;
	if (content.is_empty()) {
		content = FileAccess::get_file_as_string(path);
	}
	if (!contains_whole_word(content, p_symbol)) {
		return ERR_CANT_RESOLVE;
	}

	GDScriptParser parser;
	parser.parse(content, path, false);
	GDScriptAnalyzer analyzer(&parser);
	analyzer.analyze();
	if (parser.get_tree() == nullptr) {
		return ERR_CANT_RESOLVE;
	}

	FileCollector collector;
	collector.symbol = p_symbol;
	collector.symbol_name = StringName(p_symbol);
	collector.path = path;
	collector.source_lines = content.split("\n");
	collector.walk_class(parser.get_tree());

	for (const Collected &c : collector.out) {
		if (c.occ.line == p_line && p_column >= c.occ.start_column && p_column <= c.occ.end_column) {
			if (c.key.is_empty() || c.occ.in_string) {
				return ERR_CANT_RESOLVE;
			}
			r_renamable = !c.native_rooted &&
					(c.key.begins_with("member:") || c.key.begins_with("local:") || c.key.begins_with("enumval:") || c.key.begins_with("class:"));
			return OK;
		}
	}
	return ERR_CANT_RESOLVE;
}

Error GDScriptLanguage::find_symbol_references(const String &p_symbol, const String &p_origin_path, int p_origin_line, int p_origin_column, const HashMap<String, String> &p_buffer_overrides, SymbolReferencesResult &r_result) {
	GDScriptRefactor::Result result;
	const Error err = GDScriptRefactor::find_references(p_symbol, p_origin_path, p_origin_line, p_origin_column, p_buffer_overrides, result);
	if (err != OK) {
		return err;
	}
	r_result.references = result.references;
	r_result.unverified = result.unverified;
	r_result.origin_renamable = result.origin_renamable;
	return OK;
}

Error GDScriptLanguage::resolve_symbol_at(const String &p_symbol, const String &p_path, int p_line, int p_column, const String &p_content, bool &r_renamable) {
	return GDScriptRefactor::resolve_symbol_at(p_symbol, p_path, p_line, p_column, p_content, r_renamable);
}

#endif // TOOLS_ENABLED
