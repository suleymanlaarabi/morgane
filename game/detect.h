#pragma once

#include "resolve.h"

#include <algorithm>
#include <cmath>
#include <vector>

// Fills Contacts after positions have moved. Missiles with PreviousPosition are
// swept, so a fast shot cannot cross a target in one tick. Pairs come from the
// collision masks. Entities that leave the playfield are listed in exited;
// resolution removes them without applying damage.

namespace game {

struct Playfield {
    float min_x = 0.f;
    float min_y = 0.f;
    float max_x = 1920.f;
    float max_y = 1080.f;
};

namespace detail {

inline constexpr uint8_t missile_layers = layer::player_missile | layer::enemy_missile;
inline constexpr uint8_t exit_layers = layer::monster | missile_layers;
inline constexpr float hit_time_epsilon = 1e-4f;

struct Box {
    float x;
    float y;
    float half_width;
    float half_height;
};

inline bool overlap(Box a, Box b, float &point_x, float &point_y) {
    float a_left = a.x - a.half_width;
    float a_right = a.x + a.half_width;
    float b_left = b.x - b.half_width;
    float b_right = b.x + b.half_width;
    if (a_right < b_left || b_right < a_left)
        return false;
    float a_bottom = a.y - a.half_height;
    float a_top = a.y + a.half_height;
    float b_bottom = b.y - b.half_height;
    float b_top = b.y + b.half_height;
    if (a_top < b_bottom || b_top < a_bottom)
        return false;
    float left = std::max(a_left, b_left);
    float right = std::min(a_right, b_right);
    float bottom = std::max(a_bottom, b_bottom);
    float top = std::min(a_top, b_top);
    point_x = (left + right) * 0.5f;
    point_y = (bottom + top) * 0.5f;
    return true;
}

inline bool sweep(Box from, Box to, Box target, float &time, float &point_x, float &point_y) {
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float reach_x = target.half_width + from.half_width;
    float reach_y = target.half_height + from.half_height;
    float min_x = target.x - reach_x;
    float max_x = target.x + reach_x;
    float min_y = target.y - reach_y;
    float max_y = target.y + reach_y;
    float enter = 0.f;
    float exit = 1.f;

    auto slab = [&](float origin, float delta, float min_value, float max_value) {
        constexpr float epsilon = 1e-6f;
        if (std::abs(delta) <= epsilon)
            return origin >= min_value && origin <= max_value;
        float inverse = 1.f / delta;
        float t1 = (min_value - origin) * inverse;
        float t2 = (max_value - origin) * inverse;
        if (t1 > t2)
            std::swap(t1, t2);
        enter = std::max(enter, t1);
        exit = std::min(exit, t2);
        return enter <= exit;
    };

    if (!slab(from.x, dx, min_x, max_x) || !slab(from.y, dy, min_y, max_y))
        return false;
    if (exit < 0.f || enter > 1.f)
        return false;

    time = std::max(enter, 0.f);
    float hit_x = from.x + dx * time;
    float hit_y = from.y + dy * time;
    point_x = std::clamp(hit_x, target.x - target.half_width, target.x + target.half_width);
    point_y = std::clamp(hit_y, target.y - target.half_height, target.y + target.half_height);
    return true;
}

inline bool outside(Box box, const Playfield &field) {
    float left = box.x - box.half_width;
    float right = box.x + box.half_width;
    float bottom = box.y - box.half_height;
    float top = box.y + box.half_height;
    return right < field.min_x || left > field.max_x || top < field.min_y || bottom > field.max_y;
}

} // namespace detail

inline void detect(Contacts &contacts, const Playfield &field) {
    contacts.hits.clear();
    contacts.exited.clear();

    struct Body {
        ecs::entity entity;
        detail::Box now;
        detail::Box before;
        bool swept = false;
        uint8_t belongs = 0;
        uint8_t hits = 0;
    };
    std::vector<Body> bodies;
    ecs::query().exclude<PendingDestroy>().each(
        [&](ecs::entity entity, const Position &position, const Collider &collider,
            const CollisionMask &mask, ecs::optional<const PreviousPosition> previous) {
            bool swept = previous && (mask.belongs & detail::missile_layers) != 0;
            detail::Box now{ position.x, position.y, collider.half_width, collider.half_height };
            detail::Box before = now;
            if (swept) {
                before.x = previous->x;
                before.y = previous->y;
            }
            bodies.push_back(Body{ entity, now, before, swept, mask.belongs, mask.hits });
        });

    struct SweptHit {
        std::size_t seeker;
        std::size_t target;
        float time;
        float x;
        float y;
    };
    std::vector<SweptHit> swept_hits;
    std::vector<float> earliest(bodies.size(), 2.f);

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Body &seeker = bodies[i];
        for (std::size_t j = 0; j < bodies.size(); ++j) {
            if (i == j || (seeker.hits & bodies[j].belongs) == 0)
                continue;
            float x = 0.f;
            float y = 0.f;
            if (!seeker.swept) {
                if (!detail::overlap(seeker.now, bodies[j].now, x, y))
                    continue;
                contacts.hits.push_back(Contact{ seeker.entity, bodies[j].entity, x, y });
                continue;
            }
            float time = 0.f;
            if (!detail::sweep(seeker.before, seeker.now, bodies[j].now, time, x, y))
                continue;
            swept_hits.push_back(SweptHit{ i, j, time, x, y });
            earliest[i] = std::min(earliest[i], time);
        }
    }

    for (const SweptHit &hit : swept_hits) {
        if (hit.time > earliest[hit.seeker] + detail::hit_time_epsilon)
            continue;
        contacts.hits.push_back(
            Contact{ bodies[hit.seeker].entity, bodies[hit.target].entity, hit.x, hit.y });
    }

    for (const Body &body : bodies) {
        if ((body.belongs & detail::exit_layers) == 0)
            continue;
        if (detail::outside(body.now, field))
            contacts.exited.push_back(body.entity);
    }
}

inline ecs_system_id_t install_detection(Playfield field = {}) {
    if (!ecs::has_resource<Playfield>())
        ecs::set_resource(field);
    if (!ecs::has_resource<Contacts>())
        ecs::set_resource(Contacts{});
    return ecs::system("game.DetectContacts")
        .phase(EcsPostUpdate)
        .immediate()
        .each([](ecs::res<Contacts> contacts, ecs::res<const Playfield> playfield) {
            detect(*contacts, *playfield);
        });
}

} // namespace game
