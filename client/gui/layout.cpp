#include "internal.h"
#include <algorithm>
#include <cmath>
#include <raygui.h>
#include <span>

namespace gui::detail {

StyleScope::StyleScope(ecs::entity entity, const VTable *vtable) {
    const auto *style = entity.try_get<Style>();
    if (!style)
        return;
    _active = true;
    _control = vtable ? vtable->info->control : DEFAULT;
    if (style->text_size > 0) {
        _size = GuiGetStyle(DEFAULT, TEXT_SIZE);
        GuiSetStyle(DEFAULT, TEXT_SIZE, style->text_size);
    }
    if (style->text.a > 0) {
        _color = GuiGetStyle(_control, TEXT_COLOR_NORMAL);
        GuiSetStyle(_control, TEXT_COLOR_NORMAL, ColorToInt(style->text));
    }
    if (style->align >= 0) {
        _align = GuiGetStyle(_control, TEXT_ALIGNMENT);
        GuiSetStyle(_control, TEXT_ALIGNMENT, style->align);
    }
}

StyleScope::~StyleScope() {
    if (!_active)
        return;
    if (_size >= 0)
        GuiSetStyle(DEFAULT, TEXT_SIZE, _size);
    if (_color != -1)
        GuiSetStyle(_control, TEXT_COLOR_NORMAL, _color);
    if (_align >= 0)
        GuiSetStyle(_control, TEXT_ALIGNMENT, _align);
}

namespace {

float anchor_x(Anchor anchor) {
    switch (anchor) {
    case TopLeft:
    case Left:
    case BottomLeft:
        return 0;
    case Top:
    case Center:
    case Bottom:
        return 0.5f;
    default:
        return 1;
    }
}

float anchor_y(Anchor anchor) {
    switch (anchor) {
    case TopLeft:
    case Top:
    case TopRight:
        return 0;
    case Left:
    case Center:
    case Right:
        return 0.5f;
    default:
        return 1;
    }
}

float slack(Align align, float space) {
    if (align == Align::Center)
        return space * 0.5f;
    return align == Align::End ? space : 0;
}

float resolve(Size size, float measured, float available) {
    if (size.kind == Size::Px)
        return size.value;
    return size.kind == Size::Fill ? available * size.value : measured;
}

/** Anchor a box in `area`; a zero-sized area is a point the anchor pivots around. */
Rectangle place(const Node &node, Vector2 size, Rectangle area) {
    return {
        area.x + (area.width - size.x) * anchor_x(node.anchor) + node.offset.x,
        area.y + (area.height - size.y) * anchor_y(node.anchor) + node.offset.y,
        size.x,
        size.y,
    };
}

Rectangle intersect(Rectangle a, Rectangle b) {
    const float x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    const float right = std::min(a.x + a.width, b.x + b.width);
    const float bottom = std::min(a.y + a.height, b.y + b.height);
    return { x, y, std::max(0.f, right - x), std::max(0.f, bottom - y) };
}

Insets operator+(Insets a, Insets b) {
    return { a.top + b.top, a.right + b.right, a.bottom + b.bottom, a.left + b.left };
}

void measure(Tree &tree) {
    auto &items = tree.items;
    // Children follow their parent, so a reverse walk sees every child before its parent.
    for (std::size_t i = items.size(); i-- > 0;) {
        auto &item = items[i];
        const auto &node = *item.node;
        Vector2 intrinsic{};
        if (item.vtable) {
            StyleScope style(item.entity, item.vtable);
            intrinsic = item.vtable->measure(item.entity);
            item.chrome = item.vtable->insets(item.entity);
        }
        const Insets inset = node.padding + item.chrome;
        const float fit_w = std::max(intrinsic.x, item.content.x + inset.left + inset.right);
        const float fit_h = std::max(intrinsic.y, item.content.y + inset.top + inset.bottom);
        // Fill sizes measure as their fit size, so a fitting parent still wraps them.
        item.measured = {
            node.w.kind == Size::Px ? node.w.value : fit_w,
            node.h.kind == Size::Px ? node.h.value : fit_h,
        };
        item.bounds->content = item.content;
        if (item.parent == none)
            continue;
        auto &parent = items[item.parent];
        const auto &layout = *parent.node;
        const float gap = parent.filled++ ? layout.spacing : 0;
        if (layout.flow == Flow::Column) {
            parent.content.x = std::max(parent.content.x, item.measured.x);
            parent.content.y += item.measured.y + gap;
        } else if (layout.flow == Flow::Row) {
            parent.content.x += item.measured.x + gap;
            parent.content.y = std::max(parent.content.y, item.measured.y);
        } else {
            parent.content.x =
                std::max(parent.content.x, item.measured.x + std::abs(node.offset.x));
            parent.content.y =
                std::max(parent.content.y, item.measured.y + std::abs(node.offset.y));
        }
    }
}

void link(Tree &tree) {
    auto &items = tree.items;
    tree.roots.clear();
    std::uint32_t total = 0;
    for (std::uint32_t i = 0; i < items.size(); ++i) {
        if (items[i].parent == none)
            tree.roots.push_back(i);
        else
            ++items[items[i].parent].count;
    }
    for (auto &item : items) {
        item.first = total;
        total += item.count;
    }
    tree.children.resize(total);
    for (std::uint32_t i = 0; i < items.size(); ++i)
        if (auto &item = items[i]; item.parent != none) {
            auto &parent = items[item.parent];
            tree.children[parent.first + parent.filled++] = i;
        }
    const auto by_sequence = [&](std::uint32_t a, std::uint32_t b) {
        return items[a].bounds->sequence < items[b].bounds->sequence;
    };
    for (auto &item : items) {
        item.filled = 0;
        auto begin = tree.children.begin() + item.first;
        std::sort(begin, begin + item.count, by_sequence);
    }
    std::sort(tree.roots.begin(), tree.roots.end(), by_sequence);
}

/** Lay the children of `item` out inside its content box. */
void flow(Tree &tree, Item &item) {
    if (!item.count)
        return;
    const auto &node = *item.node;
    const Rectangle rect = item.bounds->rect;
    const Insets inset = node.padding + item.chrome;
    const Rectangle box{
        rect.x + inset.left,
        rect.y + inset.top,
        std::max(0.f, rect.width - inset.left - inset.right),
        std::max(0.f, rect.height - inset.top - inset.bottom),
    };
    const Vector2 scroll = item.vtable ? item.vtable->scroll(item.entity) : Vector2{};
    const bool clips = item.vtable && item.vtable->info->clip;
    const Rectangle clip = clips ? (item.bounds->clipped ? intersect(item.bounds->clip, box) : box)
                                 : item.bounds->clip;
    const bool clipped = clips || item.bounds->clipped;

    const auto children = std::span(tree.children).subspan(item.first, item.count);
    const bool horizontal = node.flow == Flow::Row;
    const float main_space = horizontal ? box.width : box.height;
    const float cross_space = horizontal ? box.height : box.width;
    float fixed = node.spacing * float(item.count - 1), shares = 0;
    for (auto index : children) {
        const auto &child = tree.items[index];
        const Size main = horizontal ? child.node->w : child.node->h;
        fixed += main.kind == Size::Fill ? 0 : (horizontal ? child.measured.x : child.measured.y);
        shares += (main.kind == Size::Fill ? main.value : 0) + child.node->weight;
    }
    const float free = std::max(0.f, main_space - fixed);
    float cursor = shares > 0 ? 0 : slack(node.justify_content, free);

    for (auto index : children) {
        auto &child = tree.items[index];
        const auto &spec = *child.node;
        Rectangle out;
        if (node.flow == Flow::Free) {
            out = place(
                spec,
                { resolve(spec.w, child.measured.x, box.width),
                  resolve(spec.h, child.measured.y, box.height) },
                box
            );
        } else {
            const Size main = horizontal ? spec.w : spec.h;
            const Size cross = horizontal ? spec.h : spec.w;
            const float share = (main.kind == Size::Fill ? main.value : 0) + spec.weight;
            const float main_size = (main.kind == Size::Fill ? 0
                                     : horizontal            ? child.measured.x
                                                             : child.measured.y) +
                                    (shares > 0 ? free * share / shares : 0);
            const float cross_fit = horizontal ? child.measured.y : child.measured.x;
            const float cross_size = cross.kind == Size::Fit && node.align_items == Align::Stretch
                                         ? cross_space
                                         : resolve(cross, cross_fit, cross_space);
            const float cross_at = slack(node.align_items, cross_space - cross_size);
            out = horizontal ? Rectangle{ box.x + cursor, box.y + cross_at, main_size, cross_size }
                             : Rectangle{ box.x + cross_at, box.y + cursor, cross_size, main_size };
            out.x += spec.offset.x;
            out.y += spec.offset.y;
            cursor += main_size + node.spacing;
        }
        out.x += scroll.x;
        out.y += scroll.y;
        child.bounds->rect = out;
        child.bounds->clip = clip;
        child.bounds->clipped = clipped;
    }
}

} // namespace

void arrange(Tree &tree, Rectangle screen) {
    auto &items = tree.items;
    link(tree);
    measure(tree);
    // Parents come first, so each rectangle is final before its children are placed.
    for (auto &item : items) {
        if (item.parent == none) {
            const Rectangle area =
                item.follows ? Rectangle{ item.point.x, item.point.y, 0, 0 } : screen;
            item.bounds->rect = place(
                *item.node,
                { resolve(item.node->w, item.measured.x, area.width),
                  resolve(item.node->h, item.measured.y, area.height) },
                area
            );
            item.bounds->clip = screen;
            item.bounds->clipped = false;
        }
        flow(tree, item);
    }
    tree.order.clear();
    tree.stack.assign(tree.roots.rbegin(), tree.roots.rend());
    while (!tree.stack.empty()) {
        const auto &item = items[tree.stack.back()];
        tree.stack.pop_back();
        if (item.vtable)
            tree.order.push_back({ item.entity, item.owner, item.vtable });
        for (std::uint32_t i = item.count; i-- > 0;)
            tree.stack.push_back(tree.children[item.first + i]);
    }
}

} // namespace gui::detail
