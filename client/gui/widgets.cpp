#include "internal.h"
#include <algorithm>
#include <cstdio>
#include <raygui.h>

namespace gui::detail {

namespace {

constexpr float title_height = 24; // RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT
constexpr float slider_width = 200;

float text_size() { return float(GuiGetStyle(DEFAULT, TEXT_SIZE)); }
float control_height() { return text_size() + 14; }
float text_width(const std::string &text) {
    return text.empty() ? 0 : float(GuiGetTextWidth(text.c_str()));
}
const char *text_or_null(const std::string &text) { return text.empty() ? nullptr : text.c_str(); }

/** Widest of the ';' separated raygui items. */
float items_width(const std::string &items, int *count = nullptr) {
    float widest = 0;
    int n = 0;
    std::size_t start = 0;
    while (start <= items.size()) {
        const auto end = std::min(items.find(';', start), items.size());
        widest = std::max(widest, text_width(items.substr(start, end - start)));
        ++n;
        start = end + 1;
    }
    if (count)
        *count = n;
    return widest;
}

/** Leading label drawn by us, so every labelled control measures the same way. */
float label_width(const std::string &label) {
    return label.empty() ? 0 : text_width(label) + text_size() * 0.5f;
}
Rectangle after_label(Draw &d, const std::string &label) {
    const float width = label_width(label);
    if (width > 0)
        GuiLabel({ d.rect.x, d.rect.y, width, d.rect.height }, label.c_str());
    return { d.rect.x + width, d.rect.y, std::max(0.f, d.rect.width - width), d.rect.height };
}

Vector2 text_control(const std::string &text, float padding) {
    return { text_width(text) + padding, control_height() };
}

} // namespace

void register_widgets() {
    widget<Panel>({
        { .control = DEFAULT },
        [](const Panel &w) { return Vector2{ w.title.empty() ? 0 : text_width(w.title) + 16, 0 }; },
        [](Draw &d, Panel &w) { GuiPanel(d.rect, text_or_null(w.title)); },
        [](const Panel &w) { return Insets{ w.title.empty() ? 0 : title_height, 0, 0, 0 }; },
    });
    widget<GroupBox>({
        { .control = DEFAULT },
        [](const GroupBox &w) { return Vector2{ text_width(w.title) + 24, 0 }; },
        [](Draw &d, GroupBox &w) { GuiGroupBox(d.rect, text_or_null(w.title)); },
        [](const GroupBox &) { return Insets{ text_size() * 0.5f + 4, 4, 4, 4 }; },
    });
    widget<WindowBox>({
        { .control = DEFAULT },
        [](const WindowBox &w) { return Vector2{ text_width(w.title) + title_height + 16, 0 }; },
        [](Draw &d, WindowBox &w) {
            if (GuiWindowBox(d.rect, text_or_null(w.title)))
                d.close();
        },
        [](const WindowBox &) { return Insets{ title_height, 0, 0, 0 }; },
    });
    widget<ScrollPanel>({
        { .control = LISTVIEW, .clip = true },
        nullptr,
        [](Draw &d, ScrollPanel &w) {
            const Vector2 content = d.entity.get<Bounds>().content;
            Rectangle view{};
            GuiScrollPanel(
                d.rect,
                text_or_null(w.title),
                { d.rect.x, d.rect.y, content.x, content.y },
                &w.scroll,
                &view
            );
        },
        [](const ScrollPanel &w) {
            const float bar = float(GuiGetStyle(LISTVIEW, SCROLLBAR_WIDTH));
            return Insets{ w.title.empty() ? 0 : title_height, bar, bar, 0 };
        },
        [](const ScrollPanel &w) { return w.scroll; },
    });
    widget<Label>({
        { .control = LABEL },
        [](const Label &w) { return text_control(w.text, 4); },
        [](Draw &d, Label &w) { GuiLabel(d.rect, w.text.c_str()); },
    });
    widget<Button>({
        { .control = BUTTON },
        [](const Button &w) { return text_control(w.text, text_size() * 2); },
        [](Draw &d, Button &w) {
            if (GuiButton(d.rect, text_or_null(w.text)))
                d.click();
        },
    });
    widget<LabelButton>({
        { .control = LABEL },
        [](const LabelButton &w) { return text_control(w.text, 4); },
        [](Draw &d, LabelButton &w) {
            if (GuiLabelButton(d.rect, w.text.c_str()))
                d.click();
        },
    });
    widget<StatusBar>({
        { .control = STATUSBAR },
        [](const StatusBar &w) { return text_control(w.text, 16); },
        [](Draw &d, StatusBar &w) { GuiStatusBar(d.rect, text_or_null(w.text)); },
    });
    widget<Line>({
        { .control = DEFAULT },
        [](const Line &w) { return Vector2{ text_width(w.text) + 64, text_size() }; },
        [](Draw &d, Line &w) { GuiLine(d.rect, text_or_null(w.text)); },
    });
    widget<Toggle>({
        { .control = TOGGLE },
        [](const Toggle &w) { return text_control(w.text, text_size() * 2); },
        [](Draw &d, Toggle &w) {
            const bool before = w.on;
            GuiToggle(d.rect, text_or_null(w.text), &w.on);
            if (w.on != before)
                d.change();
        },
    });
    widget<CheckBox>({
        { .control = CHECKBOX },
        [](const CheckBox &w) {
            const float box = text_size() + 6;
            return Vector2{ box + float(GuiGetStyle(CHECKBOX, TEXT_PADDING)) + text_width(w.text),
                            control_height() };
        },
        [](Draw &d, CheckBox &w) {
            const float box = std::min(d.rect.height, text_size() + 6);
            const bool before = w.checked;
            GuiCheckBox(
                { d.rect.x, d.rect.y + (d.rect.height - box) * 0.5f, box, box },
                text_or_null(w.text),
                &w.checked
            );
            if (w.checked != before)
                d.change();
        },
    });
    widget<Slider>({
        { .control = SLIDER },
        [](const Slider &w) {
            const float value = w.show_value ? text_width("0000.00") : 0;
            return Vector2{ label_width(w.label) + slider_width + value, control_height() };
        },
        [](Draw &d, Slider &w) {
            const Rectangle at = after_label(d, w.label);
            char value[32] = {};
            const bool shown = w.show_value;
            Rectangle bar = at;
            if (shown) {
                std::snprintf(value, sizeof value, "%.2f", w.value);
                bar.width = std::max(0.f, bar.width - text_width("0000.00"));
            }
            const float before = w.value;
            if (w.bar)
                GuiSliderBar(bar, nullptr, shown ? value : nullptr, &w.value, w.min, w.max);
            else
                GuiSlider(bar, nullptr, shown ? value : nullptr, &w.value, w.min, w.max);
            if (w.value != before)
                d.change();
        },
    });
    widget<ProgressBar>({
        { .control = PROGRESSBAR },
        [](const ProgressBar &w) {
            return Vector2{ label_width(w.label) + slider_width, text_size() + 6 };
        },
        [](Draw &d, ProgressBar &w) {
            GuiProgressBar(after_label(d, w.label), nullptr, nullptr, &w.value, w.min, w.max);
        },
    });
    widget<Spinner>({
        { .control = VALUEBOX },
        [](const Spinner &w) {
            return Vector2{ label_width(w.label) + text_size() * 8, control_height() };
        },
        [](Draw &d, Spinner &w) {
            const Rectangle at = after_label(d, w.label);
            const int before = w.value;
            const bool editing = d.focused();
            const int pressed = w.buttons
                                    ? GuiSpinner(at, nullptr, &w.value, w.min, w.max, editing)
                                    : GuiValueBox(at, nullptr, &w.value, w.min, w.max, editing);
            if (pressed == RESULT_PRESSED) {
                if (editing)
                    d.blur();
                else
                    d.focus();
            }
            if (w.value != before)
                d.change();
        },
    });
    widget<TextBox>({
        { .control = TEXTBOX },
        [](const TextBox &w) {
            return Vector2{ std::max(text_size() * 12, text_width(w.text) + 16), control_height() };
        },
        [](Draw &d, TextBox &w) {
            // raygui edits a fixed C buffer in place.
            static std::string buffer;
            buffer.assign(w.text, 0, std::size_t(w.max));
            buffer.resize(std::size_t(w.max) + 1, '\0');
            const bool editing = d.focused();
            const bool pressed = GuiTextBox(d.rect, buffer.data(), w.max + 1, editing);
            if (std::string_view edited = buffer.c_str(); edited != w.text) {
                w.text = edited;
                d.change();
            }
            if (!pressed)
                return;
            if (!editing) {
                d.focus();
                return;
            }
            d.blur();
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
                d.submit();
        },
    });
    widget<ComboBox>({
        { .control = COMBOBOX },
        [](const ComboBox &w) {
            return Vector2{ items_width(w.items) + text_size() * 2 +
                                float(GuiGetStyle(COMBOBOX, COMBO_BUTTON_WIDTH)),
                            control_height() };
        },
        [](Draw &d, ComboBox &w) {
            const int before = w.active;
            GuiComboBox(d.rect, w.items.c_str(), &w.active);
            if (w.active != before)
                d.change();
        },
    });
    widget<Dropdown>({
        { .control = DROPDOWNBOX, .popup = true },
        [](const Dropdown &w) {
            return Vector2{ items_width(w.items) + text_size() * 3, control_height() };
        },
        [](Draw &d, Dropdown &w) {
            const int before = w.active;
            const bool editing = d.focused();
            if (GuiDropdownBox(d.rect, w.items.c_str(), &w.active, editing)) {
                if (editing)
                    d.blur();
                else
                    d.focus();
            }
            if (w.active != before)
                d.change();
        },
    });
    widget<ToggleGroup>({
        { .control = TOGGLE },
        [](const ToggleGroup &w) {
            int count = 0;
            const float each = items_width(w.items, &count) + text_size() * 2;
            const float gap = float(GuiGetStyle(TOGGLE, GROUP_PADDING));
            return Vector2{ each * float(count) + gap * float(count - 1), control_height() };
        },
        [](Draw &d, ToggleGroup &w) {
            int count = 0;
            items_width(w.items, &count);
            const float gap = float(GuiGetStyle(TOGGLE, GROUP_PADDING));
            const float each = (d.rect.width - gap * float(count - 1)) / float(count);
            const int before = w.active;
            GuiToggleGroup({ d.rect.x, d.rect.y, each, d.rect.height }, w.items.c_str(), &w.active);
            if (w.active != before)
                d.change();
        },
    });
    widget<ListView>({
        { .control = LISTVIEW },
        [](const ListView &w) {
            return Vector2{ items_width(w.items) + text_size() * 3,
                            float(GuiGetStyle(LISTVIEW, LIST_ITEMS_HEIGHT)) * 5 };
        },
        [](Draw &d, ListView &w) {
            const int before = w.active;
            GuiListView(d.rect, w.items.c_str(), &w.scroll, &w.active);
            if (w.active != before)
                d.change();
        },
    });
}

} // namespace gui::detail
