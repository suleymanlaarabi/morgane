#pragma once

#include <sinet/net.h>
#include "protocol.h"
#include <mutex>
#include <unordered_map>
#include <vector>

namespace net::detail {

inline std::uint16_t entity_index(std::uint32_t id) { return id & 0xffff; }

struct ScopeIdentity { std::uint32_t value; };

struct Message {
    Op op = Op::component;
    std::uint32_t entity = 0;
    std::uint32_t revision = 0;
    Descriptor *type = nullptr;
    bool reliable = false;
    std::uint16_t size = 0;
    std::array<std::byte, max_value_size> value{};
};

// One queued component value per entity/type. Events retain their ordering.
class Outbox {
    std::vector<Message> messages_;
    std::vector<std::uint64_t> queued_;
    std::size_t type_count_ = 0;
    std::uint64_t head_ = 0;
    std::uint64_t tail_ = 0;

  public:
    void init(std::size_t entities, std::size_t types, std::size_t capacity = 0);
    bool push(const Message &message);
    Message *front();
    const Message *peek(std::size_t offset) const;
    void consume(std::size_t count);
    void pop();
    bool empty() const { return head_ == tail_; }
};

struct EntityState {
    ecs_entity_t entity = 0;
    std::uint32_t id = 0;
    std::uint32_t owner = 0;
    std::uint32_t lobby = 0;
    std::uint32_t game = 0;
    std::uint32_t scope = 0;
    std::uint32_t relation_revision = 0;
    bool active = false;
};
struct ValueState {
    std::uint32_t revision = 0;
    bool present = false;
};
struct Peer {
    Endpoint endpoint;
    std::uint64_t session = 0;
    std::uint32_t id = 0;
    ecs_entity_t entity = 0;
    bool connected = false;
    bool closed = false;
    std::uint32_t lobby = 0;
    std::uint32_t game = 0;
    Channel channel;
    Time received{};
    Time sent{};
    Time hello{};
    Stats stats;
    Outbox outbox;
    std::vector<std::uint32_t> visible;
};

class Runtime {
    bool server_;
    Limits limits_;
    std::uint16_t max_clients_;
    Registry *registry_;
    std::unique_ptr<Transport> transport_;
    std::vector<std::unique_ptr<Peer>> peers_;
    std::unordered_map<std::uint64_t, std::size_t> endpoints_;
    std::vector<EntityState> entities_;
    std::vector<std::uint16_t> free_ids_;
    std::vector<std::uint16_t> generations_;
    std::vector<ValueState> values_;
    std::vector<std::byte> cache_;
    std::vector<ecs_entity_t> scopes_;
    std::vector<std::uint32_t> scope_handles_;
    std::vector<std::uint16_t> scope_generations_;
    std::vector<std::uint16_t> free_scopes_;
    std::unordered_map<std::uint32_t, ecs_entity_t> owner_entities_;
    std::uint32_t next_peer_ = 1;
    std::uint32_t revision_ = 0;
    std::mutex commands_mutex_;
    Outbox commands_;
    bool initialized_ = false;
    bool stopping_ = false;
    bool remote_despawn_ = false;
    std::array<std::byte, mtu> handshake_{};

    void initialize();
    void collect();
    std::uint32_t scope(ecs_entity_t entity);
    Message relations(ecs_entity_t entity, std::uint32_t id,
                      const ecs_relation_event_t *transition = nullptr);
    void update_scope(Peer &peer);
    bool interested(const Peer &peer, const EntityState &entity) const;
    void update_interest(Peer &peer, EntityState &entity);
    void spawn(Peer &peer, const EntityState &entity);
    void fanout(const Message &message);
    void hello(const Packet &packet, const Header &header, std::span<const std::byte> bytes);
    void handle(Peer &peer, std::span<const std::byte> frames);
    void apply(Peer &peer, Op op, std::span<const std::byte> bytes);
    bool schema_matches(Reader &reader) const;
    std::size_t schema_bytes(std::span<std::byte> target) const;
    bool transmit(Peer &peer, std::span<const std::byte> payload, bool reliable, Time now);
    void reject(Endpoint endpoint, std::uint64_t session, DisconnectReason reason);
    void send_hello(Peer &peer, Time now);
    bool send_outbox(Peer &peer, Time now, std::uint32_t &budget);
    ecs_entity_t scope_entity(std::uint32_t id);
    ecs_entity_t owner_entity(Peer &peer, std::uint32_t id);
    void disconnect(Peer &peer, DisconnectReason reason);
    ValueState &value_state(std::uint32_t id, const Descriptor &d);
    std::span<std::byte> value_bytes(std::uint32_t id, const Descriptor &d);
    void enqueue(Message message);

  public:
    Runtime(bool server, Limits limits, std::uint16_t max_clients, Endpoint bind,
            Endpoint remote = {});
    ~Runtime();
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    void receive();
    void send();
    void flush() { initialize(); collect(); }
    void shutdown();
    void lifecycle(ecs_observer_event_t &event);
    void relation_change(ecs_observer_event_t &event);
    void scope_removed(ecs_observer_event_t &event);
    void component_change(ecs_observer_event_t &event, Descriptor &descriptor);
    void event(ecs_observer_event_t &event, Descriptor &descriptor);
};

struct State { std::shared_ptr<Runtime> runtime; };
void install(std::shared_ptr<Runtime> runtime, std::uint16_t tick_rate);

} // namespace net::detail
