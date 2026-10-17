#include "runtime.h"
#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace net::detail {

std::uint32_t Runtime::scope(ecs_entity_t entity) {
    if (!entity)
        return 0;
    auto target = ecs::entity(entity);
    if (auto *scope = target.try_get<ScopeIdentity>())
        return scope->value;
    if (free_scopes_.empty())
        throw std::length_error("Network scope capacity exceeded");
    auto index = free_scopes_.back();
    free_scopes_.pop_back();
    auto id = (std::uint32_t(++scope_generations_[index]) << 16) | index;
    target.set(ScopeIdentity{id});
    if (auto *network = target.try_get<NetworkId>(); network && target.has<Replicated>())
        enqueue(relations(entity, network->value));
    return id;
}

void Runtime::scope_removed(ecs_observer_event_t &event) {
    if (server_ && !stopping_ && event.component == ecs::component<ScopeIdentity>().id()) {
        auto id = static_cast<const ScopeIdentity *>(event.trigger_data)->value;
        free_scopes_.push_back(entity_index(id));
    }
}

Message Runtime::relations(ecs_entity_t entity, std::uint32_t id,
                           const ecs_relation_event_t *transition) {
    auto target = [&](ecs_relation_id_t relation) {
        return transition && transition->relation == relation ? transition->new_target
                                                              : ecs_target_id(entity, relation);
    };
    auto owner = target(ecs::relation<OwnedBy>());
    auto lobby = target(ecs::relation<MemberOf>());
    auto game = target(ecs::relation<InInstance>());
    Message message{ .op = Op::relations, .entity = id, .revision = ++revision_, .reliable = true,
                     .size = 16 };
    Writer writer(message.value);
    writer.put(owner ? ecs::entity(owner).get<PeerId>().value : 0u);
    writer.put(scope(lobby));
    writer.put(scope(game));
    auto *identity = ecs::entity(entity).try_get<ScopeIdentity>();
    writer.put(identity ? identity->value : 0u);
    return message;
}

void Runtime::lifecycle(ecs_observer_event_t &event) {
    if (!server_ || stopping_ || event.component != ecs::component<Replicated>().id())
        return;
    auto entity = ecs::entity(event.entity);
    if (event.event == EcsOnAdd) {
        if (free_ids_.empty())
            throw std::length_error("Replicated entity capacity exceeded");
        auto index = free_ids_.back();
        free_ids_.pop_back();
        auto id = (std::uint32_t(++generations_[index]) << 16) | index;
        auto &state = entities_[index];
        state = { .entity = entity.id(), .id = id, .active = true };
        entity.set(NetworkId{ id });
        auto message = relations(entity.id(), id);
        message.op = Op::spawn;
        enqueue(message);
        for (auto &d : registry_->types) {
            if (d->flow != Flow::replication)
                continue;
            auto *value = ecs_try_get_cid(entity.id(), d->component);
            if (!value)
                continue;
            Message component{ .op = Op::component, .entity = id, .revision = ++revision_,
                               .type = d.get(), .reliable = true, .size = d->wire_size };
            d->encode(*d, value, { component.value.data(), component.size });
            enqueue(component);
        }
    } else if (event.event == EcsOnRemove) {
        auto id = entity.get<NetworkId>().value;
        auto &state = entities_[entity_index(id)];
        state.active = false;
        enqueue({ .op = Op::despawn, .entity = id, .reliable = true });
        free_ids_.push_back(entity_index(id));
    }
}

void Runtime::component_change(ecs_observer_event_t &event, Descriptor &d) {
    if (stopping_ || remote_despawn_ || event.component != d.component ||
        (server_ ? d.flow != Flow::replication : d.flow != Flow::input))
        return;
    // OnSet carries the new value before ECS copies it to component storage.
    // Data components are written with set(); OnAdd only carries empty tags.
    if (event.event == EcsOnAdd && !d.empty)
        return;
    auto *network = ecs::entity(event.entity).try_get<NetworkId>();
    // A deferred batch can add a tag before Replicated's observer runs.
    // Replicated's initial snapshot includes those additions.
    if (!network)
        return;
    auto id = network->value;
    if (server_ && !entities_[entity_index(id)].active)
        return;
    Message message{ .op = event.event == EcsOnRemove ? Op::remove : Op::component,
                     .entity = id, .revision = ++revision_, .type = &d,
                     .reliable = d.reliable || event.event == EcsOnRemove,
                     .size = event.event == EcsOnRemove ? std::uint16_t(0) : d.wire_size };
    if (message.op == Op::component) {
        d.encode(d, event.trigger_data, { message.value.data(), message.size });
    }
    enqueue(message);
}

void Runtime::event(ecs_observer_event_t &event, Descriptor &d) {
    if (stopping_ ||
        (server_ ? d.flow != Flow::server_event : d.flow != Flow::client_event))
        return;
    initialize();
    auto entity = ecs::entity(event.entity);
    auto *network = entity.try_get<NetworkId>();
    assert(network || entity.has<Connection>());
    Message message{ .op = Op::event, .entity = network ? network->value : 0,
                     .revision = server_ && !network ? entity.get<PeerId>().value : 0,
                     .type = &d, .reliable = d.reliable, .size = d.wire_size };
    assert(!message.size || event.trigger_data);
    d.encode(d, event.trigger_data, { message.value.data(), message.size });
    // Events may be triggered by worker systems; only this bounded enqueue is shared.
    enqueue(message);
}

void Runtime::relation_change(ecs_observer_event_t &event) {
    if (!server_ || stopping_)
        return;
    const auto &transition = *static_cast<const ecs_relation_event_t *>(event.trigger_data);
    if (transition.relation != ecs::relation<OwnedBy>() &&
        transition.relation != ecs::relation<MemberOf>() &&
        transition.relation != ecs::relation<InInstance>())
        return;
    auto entity = ecs::entity(event.entity);
    auto *id = entity.try_get<NetworkId>();
    if (id && entities_[entity_index(id->value)].active)
        enqueue(relations(entity.id(), id->value, &transition));
    // Membership changes invalidate only the connections concerned.
    for (auto &peer : peers_) {
        if (!peer || !peer->connected)
            continue;
        auto lobby = ecs::entity(peer->entity).target<MemberOf>().id();
        if (peer->entity == event.entity || lobby == event.entity)
            enqueue({ .op = Op::relations, .entity = 0, .revision = peer->id });
    }
}

bool Runtime::interested(const Peer &peer, const EntityState &entity) const {
    return (!entity.lobby || entity.lobby == peer.lobby) &&
           (!entity.game || entity.game == peer.game);
}

void Runtime::spawn(Peer &peer, const EntityState &entity) {
    Message message{ .op = Op::spawn, .entity = entity.id, .revision = entity.relation_revision,
                     .reliable = true, .size = 16 };
    Writer writer(message.value);
    writer.put(entity.owner);
    writer.put(entity.lobby);
    writer.put(entity.game);
    writer.put(entity.scope);
    if (!peer.outbox.push(message)) {
        disconnect(peer, DisconnectReason::congestion);
        return;
    }
    peer.visible[entity_index(entity.id)] = entity.id;
    for (auto &d : registry_->types) {
        if (d->flow != Flow::replication)
            continue;
        auto &state = value_state(entity.id, *d);
        if (!state.present)
            continue;
        Message component{ .op = Op::component, .entity = entity.id, .revision = state.revision,
                           .type = d.get(), .reliable = true, .size = d->wire_size };
        auto bytes = value_bytes(entity.id, *d);
        std::copy(bytes.begin(), bytes.end(), component.value.begin());
        if (!peer.outbox.push(component)) {
            disconnect(peer, DisconnectReason::congestion);
            return;
        }
    }
}

void Runtime::update_interest(Peer &peer, EntityState &entity) {
    auto &visible = peer.visible[entity_index(entity.id)];
    bool wanted = entity.active && interested(peer, entity);
    if (visible && (visible != entity.id || !wanted)) {
        if (!peer.outbox.push({ .op = Op::despawn, .entity = visible, .reliable = true })) {
            disconnect(peer, DisconnectReason::congestion);
            return;
        }
        visible = 0;
    }
    if (wanted && !visible)
        spawn(peer, entity);
}

void Runtime::update_scope(Peer &peer) {
    auto connection = ecs::entity(peer.entity);
    auto lobby = connection.target<MemberOf>().id();
    auto game = connection.target<InInstance>().id();
    if (!game && lobby)
        game = ecs::entity(lobby).target<InInstance>().id();
    peer.lobby = scope(lobby);
    peer.game = scope(game);
    for (auto &entity : entities_) {
        if (!entity.id)
            continue;
        update_interest(peer, entity);
        if (!peer.connected)
            break;
    }
}

void Runtime::fanout(const Message &message) {
    for (auto &peer : peers_) {
        if (!peer || !peer->connected)
            continue;
        if (message.op == Op::event && !message.entity && message.revision != peer->id)
            continue;
        if (message.entity && peer->visible[entity_index(message.entity)] != message.entity)
            continue;
        if (!peer->outbox.push(message))
            disconnect(*peer, DisconnectReason::congestion);
    }
}

void Runtime::collect() {
    if (commands_.empty())
        return;
    std::unique_lock lock(commands_mutex_);
    while (auto *queued = commands_.front()) {
        auto message = *queued;
        commands_.pop();
        lock.unlock();
        if (!server_) {
            auto &peer = *peers_[0];
            if (peer.connected && (!message.entity ||
                entities_[entity_index(message.entity)].owner == peer.id)) {
                if (!peer.outbox.push(message))
                    disconnect(peer, DisconnectReason::congestion);
            }
        } else if (message.op == Op::event) {
            fanout(message);
        } else if (message.op == Op::relations && !message.entity) {
            for (auto &peer : peers_)
                if (peer && peer->connected && peer->id == message.revision)
                    update_scope(*peer);
        } else {
            auto &entity = entities_[entity_index(message.entity)];
            if (message.op == Op::despawn) {
                fanout(message);
                for (auto &peer : peers_)
                    if (peer && peer->connected && peer->visible[entity_index(message.entity)] == message.entity)
                        peer->visible[entity_index(message.entity)] = 0;
                if (entity.id == message.entity)
                    for (auto &d : registry_->types)
                        value_state(entity.id, *d) = {};
            } else if (entity.id == message.entity) {
                if (message.op == Op::spawn || message.op == Op::relations) {
                    if (message.op == Op::spawn)
                        for (auto &d : registry_->types)
                            value_state(entity.id, *d) = {};
                    Reader reader({ message.value.data(), message.size });
                    auto owner = reader.get<std::uint32_t>();
                    if (entity.owner != owner)
                        for (auto &d : registry_->types)
                            if (d->flow == Flow::input)
                                value_state(entity.id, *d).revision = 0;
                    entity.owner = owner;
                    entity.lobby = reader.get<std::uint32_t>();
                    entity.game = reader.get<std::uint32_t>();
                    entity.scope = reader.get<std::uint32_t>();
                    entity.relation_revision = message.revision;
                    for (auto &peer : peers_) {
                        if (!peer || !peer->connected)
                            continue;
                        auto previous = peer->visible[entity_index(entity.id)];
                        update_interest(*peer, entity);
                        if (peer->connected && previous == entity.id && peer->visible[entity_index(entity.id)] == entity.id &&
                            !peer->outbox.push(message))
                            disconnect(*peer, DisconnectReason::congestion);
                    }
                } else {
                    auto &state = value_state(entity.id, *message.type);
                    state = { message.revision, message.op == Op::component };
                    if (state.present) {
                        auto bytes = value_bytes(entity.id, *message.type);
                        std::copy_n(message.value.begin(), message.size, bytes.begin());
                    }
                    fanout(message);
                }
            }
        }
        lock.lock();
    }
}

} // namespace net::detail
