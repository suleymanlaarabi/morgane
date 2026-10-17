#include "internal.h"
#include <raygui.h>

namespace gui {

namespace {

/** Where a world entity is drawn this frame, in screen pixels. */
Vector2 screen_point(ecs::entity target) {
    Vector2 point{};
    if (const auto *presented = target.try_get<ray::PresentationPosition2d>())
        point = { presented->x, presented->y };
    else if (const auto *global = target.try_get<GlobalPosition2d>())
        point = { global->x, global->y };
    const auto *view = ecs::try_resource<ray::View>();
    return view ? GetWorldToScreen2D(point, view->camera) : point;
}

template <typename Tag> void clear_each_frame(const char *name) {
    ecs::system(name).phase(EcsPostUpdate).require<Tag>().each([](ecs::entity widget) {
        widget.remove<Tag>();
    });
}

} // namespace

void Layout::import() {
    scene::detail::register_owner();
    ecs::relation<Follows>(
        { .storage = EcsRelationDense, .on_delete_target = EcsDeleteSources, .acyclic = false }
    );
    detail::register_widgets();
    ecs::set_resource(detail::Tree{});
    ecs::set_resource(Capture{});
    ecs::set_resource(Focus{});

    // After gameplay and spatial propagation, before anything is drawn.
    const auto phase = ecs::phase("gui.Layout").after(EcsPostUpdate).before(EcsPreRender).id();
    auto begin = ecs::system("gui.BeginLayout")
                     .phase(phase)
                     .each([](ecs::res<detail::Tree> tree, ecs::res<Capture> capture) {
                         tree->items.clear();
                         ++tree->frame;
                         *capture = {};
                     });
    // Depth order visits parents first: their slot is known when a child looks it up.
    auto collect = ecs::system("gui.Collect")
                       .phase(phase)
                       .after(begin)
                       .order_by_depth<ecs::ChildOf>()
                       .exclude_up<ecs::ChildOf, Hidden>()
                       .each([](ecs::entity entity,
                                Node &node,
                                Bounds &bounds,
                                ecs::optional<const detail::Kind> kind,
                                ecs::res<detail::Tree> tree) {
                           detail::Item item{
                               .entity = entity,
                               .node = &node,
                               .bounds = &bounds,
                               .vtable = kind ? kind->vtable : nullptr,
                               .owner = entity.target<scene::Owner>().id(),
                           };
                           bounds.disabled = entity.has<Disabled>();
                           if (auto target = entity.target<Follows>()) {
                               item.follows = true;
                               item.point = screen_point(target);
                           } else if (auto parent = entity.target<ecs::ChildOf>()) {
                               const auto *above = parent.try_get<Bounds>();
                               if (above && above->frame == tree->frame) {
                                   item.parent = above->slot;
                                   bounds.disabled |= above->disabled;
                               } else if (!above && parent.has<GlobalPosition2d>()) {
                                   item.follows = true;
                                   item.point = screen_point(parent);
                               }
                           }
                           bounds.slot = static_cast<std::uint32_t>(tree->items.size());
                           bounds.frame = tree->frame;
                           tree->items.push_back(item);
                       });
    ecs::system("gui.Arrange")
        .phase(phase)
        .after(collect)
        .immediate()
        .each([](ecs::res<detail::Tree> tree, ecs::res<const ray::Screen> screen) {
            detail::arrange(*tree, { 0, 0, float(screen->width), float(screen->height) });
        });

    clear_each_frame<Clicked>("gui.ClearClicked");
    clear_each_frame<Changed>("gui.ClearChanged");
    clear_each_frame<Submitted>("gui.ClearSubmitted");
    ecs::observe<ecs::OnRemove>().each_for<Bounds>(
        [](ecs::entity widget, const Bounds &, ecs::res<Focus> focus) {
            if (focus->entity.id() == widget.id())
                focus->entity = ecs::entity::null();
        }
    );
}

void Gui::import(const Theme &theme) {
    ecs::import<Layout>();
    (void)ecs::resource_handle<Theme>({ .on_set = detail::apply_theme });
    ecs::set_resource(theme);

    // Each scene layer draws its widgets after its own render hooks, in screen space.
    ecs::system("gui.DrawLayer")
        .phase(EcsPostRender)
        .manual()
        .immediate()
        .require<scene::Root>()
        .each([](ecs::entity root, ecs::res<detail::Tree> tree) {
            detail::paint(*tree, root.id(), root.has<scene::Active>());
        });
    // Widgets outside any scene sit above every layer.
    const auto overlay = ecs::phase("gui.Overlay").after(EcsOnRender).before(EcsPostRender).id();
    ecs::system("gui.DrawOverlay")
        .phase(overlay)
        .immediate()
        .each([](ecs::res<detail::Tree> tree, ecs::res<ray::View> view) {
            if (view->active) {
                EndMode2D();
                view->active = false;
            }
            detail::paint(*tree, 0, true);
        });

    // Registered after ray::Window, so the font is released before CloseWindow.
    ecs::at_fini([] { detail::release_font(); });
}

namespace detail {

void release_font() {
    const Font font = GuiGetFont();
    if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id)
        UnloadFont(font);
    GuiSetFont(GetFontDefault());
}

void apply_theme(const Theme &theme) {
    release_font();
    GuiLoadStyleDefault();
    if (!theme.style.empty() && !load_builtin_style(theme.style))
        GuiLoadStyle(ray::resolve_asset(theme.style).c_str());
    if (!theme.font.empty()) {
        release_font();
        const int size = theme.font_size > 0 ? theme.font_size : 32;
        Font font = LoadFontEx(ray::resolve_asset(theme.font).c_str(), size, nullptr, 0);
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
        GuiSetFont(font);
    }
    if (theme.font_size > 0)
        GuiSetStyle(DEFAULT, TEXT_SIZE, theme.font_size);
}

} // namespace detail

} // namespace gui
