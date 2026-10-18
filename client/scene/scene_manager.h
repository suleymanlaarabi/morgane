#pragma once

#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <siecs.h>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace scene {

/** Instance membership, independent of the spatial hierarchy. Roots own themselves. */
struct Owner {};
struct Root {};
struct Active {};

/** Optional ordering dependency supplied by a renderer before importing a catalogue. */
struct RenderPipeline {
    ecs_system_id_t after = 0;
};

template <typename T> class Context : public ecs::source<Owner, T> {
    mutable ecs::entity _root;

  public:
    Context() = default;
    explicit Context(ecs::entity root) : _root(root) {}
    static Context init() noexcept { return {}; }
    void table(ecs_iter_t *it) noexcept {
        ecs::source<Owner, T>::table(it);
        const auto owner = ecs::detail::ecs_cpp_relation_id<Owner>();
        _root = ecs::entity::from(
            it->cache ? ecs_target_shared_id(it, owner) : ecs_target_id(it->entities[0], owner)
        );
    }
    ecs::entity root() const { return _root; }
    // Resolve at access time: spawning may move the root's component storage.
    T *operator->() const {
        if constexpr (std::is_const_v<T>)
            return &_root.get<std::remove_const_t<T>>();
        else
            return &_root.get_mut<T>();
    }
    T &operator*() const { return *operator->(); }
    T *get() const { return operator->(); }
    ecs::entity spawn() const
        requires(!std::is_const_v<T>)
    {
        return spawn(_root);
    }
    ecs::entity spawn(ecs::entity parent) const
        requires(!std::is_const_v<T>);
    void adopt(ecs::entity entity) const
        requires(!std::is_const_v<T>);
};

namespace detail {
template <typename Scene, typename Arg> inline constexpr bool context_matches = true;
template <typename Scene, typename T>
inline constexpr bool context_matches<Scene, Context<T>> =
    std::is_same_v<Scene, std::remove_const_t<T>>;
template <typename Scene, typename F> consteval bool valid_contexts() {
    using Args = typename ecs::function_traits<std::remove_cvref_t<F>>::args_tuple;
    return []<typename... A>(std::tuple<A...> *) {
        return (context_matches<Scene, A> && ...);
    }((Args *)nullptr);
}
struct DefinitionData {
    std::move_only_function<void(ecs::entity)> enter, exit, pause, resume;
    std::vector<std::move_only_function<void()>> systems;
    // Names remain stable through definition/resource moves and native registration.
    std::vector<std::unique_ptr<std::string>> names;
};
struct Instance {
    ecs::entity root;
    std::size_t type;
};
enum class Operation { push, pop, replace, reset };
struct Transition {
    Operation operation;
    std::size_t type = 0;
    std::move_only_function<void(ecs::entity)> initialize;
};
struct Runtime {
    Runtime() = default;
    Runtime(Runtime &&) = default;
    Runtime &operator=(Runtime &&) = default;
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    std::vector<DefinitionData> definitions;
    std::vector<Instance> stack;
    std::deque<Transition> pending;
    bool started = false;
    const void *catalogue = nullptr;
    void apply();
    void render();
    void clear();
    void remove_top();
    void suspend();
    void resume();
};
void install(Runtime runtime);
/** Register Owner as ByTarget; modules scoping by scene call it before touching Owner. */
void register_owner();
void adopt(ecs::entity root, ecs::entity entity);

// Force an entity callback for each(), including resource-only and empty signatures.
template <typename F, typename Args> struct EntityCallback;
template <typename F, typename... Args> struct EntityCallback<F, std::tuple<Args...>> {
    F callback;
    void operator()(ecs::entity, Args... args) { std::invoke(callback, args...); }
};
template <typename F> auto entity_callback(F &&callback) {
    using C = std::remove_cvref_t<F>;
    return EntityCallback<C, typename ecs::function_traits<C>::args_tuple>{
        std::forward<F>(callback)
    };
}
} // namespace detail

template <typename T>
ecs::entity Context<T>::spawn(ecs::entity parent) const
    requires(!std::is_const_v<T>)
{
    if (parent.id() != _root.id() &&
        (!parent.has_relation<Owner>() || parent.target<Owner>().id() != _root.id()))
        throw std::invalid_argument("Scene parent belongs to another instance");
    return ecs::entity::create().relate<Owner>(_root).child_of(parent);
}
template <typename T>
void Context<T>::adopt(ecs::entity entity) const
    requires(!std::is_const_v<T>)
{
    detail::adopt(_root, entity);
}

struct SystemHandle {
    std::shared_ptr<ecs_system_id_t> id;
};

template <typename T> class Definition;
template <typename T> class System : public ecs::system {
    detail::DefinitionData *_definition;
    ecs_phase_t _phase = EcsOnUpdate;
    std::vector<SystemHandle> _dependencies;

  public:
    System(detail::DefinitionData &definition, const char *name)
        : ecs::system(name), _definition(&definition) {}
    System &phase(ecs_phase_t phase) {
        _phase = phase;
        ecs::system::phase(phase);
        return *this;
    }
    template <typename... C> System &require() {
        ecs::system::require<C...>();
        return *this;
    }
    template <typename... C> System &optional() {
        ecs::system::optional<C...>();
        return *this;
    }
    template <typename... C> System &exclude() {
        ecs::system::exclude<C...>();
        return *this;
    }
    template <typename R> System &with_relation() {
        ecs::system::with_relation<R>();
        return *this;
    }
    template <typename R, typename... C> System &require_target() {
        ecs::system::require_target<R, C...>();
        return *this;
    }
    template <typename R, typename... C> System &exclude_target() {
        ecs::system::exclude_target<R, C...>();
        return *this;
    }
    template <typename R, typename... C> System &require_up() {
        ecs::system::require_up<R, C...>();
        return *this;
    }
    template <typename R, typename... C> System &exclude_up() {
        ecs::system::exclude_up<R, C...>();
        return *this;
    }
    System &immediate() {
        ecs::system::immediate();
        return *this;
    }
    System &interval(double seconds) {
        ecs::system::interval(seconds);
        return *this;
    }
    System &disabled(bool value = true) {
        ecs::system::disabled(value);
        return *this;
    }
    System &after(ecs_system_id_t id) {
        ecs::system::after(id);
        return *this;
    }
    System &after(SystemHandle handle) {
        _dependencies.push_back(std::move(handle));
        return *this;
    }
    System &order_by(ecs_query_order_t order) {
        ecs::system::order_by(order);
        return *this;
    }
    template <typename R> System &order_by_target() {
        ecs::system::order_by_target<R>();
        return *this;
    }
    template <typename R> System &order_by_depth() {
        ecs::system::order_by_depth<R>();
        return *this;
    }
    template <typename F> SystemHandle each(F &&callback) {
        static_assert(
            detail::valid_contexts<T, F>(),
            "Scene context does not match its definition"
        );
        auto handle = SystemHandle{ std::make_shared<ecs_system_id_t>(0) };
        ecs::system builder = *this;
        _definition->systems.emplace_back(
            [builder,
             phase = _phase,
             dependencies = std::move(_dependencies),
             handle,
             callback = detail::entity_callback(std::forward<F>(callback))]() mutable {
                builder.template exclude<Root>().template require_target<Owner, T>();
                if (phase == EcsPreRender || phase == EcsOnRender || phase == EcsPostRender)
                    builder.manual();
                else
                    builder.template require_target<Owner, Active>();
                for (auto &dependency : dependencies) {
                    if (!*dependency.id)
                        throw std::logic_error("Scene dependency must be registered first");
                    builder.after(*dependency.id);
                }
                *handle.id = builder.each(std::move(callback));
            }
        );
        return handle;
    }
};

template <typename T> class Definition {
    detail::DefinitionData _data;
    template <typename...> friend struct Catalog;
    template <typename F> static auto lifecycle(F &&callback) {
        static_assert(
            std::is_invocable_v<F &, Context<T>>,
            "Scene lifecycle callback must accept Context<T>"
        );
        return [callback = std::forward<F>(callback)](ecs::entity root) mutable {
            std::invoke(callback, Context<T>{ root });
        };
    }
    template <typename F> void frame(F &&callback, bool render) {
        static_assert(
            detail::valid_contexts<T, F>(),
            "Scene context does not match its definition"
        );
        using Args = typename ecs::function_traits<std::remove_cvref_t<F>>::args_tuple;
        static_assert(
            []<typename... A>(std::tuple<A...> *) {
                return (ecs::SystemParam<A> && ...);
            }((Args *)nullptr),
            "Scene frame hooks accept system parameters; use system().each() for entity components"
        );
        _data.systems.emplace_back([callback = detail::entity_callback(std::forward<F>(callback)),
                                    render]() mutable {
            auto builder = ecs::system(render ? "scene.RenderHook" : "scene.UpdateHook");
            builder.template require<Root, T>().phase(render ? EcsOnRender : EcsOnUpdate);
            if (render)
                builder.manual().immediate();
            else
                builder.template require_target<Owner, Active>();
            builder.each(std::move(callback));
        });
    }

  public:
    using scene_type = T;
    Definition() = default;
    Definition(Definition &&) = default;
    Definition &operator=(Definition &&) = default;
    Definition(const Definition &) = delete;
    template <typename F> Definition &on_enter(F &&f) & {
        _data.enter = lifecycle(std::forward<F>(f));
        return *this;
    }
    template <typename F> Definition &on_exit(F &&f) & {
        _data.exit = lifecycle(std::forward<F>(f));
        return *this;
    }
    template <typename F> Definition &on_pause(F &&f) & {
        _data.pause = lifecycle(std::forward<F>(f));
        return *this;
    }
    template <typename F> Definition &on_resume(F &&f) & {
        _data.resume = lifecycle(std::forward<F>(f));
        return *this;
    }
    template <typename F> Definition &on_update(F &&f) & {
        frame(std::forward<F>(f), false);
        return *this;
    }
    template <typename F> Definition &on_render(F &&f) & {
        frame(std::forward<F>(f), true);
        return *this;
    }
    template <typename F> Definition &&on_enter(F &&f) && {
        on_enter(std::forward<F>(f));
        return std::move(*this);
    }
    template <typename F> Definition &&on_exit(F &&f) && {
        on_exit(std::forward<F>(f));
        return std::move(*this);
    }
    template <typename F> Definition &&on_pause(F &&f) && {
        on_pause(std::forward<F>(f));
        return std::move(*this);
    }
    template <typename F> Definition &&on_resume(F &&f) && {
        on_resume(std::forward<F>(f));
        return std::move(*this);
    }
    template <typename F> Definition &&on_update(F &&f) && {
        on_update(std::forward<F>(f));
        return std::move(*this);
    }
    template <typename F> Definition &&on_render(F &&f) && {
        on_render(std::forward<F>(f));
        return std::move(*this);
    }
    System<T> system(const char *name) {
        _data.names.push_back(std::make_unique<std::string>(name));
        return System<T>{ _data, _data.names.back()->c_str() };
    }
};
template <typename T> Definition<T> define() { return {}; }

template <typename... Scenes> struct Catalog {
    static inline const char identity = 0;
    static detail::Runtime &runtime() {
        auto &state = ecs::resource<detail::Runtime>();
        if (state.catalogue != &identity)
            throw std::logic_error("Scene command belongs to another catalogue");
        return state;
    }
    template <typename T> static constexpr bool contains = (std::is_same_v<T, Scenes> || ...);
    static_assert(sizeof...(Scenes) > 0, "Scene catalogue cannot be empty");
    template <typename T> static consteval std::size_t index() {
        constexpr std::array matches{ std::is_same_v<T, Scenes>... };
        for (std::size_t i = 0; i < matches.size(); ++i)
            if (matches[i])
                return i;
        return matches.size();
    }
    static_assert(
        [] {
            constexpr std::array indices{ index<Scenes>()... };
            for (std::size_t i = 0; i < indices.size(); ++i)
                for (std::size_t j = i + 1; j < indices.size(); ++j)
                    if (indices[i] == indices[j])
                        return false;
            return true;
        }(),
        "Duplicate scene catalogue type"
    );
    template <typename T>
        requires contains<T>
    static void enqueue(detail::Operation operation, T value) {
        auto &runtime = Catalog::runtime();
        runtime.pending.push_back(
            { operation, index<T>(), [value = std::move(value)](ecs::entity root) mutable {
                 root.set(std::move(value));
             } }
        );
    }
    struct Commands {
        detail::Runtime *runtime = nullptr;
        static Commands init() { return {}; }
        static void query(ecs::system_param_query &q) { q.write_resource<detail::Runtime>(); }
        void world(ecs_world_t *) { runtime = &Catalog::runtime(); }
        template <typename T>
            requires contains<T>
        void push(T value = {}) {
            enqueue<T>(detail::Operation::push, std::move(value));
        }
        template <typename T>
            requires contains<T>
        void replace(T value = {}) {
            enqueue<T>(detail::Operation::replace, std::move(value));
        }
        template <typename T>
            requires contains<T>
        void reset(T value = {}) {
            enqueue<T>(detail::Operation::reset, std::move(value));
        }
        void pop() { runtime->pending.push_back({ detail::Operation::pop }); }
    };
    template <typename... T> static void import(Definition<T>... definitions) {
        static_assert(
            sizeof...(T) == sizeof...(Scenes),
            "Provide exactly one definition per catalogue scene"
        );
        static_assert((contains<T> && ...), "Definition is outside the scene catalogue");
        static_assert(((index<T>() < sizeof...(Scenes)) && ...));
        constexpr auto counts = []<typename U>() {
            return (std::size_t(std::is_same_v<U, T>) + ...);
        };
        static_assert(
            ((counts.template operator()<Scenes>() == 1) && ...),
            "Missing or duplicate scene definition"
        );
        detail::Runtime runtime;
        runtime.catalogue = &identity;
        runtime.definitions.resize(sizeof...(Scenes));
        ((runtime.definitions[index<T>()] = std::move(definitions._data)), ...);
        detail::install(std::move(runtime));
    }
    template <typename T>
        requires contains<T>
    static void start(T value = {}) {
        auto &runtime = Catalog::runtime();
        if (runtime.started)
            throw std::logic_error("Scene catalogue has already started");
        runtime.started = true;
        enqueue<T>(detail::Operation::reset, std::move(value));
    }
};

} // namespace scene
