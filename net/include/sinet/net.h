#pragma once
#include "registry.h"
#include <concepts>
#include <stdexcept>

namespace net {

using NetworkTypeId = std::uint64_t;
struct Replicated {};
struct NetworkId { std::uint32_t value; };
struct Connection {};
struct PeerId { std::uint32_t value; };
struct Connected {};
struct Latency { float milliseconds = 0; };
struct Stats {
    std::uint64_t sent_bytes = 0;
    std::uint64_t received_bytes = 0;
    std::uint64_t sent_packets = 0;
    std::uint64_t received_packets = 0;
    std::uint64_t retransmissions = 0;
    std::uint64_t dropped_packets = 0;
};
struct OwnedBy {};
struct MemberOf {};
struct InInstance {};
struct OnConnect {};
struct OnDisconnect {};
enum class DisconnectReason : std::uint8_t { remote, timeout, schema_mismatch, capacity, congestion };

struct Limits {
    std::uint16_t max_entities = 4096;
    std::uint16_t tick_rate = 30;
    std::uint32_t bytes_per_tick = 64 * 1024;
    float timeout_seconds = 10;
};

namespace detail {

template <typename T> ecs_component_t replicated_component() {
#ifndef NDEBUG
    if constexpr (ecs::detail::c_declared_component<T>) {
        auto *desc = ecs::detail::c_component_traits<T>::desc_storage();
        desc->mutation = EcsSetOnly;
        auto id = ecs::component<T>().id();
        if (ecs_component_info(id)->mutation != EcsSetOnly)
            throw std::logic_error("Register replicated components before their first ECS use");
        return id;
    } else {
        auto id = ecs::component<T>(ecs::component_options<T>{ .mutation = EcsSetOnly }).id();
        if (ecs_component_info(id)->mutation != EcsSetOnly)
            throw std::logic_error("Register replicated components before their first ECS use");
        return id;
    }
#else
    return ecs::component<T>().id();
#endif
}

template <typename T> bool apply_component(const Descriptor &d, ecs_entity_t entity,
                                           std::span<const std::byte> bytes) {
    T value{};
    if (!d.decode(d, &value, bytes))
        return false;
    ecs_set_cid(entity, d.component, &value);
    return true;
}

template <typename T> void trigger_event(const Descriptor &d, ecs_entity_t entity,
                                        std::span<const std::byte> bytes) {
    T value{};
    if (d.decode(d, &value, bytes))
        ecs_observer_trigger(entity, d.event, bytes.empty() ? nullptr : &value);
}

} // namespace detail

template <typename T> class Registration {
    detail::Descriptor *descriptor_;

  public:
    explicit Registration(detail::Descriptor &descriptor) : descriptor_(&descriptor) {}
    Registration &reliable() { descriptor_->reliable = true; return *this; }
    Registration &unreliable_sequenced() { descriptor_->reliable = false; return *this; }
    Registration &owner_only() { descriptor_->owner_only = true; return *this; }
    Registration &client_to_server() { descriptor_->flow = detail::Flow::client_event; return *this; }
    Registration &server_to_clients() { descriptor_->flow = detail::Flow::server_event; return *this; }
    Registration &name(std::string_view name) { descriptor_->name = name; return *this; }

    // Codec: static schema_name, wire_size, encode(const T&, span<byte>),
    //        bool decode(T&, span<const byte>). No heap storage in codecs.
    template <typename Codec> Registration &codec() {
        static_assert(Codec::wire_size <= detail::max_value_size);
        descriptor_->wire_size = Codec::wire_size;
        descriptor_->codec_schema = Codec::schema_name;
        descriptor_->encode = [](const detail::Descriptor &, const void *value,
                                 std::span<std::byte> bytes) {
            Codec::encode(*static_cast<const T *>(value), bytes);
        };
        descriptor_->decode = [](const detail::Descriptor &, void *value,
                                 std::span<const std::byte> bytes) {
            return Codec::decode(*static_cast<T *>(value), bytes);
        };
        return *this;
    }
};

template <typename T> Registration<T> replicate() {
    static_assert(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>);
    auto &d = detail::register_type(detail::replicated_component<T>(), 0, detail::Flow::replication, std::is_empty_v<T>);
    d.apply = detail::apply_component<T>;
    detail::observe_component(d);
    return Registration<T>(d);
}

template <typename T> Registration<T> input() {
    static_assert(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>);
    auto &d = detail::register_type(detail::replicated_component<T>(), 0, detail::Flow::input, std::is_empty_v<T>);
    d.apply = detail::apply_component<T>;
    detail::observe_component(d);
    return Registration<T>(d);
}

template <typename T> Registration<T> event() {
    static_assert(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>);
    auto &d = detail::register_type(ecs::component<T>().id(), ecs::event<T>(), detail::Flow::client_event, std::is_empty_v<T>);
    d.trigger = detail::trigger_event<T>;
    detail::observe_event(d);
    return Registration<T>(d);
}

} // namespace net
