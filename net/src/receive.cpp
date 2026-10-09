#include "runtime.h"
#include <algorithm>

namespace net::detail {

void Runtime::receive() {
    if (stopping_)
        return;
    initialize();
    collect();
    if (!transport_->receive.reader())
        return;
    auto now = Clock::now();
    while (auto *packet = transport_->receive.reader()) {
        Header header;
        if (!read_header(packet->bytes(), header) ||
            !valid_frames(packet->bytes().subspan(header_size))) {
            transport_->receive.consume();
            continue;
        }
        auto found = endpoints_.find(packet->endpoint.key());
        if (found == endpoints_.end()) {
            if (server_ && !header.flags) {
                Reader reader(packet->bytes().subspan(header_size));
                auto op = reader.get<std::uint8_t>();
                auto size = reader.get<std::uint16_t>();
                auto bytes = reader.bytes(size);
                if (op == std::uint8_t(Op::hello) && reader.done())
                    hello(*packet, header, bytes);
            }
            transport_->receive.consume();
            continue;
        }
        auto &peer = *peers_[found->second];
        if (header.session != peer.session || peer.closed) {
            transport_->receive.consume();
            continue;
        }
        peer.received = now;
        peer.stats.received_bytes += packet->size;
        ++peer.stats.received_packets;
        auto &channel = peer.channel;
        channel.acknowledge(header, now);
        if (header.flags & reliable_flag) {
            auto reliable = header.reliable_sequence;
            if (reliable == channel.expected_reliable || newer(reliable, channel.expected_reliable)) {
                if (reliable - channel.expected_reliable >= reliable_window) {
                    ++peer.stats.dropped_packets;
                    transport_->receive.consume();
                    continue;
                }
                auto &buffered = channel.buffered[reliable & (reliable_window - 1)];
                if (!buffered.reliable_sequence) {
                    buffered.packet.copy_from(*packet);
                    buffered.reliable_sequence = reliable;
                }
            }
            channel.receive.accept(header.sequence);
            channel.ack_pending = true;
            for (;;) {
                auto &buffered = channel.buffered[channel.expected_reliable & (reliable_window - 1)];
                if (buffered.reliable_sequence != channel.expected_reliable)
                    break;
                handle(peer, buffered.packet.bytes().subspan(header_size));
                buffered.reliable_sequence = 0;
                ++channel.expected_reliable;
                if (peer.closed)
                    break;
            }
        } else if (!channel.receive.contains(header.sequence)) {
            channel.receive.accept(header.sequence);
            handle(peer, packet->bytes().subspan(header_size));
        }
        transport_->receive.consume();
    }
    transport_->flush();
}

void Runtime::handle(Peer &peer, std::span<const std::byte> frames) {
    Reader reader(frames);
    while (reader.remaining() && !peer.closed) {
        auto op = Op(reader.get<std::uint8_t>());
        auto size = reader.get<std::uint16_t>();
        apply(peer, op, reader.bytes(size));
    }
}

ecs_entity_t Runtime::scope_entity(std::uint32_t id) {
    if (!id)
        return 0;
    auto index = entity_index(id);
    auto &entity = scopes_[index];
    if (scope_handles_[index] != id) {
        if (entity) {
            remote_despawn_ = true;
            ecs::entity(entity).kill();
            remote_despawn_ = false;
        }
        entity = ecs::entity::create().id();
        scope_handles_[index] = id;
    }
    return entity;
}

ecs_entity_t Runtime::owner_entity(Peer &peer, std::uint32_t id) {
    if (!id)
        return 0;
    if (id == peer.id)
        return peer.entity;
    auto [entry, added] = owner_entities_.try_emplace(id, 0);
    if (added)
        entry->second = ecs::entity::create().add<Connection>().set(PeerId{ id }).id();
    return entry->second;
}

void Runtime::apply(Peer &peer, Op op, std::span<const std::byte> bytes) {
    Reader reader(bytes);
    if (op == Op::welcome) {
        if (server_ || peer.connected)
            return;
        auto id = reader.get<std::uint32_t>();
        auto max_entities = reader.get<std::uint16_t>();
        if (!id || max_entities > limits_.max_entities || !schema_matches(reader) || !reader.done()) {
            disconnect(peer, DisconnectReason::schema_mismatch);
            return;
        }
        peer.id = id;
        peer.connected = true;
        ecs::entity(peer.entity).set(PeerId{ id }).add<Connected>();
        ecs::trigger<OnConnect>(ecs::entity(peer.entity));
        return;
    }
    if (op == Op::reject) {
        auto reason = reader.get<std::uint8_t>();
        if (!server_ && !peer.connected && reader.done() && reason <= std::uint8_t(DisconnectReason::congestion))
            disconnect(peer, DisconnectReason(reason));
        return;
    }
    if (op == Op::disconnect) {
        if (reader.done())
            disconnect(peer, DisconnectReason::remote);
        return;
    }
    if (!peer.connected || op == Op::heartbeat || op == Op::hello)
        return;
    auto id = reader.get<std::uint32_t>();
    if (!reader.valid() || (id && (!entity_index(id) || entity_index(id) > limits_.max_entities)))
        return;
    if (op == Op::spawn || op == Op::relations) {
        if (server_ || !id)
            return;
        auto revision = reader.get<std::uint32_t>();
        auto owner = reader.get<std::uint32_t>();
        auto lobby = reader.get<std::uint32_t>();
        auto game = reader.get<std::uint32_t>();
        auto scope_id = reader.get<std::uint32_t>();
        auto valid_scope = [&](std::uint32_t scope) {
            return !scope || (entity_index(scope) && entity_index(scope) <= limits_.max_entities);
        };
        if (!reader.done() || !valid_scope(lobby) || !valid_scope(game) || !valid_scope(scope_id))
            return;
        auto &entity = entities_[entity_index(id)];
        if (op == Op::relations && (!entity.active || entity.id != id))
            return;
        if (entity.id == id && entity.active && !newer(revision, entity.relation_revision))
            return;
        if (op == Op::spawn && (!entity.active || entity.id != id)) {
            if (entity.active)
                ecs::entity(entity.entity).kill();
            auto target = scope_id ? scope_entity(scope_id) : ecs::entity::create().id();
            entity = { .entity = target, .id = id, .active = true };
            ecs::entity(target).set(NetworkId{ id }).add<Replicated>();
            for (auto &d : registry_->types)
                value_state(id, *d) = {};
        }
        if (scope_id && op == Op::relations) {
            auto previous = scope_entity(scope_id);
            if (previous != entity.entity) {
                for (auto relation : {ecs::relation<MemberOf>(), ecs::relation<InInstance>()}) {
                    for (;;) {
                        auto sources = ecs_relation_sources(previous, relation);
                        if (!sources.count)
                            break;
                        ecs_relate_id(sources.entities[0], relation, entity.entity);
                    }
                }
                remote_despawn_ = true;
                ecs::entity(previous).kill();
                remote_despawn_ = false;
                scopes_[entity_index(scope_id)] = entity.entity;
            }
        }
        entity.owner = owner;
        entity.lobby = lobby;
        entity.game = game;
        entity.scope = scope_id;
        entity.relation_revision = revision;
        auto target = ecs::entity(entity.entity);
        if (owner)
            target.relate<OwnedBy>(ecs::entity(owner_entity(peer, owner)));
        else
            target.unrelate<OwnedBy>();
        if (lobby)
            target.relate<MemberOf>(ecs::entity(scope_entity(lobby)));
        else
            target.unrelate<MemberOf>();
        if (game)
            target.relate<InInstance>(ecs::entity(scope_entity(game)));
        else
            target.unrelate<InInstance>();
        return;
    }
    if (op == Op::despawn) {
        if (server_ || !id || !reader.done())
            return;
        auto &entity = entities_[entity_index(id)];
        if (entity.id == id && entity.active) {
            remote_despawn_ = true;
            ecs::entity(entity.entity).kill();
            remote_despawn_ = false;
            if (entity.scope && scopes_[entity_index(entity.scope)] == entity.entity) {
                scopes_[entity_index(entity.scope)] = 0;
                scope_handles_[entity_index(entity.scope)] = 0;
            }
            entity.active = false;
        }
        return;
    }
    auto slot = reader.get<std::uint16_t>();
    auto revision = reader.get<std::uint32_t>();
    if (!reader.valid() || slot >= registry_->types.size())
        return;
    auto &d = *registry_->slots[slot];
    auto payload = reader.bytes(reader.remaining());
    bool event = op == Op::event;
    if (event ? !d.event : d.event != 0)
        return;
    if (payload.size() != (op == Op::remove ? 0 : d.wire_size))
        return;
    if (server_ ? (event ? d.flow != Flow::client_event : d.flow != Flow::input)
                : (event ? d.flow != Flow::server_event : d.flow != Flow::replication))
        return;
    ecs_entity_t target = peer.entity;
    if (id) {
        const auto &entity = entities_[entity_index(id)];
        if (!entity.active || entity.id != id)
            return;
        if (server_ && (peer.visible[entity_index(id)] != id || entity.owner != peer.id))
            return;
        target = entity.entity;
    } else if (!event) {
        return;
    }
    if (event) {
        d.trigger(d, target, payload);
    } else {
        auto &state = value_state(id, d);
        if (!state.revision || newer(revision, state.revision)) {
            if (op == Op::remove) {
                ecs_remove_cid(target, d.component);
                state = { revision, false };
            } else if (op == Op::component && d.apply(d, target, payload)) {
                state = { revision, true };
            }
        }
    }
}

void change_observer(ecs_observer_event_t *event) {
    auto &state = ecs::resource<State>();
    state.runtime->component_change(*event, *reinterpret_cast<Descriptor *>(event->user_data));
}

void event_observer(ecs_observer_event_t *event) {
    auto &state = ecs::resource<State>();
    state.runtime->event(*event, *reinterpret_cast<Descriptor *>(event->user_data));
}

static void lifecycle_observer(ecs_observer_event_t *event) {
    ecs::resource<State>().runtime->lifecycle(*event);
}

static void scope_observer(ecs_observer_event_t *event) {
    ecs::resource<State>().runtime->scope_removed(*event);
}

static void relation_observer(ecs_observer_event_t *event) {
    ecs::resource<State>().runtime->relation_change(*event);
}

void install(std::shared_ptr<Runtime> runtime, std::uint16_t tick_rate) {
    ecs::set_resource(State{ std::move(runtime) });
    ecs::relation<OwnedBy>();
    ecs::relation<MemberOf>();
    ecs::relation<InInstance>();
    ecs_observer_desc_t scope_observer_desc{};
    scope_observer_desc.on = EcsOnRemove;
    scope_observer_desc.query.components[0] = {ecs::component<ScopeIdentity>().id(), EcsIn};
    scope_observer_desc.callback = scope_observer;
    ecs_observer_init(&scope_observer_desc);
    (void)ecs::event<OnConnect>();
    (void)ecs::event<OnDisconnect>();
    for (auto on : { EcsOnAdd, EcsOnRemove }) {
        ecs_observer_desc_t observer{};
        observer.on = on;
        observer.query.components[0] = { ecs::component<Replicated>().id(), EcsIn };
        observer.callback = lifecycle_observer;
        ecs_observer_init(&observer);
    }
    for (auto on : { EcsOnRelationSet, EcsOnRelationRemove }) {
        ecs_observer_desc_t observer{};
        observer.on = on;
        observer.callback = relation_observer;
        ecs_observer_init(&observer);
    }
    auto receive_phase = ecs::phase("NetReceive").after(EcsPostLoad).before(EcsPreUpdate).id();
    auto send_phase = ecs::phase("NetSend").after(EcsPostUpdate).before(EcsPreRender).id();
    ecs::system("NetReceive").phase(receive_phase).immediate().no_defer()
        .each([](ecs::res<State> state) { state->runtime->receive(); });
    auto collect_system = ecs::system("NetCollectChanges").phase(send_phase).immediate().no_defer()
        .each([](ecs::res<State> state) { state->runtime->flush(); });
    ecs::system("NetSend").phase(send_phase).after(collect_system).interval(1.0 / tick_rate).immediate().no_defer()
        .each([](ecs::res<State> state) { state->runtime->send(); });
    ecs::at_fini([] { ecs::resource<State>().runtime->shutdown(); });
}

} // namespace net::detail
