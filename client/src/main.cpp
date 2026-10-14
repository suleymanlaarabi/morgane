#include <client.h>
#include <game/components.h>
#include <raylib.h>
#include <sinet/client.h>

int main() {
    ecs::init();
    ecs::import<net::Client>(net::ClientConfig{
        .address = "127.0.0.1",
        .port = 4242,
    });
    game::register_components();

    InitWindow(1920, 1080, "Morgane");
    ecs::system("WindowEvents").immediate().each([] {
        PollInputEvents();
        if (WindowShouldClose())
            ecs::quit();
    });
    ecs::run();
    CloseWindow();
}
