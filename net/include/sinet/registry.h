#pragma once

#include "wire.h"
#include <siecs.h>
#include <memory>
#include <string>
#include <vector>

namespace net::detail {

enum class Flow : std::uint8_t { replication, input, client_event, server_event };
struct Field {
    std::uint32_t offset;
    std::uint8_t size;
    bool boolean;
};

struct Descriptor {
    std::string name;
    std::uint64_t type_id = 0;
    std::uint64_t schema = 0;
    ecs_component_t component = 0;
    ecs_event_t event = 0;
    Flow flow = Flow::replication;
    bool reliable = false;
    bool owner_only = true;
    bool empty = false;
    std::uint16_t local_index = 0;
    std::uint16_t slot = 0;
    std::uint16_t wire_size = 0;
    std::size_t offset = 0;
    std::vector<Field> fields;
    std::string codec_schema;
    void (*encode)(const Descriptor &, const void *, std::span<std::byte>) = nullptr;
    bool (*decode)(const Descriptor &, void *, std::span<const std::byte>) = nullptr;
    bool (*apply)(const Descriptor &, ecs_entity_t, std::span<const std::byte>) = nullptr;
    void (*trigger)(const Descriptor &, ecs_entity_t, std::span<const std::byte>) = nullptr;
};

struct Registry {
    std::vector<std::shared_ptr<Descriptor>> types;
    std::array<Descriptor *, max_types> slots{};
    std::size_t stride = 0;
    bool frozen = false;
    void freeze();
};

Registry &registry();
Descriptor &register_type(ecs_component_t component, ecs_event_t event, Flow flow, bool empty);
void reflect_codec(Descriptor &descriptor);
void encode_fields(const Descriptor &, const void *, std::span<std::byte>);
bool decode_fields(const Descriptor &, void *, std::span<const std::byte>);
void observe_component(Descriptor &descriptor);
void observe_event(Descriptor &descriptor);
void change_observer(ecs_observer_event_t *event);
void event_observer(ecs_observer_event_t *event);

} // namespace net::detail
