#include <game/fire.h>
#include <game/move.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            std::cerr << "Check failed at line " << __LINE__ << ": " << #expression << '\n';      \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

static bool near(float a, float b) { return std::abs(a - b) < 0.001f; }

static game::Playfield screen{};

int main() {
    ecs::init({ .worker_threads = 0 });

    {
        auto player = ecs::entity::create()
                          .set(game::Position{ 80.f, 200.f })
                          .set(game::Collider{ 16.f, 8.f })
                          .set(game::PlayerInput{ 1.f, 0.f, false })
                          .add<game::Player>();
        game::steer(player);
        game::move_body(player, 0.5f, screen);
        CHECK(near(player.get<game::Velocity>().x, game::player_speed));
        CHECK(near(player.get<game::Velocity>().y, 0.f));
        CHECK(near(player.get<game::PreviousPosition>().x, 80.f));
        CHECK(near(player.get<game::Position>().x, 80.f + game::player_speed * 0.5f));
        CHECK(near(player.get<game::Position>().y, 200.f));
        player.set(game::PlayerInput{ 0.f, 0.f, false });
        game::steer(player);
        float stopped = player.get<game::Position>().x;
        game::move_body(player, 0.5f, screen);
        CHECK(near(player.get<game::Velocity>().x, 0.f));
        CHECK(near(player.get<game::Position>().x, stopped));
        player.kill();
    }

    {
        auto player = ecs::entity::create()
                          .set(game::Position{ 0.f, 0.f })
                          .set(game::PlayerInput{ 1.f, 1.f, false })
                          .add<game::Player>();
        game::steer(player);
        float speed = std::hypot(player.get<game::Velocity>().x, player.get<game::Velocity>().y);
        CHECK(near(speed, game::player_speed));
        player.kill();
    }

    {
        auto player = ecs::entity::create()
                          .set(game::Position{ 20.f, 20.f })
                          .set(game::Collider{ 16.f, 8.f })
                          .set(game::Velocity{ -game::player_speed, 0.f })
                          .add<game::Player>();
        game::Playfield arena{ .min_x = 0.f, .min_y = 0.f, .max_x = 100.f, .max_y = 100.f };
        game::move_body(player, 1.f, arena);
        CHECK(near(player.get<game::Position>().x, 16.f));
        CHECK(near(player.get<game::Position>().y, 20.f));
        player.kill();
    }

    {
        auto missile = ecs::entity::create()
                           .set(game::Position{ -20.f, 4.f })
                           .set(game::Velocity{ game::missile_speed, 0.f })
                           .set(game::Collider{ 1.f, 1.f })
                           .set(game::player_missile_mask)
                           .set(game::Damage{ 1 })
                           .add<game::PlayerMissile>();
        auto foe = ecs::entity::create()
                       .set(game::Position{ 0.f, 4.f })
                       .set(game::Collider{ 2.f, 2.f })
                       .set(game::monster_mask)
                       .add<game::Monster>();
        game::move_body(missile, 0.1f, screen);
        CHECK(near(missile.get<game::PreviousPosition>().x, -20.f));
        CHECK(near(missile.get<game::Position>().x, -20.f + game::missile_speed * 0.1f));
        game::Contacts contacts;
        game::detect(contacts, screen);
        CHECK(contacts.hits.size() == 1);
        CHECK(contacts.hits[0].seeker.id() == missile.id());
        CHECK(contacts.hits[0].target.id() == foe.id());
        missile.kill();
        foe.kill();
    }

    {
        auto missile = ecs::entity::create()
                           .set(game::Position{ 0.f, 0.f })
                           .set(game::Velocity{ game::missile_speed, 0.f })
                           .add<game::PlayerMissile, game::PendingDestroy>();
        game::move_body(missile, 1.f, screen);
        CHECK(near(missile.get<game::Position>().x, 0.f));
        CHECK(!missile.try_get<game::PreviousPosition>());
        missile.kill();
    }

    {
        ecs::set_resource(DeltaTime{ .value = 0.1f });
        game::install_movement();
        auto player = ecs::entity::create()
                          .set(game::Position{ 80.f, 40.f })
                          .set(game::Collider{ 16.f, 8.f })
                          .set(game::PlayerInput{ 0.f, -1.f, false })
                          .add<game::Player>();
        auto missile = ecs::entity::create()
                           .set(game::Position{ 10.f, 40.f })
                           .set(game::Velocity{ game::missile_speed, 0.f })
                           .add<game::PlayerMissile>();
        ecs::run_phase(EcsOnUpdate);
        CHECK(near(player.get<game::Position>().y, 40.f - game::player_speed * 0.1f));
        CHECK(near(player.get<game::PreviousPosition>().y, 40.f));
        CHECK(near(missile.get<game::Position>().x, 10.f + game::missile_speed * 0.1f));
        CHECK(near(missile.get<game::PreviousPosition>().x, 10.f));
        player.kill();
        missile.kill();
    }

    ecs::fini();
    return 0;
}
