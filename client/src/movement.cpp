#include "movement.h"
#include <algorithm>
#include <array>

namespace game {
namespace {
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

/** Advance monotonically, bound latency backlog, and stop after a bounded outage. */
double advance_cursor(double cursor, double latest, float dt, const ClientMovementConfig &config) {
    return std::clamp(
        cursor + dt / physics::fixed_step,
        latest - config.interpolation_ticks - config.max_backlog_ticks,
        latest + config.max_extrapolation_ticks
    );
}

void record_tick(std::uint64_t tick, ServerTimeline &timeline, const ClientMovementConfig &config) {
    if (!timeline.latest)
        timeline.cursor = double(tick) - config.interpolation_ticks;
    timeline.latest = std::max(timeline.latest, tick);
}

/** Trajectory ticks and the cursor share AuthoritativeMotion's clock (completed server ticks).
 * A trajectory is held at its origin until the cursor reaches the tick it was fired. */
Position2d trajectory_position(const Trajectory &trajectory, double cursor) {
    return trajectory_at(trajectory, std::max(cursor, double(trajectory.tick)));
}
} // namespace

void ClientMovement::import(const ClientMovementConfig &config) {
    ecs::import<Kinematics>();
    ecs::set_resource(config);
    ecs::set_resource(ServerTimeline{});
    // Another server, or a restarted one, counts ticks from scratch.
    ecs::observe<net::OnConnect>().each([](ecs::entity, ecs::res<ServerTimeline> timeline) {
        *timeline = {};
    });
    ecs::component<LocalPrediction>().with<physics::Body, Driven, ray::PresentationPosition2d>();
    ecs::component<net::Controlled>().with<LocalPrediction>();
    ecs::component<RemotePresentation>()
        .with<Position2d, ray::PresentationPosition2d, RemoteHistory>();
    ecs::component<Trajectory>().with<ray::PresentationPosition2d>();
    ecs::observe<ecs::OnSet>().each_for<AuthoritativeMotion>(
        [](ecs::entity,
           ecs::observer_event event,
           ecs::res<ServerTimeline> timeline,
           ecs::res<const ClientMovementConfig> config) {
            record_tick(event.trigger_data<AuthoritativeMotion>()->tick, *timeline, *config);
        }
    );
    ecs::observe<ecs::OnSet>().each_for<Trajectory>(
        [](ecs::entity,
           ecs::observer_event event,
           ecs::res<ServerTimeline> timeline,
           ecs::res<const ClientMovementConfig> config) {
            record_tick(event.trigger_data<Trajectory>()->tick, *timeline, *config);
        }
    );
    ecs::system("game.AdvanceServerTimeline")
        .phase(EcsPreUpdate)
        .each([](ecs::res<ServerTimeline> timeline,
                 ecs::res<const DeltaTime> dt,
                 ecs::res<const ClientMovementConfig> config) {
            if (timeline->latest)
                timeline->cursor =
                    advance_cursor(timeline->cursor, double(timeline->latest), dt->value, *config);
        });
    // Snapshots are recognised by their tick, so no per-snapshot tag moves entities between tables.
    ecs::system("game.ReceiveLocalMotion")
        .phase(EcsPreUpdate)
        .require<net::Controlled>()
        .each([](ecs::entity body,
                 LocalPrediction &prediction,
                 const AuthoritativeMotion &snapshot,
                 const ray::PresentationPosition2d &presentation,
                 const Position2d &position,
                 ecs::res<const ClientMovementConfig> config) {
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
        .require<LocalPrediction>()
        .exclude<net::Controlled>()
        .each([](ecs::entity body) {
            body.remove<LocalPrediction, physics::Body, Driven, Velocity2d>();
        });
    auto track = ecs::system("game.TrackRemoteMotion")
                     .phase(EcsPreUpdate)
                     .require<AuthoritativeMotion>()
                     .exclude<net::Controlled, RemotePresentation>()
                     .each([](ecs::entity body) { body.add<RemotePresentation>(); });
    // Reads the RemoteHistory that TrackRemoteMotion's RemotePresentation brings.
    ecs::system("game.ReceiveRemoteMotion")
        .phase(EcsPreUpdate)
        .after(track)
        .exclude<net::Controlled>()
        .each([](ecs::entity body,
                 RemoteHistory &history,
                 const AuthoritativeMotion &snapshot,
                 ecs::res<const ClientMovementConfig> config) {
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
        .require<RemotePresentation>()
        .exclude<net::Controlled>()
        .each([](ecs::entity body,
                 RemoteHistory &history,
                 ecs::res<const DeltaTime> dt,
                 ecs::res<const ClientMovementConfig> config) {
            const auto &latest = history.states[history.count - 1];
            history.cursor =
                advance_cursor(history.cursor, double(latest.tick), dt->value, *config);
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
    ecs::system("game.PresentTrajectories")
        .phase(EcsOnUpdate)
        .each([](const Trajectory &trajectory,
                 ray::PresentationPosition2d &presentation,
                 ecs::res<const ServerTimeline> timeline) {
            const auto position = trajectory_position(trajectory, timeline->cursor);
            presentation = { position.x, position.y };
        });
}
} // namespace game
