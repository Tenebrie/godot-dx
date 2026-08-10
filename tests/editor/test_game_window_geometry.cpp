/**************************************************************************/
/*  test_game_window_geometry.cpp                                         */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_game_window_geometry)

#ifdef TOOLS_ENABLED

#include "editor/run/game_window_geometry.h"

#include <climits>

namespace TestGameWindowGeometry {

const Point2 EMBED_OFFSET = Point2(9, 91);
const Size2 EMBED_SIZE_DIFF = Size2(0, 90);
const Size2 WRAPPER_MARGINS = Size2(18, 76);
const Point2i NO_POSITION = Point2i(INT_MAX, INT_MAX);

TEST_CASE("[GameWindowGeometry] First open derives the window from placement") {
	GameWindowGeometry geometry = GameWindowGeometry::compute(Rect2i(), -1, false, Point2i(640, 360), 1, Size2i(2560, 1440), EMBED_OFFSET, EMBED_SIZE_DIFF, WRAPPER_MARGINS, Rect2i());
	CHECK(geometry.screen == 1);
	CHECK(geometry.window_rect == Rect2(Point2(640, 360) - EMBED_OFFSET, Size2(2560, 1440) + EMBED_SIZE_DIFF + WRAPPER_MARGINS));
	CHECK(geometry.embed_rect == Rect2i(640, 360, 2560, 1440));
}

TEST_CASE("[GameWindowGeometry] First open without a placement position keeps the saved screen") {
	GameWindowGeometry geometry = GameWindowGeometry::compute(Rect2i(), 2, false, NO_POSITION, 0, Size2i(2560, 1440), EMBED_OFFSET, EMBED_SIZE_DIFF, WRAPPER_MARGINS, Rect2i());
	CHECK(geometry.screen == 2);
	CHECK(geometry.window_rect.position == Point2());
	CHECK(geometry.window_rect.size == Size2(2560, 1440) + EMBED_SIZE_DIFF + WRAPPER_MARGINS);
}

TEST_CASE("[GameWindowGeometry] Saved rect wins over placement") {
	Rect2i saved = Rect2i(100, 50, 3300, 1900);
	GameWindowGeometry geometry = GameWindowGeometry::compute(saved, 0, false, Point2i(640, 360), 1, Size2i(2560, 1440), EMBED_OFFSET, EMBED_SIZE_DIFF, WRAPPER_MARGINS, Rect2i());
	CHECK(geometry.screen == 0);
	CHECK(geometry.window_rect == Rect2(saved));
	CHECK(geometry.embed_rect == Rect2i(Point2i(Point2(saved.position) + EMBED_OFFSET), Size2i(Size2(saved.size) - EMBED_SIZE_DIFF - WRAPPER_MARGINS)));
}

TEST_CASE("[GameWindowGeometry] Saved and placement rects round-trip through window chrome") {
	GameWindowGeometry first = GameWindowGeometry::compute(Rect2i(), -1, false, Point2i(640, 360), 1, Size2i(2560, 1440), EMBED_OFFSET, EMBED_SIZE_DIFF, WRAPPER_MARGINS, Rect2i());
	GameWindowGeometry second = GameWindowGeometry::compute(Rect2i(first.window_rect), 1, false, Point2i(0, 0), 0, Size2i(1024, 600), EMBED_OFFSET, EMBED_SIZE_DIFF, WRAPPER_MARGINS, Rect2i());
	CHECK(second.window_rect == first.window_rect);
	CHECK(second.embed_rect == first.embed_rect);
}

TEST_CASE("[GameWindowGeometry] Maximized derives the embed from the screen usable rect") {
	Rect2i saved = Rect2i(100, 50, 2578, 1606);
	Rect2i usable = Rect2i(0, 21, 3840, 2139);
	GameWindowGeometry geometry = GameWindowGeometry::compute(saved, 0, true, Point2i(640, 360), 1, Size2i(2560, 1440), EMBED_OFFSET, EMBED_SIZE_DIFF, WRAPPER_MARGINS, usable);
	CHECK(geometry.window_rect == Rect2(saved));
	CHECK(geometry.embed_rect == Rect2i(Point2i(Point2(usable.position) + EMBED_OFFSET), Size2i(Size2(usable.size) - EMBED_SIZE_DIFF - WRAPPER_MARGINS)));
}

TEST_CASE("[GameWindowGeometry] Maximized without a saved rect falls back to the placement window") {
	Rect2i usable = Rect2i(0, 21, 3840, 2139);
	GameWindowGeometry geometry = GameWindowGeometry::compute(Rect2i(), -1, true, Point2i(640, 360), 1, Size2i(2560, 1440), EMBED_OFFSET, EMBED_SIZE_DIFF, WRAPPER_MARGINS, usable);
	CHECK(geometry.window_rect.size == Size2(2560, 1440) + EMBED_SIZE_DIFF + WRAPPER_MARGINS);
	CHECK(geometry.embed_rect.size == Size2i(Size2(usable.size) - EMBED_SIZE_DIFF - WRAPPER_MARGINS));
}

TEST_CASE("[GameWindowGeometry] Maximized without screen info falls back to the saved rect") {
	Rect2i saved = Rect2i(100, 50, 2578, 1606);
	GameWindowGeometry geometry = GameWindowGeometry::compute(saved, 0, true, NO_POSITION, 0, Size2i(2560, 1440), EMBED_OFFSET, EMBED_SIZE_DIFF, WRAPPER_MARGINS, Rect2i());
	CHECK(geometry.window_rect == Rect2(saved));
	CHECK(geometry.embed_rect == Rect2i(Point2i(Point2(saved.position) + EMBED_OFFSET), Size2i(Size2(saved.size) - EMBED_SIZE_DIFF - WRAPPER_MARGINS)));
}

} // namespace TestGameWindowGeometry

#endif // TOOLS_ENABLED
