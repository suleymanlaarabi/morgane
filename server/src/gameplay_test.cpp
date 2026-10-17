#include "gameplay.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <set>
#include <vector>

using Catch::Approx;

namespace {
using game::Kind;

/** Waves would interfere with scripted encounters, so they are tested on their own. */
constexpr game::GameplayConfig calm{ .seed = 1, .wave_delay = 1'000'000 };
constexpr game::GameplayConfig waves{ .seed = 1 };

struct Gameplay {
    Gameplay() { start(calm); }
    ~Gameplay() { ecs::fini(); }

    static void start(const game::GameplayConfig &config) {
        ecs::init();
        ecs::import<game::ServerGameplay>(config);
    }
    static void restart(const game::GameplayConfig &config) {
        ecs::fini();
        start(config);
    }

    // Joins are batched like OnConnect's, so ServerPlayers' OnAdd mutations apply after the add.
    static ecs::entity join() {
        ecs::entity player;
        ecs::defer([&] { player = ecs::entity::create().add<game::ServerPlayer>(); });
        return player;
    }

    static std::set<unsigned> slots() {
        std::set<unsigned> taken;
        ecs::query().each([&](const game::PlayerSlot &slot) { taken.insert(slot.index); });
        return taken;
    }

    static void hold(ecs::entity player, game::PlayerInput input, unsigned ticks) {
        for (unsigned i = 0; i < ticks; ++i) {
            player.set(input);
            physics::advance(physics::fixed_step);
        }
    }

    static void step(std::uint64_t ticks = 1) {
        for (std::uint64_t i = 0; i < ticks; ++i)
            physics::advance(physics::fixed_step);
    }

    static std::uint64_t tick() { return ecs::resource<physics::Clock>().tick; }

    static std::vector<ecs::entity> of_kind(Kind kind) {
        std::vector<ecs::entity> found;
        ecs::query().each([&](ecs::entity body, const game::EntityKind &entity_kind) {
            if (entity_kind.value == std::uint8_t(kind))
                found.push_back(body);
        });
        return found;
    }

    // Steps while recording each body of `kind` once, in spawn order.
    std::vector<game::Trajectory> spawned(Kind kind, std::uint64_t ticks) {
        std::vector<game::Trajectory> found;
        for (std::uint64_t i = 0; i < ticks; ++i) {
            step();
            for (auto body : of_kind(kind))
                if (seen.insert(body.id()).second)
                    found.push_back(body.get<game::Trajectory>());
        }
        return found;
    }

    std::set<ecs_entity_t> seen;
};

constexpr float tolerance = 0.01f;
constexpr auto health_of = [](ecs::entity player) { return player.get<game::Health>().current; };
} // namespace

TEST_CASE_METHOD(Gameplay, "players take the lowest free slot up to max_players") {
    ecs::entity players[game::max_players];
    for (auto &player : players)
        player = join();
    CHECK(slots() == std::set<unsigned>{ 0, 1, 2, 3 });

    players[1].kill();
    CHECK(slots() == std::set<unsigned>{ 0, 2, 3 });
    const auto rejoined = join();
    CHECK(rejoined.get<game::PlayerSlot>().index == 1);

    const auto refused = join();
    CHECK_FALSE(refused.is_alive());
    CHECK(slots() == std::set<unsigned>{ 0, 1, 2, 3 });
}

TEST_CASE_METHOD(Gameplay, "players spawn with full health and a player collider") {
    auto player = join();
    const auto &health = player.get<game::Health>();
    CHECK(health.current == game::max_health);
    CHECK(health.max == game::max_health);
    CHECK(player.get<physics::Box>().half_size.x == game::player_half_size.x);
    CHECK(player.get<physics::Filter>().category == game::layer::player);
    CHECK(player.get<Position2d>().x == -game::play_half_width + 48);
}

TEST_CASE_METHOD(Gameplay, "players stay inside the playfield and stop at its edge") {
    const auto input = GENERATE(
        game::PlayerInput{ 1, -1, false },
        game::PlayerInput{ -1, 1, false },
        game::PlayerInput{ 1, 0, false },
        game::PlayerInput{ -1, 0, false },
        game::PlayerInput{ 0, 1, false },
        game::PlayerInput{ 0, -1, false }
    );
    const float reach_x = game::play_half_width - game::player_half_size.x;
    const float reach_y = game::play_half_height - game::player_half_size.y;
    auto player = join();
    for (int i = 0; i < 600; ++i) {
        hold(player, input, 1);
        const auto &position = player.get<Position2d>();
        REQUIRE(std::abs(position.x) <= reach_x + tolerance);
        REQUIRE(std::abs(position.y) <= reach_y + tolerance);
    }
    const auto &position = player.get<Position2d>();
    if (input.move_x != 0)
        CHECK(std::abs(position.x) == Approx(reach_x).margin(tolerance));
    if (input.move_y != 0)
        CHECK(std::abs(position.y) == Approx(reach_y).margin(tolerance));
    const auto &velocity = player.get<Velocity2d>();
    CHECK(velocity.x == Approx(0).margin(tolerance));
    CHECK(velocity.y == Approx(0).margin(tolerance));
}

TEST_CASE_METHOD(Gameplay, "eliminated players stop moving") {
    auto player = join();
    hold(player, { 1, 0, false }, 10);
    player.set(game::Health{ 0, game::max_health });
    CHECK(player.has<game::Eliminated>());

    const auto before = player.get<Position2d>();
    hold(player, { 1, 1, false }, 60);
    const auto &after = player.get<Position2d>();
    CHECK(after.x == Approx(before.x));
    CHECK(after.y == Approx(before.y));
}

TEST_CASE_METHOD(Gameplay, "held fire shoots once per fire_interval") {
    const auto interval = calm.fire_interval;
    const unsigned ticks = GENERATE(1u, 10u, 11u, 25u, 60u);
    auto player = join();
    hold(player, { 0, 0, true }, ticks);
    CHECK(of_kind(Kind::player_shot).size() == (ticks + interval - 1) / interval);
}

TEST_CASE_METHOD(Gameplay, "released or expired fire stops shooting") {
    auto player = join();
    hold(player, { 0, 0, false }, 60);
    CHECK(of_kind(Kind::player_shot).empty());

    // One fire input stays held until input_timeout (15 ticks), then expires.
    player.set(game::PlayerInput{ 0, 0, true });
    step(60);
    CHECK(of_kind(Kind::player_shot).size() == 2);
}

TEST_CASE_METHOD(Gameplay, "shots follow their trajectory and despawn outside the playfield") {
    auto player = join();
    const auto origin = player.get<Position2d>();
    hold(player, { 0, 0, true }, 1);
    const auto shots = of_kind(Kind::player_shot);
    REQUIRE(shots.size() == 1);
    auto shot = shots[0];
    const auto trajectory = shot.get<game::Trajectory>();
    // Spawned in the input phase of the first step, from the state at completed tick 0.
    CHECK(trajectory.tick == 0);
    CHECK(trajectory.x == Approx(origin.x + 18));
    CHECK(trajectory.y == Approx(origin.y));
    CHECK(trajectory.vx == Approx(calm.shot_speed));

    const float limit = game::play_half_width + calm.despawn_margin;
    float last_x = 0;
    for (std::uint64_t k = 1; shot.is_alive(); ++k) {
        REQUIRE(tick() == trajectory.tick + k);
        const auto &position = shot.get<Position2d>();
        const auto expected = game::trajectory_at(trajectory, double(tick()));
        REQUIRE(position.x == Approx(expected.x).margin(tolerance));
        REQUIRE(position.y == Approx(expected.y).margin(tolerance));
        REQUIRE(position.x <= limit);
        last_x = position.x;
        step();
    }
    CHECK(last_x + calm.shot_speed * physics::fixed_step > limit);
}

TEST_CASE_METHOD(Gameplay, "a shot destroys the enemy in front and itself") {
    auto player = join();
    const auto origin = player.get<Position2d>();
    auto enemy = game::spawn_expendable(Kind::enemy, { origin.x + 72, origin.y }, {}, tick());
    hold(player, { 0, 0, true }, 1);
    step(8);
    CHECK_FALSE(enemy.is_alive());
    CHECK(of_kind(Kind::player_shot).empty());
}

TEST_CASE_METHOD(Gameplay, "one shot destroys only one of two overlapping enemies") {
    auto player = join();
    const auto origin = player.get<Position2d>();
    // Both are reached at the same fraction of the same step, so one contact batch has both.
    game::spawn_expendable(Kind::enemy, { origin.x + 72, origin.y - 2 }, {}, tick());
    game::spawn_expendable(Kind::enemy, { origin.x + 72, origin.y + 2 }, {}, tick());
    hold(player, { 0, 0, true }, 1);
    step(8);
    CHECK(of_kind(Kind::enemy).size() == 1);
    CHECK(of_kind(Kind::player_shot).empty());
}

TEST_CASE_METHOD(Gameplay, "enemy shots remove one health point per invulnerability window") {
    auto player = join();
    const auto origin = player.get<Position2d>();
    SECTION("one shot") {
        game::spawn_expendable(
            Kind::enemy_shot,
            { origin.x + 30, origin.y },
            { -calm.enemy_shot_speed, 0 },
            tick()
        );
        step(10);
    }
    SECTION("two shots in the same tick") {
        game::spawn_expendable(Kind::enemy_shot, origin, {}, tick());
        game::spawn_expendable(Kind::enemy_shot, origin, {}, tick());
        step(1);
    }
    CHECK(health_of(player) == game::max_health - 1);
    CHECK(of_kind(Kind::enemy_shot).empty());
}

TEST_CASE_METHOD(Gameplay, "touching an enemy costs health and destroys the enemy") {
    auto player = join();
    const auto origin = player.get<Position2d>();
    auto enemy = game::spawn_expendable(Kind::enemy, { origin.x + 20, origin.y }, {}, tick());
    step(1);
    CHECK(health_of(player) == game::max_health - 1);
    CHECK_FALSE(enemy.is_alive());
}

TEST_CASE_METHOD(Gameplay, "player shots do not hit players") {
    auto shooter = join();
    auto target = join();
    auto shot = game::spawn_expendable(Kind::player_shot, target.get<Position2d>(), {}, tick());
    step(2);
    CHECK(shot.is_alive());
    CHECK(health_of(shooter) == game::max_health);
    CHECK(health_of(target) == game::max_health);
}

TEST_CASE_METHOD(Gameplay, "three hits eliminate a player, who then stops acting and colliding") {
    auto player = join();
    for (unsigned hit = 1; hit <= game::max_health; ++hit) {
        game::spawn_expendable(Kind::enemy_shot, player.get<Position2d>(), {}, tick());
        step(1);
        REQUIRE(health_of(player) == game::max_health - hit);
        step(calm.invulnerability);
    }
    REQUIRE(player.has<game::Eliminated>());
    CHECK(player.get<physics::Filter>().mask == 0);

    const auto before = player.get<Position2d>();
    hold(player, { 1, 1, true }, 30);
    const auto &after = player.get<Position2d>();
    CHECK(after.x == Approx(before.x));
    CHECK(after.y == Approx(before.y));
    CHECK(of_kind(Kind::player_shot).empty());

    auto shot = game::spawn_expendable(Kind::enemy_shot, after, {}, tick());
    auto enemy = game::spawn_expendable(Kind::enemy, after, {}, tick());
    step(5);
    CHECK(shot.is_alive());
    CHECK(enemy.is_alive());
    CHECK(health_of(player) == 0);
}

TEST_CASE_METHOD(Gameplay, "waves start once a player is alive and repeat") {
    restart(waves);
    step(waves.wave_delay * 3);
    CHECK(of_kind(Kind::enemy).empty());

    // Enemies and their shots would otherwise eliminate the player and stop the waves.
    auto player = join();
    player.set(game::Invulnerable{ UINT64_MAX });
    const auto joined = tick();
    // 24 units at 80 units/s is 0.3 s, rounded up to whole ticks.
    const std::uint64_t spacing = 18;
    const auto wave_length = (waves.wave_size - 1) * spacing;

    CHECK(spawned(Kind::enemy, waves.wave_delay).empty());
    const auto first = spawned(Kind::enemy, 1 + wave_length);
    REQUIRE(first.size() == waves.wave_size);
    CHECK(first.front().tick == joined + waves.wave_delay);
    std::vector<float> heights;
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(first[i].tick == first.front().tick + i * spacing);
        CHECK(first[i].x == game::play_half_width + waves.spawn_offset);
        CHECK(std::abs(first[i].y) <= game::play_half_height - 24);
        CHECK(first[i].vx == -waves.enemy_speed);
        heights.push_back(first[i].y);
    }
    CHECK(std::set<float>(heights.begin(), heights.end()).size() > 1);

    CHECK(spawned(Kind::enemy, waves.wave_interval - 1).empty());
    const auto second = spawned(Kind::enemy, 1 + wave_length);
    REQUIRE(second.size() == waves.wave_size);
    CHECK(second.front().tick == first.back().tick + waves.wave_interval);

    const auto replay = [&](const game::GameplayConfig &config) {
        restart(config);
        seen.clear();
        join();
        std::vector<float> replayed;
        for (const auto &enemy : spawned(Kind::enemy, config.wave_delay + 1 + wave_length))
            replayed.push_back(enemy.y);
        return replayed;
    };
    CHECK(replay(waves) == heights);
    auto reseeded = waves;
    reseeded.seed = 2;
    CHECK(replay(reseeded) != heights);
}

TEST_CASE_METHOD(Gameplay, "enemies aim at the nearest living player") {
    auto near = join();
    auto far = join();
    near.set(Position2d{ -200, -60 });
    far.set(Position2d{ -200, 160 });
    const Position2d origin{ 0, 0 };
    game::spawn_expendable(Kind::enemy, origin, {}, tick(), game::EnemyGun{ 0 });
    const auto shots = spawned(Kind::enemy_shot, 1);
    REQUIRE(shots.size() == 1);
    const auto &trajectory = shots[0];
    const float speed = std::hypot(trajectory.vx, trajectory.vy);
    CHECK(speed == Approx(calm.enemy_shot_speed));
    const auto alignment = [&](Position2d target) {
        const auto dx = target.x - origin.x, dy = target.y - origin.y;
        return (trajectory.vx * dx + trajectory.vy * dy) / (speed * std::hypot(dx, dy));
    };
    CHECK(alignment(near.get<Position2d>()) > 0.99f);
    CHECK(alignment(far.get<Position2d>()) < 0.99f);

    CHECK(spawned(Kind::enemy_shot, calm.enemy_fire_interval - 1).empty());
    CHECK(spawned(Kind::enemy_shot, 1).size() == 1);
}

TEST_CASE_METHOD(Gameplay, "enemies hold fire without targets or while off-screen") {
    auto enemy = game::spawn_expendable(Kind::enemy, { 0, 0 }, {}, tick(), game::EnemyGun{ 0 });
    step(60);
    CHECK(of_kind(Kind::enemy_shot).empty());

    join();
    enemy.set(Position2d{ game::play_half_width + 10, 0 });
    step(60);
    CHECK(of_kind(Kind::enemy_shot).empty());

    enemy.set(Position2d{ game::play_half_width - 10, 0 });
    step(1);
    CHECK(of_kind(Kind::enemy_shot).size() == 1);
}
