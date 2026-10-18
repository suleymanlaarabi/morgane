#include <sinet/registry.h>
#include <sinet/net.h>
#include <algorithm>
#include <stdexcept>

namespace net::detail {

Registry &registry() {
    if (!ecs::has_resource<Registry>())
        ecs::set_resource(Registry{});
    return ecs::resource<Registry>();
}

static void append_fields(Descriptor &d, sireflect_handle_t type, std::size_t offset,
                          std::uint64_t &schema) {
    auto *info = sireflect_type_info(type);
    schema = hash(sireflect_kind_name(info->kind), schema);
    if (info->kind == sireflect_kind_struct) {
        for (std::size_t i = 0; i < info->fields.field_count; ++i) {
            const auto &field = info->fields.fields[i];
            schema = hash(field.name, schema);
            append_fields(d, field.type, offset + field.offset, schema);
        }
    } else if (info->kind == sireflect_kind_array) {
        auto size = sireflect_type_size(info->element_type);
        schema = hash(std::to_string(info->element_count), schema);
        for (std::size_t i = 0; i < info->element_count; ++i)
            append_fields(d, info->element_type, offset + i * size, schema);
    } else {
        if (info->kind == sireflect_kind_ptr || info->kind == sireflect_kind_pointer ||
            info->kind == sireflect_kind_function_pointer || info->size > 8)
            throw std::invalid_argument("Network reflection accepts scalar fields and fixed arrays");
        d.fields.push_back({ std::uint32_t(offset), std::uint8_t(info->size),
                             info->kind == sireflect_kind_bool });
        d.wire_size += info->size;
        schema = hash(std::to_string(info->size), schema);
    }
}

void reflect_codec(Descriptor &d) {
    auto *info = ecs_component_info(d.component);
    auto *type = sireflect_type_info(info->type);
    if (!d.empty && type->fields.field_count == 0)
        return; // A custom codec can be installed by the registration builder.
    std::uint64_t schema = hash("sinet.reflected.v1");
    append_fields(d, info->type, 0, schema);
    if (d.wire_size > max_value_size)
        throw std::invalid_argument("Network component exceeds 256 encoded bytes");
    d.codec_schema = std::to_string(schema);
    d.encode = encode_fields;
    d.decode = decode_fields;
}

void encode_fields(const Descriptor &d, const void *object, std::span<std::byte> bytes) {
    auto *source = static_cast<const std::byte *>(object);
    std::size_t cursor = 0;
    for (const auto &field : d.fields) {
        for (std::size_t i = 0; i < field.size; ++i) {
            auto index = std::endian::native == std::endian::little ? i : field.size - 1 - i;
            bytes[cursor++] = source[field.offset + index];
        }
    }
}

bool decode_fields(const Descriptor &d, void *object, std::span<const std::byte> bytes) {
    if (bytes.size() != d.wire_size)
        return false;
    auto *target = static_cast<std::byte *>(object);
    std::size_t cursor = 0;
    for (const auto &field : d.fields) {
        if (field.boolean && std::to_integer<unsigned>(bytes[cursor]) > 1)
            return false;
        for (std::size_t i = 0; i < field.size; ++i) {
            auto index = std::endian::native == std::endian::little ? i : field.size - 1 - i;
            target[field.offset + index] = bytes[cursor++];
        }
    }
    return true;
}

Descriptor &register_type(ecs_component_t component, ecs_event_t event, Flow flow, bool empty) {
    auto &r = registry();
    if (r.frozen)
        throw std::logic_error("Network schemas must be registered before ecs::run()");
    if (r.types.size() == max_types)
        throw std::length_error("At most 64 network types per schema");
    for (const auto &d : r.types)
        if (d->component == component)
            throw std::logic_error("A network type must be registered once");
    auto descriptor = std::make_shared<Descriptor>();
    auto &d = *descriptor;
    d.component = component;
    d.event = event;
    d.empty = empty;
    d.flow = flow;
    d.local_index = r.types.size();
    d.name = ecs_component_info(component)->name;
    r.types.push_back(std::move(descriptor));
    return d;
}

void Registry::freeze() {
    if (frozen)
        return;
    for (auto &d : types) {
        if (!d->encode)
            reflect_codec(*d);
        if (!d->encode)
            throw std::invalid_argument("Provide reflected fields or a custom codec for " + d->name);
        d->type_id = hash(d->name);
        d->schema = hash(d->codec_schema, hash(d->name));
        d->schema = hash(std::to_string(unsigned(d->flow)), d->schema);
        d->schema = hash(d->reliable ? "reliable" : "sequenced", d->schema);
        d->schema = hash(d->owner_only ? "owner" : "any", d->schema);
        d->schema = hash(std::to_string(d->wire_size), d->schema);
        slots[d->local_index] = d.get();
    }
    std::sort(slots.begin(), slots.begin() + types.size(), [](auto *a, auto *b) {
        return a->type_id < b->type_id;
    });
    for (std::size_t i = 0; i < types.size(); ++i) {
        auto &d = *slots[i];
        if (i && slots[i - 1]->type_id == d.type_id)
            throw std::invalid_argument("Network type name/hash collision: " + d.name);
        d.slot = i;
        d.offset = stride;
        if (!d.event)
            stride += d.wire_size;
    }
    frozen = true;
}

void observe_component(Descriptor &d) {
    for (auto on : { EcsOnAdd, EcsOnSet, EcsOnRemove }) {
        ecs_observer_desc_t observer{};
        observer.on = on;
        observer.query.components[0] = { d.component, EcsIn };
        observer.query.components[1] = { ecs::component<Replicated>().id(), EcsFilter };
        observer.callback = change_observer;
        observer.user_data = reinterpret_cast<std::uintptr_t>(&d);
        ecs_observer_init(&observer);
    }
}

void observe_event(Descriptor &d) {
    ecs_observer_desc_t observer{};
    observer.on = d.event;
    observer.callback = event_observer;
    observer.user_data = reinterpret_cast<std::uintptr_t>(&d);
    ecs_observer_init(&observer);
}

} // namespace net::detail
