#include <game/fire.h>

#include <cstdlib>
#include <iostream>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            std::cerr << "Check failed at line " << __LINE__ << ": " << #expression << '\n';      \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

static ecs::entity armed_player(bool fire, int power = 0) {
    auto connection = ecs::entity::create();
    auto player = game::spawn_player(connection);
    player.set(game::PlayerInput{ 0.f, 0.f, fire });
    if (power > 0)
        player.set(game::Power{ power });
    return player;
}

static ecs::entity only_missile() {
    auto missile = ecs::query().require<game::PlayerMissile>().first();
    CHECK(missile.is_alive());
    return missile;
}

int main() {
    ecs::init({ .worker_threads = 0 });

    {
        auto player = armed_player(false);
        CHECK(!game::try_fire(player, 0.016f).is_alive());
        CHECK(ecs::query().require<game::PlayerMissile>().first().id() == 0);
        player.kill();
    }

    {
        auto player = armed_player(true);
        auto shot = game::try_fire(player, 0.016f);
        CHECK(shot.is_alive());
        CHECK(shot.has<game::PlayerMissile>());
        CHECK(shot.has<net::Replicated>());
        CHECK(shot.get<game::Damage>().value == game::missile_damage);
        CHECK(shot.get<game::Velocity>().x == game::missile_speed);
        CHECK(shot.get<game::Position>().x == player.get<game::Position>().x +
                                                  player.get<game::Collider>().half_width +
                                                  game::missile_half_width);
        CHECK(shot.get<game::Position>().y == player.get<game::Position>().y);
        CHECK(shot.get<game::PreviousPosition>().x == shot.get<game::Position>().x);
        CHECK(shot.target<net::OwnedBy>().id() == player.target<net::OwnedBy>().id());
        CHECK((shot.get<game::CollisionMask>().belongs & game::layer::player_missile) != 0);
        CHECK(!game::try_fire(player, 0.f).is_alive());
        CHECK(ecs::query().require<game::PlayerMissile>().first().id() == shot.id());
        shot.kill();
        player.kill();
    }

    {
        auto player = armed_player(true);
        auto first = game::try_fire(player, 0.f);
        auto second = game::try_fire(player, game::fire_interval);
        CHECK(first.is_alive());
        CHECK(second.is_alive());
        CHECK(first.id() != second.id());
        first.kill();
        second.kill();
        player.kill();
    }

    {
        auto player = armed_player(true, 2);
        auto shot = game::try_fire(player, 0.f);
        CHECK(shot.get<game::Damage>().value == game::missile_damage + 2);
        shot.kill();
        player.kill();
    }

    {
        auto player = armed_player(true);
        player.add<game::PendingDestroy>();
        CHECK(!game::try_fire(player, 0.f).is_alive());
        player.kill();
    }

    {
        ecs::set_resource(DeltaTime{ .value = 0.016f });
        game::install_weapons();
        auto player = armed_player(true);
        ecs::run_phase(EcsOnUpdate);
        auto shot = only_missile();
        CHECK(shot.get<game::Velocity>().x == game::missile_speed);
        shot.kill();
        player.kill();
    }

    ecs::fini();
    return 0;
}
