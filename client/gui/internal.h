#pragma once

#include "gui.h"
#include <vector>

namespace gui::detail {

inline constexpr std::uint32_t none = UINT32_MAX;

/** One laid out node; pointers stay valid between gui.Collect and gui.Arrange. */
struct Item {
    ecs::entity entity;
    Node *node;
    Bounds *bounds;
    const VTable *vtable;
    ecs_entity_t owner;
    std::uint32_t parent = none;
    std::uint32_t first = 0, count = 0, filled = 0;
    bool follows = false;
    Vector2 point{}; // followed world position, in screen space
    Vector2 measured{};
    Vector2 content{};
    Insets chrome{};
};

/** Widget in painter order (parents first, siblings by creation). */
struct Entry {
    ecs::entity entity;
    ecs_entity_t owner;
    const VTable *vtable;
};

/** Resource: per-frame scratch of the layout, reused without reallocating. */
struct Tree {
    std::uint32_t frame = 0;
    std::vector<Item> items;
    std::vector<std::uint32_t> children;
    std::vector<std::uint32_t> roots;
    std::vector<std::uint32_t> stack;
    std::vector<Entry> order;
};

/** Applies gui::Style around measuring and drawing a widget. */
class StyleScope {
    int _control = 0;
    int _size = -1, _color = -1, _align = -1;
    bool _active = false;

  public:
    StyleScope(ecs::entity entity, const VTable *vtable);
    ~StyleScope();
    StyleScope(const StyleScope &) = delete;
    StyleScope &operator=(const StyleScope &) = delete;
};

void arrange(Tree &tree, Rectangle screen);
void paint(Tree &tree, ecs_entity_t owner, bool interactive);
void register_widgets();
void apply_theme(const Theme &theme);
void release_font();
/** Defined in raygui.cpp, next to the embedded styles. */
bool load_builtin_style(std::string_view name);

} // namespace gui::detail
