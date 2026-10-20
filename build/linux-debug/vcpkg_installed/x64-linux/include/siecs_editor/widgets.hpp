#pragma once

/*
 * Custom inspector widgets.
 *
 * Only for the manifest's editor_sources: the editor compiles them into its
 * plugin, so the game itself never depends on ImGui.
 *
 *   #include <siecs_editor/widgets.hpp>
 *   #include "../shared/contract.hpp"
 *
 *   SIECS_EDITOR_WIDGETS {
 *       // Replaces the generic fields of the Health card.
 *       siecs::editor::inspector<Health>([](Health &health) {
 *           siecs::editor::field("Value");
 *           return ImGui::SliderInt("##value", &health.value, 0, health.max);
 *       });
 *       // Used by fields tagged siecs::meta<Unit>().widget("level", "stars").
 *       siecs::editor::widget<int32_t>("stars", [](int32_t &level) {
 *           ...
 *           return changed;
 *       });
 *   }
 *
 * Callbacks may also take the edited entity (inspectors) or the field
 * description (widgets) as a second parameter. A field widget draws in the
 * value column, after the editor drew the label. Return true on the frames the
 * value changes: the editor applies it and turns each interaction (a drag,
 * typing) into one undo step.
 */

#include <imgui.h>
#include <optional>
#include <siecs_editor/contract.hpp>
#include <type_traits>

namespace siecs::editor {

namespace detail {

inline const siecs_editor_host_t *&host() {
    static const siecs_editor_host_t *current = nullptr;
    return current;
}

inline void bind(const siecs_editor_host_t *host) {
    detail::host() = host;
    ImGui::SetCurrentContext(static_cast<ImGuiContext *>(host->imgui_context));
    IMGUI_CHECKVERSION();
}

template <typename T, typename F> bool call_inspector(void *value, ecs_entity_t entity, void *ctx) {
    F &fn = *static_cast<F *>(ctx);
    T &component = *static_cast<T *>(value);
    if constexpr (std::is_invocable_v<F &, T &, ecs_entity_t>)
        return fn(component, entity);
    else
        return fn(component);
}

template <typename T, typename F> bool call_widget(void *value, const siecs_editor_field_t *field, void *ctx) {
    if (field->size != sizeof(T)) {
        ImGui::TextDisabled("widget expects %zu bytes, field has %zu", sizeof(T), field->size);
        return false;
    }
    F &fn = *static_cast<F *>(ctx);
    T &data = *static_cast<T *>(value);
    if constexpr (std::is_invocable_v<F &, T &, const siecs_editor_field_t &>)
        return fn(data, *field);
    else
        return fn(data);
}

} // namespace detail

/* Draw a field label in the inspector's label column; the next widget fills
 * the value column. */
inline void field(const char *label, const char *tooltip = nullptr) {
    if (const siecs_editor_host_t *host = detail::host(); host && host->field_label)
        host->field_label(label, tooltip);
}

/* Replace the generic inspector body of component T. fn: bool(T &) or
 * bool(T &, ecs_entity_t). */
template <typename T, typename F> void inspector(F fn) {
    static_assert(std::is_invocable_r_v<bool, F &, T &> || std::is_invocable_r_v<bool, F &, T &, ecs_entity_t>,
                  "inspector callback must be bool(T &) or bool(T &, ecs_entity_t)");
    static std::optional<F> slot;
    slot.emplace(std::move(fn));
    detail::host()->inspector(ecs::component<T>().id(), detail::call_inspector<T, F>, &*slot);
}

/* Named widget for fields of type T whose siecs.role is name. fn: bool(T &)
 * or bool(T &, const siecs_editor_field_t &). */
template <typename T, typename F> void widget(const char *name, F fn) {
    static_assert(std::is_invocable_r_v<bool, F &, T &> ||
                      std::is_invocable_r_v<bool, F &, T &, const siecs_editor_field_t &>,
                  "widget callback must be bool(T &) or bool(T &, const siecs_editor_field_t &)");
    static std::optional<F> slot;
    slot.emplace(std::move(fn));
    detail::host()->widget(name, detail::call_widget<T, F>, &*slot);
}

} // namespace siecs::editor

/* Body registering the custom widgets of one editor source file. */
#define SIECS_EDITOR_WIDGETS SIECS_EDITOR_WIDGETS_(SIECS_CONTRACT_CAT(siecs_editor_widgets_, __LINE__))
#define SIECS_EDITOR_WIDGETS_(fn)                                                                  \
    static void fn();                                                                              \
    [[maybe_unused]] static const bool SIECS_CONTRACT_CAT(fn, _registered) =                       \
        (::siecs::detail::widget_registrations().push_back([](const siecs_editor_host_t *host) {   \
             ::siecs::editor::detail::bind(host);                                                  \
             fn();                                                                                 \
         }),                                                                                       \
         true);                                                                                    \
    static void fn()
