#include "movement.h"
#include <algorithm>
#include <array>

namespace game {
namespace {
struct SnapshotChanged {};
struct LocalPrediction {
    std::uint64_t tick = 0;
    std::uint32_t epoch = 0;
    float offset_x = 0, offset_y = 0;
};
struct RemoteHistory {
    std::array<AuthoritativeMotion, 8> states{};
    unsigned count = 0;
    double cursor = 0;
};
struct RemotePresentation {};
} // namespace

void ClientMovement::import(const ClientMovementConfig &config) {
    ecs::import<Kinematics>();
    ecs::set_resource(config);
    ecs::component<LocalPrediction>().with<physics::Body, Driven, ray::PresentationPosition2d>();
    ecs::component<net::Controlled>().with<LocalPrediction>();
    ecs::component<RemotePresentation>().with<Position2d, ray::PresentationPosition2d, RemoteHistory>();
    ecs::observe<ecs::OnSet>().each_for<AuthoritativeMotion>([](ecs::entity body) {
        body.add<SnapshotChanged>();
    });
    ecs::system("game.ReceiveLocalMotion")
        .phase(EcsPreUpdate)
        .immediate()
        .require<SnapshotChanged, net::Controlled>()
        .each([](ecs::entity body,
                 LocalPrediction &prediction,
                 const AuthoritativeMotion &snapshot,
                 const ray::PresentationPosition2d &presentation,
                 const Position2d &position,
                 ecs::res<const ClientMovementConfig> config) {
            body.remove<SnapshotChanged>();
            if (snapshot.tick <= prediction.tick)
                return;
            const auto error = std::hypot(position.x - snapshot.x, position.y - snapshot.y);
            const bool snap = prediction.tick == 0 || prediction.epoch != snapshot.epoch ||
                              error > config->snap_distance;
            prediction.offset_x = snap ? 0 : presentation.x - snapshot.x;
            prediction.offset_y = snap ? 0 : presentation.y - snapshot.y;
            if (prediction.tick == 0)
                body.set(player_filter);
            prediction.tick = snapshot.tick;
            prediction.epoch = snapshot.epoch;
            body.set(Position2d{ snapshot.x, snapshot.y }, Velocity2d{ snapshot.vx, snapshot.vy });
        });
    ecs::system("game.ReleasePrediction")
        .phase(EcsPreUpdate)
        .immediate()
        .require<LocalPrediction>()
        .exclude<net::Controlled>()
        .each([](ecs::entity body) {
            body.remove<LocalPrediction, physics::Body, Driven, Velocity2d>();
        });
    auto track = ecs::system("game.TrackRemoteMotion")
                     .phase(EcsPreUpdate)
                     .immediate()
                     .require<SnapshotChanged, AuthoritativeMotion>()
                     .exclude<net::Controlled, RemotePresentation>()
                     .each([](ecs::entity body) {
                         body.add<RemotePresentation>();
                     });
    ecs::system("game.ReceiveRemoteMotion")
        .phase(EcsPreUpdate)
        .immediate()
        .after(track)
        .require<SnapshotChanged>()
        .exclude<net::Controlled>()
        .each([](ecs::entity body,
                 RemoteHistory &history,
                 const AuthoritativeMotion &snapshot,
                 ecs::res<const ClientMovementConfig> config) {
            body.remove<SnapshotChanged>();
            if (history.count && snapshot.tick <= history.states[history.count - 1].tick)
                return;
            if (!history.count || history.states[history.count - 1].epoch != snapshot.epoch) {
                history.count = 0;
                history.cursor = double(snapshot.tick) - config->interpolation_ticks;
                body.set(ray::PresentationPosition2d{ snapshot.x, snapshot.y });
            }
            if (history.count == history.states.size()) {
                std::move(history.states.begin() + 1, history.states.end(), history.states.begin());
                --history.count;
            }
            history.states[history.count++] = snapshot;
            body.set(Position2d{ snapshot.x, snapshot.y });
        });
    ecs::system("game.PresentLocalMotion")
        .phase(EcsOnUpdate)
        .immediate()
        .require<net::Controlled>()
        .each([](ecs::entity body,
                 LocalPrediction &prediction,
                 const Position2d &position,
                 ecs::res<const DeltaTime> dt,
                 ecs::res<const ClientMovementConfig> config) {
            const float decay = std::exp(-dt->value / config->correction_seconds);
            prediction.offset_x *= decay;
            prediction.offset_y *= decay;
            body.set(
                ray::PresentationPosition2d{ position.x + prediction.offset_x,
                                        position.y + prediction.offset_y }
            );
        });
    ecs::system("game.InterpolateRemoteMotion")
        .phase(EcsOnUpdate)
        .immediate()
        .require<RemotePresentation>()
        .exclude<net::Controlled>()
        .each([](ecs::entity body,
                 RemoteHistory &history,
                 ecs::res<const DeltaTime> dt,
                 ecs::res<const ClientMovementConfig> config) {
            const auto &latest = history.states[history.count - 1];
            // Advance monotonically, bound latency backlog, and stop after a bounded outage.
            history.cursor = std::clamp(
                history.cursor + dt->value / physics::fixed_step,
                double(latest.tick) - config->interpolation_ticks - config->max_backlog_ticks,
                double(latest.tick) + config->max_extrapolation_ticks
            );
            const AuthoritativeMotion *a = &history.states[0], *b = a;
            for (unsigned i = 1; i < history.count; ++i) {
                b = &history.states[i];
                if (history.cursor <= double(b->tick))
                    break;
                a = b;
            }
            float x, y;
            if (history.cursor > double(latest.tick)) {
                const float extra =
                    float(history.cursor - double(latest.tick)) * physics::fixed_step;
                x = latest.x + latest.vx * extra;
                y = latest.y + latest.vy * extra;
            } else {
                const float t =
                    a->tick == b->tick
                        ? 0
                        : std::clamp(
                              float((history.cursor - double(a->tick)) / double(b->tick - a->tick)),
                              0.0f,
                              1.0f
                          );
                x = std::lerp(a->x, b->x, t);
                y = std::lerp(a->y, b->y, t);
            }
            body.set(ray::PresentationPosition2d{ x, y });
        });
}
} // namespace game
