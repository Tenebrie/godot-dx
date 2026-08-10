/**************************************************************************/
/*  game_window_geometry.cpp                                              */
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

#include "game_window_geometry.h"

#include <climits>

GameWindowGeometry GameWindowGeometry::compute(const Rect2i &p_saved_rect, int p_saved_screen, bool p_saved_maximized, const Point2i &p_placement_position, int p_placement_screen, const Size2i &p_placement_size, const Point2 &p_embed_offset, const Size2 &p_embed_size_diff, const Size2 &p_wrapper_margins, const Rect2i &p_usable_screen_rect) {
	GameWindowGeometry geometry;
	Point2 position = p_saved_rect.position;
	Size2 size = p_saved_rect.size;
	geometry.screen = p_saved_screen;

	if (p_saved_rect == Rect2i()) {
		if (p_placement_position != Point2i(INT_MAX, INT_MAX)) {
			position = Point2(p_placement_position) - p_embed_offset;
			geometry.screen = p_placement_screen;
		}
		if (p_placement_size != Size2i()) {
			size = Size2(p_placement_size) + p_embed_size_diff + p_wrapper_margins;
		}
	}

	geometry.window_rect = Rect2(position, size);

	if (p_saved_maximized && p_usable_screen_rect != Rect2i()) {
		geometry.embed_rect = Rect2i(Point2i(Point2(p_usable_screen_rect.position) + p_embed_offset), Size2i(Size2(p_usable_screen_rect.size) - p_embed_size_diff - p_wrapper_margins));
	} else {
		geometry.embed_rect = Rect2i(Point2i(position + p_embed_offset), Size2i(size - p_embed_size_diff - p_wrapper_margins));
	}

	return geometry;
}
