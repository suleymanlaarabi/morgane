#include <game/detect.h>

#include <cstdlib>
#include <iostream>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            std::cerr << "Check failed at line " << __LINE__ << ": " << #expression << '\n';      \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

static game::Playfield arena{ .min_x = 0.f, .min_y = 0.f, .max_x = 100.f, .max_y = 100.f };

static ecs::entity ship(float x, float y) {
    return ecs::entity::create()
        .set(game::Position{ x, y })
        .set(game::Collider{ 10.f, 10.f })
        .set(game::player_mask)
        .set(game::Health{ 10 })
        .set(game::PlayerSlot{ 1 })
        .add<game::Player>();
}

static ecs::entity foe(float x, float y, int health = 5) {
    return ecs::entity::create()
        .set(game::Position{ x, y })
        .set(game::Collider{ 2.f, 2.f })
        .set(game::monster_mask)
        .set(game::Health{ health })
        .set(game::Damage{ 4 })
        .add<game::Monster>();
}

static ecs::entity shot(float from_x, float to_x, float y = 0.f) {
    return ecs::entity::create()
        .set(game::Position{ to_x, y })
        .set(game::PreviousPosition{ from_x, y })
        .set(game::Collider{ 1.f, 1.f })
        .set(game::player_missile_mask)
        .set(game::Damage{ 3 })
        .add<game::PlayerMissile>();
}

static bool seeks(const game::Contacts &contacts, ecs::entity seeker, ecs::entity target) {
    for (const game::Contact &hit : contacts.hits)
        if (hit.seeker.id() == seeker.id() && hit.target.id() == target.id())
            return true;
    return false;
}

int main() {
    ecs::init({ .worker_threads = 0 });

    {
        auto player = ship(0.f, 0.f);
        auto monster = foe(8.f, 0.f);
        game::Contacts contacts;
        game::detect(contacts, arena);
        CHECK(contacts.hits.size() == 1);
        CHECK(seeks(contacts, player, monster));
        CHECK(contacts.exited.empty());
        player.kill();
        monster.kill();
    }

    {
        auto player = ship(0.f, 0.f);
        auto monster = foe(40.f, 0.f);
        game::Contacts contacts;
        game::detect(contacts, arena);
        CHECK(contacts.hits.empty());
        player.kill();
        monster.kill();
    }

    {
        auto player = ship(0.f, 0.f);
        auto monster = foe(8.f, 0.f);
        monster.add<game::PendingDestroy>();
        game::Contacts contacts;
        game::detect(contacts, arena);
        CHECK(contacts.hits.empty());
        player.kill();
        monster.kill();
    }

    {
        auto missile = shot(-20.f, 20.f);
        auto monster = foe(0.f, 0.f);
        game::Contacts contacts;
        game::detect(contacts, arena);
        CHECK(contacts.hits.size() == 1);
        CHECK(seeks(contacts, missile, monster));
        missile.kill();
        monster.kill();
    }

    {
        auto missile = shot(-20.f, 80.f);
        auto first = foe(0.f, 0.f);
        auto second = foe(30.f, 0.f);
        game::Contacts contacts;
        game::detect(contacts, arena);
        CHECK(contacts.hits.size() == 1);
        CHECK(seeks(contacts, missile, first));
        CHECK(!seeks(contacts, missile, second));
        missile.kill();
        first.kill();
        second.kill();
    }

    {
        auto player = ship(-50.f, 0.f);
        auto monster = foe(150.f, 0.f);
        game::Contacts contacts;
        game::detect(contacts, arena);
        CHECK(contacts.hits.empty());
        CHECK(contacts.exited.size() == 1);
        CHECK(contacts.exited[0].id() == monster.id());
        game::resolve(contacts.hits, contacts.exited);
        CHECK(!monster.is_alive());
        CHECK(player.is_alive());
        CHECK(player.get<game::Health>().value == 10);
        player.kill();
    }

    {
        auto detection = game::install_detection(arena);
        game::install_resolution(detection);
        auto player = ship(0.f, 0.f);
        auto monster = foe(8.f, 0.f);
        ecs::run_phase(EcsPostUpdate);
        CHECK(player.get<game::Health>().value == 6);
        CHECK(monster.get<game::Health>().value == 5);
        CHECK(ecs::resource<game::Contacts>().hits.empty());
        player.kill();
        monster.kill();
    }

    ecs::fini();
    return 0;
}
