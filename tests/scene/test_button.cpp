/**************************************************************************/
/*  test_button.cpp                                                       */
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

TEST_FORCE_LINK(test_button)

#include "scene/gui/button.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "tests/display_server_mock.h"

#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/object/callable_mp.h"

namespace TestButton {

TEST_CASE("[SceneTree][Button] is_hovered()") {
	// Create new button instance.
	Button *button = memnew(Button);
	CHECK(button != nullptr);
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(button);

	// Set up button's size and position.
	button->set_size(Size2i(50, 50));
	button->set_position(Size2i(10, 10));

	// Button should initially be not hovered.
	CHECK(button->is_hovered() == false);

	// Simulate mouse entering the button.
	SEND_GUI_MOUSE_MOTION_EVENT(Point2i(25, 25), MouseButtonMask::NONE, Key::NONE);
	CHECK(button->is_hovered() == true);

	// Simulate mouse exiting the button.
	SEND_GUI_MOUSE_MOTION_EVENT(Point2i(150, 150), MouseButtonMask::NONE, Key::NONE);
	CHECK(button->is_hovered() == false);

	memdelete(button);
}

static int capture_press_count = 0;
static int capture_pressed_index = -2;
static Ref<InputEvent> capture_pressed_event;
static void _capture_pressed(int p_index, const Ref<InputEvent> &p_event) {
	capture_press_count++;
	capture_pressed_index = p_index;
	capture_pressed_event = p_event;
}

static int capture_down_index = -2;
static void _capture_button_down(int p_index, const Ref<InputEvent> &p_event) {
	capture_down_index = p_index;
}

static int capture_up_index = -2;
static void _capture_button_up(int p_index, const Ref<InputEvent> &p_event) {
	capture_up_index = p_index;
}

static int zero_arg_press_count = 0;
static void _zero_arg_pressed_handler() {
	zero_arg_press_count++;
}

TEST_CASE("[SceneTree][Button] Activation signals report the triggering mouse button and event") {
	Button *button = memnew(Button);
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(button);
	button->set_size(Size2i(50, 50));
	button->set_position(Size2i(10, 10));
	BitField<MouseButtonMask> mask;
	mask.set_flag(MouseButtonMask::LEFT);
	mask.set_flag(MouseButtonMask::RIGHT);
	button->set_button_mask(mask);

	capture_press_count = 0;
	capture_pressed_index = -2;
	capture_pressed_event.unref();
	capture_down_index = -2;
	capture_up_index = -2;
	zero_arg_press_count = 0;

	button->connect("pressed", callable_mp_static(&_capture_pressed));
	button->connect("button_down", callable_mp_static(&_capture_button_down));
	button->connect("button_up", callable_mp_static(&_capture_button_up));
	// Handlers declared without the new parameters must keep working (extra args are dropped).
	button->connect("pressed", callable_mp_static(&_zero_arg_pressed_handler));

	SEND_GUI_MOUSE_MOTION_EVENT(Point2i(25, 25), MouseButtonMask::NONE, Key::NONE);

	// Right button: `button_down` on press; `pressed` and `button_up` on release
	// (default action mode is ACTION_MODE_BUTTON_RELEASE).
	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(25, 25), MouseButton::RIGHT, MouseButtonMask::RIGHT, Key::NONE);
	CHECK(capture_down_index == (int)MouseButton::RIGHT);
	CHECK(capture_press_count == 0);
	SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(Point2i(25, 25), MouseButton::RIGHT, MouseButtonMask::NONE, Key::NONE);
	CHECK(capture_press_count == 1);
	CHECK(capture_pressed_index == (int)MouseButton::RIGHT);
	CHECK(capture_up_index == (int)MouseButton::RIGHT);
	{
		Ref<InputEventMouseButton> mb = capture_pressed_event;
		REQUIRE(mb.is_valid());
		CHECK(mb->get_button_index() == MouseButton::RIGHT);
	}

	// Left button reports LEFT.
	SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(25, 25), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
	CHECK(capture_down_index == (int)MouseButton::LEFT);
	SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(Point2i(25, 25), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
	CHECK(capture_press_count == 2);
	CHECK(capture_pressed_index == (int)MouseButton::LEFT);
	CHECK(capture_up_index == (int)MouseButton::LEFT);

	// Keyboard activation reports no mouse button but still passes the event.
	button->grab_focus();
	SEND_GUI_ACTION("ui_accept");
	CHECK(capture_down_index == (int)MouseButton::NONE);
	{
		// SEND_GUI_ACTION only presses; `pressed` fires on release (default action mode).
		const List<Ref<InputEvent>> *events = InputMap::get_singleton()->action_get_events("ui_accept");
		Ref<InputEventKey> event = events->front()->get()->duplicate();
		event->set_pressed(false);
		_SEND_DISPLAYSERVER_EVENT(event);
		MessageQueue::get_singleton()->flush();
	}
	CHECK(capture_press_count == 3);
	CHECK(capture_pressed_index == (int)MouseButton::NONE);
	CHECK(capture_pressed_event.is_valid());

	// The zero-arg handler ran for every activation.
	CHECK(zero_arg_press_count == 3);

	capture_pressed_event.unref();
	memdelete(button);
}

} // namespace TestButton
