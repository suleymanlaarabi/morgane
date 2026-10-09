#include <server.h>
#include <sinet/server.h>

int main() {
    ecs::init({ .worker_threads = ECS_WORKERS_AUTO });
    ecs::import<net::Server>(net::ServerConfig{
        .port = 4242,
        .tick_rate = 30,
        .max_clients = 128,
    });
    ecs::run();
}
