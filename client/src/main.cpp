#include <client.h>
#include <client/raylib/animation.h>
#include <client/raylib/ray.h>
#include <sinet/client.h>

int main() {
    ecs::init();
    ecs::import<net::Client>(net::ClientConfig{
        .address = "127.0.0.1",
        .port = 4242,
    });
    ecs::import<ray::Window>(ray::WindowConfig{
        .width = 1920,
        .height = 1080,
        .title = "Morgane",
    });

    ecs::import<ray::Animations>();

    constexpr ray::Animation ShipIdle = {
        .frame = { 0, 0, 33, 17 },
        .frames = 5,
        .fps = 8,
        .playback = ray::Playback::ping_pong,
    };

    ecs::entity::create().set(Position2d{ 0, 0 }, ray::Camera{ .zoom = 3 });
    ecs::entity::create().set(
        Position2d{ -100, 0 },
        Velocity2d{ 40, 0 },
        ray::Sprite{ .texture = ray::load_texture("assets/r-typesheet42.gif") },
        ShipIdle
    );
    ecs::run();
}
