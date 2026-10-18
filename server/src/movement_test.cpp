#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <limits>
#include <shared/movement.h>

using Catch::Approx;

namespace {
struct World {
    ecs::entity player;
    World() {
        ecs::init();
        ecs::import<game::Kinematics>();
        player = ecs::entity::create()
                     .add<game::Driven, physics::Body>()
                     .set(Position2d{ 0, 0 }, physics::Box{});
    }
    ~World() { ecs::fini(); }

    Position2d step(game::PlayerInput input) {
        player.set(input);
        physics::advance(physics::fixed_step);
        return player.get<Position2d>();
    }
};

constexpr float step_distance = game::player_speed * physics::fixed_step;
} // namespace

TEST_CASE_METHOD(World, "input moves the player at player speed") {
    const auto position = step({ 1, 0, false });
    CHECK(position.x == Approx(step_distance));
    CHECK(position.y == Approx(0));
}

TEST_CASE_METHOD(World, "diagonal input is not faster than straight input") {
    const auto position = step({ 1, 1, false });
    CHECK(std::hypot(position.x, position.y) == Approx(step_distance));
}

TEST_CASE_METHOD(World, "out-of-range or non-finite input stops the player") {
    const auto invalid = GENERATE(
        game::PlayerInput{ 2, 0, false },
        game::PlayerInput{ std::numeric_limits<float>::quiet_NaN(), 0, false },
        game::PlayerInput{ 0, std::numeric_limits<float>::infinity(), false }
    );
    const auto before = step({ 1, 0, false });
    const auto after = step(invalid);
    CHECK(after.x == Approx(before.x));
    CHECK(after.y == Approx(before.y));
}
