#pragma once

#include <client/raylib/ray.h>
#include <client/scene/scene_manager.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace gui {

/** Resource: raygui look. Setting it again swaps the theme at runtime. */
struct Theme {
    std::string font;  // .ttf/.otf path; empty keeps the style font
    int font_size = 0; // 0 keeps the style text size
    std::string style; // builtin raygui style ("dark", "cyber", ...) or .rgs path
};

/** Module: raygui widgets as entities. Import after ray::Window, before using any widget. */
struct Gui {
    static void import(const Theme &theme = {});
};

/** Module: the headless half of Gui (widgets types and layout), usable without a window. */
struct Layout {
    static void import();
};

// --- Layout ------------------------------------------------------------------------------------

/** Pixels (implicit from a number), gui::fit (content) or gui::fill (remaining parent space). */
struct Size {
    enum Kind : std::uint8_t { Px, Fit, Fill };
    Kind kind = Fit;
    float value = 0;
    constexpr Size() = default;
    constexpr Size(float px) : kind(Px), value(px) {}
    constexpr Size(Kind kind, float value) : kind(kind), value(value) {}
};
inline constexpr Size fit{ Size::Fit, 0 };
struct FillSize {
    constexpr operator Size() const { return { Size::Fill, 1 }; }
    constexpr Size operator()(float share) const { return { Size::Fill, share }; }
};
inline constexpr FillSize fill{};

struct Insets {
    float top = 0, right = 0, bottom = 0, left = 0;
    constexpr Insets() = default;
    constexpr Insets(float all) : top(all), right(all), bottom(all), left(all) {}
    constexpr Insets(float vertical, float horizontal)
        : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
    constexpr Insets(float top, float right, float bottom, float left)
        : top(top), right(right), bottom(bottom), left(left) {}
};

enum class Anchor : std::uint8_t {
    TopLeft,
    Top,
    TopRight,
    Left,
    Center,
    Right,
    BottomLeft,
    Bottom,
    BottomRight
};
using enum Anchor;
enum class Align : std::uint8_t { Start, Center, End, Stretch };
enum class Flow : std::uint8_t { Free, Row, Column };

/** Layout description, added automatically with every widget (fit size, in the parent flow). */
struct Node {
    Flow flow = Flow::Free;
    Anchor anchor = TopLeft; // placement inside a Free parent, the screen or a followed point
    Vector2 offset{};
    Size w = fit, h = fit;
    float spacing = 0;
    Insets padding{};
    Align align_items = Align::Start;     // cross axis of Row/Column children
    Align justify_content = Align::Start; // main axis of Row/Column children
    float weight = 0;                     // share of the parent's free main-axis space

    constexpr Node &at(float x, float y) & {
        offset = { x, y };
        return *this;
    }
    constexpr Node &at(Anchor a, float x = 0, float y = 0) & {
        anchor = a;
        return at(x, y);
    }
    constexpr Node &center() & { return at(Center); }
    constexpr Node &size(Size width, Size height) & {
        w = width;
        h = height;
        return *this;
    }
    constexpr Node &width(Size value) & {
        w = value;
        return *this;
    }
    constexpr Node &height(Size value) & {
        h = value;
        return *this;
    }
    constexpr Node &grow(float share = 1) & {
        weight = share;
        return *this;
    }
    constexpr Node &gap(float value) & {
        spacing = value;
        return *this;
    }
    constexpr Node &pad(Insets value) & {
        padding = value;
        return *this;
    }
    constexpr Node &pad(float vertical, float horizontal) & {
        return pad({ vertical, horizontal });
    }
    constexpr Node &align(Align value) & {
        align_items = value;
        return *this;
    }
    constexpr Node &justify(Align value) & {
        justify_content = value;
        return *this;
    }

    // Builders mostly run on temporaries: every setter also chains on rvalues.
    template <typename... A> constexpr Node &&at(A... a) && { return std::move(at(a...)); }
    constexpr Node &&center() && { return std::move(center()); }
    constexpr Node &&size(Size a, Size b) && { return std::move(size(a, b)); }
    constexpr Node &&width(Size v) && { return std::move(width(v)); }
    constexpr Node &&height(Size v) && { return std::move(height(v)); }
    constexpr Node &&grow(float v = 1) && { return std::move(grow(v)); }
    constexpr Node &&gap(float v) && { return std::move(gap(v)); }
    constexpr Node &&pad(Insets v) && { return std::move(pad(v)); }
    constexpr Node &&pad(float a, float b) && { return std::move(pad(a, b)); }
    constexpr Node &&align(Align v) && { return std::move(align(v)); }
    constexpr Node &&justify(Align v) && { return std::move(justify(v)); }
};

constexpr Node column(float gap = 0) { return { .flow = Flow::Column, .spacing = gap }; }
constexpr Node row(float gap = 0) { return { .flow = Flow::Row, .spacing = gap }; }
constexpr Node stack() { return {}; }
constexpr Node node() { return {}; }

namespace detail {
inline std::uint32_t next_sequence() {
    static std::uint32_t sequence = 0;
    return ++sequence;
}
} // namespace detail

/** Computed every frame by gui.Layout, in screen pixels. Read it, never set it. */
struct Bounds {
    Rectangle rect{};
    Rectangle clip{};
    Vector2 content{}; // measured size of the children
    bool clipped = false;
    bool disabled = false;
    std::uint32_t sequence = detail::next_sequence(); // creation order among siblings
    std::uint32_t slot = 0, frame = 0;
};

/** Relation: anchors a node on a world entity (sispatial); the node dies with its target. */
struct Follows {};

// --- State, style, events ----------------------------------------------------------------------

/** Tag: neither laid out nor drawn, with its whole subtree. */
struct Hidden {};
/** Tag: drawn greyed and not interactive, with its whole subtree. */
struct Disabled {};
/** Tag maintained by the module while the mouse is over the widget. */
struct Hovered {};

/** Frame tags: added while rendering frame N, visible to frame N+1 updates, then removed. */
struct Clicked {};
struct Changed {};
struct Submitted {};

/** Events triggered on the widget entity as soon as raygui reports them. */
struct OnClick {};
struct OnChange {};
struct OnSubmit {};
struct OnClose {};
struct OnFocus {};
struct OnBlur {};

/** Local raygui style overrides, restored after the widget is drawn. */
struct Style {
    int text_size = 0;
    Color text{};   // alpha 0 keeps the style color
    int align = -1; // TEXT_ALIGN_LEFT / TEXT_ALIGN_CENTER / TEXT_ALIGN_RIGHT
};

/** Hover text shown by raygui. */
struct Tooltip {
    std::string text;
};

/** Resource: the UI used the mouse or keyboard last frame; gameplay should ignore them. */
struct Capture {
    bool mouse = false;
    bool keyboard = false;
};

/** Resource: widget currently edited (text box, value box, open dropdown). */
struct Focus {
    ecs::entity entity;
};

// --- Widgets (raygui controls) -----------------------------------------------------------------

struct Panel {
    std::string title;
};
struct GroupBox {
    std::string title;
};
/** Panel with a close button, which triggers gui::OnClose. */
struct WindowBox {
    std::string title;
};
/** Clips its children and scrolls them with the wheel or the scroll bars. */
struct ScrollPanel {
    std::string title;
    Vector2 scroll{};
};
struct Label {
    std::string text;
};
struct Button {
    std::string text;
};
struct LabelButton {
    std::string text;
};
struct StatusBar {
    std::string text;
};
struct Line {
    std::string text;
};
struct Toggle {
    std::string text;
    bool on = false;
};
struct CheckBox {
    std::string text;
    bool checked = false;
};
/** `bar` draws a filled slider bar; `show_value` prints the value on the right. */
struct Slider {
    std::string label;
    float value = 0, min = 0, max = 1;
    bool bar = false;
    bool show_value = false;
};
struct ProgressBar {
    float value = 0, min = 0, max = 1;
    std::string label;
};
/** Integer input; `buttons = false` draws a plain value box. */
struct Spinner {
    std::string label;
    int value = 0, min = 0, max = 100;
    bool buttons = true;
};
struct TextBox {
    std::string text;
    int max = 64;
};
/** Items are separated by ';' as in raygui. */
struct ComboBox {
    std::string items;
    int active = 0;
};
struct Dropdown {
    std::string items;
    int active = 0;
};
struct ToggleGroup {
    std::string items;
    int active = 0;
};
struct ListView {
    std::string items;
    int active = -1;
    int scroll = 0;
};

// --- Custom widgets ----------------------------------------------------------------------------

/** Draw context handed to a widget for the current frame. */
class Draw {
  public:
    ecs::entity entity;
    Rectangle rect;

    bool hovered() const;
    bool focused() const;
    void focus();
    void blur();
    void click();  // Clicked + OnClick
    void change(); // Changed + OnChange
    void submit(); // Submitted + OnSubmit
    void close();  // OnClose
};

namespace detail {
struct WidgetInfo {
    int control = 0;    // raygui control used by gui::Style (DEFAULT = 0)
    bool clip = false;  // children are clipped to the content rect
    bool popup = false; // drawn last, alone interactive, while focused
};
} // namespace detail

/** Behaviour of widget type `T`; every callback is optional and stateless. */
template <typename T> struct Widget : detail::WidgetInfo {
    Vector2 (*measure)(const T &) = nullptr; // intrinsic size used by gui::fit
    void (*draw)(Draw &, T &) = nullptr;     // raygui calls, in screen space
    Insets (*insets)(const T &) = nullptr;   // chrome around the children (title bars)
    Vector2 (*scroll)(const T &) = nullptr;  // offset applied to the children
};

namespace detail {
struct VTable {
    Vector2 (*measure)(ecs::entity);
    void (*draw)(Draw &, ecs::entity);
    Insets (*insets)(ecs::entity);
    Vector2 (*scroll)(ecs::entity);
    const WidgetInfo *info;
};
template <typename T> inline Widget<T> spec{};
template <typename T>
inline constexpr VTable vtable{
    .measure =
        [](ecs::entity e) { return spec<T>.measure ? spec<T>.measure(e.get<T>()) : Vector2{}; },
    .draw =
        [](Draw &d, ecs::entity e) {
            if (spec<T>.draw)
                spec<T>.draw(d, e.get_mut<T>());
        },
    .insets = [](ecs::entity e) { return spec<T>.insets ? spec<T>.insets(e.get<T>()) : Insets{}; },
    .scroll = [](ecs::entity e) { return spec<T>.scroll ? spec<T>.scroll(e.get<T>()) : Vector2{}; },
    .info = &spec<T>,
};
/** Component: dispatch table of the entity's widget, filled when the widget is added. */
struct Kind {
    const VTable *vtable = nullptr;
};
} // namespace detail

/** Register widget type `T` before its first use; it brings Node and Bounds with it. */
template <typename T> void widget(const Widget<T> &behaviour) {
    detail::spec<T> = behaviour;
    ecs::component<T>(ecs::component_hooks<T>{
                          .on_add =
                              [](ecs_entity_t e, T &) {
                                  ecs::entity::from(e).get_mut<detail::Kind>().vtable =
                                      &detail::vtable<T>;
                              },
                      })
        .template with<Node, Bounds, detail::Kind>();
}

// --- Reactions ---------------------------------------------------------------------------------

namespace detail {
template <typename Tag, typename Frame, typename F>
ecs_system_id_t on_frame(std::string_view kind, F &&callback) {
    static const std::string name =
        "gui." + std::string(kind) + "<" + std::string(ecs::type_name<Tag>()) + ">";
    return ecs::system(name.c_str())
        .phase(EcsOnUpdate)
        .require<Frame, Tag>()
        .each(scene::detail::entity_callback(std::forward<F>(callback)));
}
} // namespace detail

/** Run `callback` (system parameters and widget components) when a `Tag` widget was clicked. */
template <typename Tag, typename F> ecs_system_id_t on_click(F &&callback) {
    return detail::on_frame<Tag, Clicked>("on_click", std::forward<F>(callback));
}
/** Same as on_click, when the value of a `Tag` widget changed. */
template <typename Tag, typename F> ecs_system_id_t on_change(F &&callback) {
    return detail::on_frame<Tag, Changed>("on_change", std::forward<F>(callback));
}
/** Same as on_click, when Enter is pressed in a `Tag` text box. */
template <typename Tag, typename F> ecs_system_id_t on_submit(F &&callback) {
    return detail::on_frame<Tag, Submitted>("on_submit", std::forward<F>(callback));
}

} // namespace gui
