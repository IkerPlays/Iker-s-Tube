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
		list->views = {bar, new EmptyView(0, 0, 320, 600)};
		ScrollView *sv = new ScrollView(0, 0, 320, 200);
		sv->set_views({list});
		sv->update(touch(100, 15, true));
		for (int y = 15; y < 60; y += 5) sv->update(touch(100 + y, y, false)); // drifts down while sliding right
		CHECK(sv->get_offset() == 0, "the page does not scroll while a slider is dragged");
		sv->update(release());
	}
	puts(bad ? "FAIL" : "OK");
	return bad;
}
