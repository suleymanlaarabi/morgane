#pragma once

#include <sinet/net.h>
#include "protocol.h"
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace net::detail {

inline std::uint16_t entity_index(std::uint32_t id) { return id & 0xffff; }

struct ScopeIdentity { std::uint32_t value; };

/** A message being built; `value` has room for any registered value. */
struct Message {
    Op op = Op::component;
    std::uint32_t entity = 0;
    std::uint32_t revision = 0;
    Descriptor *type = nullptr;
    bool reliable = false;
    std::uint16_t size = 0;
    std::array<std::byte, max_value_size> value{};
};

/** A message stored in an Outbox; `value` points at its slot in the outbox payload. */
struct QueuedMessage {
    Op op = Op::component;
    std::uint32_t entity = 0;
    std::uint32_t revision = 0;
    Descriptor *type = nullptr;
    bool reliable = false;
    std::uint16_t size = 0;
    std::byte *value = nullptr;
    std::span<const std::byte> bytes() const { return { value, size }; }
    Message message() const;
};

// One queued component value per entity/type. Events retain their ordering.
// Payload slots are sized for the largest registered value, not max_value_size.
class Outbox {
    std::vector<QueuedMessage> messages_;
    std::vector<std::byte> payload_;
    std::vector<std::uint64_t> queued_;
    std::size_t type_count_ = 0;
    std::size_t value_size_ = 0;
    std::uint64_t head_ = 0;
    std::uint64_t tail_ = 0;

  public:
    Outbox() = default;
    // Slots point into payload_, so an Outbox is never copied.
    Outbox(const Outbox &) = delete;
    Outbox &operator=(const Outbox &) = delete;
    void init(std::size_t entities, std::size_t types, std::size_t value_size, std::size_t capacity = 0);
    bool push(const Message &message);
    const QueuedMessage *front() const;
    const QueuedMessage *peek(std::size_t offset) const;
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
/** A host name lookup running on its own thread, so a slow DNS answer never stalls a frame. */
struct Resolution {
    std::atomic<bool> done{ false };
    std::optional<Endpoint> endpoint; // written before `done`
};
struct Peer {
    Endpoint endpoint;
    std::shared_ptr<Resolution> resolving; // client peer whose endpoint is still being looked up
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
    // Largest value a message carries: spawn/relations payloads or a registered type.
    std::size_t value_size_ = 16;
    std::mutex commands_mutex_;
    Outbox commands_;
    bool initialized_ = false;
    bool stopping_ = false;
    bool remote_despawn_ = false;
    std::array<std::byte, mtu> handshake_{};
    // Client connection requests wait for NetReceive, outside every peer loop and observer.
    struct Address {
        std::string host;
        std::uint16_t port;
    };
    bool close_requested_ = false;
    std::optional<Address> open_requested_;
    // A client disconnect despawns the replicated world at the next NetReceive, where every
    // remote despawn happens, never between other systems of the frame.
    bool despawn_pending_ = false;

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
    void despawn_remote();
    void apply_requests();
    void apply_resolution();
    ValueState &value_state(std::uint32_t id, const Descriptor &d);
    std::span<std::byte> value_bytes(std::uint32_t id, const Descriptor &d);
    void enqueue(const Message &message);

  public:
    Runtime(bool server, Limits limits, std::uint16_t max_clients, Endpoint bind);
    ~Runtime();
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    void receive();
    void send();
    void flush() { initialize(); collect(); }
    void shutdown();
    void request_open(std::string host, std::uint16_t port) {
        close_requested_ = true;
        open_requested_ = Address{ std::move(host), port };
    }
    void request_close() { close_requested_ = true; open_requested_.reset(); }
    void lifecycle(ecs_observer_event_t &event);
    void relation_change(ecs_observer_event_t &event);
    void scope_removed(ecs_observer_event_t &event);
    void component_change(ecs_observer_event_t &event, Descriptor &descriptor);
    void event(ecs_observer_event_t &event, Descriptor &descriptor);
};

struct State { std::shared_ptr<Runtime> runtime; };
void install(std::shared_ptr<Runtime> runtime, std::uint16_t tick_rate);

} // namespace net::detail
