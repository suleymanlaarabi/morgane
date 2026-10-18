#pragma once

#include "components.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Applies one tick of collision results. Detection fills Contacts::hits;
// this step does not look for overlaps. Each pair is applied once, then
// the queued effects (damage, pickup, separation, drop, death) run together.

namespace game {

inline constexpr float powerup_half = 8.f;

struct Contact {
    ecs::entity seeker;
    ecs::entity target;
    float x = 0;
    float y = 0;
};

struct Contacts {
    std::vector<Contact> hits;
};

namespace detail {

inline bool on_layer(ecs::entity entity, uint8_t layer) {
    auto *mask = entity.try_get<CollisionMask>();
    return mask && (mask->belongs & layer) != 0;
}

inline int dealt_by(ecs::entity attacker) {
    if (auto *damage = attacker.try_get<Damage>())
        return damage->value;
    return 1;
}

inline void separate(ecs::entity player, ecs::entity solid) {
    auto *player_position = player.try_get<Position>();
    auto *player_box = player.try_get<Collider>();
    auto *solid_position = solid.try_get<Position>();
    auto *solid_box = solid.try_get<Collider>();
    if (!player_position || !player_box || !solid_position || !solid_box)
        return;

    float dx = player_position->x - solid_position->x;
    float dy = player_position->y - solid_position->y;
    float overlap_x = player_box->half_width + solid_box->half_width - std::abs(dx);
    float overlap_y = player_box->half_height + solid_box->half_height - std::abs(dy);
    if (overlap_x <= 0.f || overlap_y <= 0.f)
        return;

    Position next = *player_position;
    Velocity velocity{};
    bool has_velocity = player.try_get<Velocity>() != nullptr;
    if (has_velocity)
        velocity = *player.try_get<Velocity>();

    if (overlap_x < overlap_y) {
        next.x += (dx < 0.f ? -overlap_x : overlap_x);
        velocity.x = 0.f;
    } else {
        next.y += (dy < 0.f ? -overlap_y : overlap_y);
        velocity.y = 0.f;
    }
    player.set(next);
    if (has_velocity)
        player.set(velocity);
}

struct Drop {
    float x;
    float y;
    ecs::entity instance;
};

} // namespace detail

inline void resolve(std::span<const Contact> contacts);

inline void resolve(std::initializer_list<Contact> contacts) {
    resolve(std::span<const Contact>(contacts.begin(), contacts.size()));
}

inline void resolve(std::span<const Contact> contacts) {
    struct PairKey {
        ecs_entity_t first;
        ecs_entity_t second;
        bool operator==(const PairKey &) const = default;
    };
    struct PairHash {
        std::size_t operator()(PairKey key) const noexcept {
            return std::hash<ecs_entity_t>{}(key.first) ^
                   (std::hash<ecs_entity_t>{}(key.second) << 1);
        }
    };
    std::unordered_set<PairKey, PairHash> seen_pairs;
    std::unordered_set<ecs_entity_t> doomed;
    std::unordered_map<ecs_entity_t, int> damage;
    std::vector<ecs::entity> grants;
    std::vector<std::pair<ecs::entity, ecs::entity>> pushes;
    seen_pairs.reserve(contacts.size());

    auto doom = [&](ecs::entity entity) { doomed.insert(entity.id()); };
    auto hurt = [&](ecs::entity victim, ecs::entity attacker) {
        damage[victim.id()] += detail::dealt_by(attacker);
    };

    for (const Contact &contact : contacts) {
        if (contact.seeker.id() == contact.target.id())
            continue;
        if (!contact.seeker.is_alive() || !contact.target.is_alive())
            continue;
        if (contact.seeker.has<PendingDestroy>() || contact.target.has<PendingDestroy>())
            continue;
        if (doomed.contains(contact.seeker.id()) || doomed.contains(contact.target.id()))
            continue;

        ecs_entity_t first = std::min(contact.seeker.id(), contact.target.id());
        ecs_entity_t second = std::max(contact.seeker.id(), contact.target.id());
        if (!seen_pairs.insert(PairKey{ first, second }).second)
            continue;

        ecs::entity player;
        ecs::entity missile;
        ecs::entity monster;
        ecs::entity obstacle;
        ecs::entity tile;
        ecs::entity powerup;
        for (ecs::entity entity : { contact.seeker, contact.target }) {
            if (detail::on_layer(entity, layer::player))
                player = entity;
            if (detail::on_layer(entity, layer::player_missile) ||
                detail::on_layer(entity, layer::enemy_missile))
                missile = entity;
            if (detail::on_layer(entity, layer::monster))
                monster = entity;
            if (detail::on_layer(entity, layer::obstacle))
                obstacle = entity;
            if (detail::on_layer(entity, layer::destructible))
                tile = entity;
            if (detail::on_layer(entity, layer::powerup))
                powerup = entity;
        }

        bool player_missile = missile && detail::on_layer(missile, layer::player_missile);
        if (player && powerup) {
            grants.push_back(player);
            doom(powerup);
        } else if (missile && obstacle) {
            doom(missile);
        } else if (player && obstacle) {
            pushes.emplace_back(player, obstacle);
        } else if (player_missile && tile) {
            hurt(tile, missile);
            doom(missile);
        } else if (player && tile) {
            pushes.emplace_back(player, tile);
        } else if (player_missile && monster) {
            hurt(monster, missile);
            doom(missile);
        } else if (missile && player && detail::on_layer(missile, layer::enemy_missile)) {
            hurt(player, missile);
            doom(missile);
        } else if (player && monster) {
            hurt(player, monster);
        }
    }

    std::vector<detail::Drop> drops;
    for (const auto &[id, amount] : damage) {
        ecs::entity entity = ecs::entity::from(id);
        if (!entity.is_alive())
            continue;
        if (auto *health = entity.try_get<Health>()) {
            int left = health->value - amount;
            entity.set(Health{ left });
            if (left > 0)
                continue;
        }
        doomed.insert(id);
        if (entity.has<Monster>() && entity.has<DropsPowerup>()) {
            auto *position = entity.try_get<Position>();
            if (!position)
                continue;
            detail::Drop drop{ position->x, position->y, {} };
            if (entity.has_relation<net::InInstance>())
                drop.instance = entity.target<net::InInstance>();
            drops.push_back(drop);
        }
    }

    for (ecs::entity player : grants) {
        if (!player.is_alive() || doomed.contains(player.id()))
            continue;
        int level = 1;
        if (auto *power = player.try_get<Power>())
            level = power->level + 1;
        player.set(Power{ level });
    }

    for (const auto &[player, solid] : pushes) {
        if (!player.is_alive() || doomed.contains(player.id()) || !solid.is_alive())
            continue;
        detail::separate(player, solid);
    }

    for (const detail::Drop &drop : drops) {
        auto bonus = ecs::entity::create()
                         .set(Position{ drop.x, drop.y })
                         .set(Collider{ powerup_half, powerup_half })
                         .set(powerup_mask)
                         .add<Powerup, net::Replicated>();
        if (drop.instance.id() != 0)
            bonus.relate<net::InInstance>(drop.instance);
    }

    for (ecs_entity_t id : doomed) {
        ecs::entity entity = ecs::entity::from(id);
        if (!entity.is_alive())
            continue;
        if (entity.has<Player>()) {
            uint8_t slot = 0;
            if (auto *player_slot = entity.try_get<PlayerSlot>())
                slot = player_slot->index;
            PlayerKilled killed{ slot };
            ecs::trigger<PlayerKilled>(entity, &killed);
        }
        entity.add<PendingDestroy>();
        entity.kill();
    }
}

inline ecs_system_id_t install_resolution() {
    if (!ecs::has_resource<Contacts>())
        ecs::set_resource(Contacts{});
    return ecs::system("game.ResolveContacts")
        .phase(EcsPostUpdate)
        .immediate()
        .each([](ecs::res<Contacts> contacts) {
            resolve(contacts->hits);
            contacts->hits.clear();
        });
}

} // namespace game
