#pragma once

#include <client.h>
#include <raylib.h>
#include <functional>
#include <string>
#include <typeindex>

namespace ui {

    // Données pures : aucun de ces composants ne dessine ni ne réagit seul.
    struct Widget { Rectangle rect{}; bool visible = true; bool enabled = true; std::type_index scene = typeid(void);};
    struct Panel  { Color color{0, 0, 0, 170}; };
    struct Label  { std::string text; int font_size = 24; Color color = RAYWHITE; };
    struct Button { std::string text; std::function<void()> on_click; bool pressed = false; };

    struct Ui {
        static void import();
    };

    ecs::entity panel(ecs::entity parent, Rectangle rect, Color color = {0, 0, 0, 170});

    /** Empile verticalement des widgets dans une zone. Tous deviennent enfants de `parent`. */
    class Stack {
    public:
        Stack(ecs::entity parent, Rectangle area, float item_height = 56.f, float gap = 16.f);
        ecs::entity label(std::string text, int font_size = 24);
        ecs::entity button(std::string text, std::function<void()> on_click);

    private:
        Rectangle next();
        ecs::entity _parent;
        Rectangle _area;
        float _height, _gap, _cursor = 0.f;
    };

}