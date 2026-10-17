#include "internal.h"
#include <raygui.h>

namespace gui {

bool Draw::hovered() const {
    return !GuiIsLocked() && GuiGetState() != STATE_DISABLED &&
           CheckCollisionPointRec(GetMousePosition(), rect);
}

bool Draw::focused() const { return ecs::resource<Focus>().entity.id() == entity.id(); }

void Draw::focus() {
    auto &focus = ecs::resource<Focus>();
    if (focus.entity.id() == entity.id())
        return;
    if (focus.entity)
        ecs::trigger<OnBlur>(focus.entity);
    focus.entity = entity;
    ecs::trigger<OnFocus>(entity);
}

void Draw::blur() {
    auto &focus = ecs::resource<Focus>();
    if (focus.entity.id() != entity.id())
        return;
    focus.entity = ecs::entity::null();
    ecs::trigger<OnBlur>(entity);
}

void Draw::click() {
    entity.add<Clicked>();
    ecs::trigger<OnClick>(entity);
}

void Draw::change() {
    entity.add<Changed>();
    ecs::trigger<OnChange>(entity);
}

void Draw::submit() {
    entity.add<Submitted>();
    ecs::trigger<OnSubmit>(entity);
}

void Draw::close() { ecs::trigger<OnClose>(entity); }

namespace detail {

namespace {

void draw(const Entry &entry, bool locked, Capture &capture) {
    auto widget = entry.entity;
    const Bounds &bounds = widget.get<Bounds>();
    if (locked)
        GuiLock();
    if (bounds.disabled)
        GuiDisable();
    if (bounds.clipped)
        BeginScissorMode(
            int(bounds.clip.x),
            int(bounds.clip.y),
            int(bounds.clip.width),
            int(bounds.clip.height)
        );
    const auto *tooltip = widget.try_get<Tooltip>();
    if (tooltip) {
        GuiEnableTooltip();
        GuiSetTooltip(tooltip->text.c_str());
    }
    {
        StyleScope style(widget, entry.vtable);
        Draw context{ widget, bounds.rect };
        entry.vtable->draw(context, widget);
    }
    if (tooltip) {
        GuiSetTooltip(nullptr);
        GuiDisableTooltip();
    }
    if (bounds.clipped)
        EndScissorMode();
    GuiEnable();
    GuiUnlock();

    const Vector2 mouse = GetMousePosition();
    const bool hovered = !locked && !bounds.disabled &&
                         CheckCollisionPointRec(mouse, bounds.rect) &&
                         (!bounds.clipped || CheckCollisionPointRec(mouse, bounds.clip));
    if (hovered != widget.has<Hovered>()) {
        if (hovered)
            widget.add<Hovered>();
        else
            widget.remove<Hovered>();
    }
    capture.mouse |= hovered;
    capture.keyboard |= !locked && ecs::resource<Focus>().entity.id() == widget.id();
}

} // namespace

void paint(Tree &tree, ecs_entity_t owner, bool interactive) {
    auto &capture = ecs::resource<Capture>();
    const auto focused = ecs::resource<Focus>().entity.id();
    // An open popup (dropdown) is drawn last and is the only interactive widget of its layer.
    const Entry *popup = nullptr;
    for (const auto &entry : tree.order)
        if (entry.owner == owner && entry.entity.id() == focused && entry.vtable->info->popup)
            popup = &entry;
    for (const auto &entry : tree.order)
        if (entry.owner == owner && &entry != popup)
            draw(entry, !interactive || popup, capture);
    if (popup)
        draw(*popup, !interactive, capture);
}

} // namespace detail

} // namespace gui
