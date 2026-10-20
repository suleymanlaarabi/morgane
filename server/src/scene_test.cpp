#include "gameplay.h"
#include <catch2/catch_test_macros.hpp>
#include <shared/scene.h>

TEST_CASE("scene loads spawn points and skips replicated entities on the client") {
    ecs::init();
    REQUIRE(game::load_level(false));
    unsigned spawns = 0;
    ecs::query().each([&](const game::SpawnPoint &) { ++spawns; });
    CHECK(spawns == 4);
    unsigned replicated = 0;
    ecs::query().each([&](const net::Replicated &) { ++replicated; });
    CHECK(replicated >= 1);
    ecs::fini();

    ecs::init();
    REQUIRE(game::load_level(true));
    spawns = 0;
    ecs::query().each([&](const game::SpawnPoint &) { ++spawns; });
    CHECK(spawns == 4);
    replicated = 0;
    ecs::query().each([&](const net::Replicated &) { ++replicated; });
    CHECK(replicated == 0);
    ecs::fini();
}
