#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sibox2d/physics.h>
#include <vector>

using Catch::Approx;

namespace {
/** SIECS observers are stateless, so contacts are collected here. */
std::vector<physics::Contact> contacts;

struct World {
    World() {
        contacts.clear();
        ecs::init();
        ecs::import<physics::World>();
        ecs::observe<physics::OnContact>().each([](ecs::entity, ecs::observer_event event) {
            contacts.push_back(*event.trigger_data<physics::Contact>());
        });
    }
    ~World() { ecs::fini(); }
};

ecs::entity body(Position2d position, Velocity2d velocity = {}) {
    return ecs::entity::create().set(position, velocity, physics::Body{}, physics::Box{});
}
} // namespace

TEST_CASE_METHOD(World, "a fixed step moves bodies by their velocity") {
    const auto moving = body({ 0, 0 }, { 60, 0 });
    physics::advance(physics::fixed_step);

    CHECK(moving.get<Position2d>().x == Approx(1));
    CHECK(moving.get<Position2d>().y == Approx(0));
    CHECK(ecs::resource<physics::Clock>().tick == 1);
}

TEST_CASE_METHOD(World, "partial steps accumulate until a full step is available") {
    physics::advance(physics::fixed_step / 2);
    CHECK(ecs::resource<physics::Clock>().tick == 0);
    physics::advance(physics::fixed_step / 2);
    CHECK(ecs::resource<physics::Clock>().tick == 1);
}

TEST_CASE_METHOD(World, "catch-up is bounded and the excess time is dropped") {
    physics::advance(1.0);
    const auto &clock = ecs::resource<physics::Clock>();
    CHECK(clock.tick == physics::WorldConfig{}.max_catch_up_steps);
    CHECK(clock.dropped_seconds > 0);
}

TEST_CASE_METHOD(World, "overlapping bodies report one contact") {
    body({ 0, 0 });
    body({ 4, 0 });
    physics::advance(physics::fixed_step);
    physics::advance(physics::fixed_step);

    REQUIRE(contacts.size() == 1);
    CHECK(contacts[0].tick == 1);
}

TEST_CASE_METHOD(World, "filters prevent contacts between unrelated categories") {
    body({ 0, 0 }).set(physics::Filter{ 1, 1 });
    body({ 4, 0 }).set(physics::Filter{ 2, 2 });
    physics::advance(physics::fixed_step);

    CHECK(contacts.empty());
}
