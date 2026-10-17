#include "gui.h"
#include <catch2/catch_test_macros.hpp>

namespace {
/** Headless world: only the layout half of gui, on an 800x600 screen. */
struct World {
    World() {
        ecs::init();
        ecs::set_resource(ray::Screen{ 800, 600 });
        ecs::import<gui::Layout>();
    }
    ~World() { ecs::fini(); }
};

ecs::entity node(gui::Node layout) { return ecs::entity::create().set(layout, gui::Bounds{}); }
ecs::entity node(gui::Node layout, ecs::entity parent) { return node(layout).child_of(parent); }
Rectangle rect(ecs::entity e) { return e.get<gui::Bounds>().rect; }

bool same(Rectangle a, Rectangle b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

int clicks = 0;
struct Play {};
} // namespace

TEST_CASE_METHOD(World, "a column stacks its children with gap and padding") {
    const auto box = node(gui::column(10).pad(5).at(20, 30));
    const auto first = node(gui::node().size(100, 40), box);
    const auto second = node(gui::node().size(60, 20), box);
    ecs::progress();

    CHECK(same(rect(box), { 20, 30, 110, 80 }));
    CHECK(same(rect(first), { 25, 35, 100, 40 }));
    CHECK(same(rect(second), { 25, 85, 60, 20 }));
}

TEST_CASE_METHOD(World, "fill takes the space left in a row") {
    const auto bar = node(gui::row().size(300, 50));
    node(gui::node().size(50, 50), bar);
    const auto middle = node(gui::node().width(gui::fill).height(50), bar);
    const auto last = node(gui::node().size(50, 50), bar);
    ecs::progress();

    CHECK(same(rect(middle), { 50, 0, 200, 50 }));
    CHECK(same(rect(last), { 250, 0, 50, 50 }));
}

TEST_CASE_METHOD(World, "anchors place a node on the screen and inside free parents") {
    const auto dialog = node(gui::node().size(200, 100).center());
    const auto close = node(gui::node().size(10, 10).at(gui::TopRight, -5, 5), dialog);
    ecs::progress();

    CHECK(same(rect(dialog), { 300, 250, 200, 100 }));
    CHECK(same(rect(close), { 485, 255, 10, 10 }));
}

TEST_CASE_METHOD(World, "align and justify distribute the cross and main axes") {
    const auto box =
        node(gui::column().size(100, 100).align(gui::Align::Center).justify(gui::Align::End));
    const auto child = node(gui::node().size(40, 20), box);
    const auto stretched = node(gui::column().size(100, 100).align(gui::Align::Stretch));
    const auto wide = node(gui::node().height(20), stretched);
    ecs::progress();

    CHECK(same(rect(child), { 30, 80, 40, 20 }));
    CHECK(rect(wide).width == 100);
}

TEST_CASE_METHOD(World, "siblings keep their creation order across component tables") {
    const auto list = node(gui::column());
    const auto first = node(gui::node().size(10, 10), list).set(gui::Tooltip{ "first" });
    const auto second = node(gui::node().size(10, 10), list);
    ecs::progress();

    CHECK(rect(first).y == 0);
    CHECK(rect(second).y == 10);
}

TEST_CASE_METHOD(World, "hidden subtrees take no space and disabled ones propagate") {
    const auto list = node(gui::column());
    const auto hidden = node(gui::node().size(10, 10), list).add<gui::Hidden>();
    const auto shown = node(gui::column().size(10, 10), list).add<gui::Disabled>();
    const auto inner = node(gui::node().size(5, 5), shown);
    ecs::progress();

    CHECK(rect(shown).y == 0);
    CHECK(hidden.get<gui::Bounds>().frame != shown.get<gui::Bounds>().frame);
    CHECK(inner.get<gui::Bounds>().disabled);
}

TEST_CASE_METHOD(World, "widgets bring their node, bounds and dispatch table") {
    const auto panel = ecs::entity::create().set(gui::Panel{});
    CHECK(panel.has<gui::Node, gui::Bounds>());
    CHECK(panel.get<gui::detail::Kind>().vtable == &gui::detail::vtable<gui::Panel>);
}

TEST_CASE_METHOD(World, "nodes follow a world parent and die with a followed target") {
    const auto ship = ecs::entity::create().set(GlobalPosition2d{ 100, 200 });
    const auto label = node(gui::node().size(20, 10).at(gui::Bottom, 0, -4), ship);
    auto enemy = ecs::entity::create().set(GlobalPosition2d{ 10, 10 });
    const auto bar = node(gui::node().size(4, 4)).relate<gui::Follows>(enemy);
    ecs::progress();

    CHECK(same(rect(label), { 90, 186, 20, 10 }));
    CHECK(same(rect(bar), { 10, 10, 4, 4 }));
    enemy.kill();
    CHECK_FALSE(bar.is_alive());
}

TEST_CASE_METHOD(World, "clicks are visible for one frame to on_click systems") {
    clicks = 0;
    gui::on_click<Play>([] { ++clicks; });
    const auto button = node(gui::node()).add<Play, gui::Clicked>();
    ecs::progress();
    CHECK(clicks == 1);
    CHECK_FALSE(button.has<gui::Clicked>());
    ecs::progress();
    CHECK(clicks == 1);
}
