#include "ui/ui.hpp"
#include <cstdio>
// Host regression tests for the touch handling of the views (see README). Run: ./input_test.sh
static int bad = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); bad++; } } while (0)
static Hid_info touch(int x, int y, bool first) { Hid_info k; k.touch_x = x; k.touch_y = y; k.p_touch = first; return k; }
static Hid_info release() { Hid_info k; k.touch_x = -1; k.touch_y = -1; return k; }

int main() {
	var_round_buttons = true;
	{ // a button fires once on release, and not again while nothing touches the screen
		int fired = 0;
		TextView *b = new TextView(10, 10, 100, 22);
		b->set_text("x")->set_on_view_released([&](View &) { fired++; });
		b->update(touch(20, 15, true)); b->update(touch(20, 15, false)); b->update(release());
		CHECK(fired == 1, "button fires once");
		for (int i = 0; i < 5; i++) b->update(release());
		CHECK(fired == 1, "button does not fire again without a touch");
	}
	{ // a view that covers the point (-1, -1) must not treat "no touch" as a touch
		int fired = 0;
		TextView *b = new TextView(-5, -5, 100, 40);
		b->set_text("x")->set_on_view_released([&](View &) { fired++; });
		b->update(touch(10, 10, true)); b->update(release());
		for (int i = 0; i < 5; i++) b->update(release());
		CHECK(fired == 1, "view over (-1,-1) fires once");
	}
	{ // tabs : a tap on a tab selects it and the selection does not repeat afterwards
		TabView *t = new TabView(0, 0, 320, 210);
		t->set_views({new EmptyView(0, 0, 320, 0), new EmptyView(0, 0, 320, 0), new EmptyView(0, 0, 320, 0)})->set_tab_texts<std::string>({"a", "b", "c"});
		t->update(touch(250, 205, true)); t->update(release());
		CHECK(t->selected_tab == 2, "tab 2 selected");
		t->update(touch(10, 205, true)); t->update(release());
		CHECK(t->selected_tab == 0, "tab 0 selected");
	}
	{ // a slider is dragged inside a scroll view : the page must not scroll under the finger
		BarView *bar = new BarView(0, 0, 260, 30);
		bar->set_values(0, 1, 0);
		VerticalListView *list = new VerticalListView(0, 0, 320);
		list->views = {new EmptyView(0, 0, 320, 100), bar, new EmptyView(0, 0, 320, 600)}; // the slider is at y = 115
		ScrollView *sv = new ScrollView(0, 0, 320, 200);
		sv->set_views({list});
		sv->update(touch(100, 115, true));
		for (int i = 1; i <= 9; i++) sv->update(touch(100 + i * 10, 115 - i * 4, false)); // drifts up while sliding right
		CHECK(sv->get_offset() == 0, "the page does not scroll while a slider is dragged");
		sv->update(release());
	}
	{ // a yes / no switch toggles on a tap in its row, and not on a tap beside it
		SelectorView *sw = new SelectorView(0, 0, 200, 35, true);
		sw->set_texts({std::function<std::string()>([]() { return std::string("Off"); }), std::function<std::string()>([]() { return std::string("On"); })}, 0)->set_title([](const SelectorView &) { return std::string("Title"); });
		int changes = 0;
		sw->set_on_change([&](const SelectorView &) { changes++; });
		sw->update(touch(100, 15, true)); sw->update(release());
		CHECK(sw->selected_button == 1 && changes == 1, "switch turns on");
		sw->update(touch(100, 15, true)); sw->update(release());
		CHECK(sw->selected_button == 0 && changes == 2, "switch turns off");
		sw->update(touch(250, 15, true)); sw->update(release());
		CHECK(sw->selected_button == 0 && changes == 2, "a tap beside the switch does nothing");
		sw->selected_button = 7; // a damaged saved value
		sw->update(touch(100, 15, true)); sw->update(release());
		CHECK(sw->selected_button == 1, "a stored value other than 0 / 1 toggles to on");
	}
	{ // the second tab bar (channel page) : a tap on the header selects and stays selected, no endless refresh
		Tab2View *t = new Tab2View(0, 0, 320);
		t->set_views({new EmptyView(0, 0, 320, 0), new EmptyView(0, 0, 320, 0)})->set_tab_texts<std::string>({"a", "b"});
		t->update(touch(250, 5, true)); t->update(release());
		CHECK(t->selected_tab == 1, "tab2 selects the second tab");
		var_need_refresh = false;
		for (int i = 0; i < 3; i++) t->update(release());
		CHECK(!var_need_refresh, "tab2 does not refresh forever after a tap");
	}
	{ // a row inside a scroll view : the finger slides out of the area while down, the row must not fire
		int fired = 0;
		TextView *row = new TextView(0, 0, 320, 30);
		row->set_text("row")->set_on_view_released([&](View &) { fired++; });
		ScrollView *sv = new ScrollView(0, 0, 320, 100);
		sv->set_views({row});
		sv->update(touch(100, 15, true)); sv->update(touch(100, 150, false)); sv->update(release());
		CHECK(fired == 0, "sliding out of the scroll area cancels the row");
		sv->update(touch(100, 15, true)); sv->update(release());
		CHECK(fired == 1, "a plain tap on the row fires it");
	}
	puts(bad ? "FAIL" : "OK");
	return bad;
}
