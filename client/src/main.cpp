#include <client.h>
#include <raylib.h>
#include <siecs.h>
#include <string>

int main(int argc, char *argv[]) {
    ecs::init();

    InitWindow(1920, 1080, "sasa");

    ecs::run();
}
