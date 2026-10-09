#include "client/net/client.h"
#include "server/net/server.h"
#include <chrono>
#include <iostream>
#include <string>

struct Position { reflected(float x; float y;) };
struct Health { reflected(int value;) };
struct PlayerInput { reflected(float move; bool shoot;) };
struct Shoot {};
struct Ready {};
struct Removed {};
struct Restored {};
struct RoomLeft {};
struct Respawned {};
struct Finished {};
struct Complete {};

struct Scenario {
    bool server = false;
    bool input_received = false;
    bool shot = false;
    bool complete = false;
    bool saw_player = false;
    bool schema_rejected = false;
    int stage = 0;
    ecs::entity connection;
    ecs::entity player;
    ecs::entity enemy;
    ecs::entity lobby;
    ecs::entity game;
    std::chrono::steady_clock::time_point finish{};
};

static void register_types(bool server, bool incompatible) {
    // Reverse registration order deliberately: the wire schema must remain identical.
    if (server) {
        net::replicate<Position>();
        net::replicate<Health>().reliable();
    } else {
        net::replicate<Health>().reliable();
        net::replicate<Position>();
    }
    if (incompatible)
        ecs::resource<net::detail::Registry>().types[0]->name = "incompatible.Health";
    net::input<PlayerInput>().owner_only();
    net::event<Shoot>().client_to_server().reliable();
    net::event<Ready>().client_to_server().reliable();
    net::event<Removed>().client_to_server().reliable();
    net::event<Restored>().client_to_server().reliable();
    net::event<RoomLeft>().client_to_server().reliable();
    net::event<Respawned>().client_to_server().reliable();
    net::event<Finished>().client_to_server().reliable();
    net::event<Complete>().server_to_clients().reliable();
}

static void observers() {
    ecs::observe<net::OnConnect>().each([](ecs::entity connection) {
        auto &s = ecs::resource<Scenario>();
        s.connection = connection;
        if (!s.server)
            return;
        connection.relate<net::MemberOf>(s.lobby);
        s.player = ecs::entity::create().set(Position{ 1, 2 }).set(Health{ 100 })
            .add<net::Replicated>().relate<net::OwnedBy>(connection).relate<net::InInstance>(s.game);
        s.enemy = ecs::entity::create().set(Position{ 9, 9 }).set(Health{ 100 })
            .add<net::Replicated>().relate<net::InInstance>(s.game);
    });
    if (!ecs::resource<Scenario>().server) {
        ecs::observe<Complete>().each([](ecs::entity) {
            auto &s = ecs::resource<Scenario>();
            s.complete = true;
            s.finish = std::chrono::steady_clock::now();
        });
        ecs::observe<net::OnDisconnect>().each([](ecs::observer_event event) {
            auto &s = ecs::resource<Scenario>();
            s.schema_rejected = *event.trigger_data<net::DisconnectReason>() == net::DisconnectReason::schema_mismatch;
        });
        return;
    }
    ecs::observe<Shoot>().each([](ecs::entity) {
        ecs::resource<Scenario>().shot = true;
    });
    ecs::observe<Ready>().each([](ecs::entity player) {
        ecs::resource<Scenario>().stage = 1;
        player.remove<Health>();
    });
    ecs::observe<Removed>().each([](ecs::entity player) { player.set(Health{ 50 }); });
    ecs::observe<Restored>().each([](ecs::entity) {
        auto &s = ecs::resource<Scenario>();
        s.stage = 2;
        s.connection.unrelate<net::MemberOf>();
    });
    ecs::observe<RoomLeft>().each([](ecs::entity connection) {
        auto &s = ecs::resource<Scenario>();
        connection.relate<net::MemberOf>(s.lobby);
    });
    ecs::observe<Respawned>().each([](ecs::entity player) {
        auto &s = ecs::resource<Scenario>();
        player.kill();
        s.player = ecs::entity::create().set(Position{22, 33}).add<net::Replicated>()
            .relate<net::OwnedBy>(s.connection).relate<net::InInstance>(s.game);
    });
    ecs::observe<Finished>().each([](ecs::entity connection) {
        auto &s = ecs::resource<Scenario>();
        s.complete = s.input_received && s.shot && !s.enemy.try_get<PlayerInput>();
        s.finish = std::chrono::steady_clock::now();
        ecs::trigger<Complete>(connection);
    });

}

static void server_system() {
    ecs::system("CheckInputs").each([](const PlayerInput &input, ecs::res<Scenario> scenario) {
        if (input.move == 7 && input.shoot)
            scenario->input_received = true;
        auto &s = *scenario;
        if (s.input_received && s.shot && s.stage == 0 && s.player.get<Health>().value == 100) {
            s.player.set(Health{75});
            s.player.set(Position{10, 2});
            s.player.set(Position{11, 2});
        }
    });
}

static void client_system() {
    ecs::system("ClientScenario").each([](ecs::entity entity, const net::Replicated &, const Position &position, ecs::res<Scenario> scenario) {
        auto &s = *scenario;
        auto owner = entity.target<net::OwnedBy>();
        if (!owner || owner.id() != s.connection.id())
            return;
        auto *health = entity.try_get<Health>();
        s.player = entity;
        s.saw_player = true;
        if (s.stage == 0) {
            entity.set(PlayerInput{ 7, true });
            if (!s.shot) {
                ecs::trigger<Shoot>(entity);
                s.shot = true;
            }
            if (health && health->value == 75 && position.x == 11) {
                ecs::trigger<Ready>(entity);
                s.stage = 1;
            }
        } else if (s.stage == 1 && !health) {
            ecs::trigger<Removed>(entity);
            s.stage = 2;
        } else if (s.stage == 2 && health && health->value == 50) {
            ecs::trigger<Restored>(entity);
            s.stage = 3;
        } else if (s.stage == 4 && health && health->value == 50 && position.x == 11) {
            ecs::trigger<Respawned>(entity);
            s.stage = 5;
        } else if (s.stage == 5 && !health && position.x == 22) {
            ecs::trigger<Finished>(entity);
            s.stage = 6;
        }
    });
    ecs::system("ClientLifecycle").immediate().each([](ecs::res<Scenario> scenario) {
        auto &s = *scenario;
        if (!s.saw_player || s.player.is_alive())
            return;
        if (s.stage == 3) {
            ecs::trigger<RoomLeft>(s.connection);
            s.stage = 4;
        }
    });
}

int main(int argc, char **argv) {
    bool server = std::string(argv[1]) == "server";
    auto port = static_cast<std::uint16_t>(std::stoi(argv[2]));
    bool incompatible = argc == 4;
    ecs::init({ .target_fps = 120, .worker_threads = 2 });
    ecs::set_resource(Scenario{ .server = server });
    net::Limits limits{ .max_entities = 32, .tick_rate = 30, .timeout_seconds = 3 };
    if (server)
        ecs::import<net::Server>(net::ServerConfig{ .port = port, .max_clients = 4, .limits = limits });
    else
        ecs::import<net::Client>(net::ClientConfig{ .address = "127.0.0.1", .port = port, .limits = limits });
    register_types(server, incompatible);
    observers();
    if (server) {
        auto &s = ecs::resource<Scenario>();
        s.game = ecs::entity::create();
        s.lobby = ecs::entity::create().relate<net::InInstance>(s.game);
        server_system();
    } else {
        client_system();
    }
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(8)) {
        ecs::progress();
        auto &s = ecs::resource<Scenario>();
        if (incompatible && s.schema_rejected)
            break;
        if (s.finish != std::chrono::steady_clock::time_point{} &&
            std::chrono::steady_clock::now() - s.finish > std::chrono::milliseconds(server ? 400 : 150))
            break;
    }
    auto &s = ecs::resource<Scenario>();
    bool passed = incompatible ? s.schema_rejected : s.complete;
    std::cout << (server ? "server" : "client") << " stage=" << s.stage
              << " connection=" << s.connection.id() << " player=" << s.player.id() << " input=" << s.input_received << " shot=" << s.shot
              << " result=" << (passed ? "ok" : "failed") << '\n';
    ecs::fini();
    return passed ? 0 : 1;
}
