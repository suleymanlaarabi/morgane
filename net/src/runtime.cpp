#include "runtime.h"
#include <algorithm>
#include <cassert>
#include <random>
#include <stdexcept>

namespace net::detail {

static std::uint64_t session_token() {
    std::random_device source;
    auto token = (std::uint64_t(source()) << 32) | source();
    return token ? token : 1;
}

void Outbox::init(std::size_t entities, std::size_t types, std::size_t capacity) {
    type_count_ = types;
    messages_.resize(capacity ? capacity : std::max<std::size_t>(1024, entities * (types + 3)));
    queued_.assign((entities + 1) * types, UINT64_MAX);
    head_ = tail_ = 0;
}

bool Outbox::push(const Message &message) {
    bool component = message.op == Op::component || message.op == Op::remove;
    std::size_t key = 0;
    if (component) {
        key = std::size_t(entity_index(message.entity)) * type_count_ + message.type->local_index;
        auto serial = queued_[key];
        if (serial >= head_ && serial < tail_) {
            auto &previous = messages_[serial % messages_.size()];
            if (previous.entity == message.entity) {
                bool reliable = previous.reliable || message.reliable;
                previous = message;
                previous.reliable = reliable;
                return true;
            }
        }
    }
    if (tail_ - head_ == messages_.size())
        return false;
    if (component)
        queued_[key] = tail_;
    messages_[tail_++ % messages_.size()] = message;
    return true;
}

Message *Outbox::front() {
    return empty() ? nullptr : &messages_[head_ % messages_.size()];
}

void Outbox::pop() { ++head_; }

const Message *Outbox::peek(std::size_t offset) const {
    return head_ + offset < tail_ ? &messages_[(head_ + offset) % messages_.size()] : nullptr;
}

void Outbox::consume(std::size_t count) { head_ += count; }

Runtime::Runtime(bool server, Limits limits, std::uint16_t max_clients, Endpoint bind,
                 Endpoint remote)
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
    if (!server) {
        auto peer = std::make_unique<Peer>();
        peer->endpoint = remote;
        peer->session = session_token();
        peer->received = Clock::now();
        peer->entity = ecs::entity::create().add<Connection>()
            .set(PeerId{0}).set(Latency{}).set(Stats{}).id();
        endpoints_.emplace(remote.key(), 0);
        peers_[0] = std::move(peer);
    }
}

Runtime::~Runtime() = default;

void Runtime::shutdown() {
    stopping_ = true;
    const std::array<std::byte, 3> goodbye{std::byte(Op::disconnect), std::byte(0), std::byte(0)};
    auto now = Clock::now();
    for (auto &peer : peers_)
        if (peer && peer->connected)
            (void)transmit(*peer, goodbye, false, now);
    transport_->flush();
    transport_.reset();
}

void Runtime::initialize() {
    if (initialized_)
        return;
    registry_->freeze();
    auto count = registry_->types.size();
    commands_.init(limits_.max_entities, count);
    values_.resize((std::size_t(limits_.max_entities) + 1) * count);
    cache_.resize((std::size_t(limits_.max_entities) + 1) * registry_->stride);
    for (auto &peer : peers_) {
        if (!peer)
            continue;
        peer->outbox.init(limits_.max_entities, count);
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

void Runtime::enqueue(Message message) {
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
    ecs::trigger<OnDisconnect>(entity, &reason);
    entity.kill();
    peer.entity = 0;
}

} // namespace net::detail
