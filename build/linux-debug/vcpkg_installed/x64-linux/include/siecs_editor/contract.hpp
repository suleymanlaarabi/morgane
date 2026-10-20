#pragma once

/*
 * Game contract for siecs-editor.
 *
 * A contract is one header shared by the game and the editor. It registers the
 * components and relations scenes may contain, and describes how the editor
 * shows them. The game calls the generated `<name>_contract()` after
 * ecs::init() on both client and server; the editor compiles the same header
 * into a plugin and calls it in its edit world. Nothing here depends on ImGui.
 *
 *   #include <siecs_editor/contract.hpp>
 *   #include <si2d.h>
 *
 *   enum class Team { Blue, Red };
 *   SIECS_ENUM(Team, Blue, Red);
 *
 *   struct Health { reflected(int value = 3; Team team;) };
 *
 *   SIECS_CONTRACT(mygame) {
 *       ecs::import<si2d>();
 *       siecs::meta<Health>().category("Gameplay").range("value", 0, 10);
 *       ecs::relation<Targets>();
 *   }
 *
 * Field types used by reflected components must be reflected first: C value
 * types with SIREFLECT_STRUCT, C++ ones with SIECS_TYPE(T), enums with
 * SIECS_ENUM. Those register automatically when the contract runs. Field types
 * are named without namespaces in reflected field lists.
 *
 * Metadata keys read by the editor and the scene loader:
 *   component: siecs.category, siecs.tooltip, siecs.icon, siecs.hidden,
 *              siecs.transient (never saved)
 *   field:     siecs.min, siecs.max, siecs.step, siecs.role ("color",
 *              "asset", "angle", "entity" or a custom widget name),
 *              siecs.filter (asset extensions), siecs.tooltip, siecs.hidden,
 *              siecs.readonly
 *
 * Custom ImGui widgets live in the manifest's editor_sources, compiled only
 * into the editor plugin (see siecs_editor/widgets.hpp).
 */

#include <siecs.h>
#include <sireflect.h>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#define SIECS_EDITOR_ABI 2

#if defined(_WIN32)
#define SIECS_EDITOR_EXPORT __declspec(dllexport)
#else
#define SIECS_EDITOR_EXPORT __attribute__((__visibility__("default")))
#endif

extern "C" {
/* Field handed to a named widget (see meta_builder::widget). */
typedef struct siecs_editor_field_t {
    const char *name;
    /* Reflected type of the field and its size in bytes. */
    sireflect_handle_t type;
    size_t size;
    /* Entity being edited. */
    ecs_entity_t entity;
} siecs_editor_field_t;

/* Draw with ImGui and edit value in place; return true when it changed. */
typedef bool (*siecs_editor_inspector_fn)(void *value, ecs_entity_t entity, void *ctx);
typedef bool (*siecs_editor_widget_fn)(void *value, const siecs_editor_field_t *field, void *ctx);

/* Services the editor offers to the plugin's editor sources. */
typedef struct siecs_editor_host_t {
    uint32_t abi;
    /* ImGuiContext * of the editor. */
    void *imgui_context;
    /* Replace the generic inspector body of a component. */
    void (*inspector)(ecs_component_t component, siecs_editor_inspector_fn fn, void *ctx);
    /* Widget for fields whose siecs.role is name. */
    void (*widget)(const char *name, siecs_editor_widget_fn fn, void *ctx);
    /* Label in the inspector's label column; the next item fills the value column. */
    void (*field_label)(const char *label, const char *tooltip);
} siecs_editor_host_t;

/* Entry point exported by an editor plugin. */
typedef struct siecs_editor_plugin_t {
    uint32_t abi;
    const char *name;
    void (*contract)(void);
    /* Registers the custom widgets of the editor sources; called after contract
     * in every edit world. */
    void (*widgets)(const siecs_editor_host_t *host);
} siecs_editor_plugin_t;
}

namespace siecs {

namespace detail {

/* Returns whether the type is registered afterwards. */
using type_register_fn = bool (*)();

inline std::vector<type_register_fn> &pending_types() {
    static std::vector<type_register_fn> types;
    return types;
}

inline bool queue_type(type_register_fn fn) {
    pending_types().push_back(fn);
    return true;
}

/* Value types may reference each other in any declaration order: retry the
 * failed ones until a pass makes no progress. */
inline void register_types() {
    std::vector<type_register_fn> &types = pending_types();
    std::vector<bool> done(types.size(), false);
    for (bool progress = true; progress;) {
        progress = false;
        for (size_t i = 0; i < types.size(); i++) {
            if (!done[i] && types[i]()) {
                done[i] = true;
                progress = true;
            }
        }
    }
}

/* Custom widget registrations of the editor sources (see widgets.hpp). */
using widgets_fn = void (*)(const siecs_editor_host_t *);

inline std::vector<widgets_fn> &widget_registrations() {
    static std::vector<widgets_fn> registrations;
    return registrations;
}

inline void register_widgets(const siecs_editor_host_t *host) {
    for (widgets_fn fn : widget_registrations())
        fn(host);
}

inline const char *short_name(std::string_view name) {
    size_t colon = name.rfind("::");
    if (colon != std::string_view::npos)
        name.remove_prefix(colon + 2);
    static std::deque<std::string> names;
    for (const std::string &existing : names) {
        if (existing == name)
            return existing.c_str();
    }
    names.emplace_back(name);
    return names.back().c_str();
}

inline void set_meta(sireflect_handle_t type, const char *field, sireflect_meta_t meta) {
    if (!type)
        return;
    if (field)
        sireflect_field_set_meta(type, field, &meta);
    else
        sireflect_type_set_meta(type, &meta);
}

inline sireflect_meta_t meta_string(const char *key, const char *value) {
    return { .key = key, .kind = SIREFLECT_META_STRING, .value = { .string = value } };
}

inline sireflect_meta_t meta_bool(const char *key) {
    return { .key = key, .kind = SIREFLECT_META_BOOL, .value = { .boolean = true } };
}

inline sireflect_meta_t meta_f64(const char *key, double value) {
    sireflect_meta_t meta = { .key = key, .kind = SIREFLECT_META_F64 };
    meta.value.f64 = value;
    return meta;
}

} // namespace detail

/* Register a C++ value type declared with reflected(...) under its
 * unqualified name. */
template <typename T> sireflect_handle_t reflect() {
    const char *name = detail::short_name(ecs::type_name<T>());
    if (sireflect_handle_t existing = sireflect_type_by_name(name))
        return existing;
    static const sireflect_struct_desc_t desc = {
        .name = name,
        .fields = T::fields,
        .size = sizeof(T),
        .align = alignof(T),
    };
    return sireflect_try_register_struct(&desc);
}

/* Editor description of a component. Every call registers T if needed. */
template <typename T> class meta_builder {
  public:
    meta_builder() {
        const ecs_component_info_t *info = ecs_component_info(ecs::component<T>().id());
        _type = info ? info->type : 0;
    }

    /* Group in the add-component menu, e.g. "Gameplay" or "Physics/Bodies". */
    meta_builder &category(const char *name) { return set(nullptr, detail::meta_string("siecs.category", name)); }
    meta_builder &tooltip(const char *text) { return set(nullptr, detail::meta_string("siecs.tooltip", text)); }
    meta_builder &icon(const char *name) { return set(nullptr, detail::meta_string("siecs.icon", name)); }
    /* Not offered in the add-component menu nor shown in the inspector. */
    meta_builder &hidden() { return set(nullptr, detail::meta_bool("siecs.hidden")); }
    /* Runtime-only state, never written to scene files. */
    meta_builder &transient() { return set(nullptr, detail::meta_bool("siecs.transient")); }

    meta_builder &range(const char *field, double min, double max, double step = 0) {
        set(field, detail::meta_f64("siecs.min", min));
        set(field, detail::meta_f64("siecs.max", max));
        if (step > 0)
            set(field, detail::meta_f64("siecs.step", step));
        return *this;
    }
    meta_builder &step(const char *field, double step) { return set(field, detail::meta_f64("siecs.step", step)); }
    /* Edit with a named widget: "color", "asset", "angle", "entity", or a
     * widget registered by the game's editor sources. */
    meta_builder &widget(const char *field, const char *name) { return set(field, detail::meta_string("siecs.role", name)); }
    meta_builder &color(const char *field) { return widget(field, "color"); }
    /* Radians stored, degrees shown. */
    meta_builder &angle(const char *field) { return widget(field, "angle"); }
    /* Asset picker; extensions is a comma separated list such as "png,jpg". */
    meta_builder &asset(const char *field, const char *extensions = nullptr) {
        widget(field, "asset");
        return extensions ? set(field, detail::meta_string("siecs.filter", extensions)) : *this;
    }
    meta_builder &tooltip(const char *field, const char *text) {
        return set(field, detail::meta_string("siecs.tooltip", text));
    }
    meta_builder &hide(const char *field) { return set(field, detail::meta_bool("siecs.hidden")); }
    meta_builder &readonly(const char *field) { return set(field, detail::meta_bool("siecs.readonly")); }

    [[nodiscard]] sireflect_handle_t type() const { return _type; }

  private:
    meta_builder &set(const char *field, sireflect_meta_t meta) {
        detail::set_meta(_type, field, meta);
        return *this;
    }

    sireflect_handle_t _type = 0;
};

template <typename T> meta_builder<T> meta() { return meta_builder<T>(); }

} // namespace siecs

#define SIECS_CONTRACT_CAT_(a, b) a##b
#define SIECS_CONTRACT_CAT(a, b) SIECS_CONTRACT_CAT_(a, b)

/* Reflect a C++ value type (with a reflected(...) field list) used by
 * component fields. Place after the type, at namespace scope. */
#define SIECS_TYPE(T)                                                                              \
    [[maybe_unused]] inline const bool SIECS_CONTRACT_CAT(siecs_type_registered_, __LINE__) =     \
        ::siecs::detail::queue_type([] { return ::siecs::reflect<T>() != 0; })

/* Reflect an enum used by component fields; list every enumerator in order.
 * Explicit values are written as `Name = value`. */
#define SIECS_ENUM(T, ...)                                                                         \
    [[maybe_unused]] inline const bool SIECS_CONTRACT_CAT(siecs_enum_registered_, __LINE__) =     \
        ::siecs::detail::queue_type([] {                                                           \
            const char *name = ::siecs::detail::short_name(#T);                                    \
            if (sireflect_type_by_name(name))                                                      \
                return true;                                                                       \
            static const sireflect_enum_desc_t desc = {                                            \
                .name = name,                                                                      \
                .values = "{" #__VA_ARGS__ "}",                                                    \
                .size = sizeof(T),                                                                 \
                .align = alignof(T),                                                               \
            };                                                                                     \
            return sireflect_try_register_enum(&desc) != 0;                                        \
        })

#ifdef SIECS_EDITOR_PLUGIN_MAIN
#define SIECS_CONTRACT_ENTRY_(name)                                                                \
    extern "C" SIECS_EDITOR_EXPORT const siecs_editor_plugin_t siecs_editor_plugin = {            \
        SIECS_EDITOR_ABI,                                                                          \
        #name,                                                                                     \
        name##_contract,                                                                           \
        ::siecs::detail::register_widgets,                                                         \
    };
#else
#define SIECS_CONTRACT_ENTRY_(name)
#endif

/* Define the contract body; the game calls name##_contract() after
 * ecs::init(). Calling it again in the same world is harmless. */
#define SIECS_CONTRACT(name)                                                                       \
    inline void siecs_contract_body_##name();                                                      \
    inline void name##_contract() {                                                                \
        ::siecs::detail::register_types();                                                         \
        siecs_contract_body_##name();                                                              \
    }                                                                                              \
    SIECS_CONTRACT_ENTRY_(name)                                                                    \
    inline void siecs_contract_body_##name()
