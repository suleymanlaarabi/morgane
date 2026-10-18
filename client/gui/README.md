# gui — raygui widgets as SIECS entities

A widget is an entity, the UI tree is `ecs::ChildOf`, and layout and drawing are systems.
raygui only draws and reports interaction. What it reports comes back as frame tags and
events. Scenes own their widgets, which disappear with the scene.

```cpp
ecs::import<ray::Window>(...);
ecs::import<gui::Gui>(gui::Theme{ .font_size = 20, .style = "dark" }); // before any widget use

struct Play {};
auto menu = scene::define<Menu>().on_enter([](scene::Context<Menu> ctx) {
    auto box = ctx.spawn().set(gui::column(12).center().width(360).pad(24), gui::Panel{ "MORGANE" });
    ctx.spawn(box).set(gui::Button{ "Play" }).add<Play>();
});
gui::on_click<Play>([](Commands scenes) { scenes.replace<Game>(); });
```

## Layout

- `gui::column(gap)`, `row(gap)`, `stack()` and `node()` build a `gui::Node`.
- Chained setters: `.at(x, y)`, `.at(gui::TopRight, x, y)`, `.center()`, `.size(w, h)`, `.width()`,
  `.height()`, `.grow()`, `.gap()`, `.pad()`, `.align()` (cross axis) and `.justify()` (main axis).
- Sizes are pixels, `gui::fit` (the default, wraps the content) or `gui::fill` / `gui::fill(share)`.
- Widgets bring a default `Node`, so a leaf needs no layout of its own.
- `gui.Layout` runs every frame, after `PostUpdate`. It measures the tree from the leaves up,
  then places it from the root down, and writes `gui::Bounds` (rect, clip).
- Siblings keep their creation order.
- To anchor UI on the world, make it a child of an entity with `GlobalPosition2d`, or relate it
  with `gui::Follows`, which also kills it when the target dies.

## State and events

| What | How |
| --- | --- |
| Hide or grey out a subtree | `gui::Hidden` / `gui::Disabled` tags |
| Hover | `gui::Hovered`, maintained by the module |
| React in a system | `gui::on_click<Tag>`, `on_change<Tag>`, `on_submit<Tag>`. They take any system parameters and widget components, and run in the `OnUpdate` after the frame where it happened |
| React immediately | observers on `gui::OnClick`, `OnChange`, `OnSubmit`, `OnClose`, `OnFocus`, `OnBlur` |
| Keep gameplay out of the UI | `gui::Capture{ mouse, keyboard }` resource |
| Edited widget | `gui::Focus` resource |
| Styling | `gui::Style` (text size, color, alignment) and `gui::Tooltip` per widget; `gui::Theme` resource for the whole UI, swappable at runtime |

## Scenes

- Each scene layer draws its widgets in `PostRender`, after its own `on_render` hooks, in screen
  space.
- Layers that are not `scene::Active` (paused under another scene) are still drawn, but
  locked, so you cannot click through a pause menu.
- Widgets outside any scene are drawn last (`gui.Overlay`) and stay interactive.
- An open dropdown is drawn last in its layer and locks the rest of that layer.

## Custom widgets

```cpp
struct HealthBar { float value, max; };
gui::widget<HealthBar>({
    {},
    [](const HealthBar &) { return Vector2{ 200, 16 }; },
    [](gui::Draw &d, HealthBar &h) { GuiProgressBar(d.rect, nullptr, nullptr, &h.value, 0, h.max); },
});
```

Register a widget before the type is first used, because its hooks are installed when it is registered.

## Tests

`gui_tests` (`layout_test.cpp`) imports `gui::Layout` only, so it runs headless.
