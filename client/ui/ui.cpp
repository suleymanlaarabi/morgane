#include <client/raylib/ray.h>
#include <client/ui/ui.h>

namespace ui {

static void draw_centered(const std::string &text, Rectangle r, int font_size, Color color)
{
    int width = MeasureText(text.c_str(), font_size);
    DrawText(text.c_str(),
             int(r.x + (r.width - width) / 2),
             int(r.y + (r.height - font_size) / 2),
             font_size, color);
}

void Ui::import()
{
    ecs::component<Widget>();
    ecs::component<Panel>();
    ecs::component<Label>();
    ecs::component<Button>();

    // Sort du mode caméra : l'UI est dessinée en coordonnées écran.
    auto begin = ecs::system("ui.Begin")
        .phase(EcsOnRender)
        .immediate()
        .each([](ecs::res<const ray::View> view) {
            if (view->active)
                EndMode2D();
        });

    auto panels = ecs::system("ui.Panel")
        .phase(EcsOnRender)
        .after(begin)
        .immediate()
        .each([](const Widget &w, const Panel &p) {
            if (!w.visible)
                return;
            DrawRectangleRec(w.rect, p.color);
            DrawRectangleLinesEx(w.rect, 2.f, Fade(RAYWHITE, 0.4f));
        });

    auto labels = ecs::system("ui.Label")
        .phase(EcsOnRender)
        .after(panels)
        .immediate()
        .each([](const Widget &w, const Label &l) {
            if (!w.visible)
                return;
            draw_centered(l.text, w.rect, l.font_size, l.color);
        });

    auto buttons = ecs::system("ui.Button")
        .phase(EcsOnRender)
        .after(labels)
        .immediate()
        .each([](const Widget &w, Button &b) {
            if (!w.visible)
                return;

            const Vector2 mouse = GetMousePosition();
            const bool hovered = w.enabled && CheckCollisionPointRec(mouse, w.rect);

            // Un clic = appui ET relâchement sur le même bouton.
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                b.pressed = true;

            bool clicked = false;
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                clicked = b.pressed && hovered;
                b.pressed = false;
            }

            Color fill = {40, 40, 60, 255};
            Color text = RAYWHITE;
            if (!w.enabled) {
                fill = {30, 30, 30, 255};
                text = GRAY;
            } else if (b.pressed && hovered) {
                fill = {90, 90, 140, 255};
            } else if (hovered) {
                fill = {65, 65, 100, 255};
            }

            DrawRectangleRec(w.rect, fill);
            DrawRectangleLinesEx(w.rect, 2.f, hovered ? RAYWHITE : Fade(RAYWHITE, 0.5f));
            draw_centered(b.text, w.rect, 24, text);

            if (clicked && b.on_click)
                b.on_click();
        });

    // Rouvre la caméra pour que ray.EndFrame puisse la fermer.
    ecs::system("ui.End")
        .phase(EcsOnRender)
        .after(buttons)
        .immediate()
        .each([](ecs::res<const ray::View> view) {
            if (view->active)
                BeginMode2D(view->camera);
        });
}

ecs::entity panel(ecs::entity parent, Rectangle rect, Color color)
{
    auto e = ecs::entity::create();
    e.set(Widget{.rect = rect}, Panel{color});
    e.child_of(parent);
    return e;
}

Stack::Stack(ecs::entity parent, Rectangle area, float item_height, float gap)
    : _parent(parent), _area(area), _height(item_height), _gap(gap) {}

Rectangle Stack::next()
{
    Rectangle r{_area.x, _area.y + _cursor, _area.width, _height};
    _cursor += _height + _gap;
    return r;
}

ecs::entity Stack::label(std::string text, int font_size)
{
    auto e = ecs::entity::create();
    e.set(Widget{.rect = next()}, Label{std::move(text), font_size});
    e.child_of(_parent);
    return e;
}

ecs::entity Stack::button(std::string text, std::function<void()> on_click)
{
    auto e = ecs::entity::create();
    e.set(Widget{.rect = next()}, Button{std::move(text), std::move(on_click)});
    e.child_of(_parent);
    return e;
}

} // namespace ui