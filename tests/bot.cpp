// Headless test client: joins a server, plays, and prints what it observes, one event per line.
// Usage: r-type_bot address port seconds [--fire] [--vote]
//        r-type_bot --schema   prints the network schema table documented in docs/protocol.md
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <shared/game.h>
#include <siecs.h>
#include <sinet/client.h>
#include <string_view>

namespace {
struct Bot {
    double seconds = 0;
    bool fire = false;
    bool vote = false;
    double elapsed = 0;
    std::uint8_t phase = UINT8_MAX;
    std::uint8_t health = UINT8_MAX;
    unsigned destroyed = 0;
    unsigned kinds[3] = {};
    std::minstd_rand random{ std::random_device{}() };
    float move_x = 0, move_y = 0;
};

void say(const char *format, auto... values) {
    std::printf(format, values...);
    std::putchar('\n');
    std::fflush(stdout);
}
} // namespace

/** One line per network type, in slot order: slot, type id, schema hash, wire size, name. */
int print_schema() {
    ecs::init();
    game::register_network();
    auto &registry = net::detail::registry();
    registry.freeze();
    for (std::size_t i = 0; i < registry.types.size(); ++i) {
        const auto &type = *registry.slots[i];
        std::printf(
            "%zu 0x%016llx 0x%016llx %u %s\n",
            i,
            (unsigned long long)type.type_id,
            (unsigned long long)type.schema,
            unsigned(type.wire_size),
            type.name.c_str()
        );
    }
    ecs::fini();
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--schema")
        return print_schema();
    std::uint16_t port = 0;
    if (argc < 4 || std::from_chars(argv[2], argv[2] + std::string_view(argv[2]).size(), port).ec !=
                        std::errc{}) {
        std::fprintf(stderr, "usage: %s address port seconds [--fire] [--vote]\n", argv[0]);
        return 2;
    }
    Bot bot{ .seconds = std::atof(argv[3]) };
    for (int i = 4; i < argc; ++i) {
        bot.fire |= std::string_view(argv[i]) == "--fire";
        bot.vote |= std::string_view(argv[i]) == "--vote";
    }

    ecs::init({ .target_fps = 60 });
    ecs::import<net::Client>(
        net::ClientConfig{ .address = argv[1], .port = port, .limits = { .timeout_seconds = 5 } }
    );
    game::register_network();
    ecs::set_resource(bot);

    ecs::observe<net::OnConnect>().each([](ecs::entity) { say("connected"); });
    ecs::observe<net::OnDisconnect>().each([](ecs::entity, ecs::observer_event event) {
        const auto reason = *event.trigger_data<net::DisconnectReason>();
        say("disconnected %u", unsigned(reason));
        ecs::quit();
    });
    ecs::observe<game::PlayerLeft>().each([](ecs::entity, ecs::observer_event event) {
        const auto &left = *event.trigger_data<game::PlayerLeft>();
        say("left %u %u", unsigned(left.slot), unsigned(left.reason));
    });
    ecs::observe<ecs::OnSet>().each_for<game::EntityKind>(
        [](ecs::entity, ecs::observer_event event, ecs::res<Bot> bot) {
            if (const auto value = event.trigger_data<game::EntityKind>()->value;
                value < std::size(bot->kinds))
                ++bot->kinds[value];
        }
    );
    ecs::observe<game::Destroyed>().each([](ecs::entity, ecs::res<Bot> bot) { ++bot->destroyed; });

    ecs::system("bot.Clock")
        .phase(EcsOnUpdate)
        .each([](ecs::res<Bot> bot, ecs::res<const DeltaTime> dt) {
            bot->elapsed += dt->value;
            if (bot->elapsed >= bot->seconds) {
                say("done destroyed=%u player_shots=%u enemies=%u enemy_shots=%u",
                    bot->destroyed,
                    bot->kinds[0],
                    bot->kinds[1],
                    bot->kinds[2]);
                net::disconnect();
                ecs::quit();
            }
        });
    ecs::system("bot.WatchMatch")
        .phase(EcsOnUpdate)
        .each([](const game::Match &match, ecs::res<Bot> bot, ecs::res<const net::LocalPeer> peer) {
            if (match.phase == bot->phase)
                return;
            bot->phase = match.phase;
            say("phase %u players=%u wave=%u",
                unsigned(match.phase),
                unsigned(match.players),
                unsigned(match.wave));
            if (bot->vote && match.phase == std::uint8_t(game::Phase::over)) {
                const game::RestartVote vote{ true };
                ecs::trigger<game::RestartVote>(peer->connection, &vote);
            }
        });
    ecs::system("bot.Play")
        .phase(EcsPreUpdate)
        .require<net::Controlled>()
        .each([](ecs::entity player,
                 const game::PlayerSlot &slot,
                 const game::Health &health,
                 ecs::res<Bot> bot) {
            if (health.current != bot->health) {
                if (bot->health == UINT8_MAX)
                    say("spawned slot=%u", unsigned(slot.index));
                bot->health = health.current;
                say("health %u", unsigned(health.current));
            }
            // Wander, changing direction now and then.
            if (std::uniform_int_distribution(0, 30)(bot->random) == 0) {
                std::uniform_real_distribution<float> axis(-1, 1);
                bot->move_x = axis(bot->random);
                bot->move_y = axis(bot->random);
            }
            player.set(game::PlayerInput{ bot->move_x, bot->move_y, bot->fire });
        });
    ecs::run();
}
