#include <game/resolve.h>

#include <cstdlib>
#include <iostream>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            std::cerr << "Check failed at line " << __LINE__ << ": " << #expression << '\n';      \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

struct KillLog {
    int count = 0;
    uint8_t slot = 0;
};

struct EffectLog {
    int destroyed = 0;
    uint8_t layer = 0;
    int bonuses = 0;
    float bonus_x = 0;
    float bonus_y = 0;
    int pickups = 0;
    int level = 0;
    uint8_t slot = 0;
    int left = 0;
    uint8_t left_slot = 0;
};

static ecs::entity missile(float x, int damage) {
    return ecs::entity::create()
        .set(game::Position{ x, 0.f })
        .set(game::Collider{ 2.f, 2.f })
        .set(game::player_missile_mask)
        .set(game::Damage{ damage })
        .add<game::PlayerMissile>();
}

static ecs::entity enemy_shot(int damage) {
    return ecs::entity::create()
        .set(game::Position{ 0.f, 0.f })
        .set(game::Collider{ 2.f, 2.f })
        .set(game::enemy_missile_mask)
        .set(game::Damage{ damage })
        .add<game::EnemyMissile>();
}

static ecs::entity monster(int health, bool drops) {
    auto entity = ecs::entity::create()
                      .set(game::Position{ 30.f, 4.f })
                      .set(game::Collider{ 8.f, 8.f })
                      .set(game::monster_mask)
                      .set(game::Health{ health })
                      .set(game::Damage{ 4 })
                      .add<game::Monster>();
    if (drops)
        entity.add<game::DropsPowerup>();
    return entity;
}

static ecs::entity player(int health) {
    return ecs::entity::create()
        .set(game::Position{ 0.f, 0.f })
        .set(game::Collider{ 10.f, 10.f })
        .set(game::Velocity{ 40.f, -5.f })
        .set(game::player_mask)
        .set(game::Health{ health })
        .set(game::PlayerSlot{ 2 })
        .add<game::Player>();
}

int main() {
    ecs::init({ .worker_threads = 0 });
    KillLog kills;
    ecs::observe<game::PlayerKilled>().user_data(&kills).each([](ecs::observer_event event) {
        auto *log = event.user_data<KillLog>();
        log->count += 1;
        log->slot = event.trigger_data<game::PlayerKilled>()->slot;
    });
    EffectLog effects;
    ecs::observe<game::Destroyed>().user_data(&effects).each([](ecs::observer_event event) {
        auto *log = event.user_data<EffectLog>();
        log->destroyed += 1;
        log->layer = event.trigger_data<game::Destroyed>()->layer;
    });
    ecs::observe<game::BonusSpawned>().user_data(&effects).each([](ecs::observer_event event) {
        auto *log = event.user_data<EffectLog>();
        auto *spawned = event.trigger_data<game::BonusSpawned>();
        log->bonuses += 1;
        log->bonus_x = spawned->x;
        log->bonus_y = spawned->y;
    });
    ecs::observe<game::PickedUp>().user_data(&effects).each([](ecs::observer_event event) {
        auto *log = event.user_data<EffectLog>();
        auto *picked = event.trigger_data<game::PickedUp>();
        log->pickups += 1;
        log->level = picked->level;
        log->slot = picked->slot;
    });
    ecs::observe<game::PlayerLeft>().user_data(&effects).each([](ecs::observer_event event) {
        auto *log = event.user_data<EffectLog>();
        log->left += 1;
        log->left_slot = event.trigger_data<game::PlayerLeft>()->slot;
    });

    {
        auto shot = missile(0.f, 3);
        auto foe = monster(5, false);
        game::resolve({ { shot, foe, 0.f, 0.f } });
        CHECK(foe.is_alive());
        CHECK(foe.get<game::Health>().value == 2);
        CHECK(!shot.is_alive());
        CHECK(effects.destroyed == 1);
        CHECK(effects.layer == game::layer::player_missile);
        CHECK(effects.bonuses == 0);
        CHECK(ecs::query().require<game::Powerup>().first().id() == 0);
    }

    {
        auto shot = missile(0.f, 5);
        auto foe = monster(5, true);
        auto game_instance = ecs::entity::create();
        foe.relate<net::InInstance>(game_instance);
        game::resolve({ { shot, foe, 0.f, 0.f } });
        CHECK(!foe.is_alive());
        CHECK(!shot.is_alive());
        CHECK(effects.bonuses == 1);
        CHECK(effects.bonus_x == 30.f);
        CHECK(effects.bonus_y == 4.f);
        auto bonus = ecs::query().require<game::Powerup>().first();
        CHECK(bonus.is_alive());
        CHECK(bonus.get<game::Position>().x == 30.f);
        CHECK(bonus.get<game::Position>().y == 4.f);
        CHECK(bonus.has<net::Replicated>());
        CHECK(bonus.target<net::InInstance>().id() == game_instance.id());
        bonus.kill();
    }

    {
        auto first = missile(0.f, 3);
        auto second = missile(1.f, 3);
        auto foe = monster(4, true);
        game::resolve({
            { first, foe, 0.f, 0.f },
            { second, foe, 0.f, 0.f },
        });
        CHECK(!foe.is_alive());
        CHECK(!first.is_alive());
        CHECK(!second.is_alive());
        auto bonus = ecs::query().require<game::Powerup>().first();
        CHECK(bonus.is_alive());
        bonus.kill();
    }

    {
        auto ship = player(10);
        auto foe = monster(9, false);
        game::resolve({ { ship, foe, 0.f, 0.f } });
        CHECK(ship.get<game::Health>().value == 6);
        CHECK(foe.get<game::Health>().value == 9);
        CHECK(kills.count == 0);
    }

    {
        auto ship = player(3);
        auto shot = enemy_shot(3);
        game::resolve({
            { ship, shot, 0.f, 0.f },
            { shot, ship, 0.f, 0.f },
        });
        CHECK(!ship.is_alive());
        CHECK(!shot.is_alive());
        CHECK(kills.count == 1);
        CHECK(kills.slot == 2);
    }

    {
        auto ship = player(10);
        auto wall = ecs::entity::create()
                        .set(game::Position{ 15.f, 0.f })
                        .set(game::Collider{ 10.f, 10.f })
                        .set(game::obstacle_mask)
                        .add<game::Obstacle>();
        auto shot = missile(0.f, 1);
        game::resolve({
            { ship, wall, 0.f, 0.f },
            { shot, wall, 0.f, 0.f },
        });
        CHECK(ship.get<game::Position>().x == -5.f);
        CHECK(ship.get<game::Velocity>().x == 0.f);
        CHECK(ship.get<game::Velocity>().y == -5.f);
        CHECK(ship.get<game::Health>().value == 10);
        CHECK(wall.is_alive());
        CHECK(!shot.is_alive());
    }

    {
        auto ship = player(10);
        auto tile = ecs::entity::create()
                        .set(game::Position{ 15.f, 0.f })
                        .set(game::Collider{ 10.f, 10.f })
                        .set(game::destructible_mask)
                        .set(game::Health{ 4 })
                        .add<game::DestructibleTile>();
        game::resolve({ { ship, tile, 0.f, 0.f } });
        CHECK(ship.get<game::Position>().x == -5.f);
        CHECK(tile.get<game::Health>().value == 4);

        auto shot = missile(0.f, 4);
        game::resolve({ { shot, tile, 0.f, 0.f } });
        CHECK(!tile.is_alive());
        CHECK(!shot.is_alive());
    }

    {
        auto ship = player(10);
        auto bonus = ecs::entity::create()
                         .set(game::Position{ 1.f, 1.f })
                         .set(game::Collider{ 4.f, 4.f })
                         .set(game::powerup_mask)
                         .add<game::Powerup>();
        auto another = ecs::entity::create()
                           .set(game::Position{ 2.f, 2.f })
                           .set(game::Collider{ 4.f, 4.f })
                           .set(game::powerup_mask)
                           .add<game::Powerup>();
        game::resolve({
            { ship, bonus, 0.f, 0.f },
            { ship, bonus, 0.f, 0.f },
            { ship, another, 0.f, 0.f },
        });
        CHECK(ship.get<game::Power>().level == 2);
        CHECK(effects.pickups == 2);
        CHECK(effects.level == 2);
        CHECK(effects.slot == 2);
        CHECK(!bonus.is_alive());
        CHECK(!another.is_alive());
    }

    {
        auto ship = player(10);
        auto connection = ecs::entity::create();
        ship.relate<net::OwnedBy>(connection);
        game::release_connection(connection);
        CHECK(!ship.is_alive());
        CHECK(connection.is_alive());
        CHECK(effects.left == 1);
        CHECK(effects.left_slot == 2);
    }

    {
        auto shot = missile(0.f, 1);
        shot.add<game::PendingDestroy>();
        auto foe = monster(5, false);
        game::resolve({ { shot, foe, 0.f, 0.f } });
        CHECK(foe.get<game::Health>().value == 5);
        CHECK(shot.is_alive());
    }

    {
        game::install_resolution();
        auto shot = missile(0.f, 2);
        auto foe = monster(5, false);
        ecs::resource<game::Contacts>().hits.push_back({ shot, foe, 0.f, 0.f });
        ecs::run_phase(EcsPostUpdate);
        CHECK(foe.get<game::Health>().value == 3);
        CHECK(!shot.is_alive());
        CHECK(ecs::resource<game::Contacts>().hits.empty());
    }

    ecs::fini();
    return 0;
}
