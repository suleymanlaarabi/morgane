#include "runtime.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <random>
#include <stdexcept>
#include <thread>

namespace net::detail {

static std::uint64_t session_token() {
    std::random_device source;
    auto token = (std::uint64_t(source()) << 32) | source();
    return token ? token : 1;
}

Message QueuedMessage::message() const {
    Message message{ .op = op, .entity = entity, .revision = revision, .type = type,
                     .reliable = reliable, .size = size };
    std::memcpy(message.value.data(), value, size);
    return message;
}

void Outbox::init(std::size_t entities, std::size_t types, std::size_t value_size, std::size_t capacity) {
    type_count_ = types;
    value_size_ = value_size;
    messages_.assign(capacity ? capacity : std::max<std::size_t>(1024, entities * (types + 3)), {});
    payload_.assign(messages_.size() * value_size, std::byte{});
    for (std::size_t i = 0; i < messages_.size(); ++i)
        messages_[i].value = payload_.data() + i * value_size;
    queued_.assign((entities + 1) * types, UINT64_MAX);
    head_ = tail_ = 0;
}

static void store(QueuedMessage &slot, const Message &message) {
    slot.op = message.op;
    slot.entity = message.entity;
    slot.revision = message.revision;
    slot.type = message.type;
    slot.reliable = message.reliable;
    slot.size = message.size;
    std::memcpy(slot.value, message.value.data(), message.size);
}

bool Outbox::push(const Message &message) {
    assert(message.size <= value_size_);
    bool component = message.op == Op::component || message.op == Op::remove;
    std::size_t key = 0;
    if (component) {
        key = std::size_t(entity_index(message.entity)) * type_count_ + message.type->local_index;
        auto serial = queued_[key];
        if (serial >= head_ && serial < tail_) {
            auto &previous = messages_[serial % messages_.size()];
            if (previous.entity == message.entity) {
                bool reliable = previous.reliable || message.reliable;
                store(previous, message);
                previous.reliable = reliable;
                return true;
            }
        }
    }
    if (tail_ - head_ == messages_.size())
        return false;
    if (component)
        queued_[key] = tail_;
    store(messages_[tail_++ % messages_.size()], message);
    return true;
}

const QueuedMessage *Outbox::front() const {
    return empty() ? nullptr : &messages_[head_ % messages_.size()];
}

void Outbox::pop() { ++head_; }

const QueuedMessage *Outbox::peek(std::size_t offset) const {
    return head_ + offset < tail_ ? &messages_[(head_ + offset) % messages_.size()] : nullptr;
}

void Outbox::consume(std::size_t count) { head_ += count; }

Runtime::Runtime(bool server, Limits limits, std::uint16_t max_clients, Endpoint bind)
    : server_(server), limits_(limits), max_clients_(max_clients), registry_(&registry()),
      transport_(std::make_unique<Transport>(bind)), peers_(max_clients),
      entities_(std::size_t(limits.max_entities) + 1),
      generations_(std::size_t(limits.max_entities) + 1),
      scopes_(std::size_t(limits.max_entities) + 1),
      scope_handles_(std::size_t(limits.max_entities) + 1),
      scope_generations_(std::size_t(limits.max_entities) + 1) {
    if (!limits.max_entities || !limits.tick_rate || !max_clients || limits.timeout_seconds <= 0 ||
        limits.bytes_per_tick < mtu)
        throw std::invalid_argument("Invalid network limits");
    free_ids_.reserve(limits.max_entities);
    for (std::uint32_t i = limits.max_entities; i; --i)
        free_ids_.push_back(i);
    free_scopes_.reserve(limits.max_entities);
    for (std::uint32_t i = limits.max_entities; i; --i)
        free_scopes_.push_back(i);
    endpoints_.reserve(max_clients);
}

Runtime::~Runtime() = default;

void Runtime::shutdown() {
    stopping_ = true;
    const std::array<std::byte, 3> goodbye{std::byte(Op::disconnect), std::byte(0), std::byte(0)};
    auto now = Clock::now();
    for (auto &peer : peers_)
        if (peer && !peer->closed && !peer->resolving)
            (void)transmit(*peer, goodbye, false, now);
    transport_->flush();
    transport_.reset();
}

void Runtime::apply_requests() {
    if (!close_requested_)
        return;
    close_requested_ = false;
    if (auto &previous = peers_[0]) {
        // A handshaking peer says goodbye too: the server may have accepted its hello already.
        if (!previous->closed && !previous->resolving) {
            const std::array<std::byte, 3> goodbye{ std::byte(Op::disconnect), std::byte(0),
                                                    std::byte(0) };
            (void)transmit(*previous, goodbye, false, Clock::now());
            transport_->flush();
        }
        disconnect(*previous, DisconnectReason::local);
        previous.reset();
        endpoints_.clear();
    }
    if (!open_requested_)
        return;
    auto peer = std::make_unique<Peer>();
    peer->session = session_token();
    peer->received = Clock::now();
    peer->outbox.init(limits_.max_entities, registry_->types.size(), value_size_);
    peer->visible.resize(std::size_t(limits_.max_entities) + 1);
    peer->entity = ecs::entity::create().add<Connection>()
        .set(PeerId{0}).set(Latency{}).set(Stats{}).id();
    auto address = std::move(*open_requested_);
    open_requested_.reset();
    try {
        peer->endpoint = Endpoint::parse(address.host.c_str(), address.port);
        endpoints_.emplace(peer->endpoint.key(), 0);
    } catch (const std::invalid_argument &) {
        // The thread only shares the Resolution: a closed peer or a shut down runtime drops it.
        peer->resolving = std::make_shared<Resolution>();
        std::thread([resolution = peer->resolving, address = std::move(address)] {
            try {
                resolution->endpoint = Endpoint::resolve(address.host.c_str(), address.port);
            } catch (const std::invalid_argument &) {
                // No endpoint tells apply_resolution() that the host did not resolve.
            }
            resolution->done.store(true, std::memory_order_release);
        }).detach();
    }
    peers_[0] = std::move(peer);
}

void Runtime::apply_resolution() {
    auto &peer = peers_[0];
    if (!peer || !peer->resolving || !peer->resolving->done.load(std::memory_order_acquire))
        return;
    auto endpoint = peer->resolving->endpoint;
    peer->resolving.reset();
    if (peer->closed)
        return;
    if (!endpoint) {
        disconnect(*peer, DisconnectReason::unresolved);
        return;
    }
    peer->endpoint = *endpoint;
    endpoints_.emplace(peer->endpoint.key(), 0);
}

void Runtime::despawn_remote() {
    remote_despawn_ = true;
    auto kill = [](ecs_entity_t id) {
        if (auto entity = ecs::entity(id); id && entity.is_alive())
            entity.kill();
    };
    for (auto &entity : entities_) {
        if (entity.active)
            kill(entity.entity);
        entity = {};
    }
    for (auto &scope : scopes_) {
        kill(scope);
        scope = 0;
    }
    std::fill(scope_handles_.begin(), scope_handles_.end(), 0);
    for (auto &[id, owner] : owner_entities_)
        kill(owner);
    owner_entities_.clear();
    std::fill(values_.begin(), values_.end(), ValueState{});
    remote_despawn_ = false;
}

void Runtime::initialize() {
    if (initialized_)
        return;
    registry_->freeze();
    auto count = registry_->types.size();
    for (const auto &d : registry_->types)
        value_size_ = std::max<std::size_t>(value_size_, d->wire_size);
    commands_.init(limits_.max_entities, count, value_size_);
    values_.resize((std::size_t(limits_.max_entities) + 1) * count);
    cache_.resize((std::size_t(limits_.max_entities) + 1) * registry_->stride);
    for (auto &peer : peers_) {
        if (!peer)
            continue;
        peer->outbox.init(limits_.max_entities, count, value_size_);
        peer->visible.resize(std::size_t(limits_.max_entities) + 1);
    }
    initialized_ = true;
}

ValueState &Runtime::value_state(std::uint32_t id, const Descriptor &d) {
    return values_[std::size_t(entity_index(id)) * registry_->types.size() + d.local_index];
}

std::span<std::byte> Runtime::value_bytes(std::uint32_t id, const Descriptor &d) {
    return std::span(cache_).subspan(std::size_t(entity_index(id)) * registry_->stride + d.offset, d.wire_size);
}

void Runtime::enqueue(const Message &message) {
    initialize();
    std::lock_guard lock(commands_mutex_);
    if (!commands_.push(message))
        throw std::length_error("Network event queue capacity exceeded");
}

void Runtime::disconnect(Peer &peer, DisconnectReason reason) {
    if (peer.closed)
        return;
    peer.closed = true;
    peer.connected = false;
    auto entity = ecs::entity(peer.entity);
    entity.remove<Connected>();
    if (!server_)
        ecs::set_resource(LocalPeer{});
    ecs::trigger<OnDisconnect>(entity, &reason);
    entity.kill();
    peer.entity = 0;
    // A client's replicated world belongs to its connection.
    if (!server_)
        despawn_pending_ = true;
}

} // namespace net::detail
