#include "client/net/client.h"
#include "server/net/server.h"
#include <chrono>
#include <iostream>

struct Ping {};
struct Reply { reflected(uint32_t peer;) };
struct Scenario {
    unsigned replies = 0;
    bool wrong_recipient = false;
    std::chrono::steady_clock::time_point received{};
};

int main(int, char **argv) {
    bool server = std::string_view(argv[1]) == "server";
    auto port = static_cast<std::uint16_t>(std::stoi(argv[2]));
    ecs::init({.target_fps = 120});
    ecs::set_resource(Scenario{});
    net::Limits limits{.max_entities = 32, .timeout_seconds = 3};
    if (server)
        ecs::import<net::Server>(net::ServerConfig{.port = port, .max_clients = 2, .limits = limits});
    else
        ecs::import<net::Client>(net::ClientConfig{.port = port, .limits = limits});
    net::event<Ping>().reliable();
    net::event<Reply>().server_to_clients().reliable();
    if (server) {
        ecs::observe<Ping>().each([](ecs::entity connection) {
            auto &s = ecs::resource<Scenario>();
            ++s.replies;
            s.received = std::chrono::steady_clock::now();
            Reply reply{connection.get<net::PeerId>().value};
            ecs::trigger<Reply>(connection, &reply);
        });
    } else {
        ecs::observe<net::OnConnect>().each([](ecs::entity connection) {
            ecs::trigger<Ping>(connection);
        });
        ecs::observe<Reply>().each([](ecs::observer_event event) {
            auto &s = ecs::resource<Scenario>();
            s.wrong_recipient |= event.trigger_data<Reply>()->peer != event.target().get<net::PeerId>().value;
            ++s.replies;
            s.received = std::chrono::steady_clock::now();
        });
    }
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(6)) {
        ecs::progress();
        auto &s = ecs::resource<Scenario>();
        if (s.replies >= (server ? 2u : 1u) &&
            std::chrono::steady_clock::now() - s.received > std::chrono::seconds(1))
            break;
    }
    auto &s = ecs::resource<Scenario>();
    bool passed = !s.wrong_recipient && s.replies == (server ? 2u : 1u);
    std::cout << (server ? "server" : "client") << " replies=" << s.replies
              << " result=" << (passed ? "ok" : "failed") << '\n';
    ecs::fini();
    return passed ? 0 : 1;
}
