#include "session.h"
#include <charconv>
#include <client/gui/gui.h>
#include <format>
#include <raylib.h>
#include <shared/game.h>
#include <sinet/client.h>
#include <stdexcept>

namespace game {
namespace {
/** Notice lifetime, in seconds. */
struct Toast {
    float remaining = 4;
};
/** Resource: the column that stacks notices. */
struct Toasts {
    ecs::entity stack;
};

std::string describe(net::DisconnectReason reason, const Session &session) {
    switch (reason) {
    case net::DisconnectReason::capacity:
        return std::format("Match full: {} players maximum.", max_players);
    case net::DisconnectReason::schema_mismatch:
        return "The server runs a different game version.";
    case net::DisconnectReason::timeout:
        return session.connected ? "Connection lost: the server stopped answering."
                                 : std::format("No server answers at {}.", session.address);
    case net::DisconnectReason::remote:
        return "The server closed the connection.";
    case net::DisconnectReason::congestion:
        return "Connection too slow: the match was left.";
    case net::DisconnectReason::local:
        break;
    }
    return {};
}

std::string departure(const PlayerLeft &left) {
    const bool crashed = left.reason == std::uint8_t(net::DisconnectReason::timeout) ||
                         left.reason == std::uint8_t(net::DisconnectReason::congestion);
    return std::format(
        "P{} {}",
        left.slot + 1,
        crashed ? "lost connection" : "left the match"
    );
}
} // namespace

bool join(Session &session) {
    std::string_view address = session.address;
    std::uint16_t port = 4242;
    if (const auto colon = address.rfind(':'); colon != std::string_view::npos) {
        const auto digits = address.substr(colon + 1);
        const auto [end, error] =
            std::from_chars(digits.data(), digits.data() + digits.size(), port);
        if (error != std::errc{} || end != digits.data() + digits.size() || !port) {
            session.error = "Invalid port: use host:port, for example 127.0.0.1:4242.";
            return false;
        }
        address = address.substr(0, colon);
    }
    try {
        net::connect(address, port);
    } catch (const std::invalid_argument &) {
        session.error = std::format("Unknown address: {}.", address);
        return false;
    }
    TraceLog(LOG_INFO, "NET: Connecting to %s", session.address.c_str());
    session.error.clear();
    session.connecting = true;
    session.connected = false;
    return true;
}

void notify(std::string text) {
    ecs::entity::create()
        .set(gui::Label{ std::move(text) }, Toast{})
        .child_of(ecs::resource<Toasts>().stack);
}

void Sessions::import(const Session &session) {
    ecs::set_resource(session);
    ecs::set_resource(
        Toasts{ ecs::entity::create().set(
            gui::column(4).at(gui::Top, 0, 56).align(gui::Align::Center),
            gui::Bounds{}
        ) }
    );

    ecs::observe<net::OnConnect>().each([](ecs::entity, ecs::res<Session> session) {
        TraceLog(LOG_INFO, "NET: Connected to %s", session->address.c_str());
        session->connecting = false;
        session->connected = true;
    });
    ecs::observe<net::OnDisconnect>().each(
        [](ecs::entity, ecs::observer_event event, ecs::res<Session> session) {
            const auto reason = *event.trigger_data<net::DisconnectReason>();
            TraceLog(LOG_INFO, "NET: Disconnected (reason %d)", int(reason));
            session->error = describe(reason, *session);
            session->connecting = false;
            session->connected = false;
        }
    );
    ecs::observe<PlayerLeft>().each([](ecs::entity, ecs::observer_event event) {
        notify(departure(*event.trigger_data<PlayerLeft>()));
    });
    ecs::system("game.ExpireToasts")
        .phase(EcsOnUpdate)
        .each([](ecs::entity toast, Toast &life, ecs::res<const DeltaTime> dt) {
            life.remaining -= dt->value;
            if (life.remaining <= 0)
                toast.kill();
        });
}

} // namespace game
