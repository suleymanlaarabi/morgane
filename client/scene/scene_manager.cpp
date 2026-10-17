#include "scene_manager.h"

namespace scene::detail {

void Runtime::suspend() {
    if (stack.empty())
        return;
    auto instance = stack.back();
    if (definitions[instance.type].pause)
        definitions[instance.type].pause(instance.root);
    instance.root.remove<Active>().add<SimulationPaused>();
}

void Runtime::resume() {
    if (stack.empty())
        return;
    auto instance = stack.back();
    instance.root.remove<SimulationPaused>().add<Active>();
    if (definitions[instance.type].resume)
        definitions[instance.type].resume(instance.root);
}

void Runtime::remove_top() {
    if (stack.empty())
        return;
    auto instance = stack.back();
    if (definitions[instance.type].exit)
        definitions[instance.type].exit(instance.root);
    instance.root.kill();
    stack.pop_back();
}

void Runtime::clear() {
    while (!stack.empty())
        remove_top();
}

void Runtime::apply() {
    const auto count = pending.size();
    for (std::size_t i = 0; i < count; ++i) {
        auto transition = std::move(pending.front());
        pending.pop_front();
        if (transition.operation == Operation::pop) {
            if (!stack.empty()) {
                remove_top();
                resume();
            }
            continue;
        }
        // Construct the state before destroying any existing scene.
        auto root = ecs::entity::create().add<Root>();
        try {
            transition.initialize(root);
            root.relate<Owner>(root);
        } catch (...) {
            root.kill();
            throw;
        }
        try {
            if (transition.operation == Operation::push)
                suspend();
            else if (transition.operation == Operation::replace)
                remove_top();
            else
                clear();
            stack.push_back({ root, transition.type });
            root.add<Active>();
            auto &definition = definitions[transition.type];
            if (definition.enter)
                definition.enter(root);
        } catch (...) {
            root.kill();
            if (!stack.empty() && stack.back().root.id() == root.id())
                stack.pop_back();
            if (!stack.empty() && stack.back().root.has<SimulationPaused>())
                resume();
            throw;
        }
    }
}

void Runtime::render() {
    auto pass = [](ecs::entity root) {
        const auto relation = ecs::detail::ecs_cpp_relation_id<Owner>();
        ecs_run_phase_target(EcsPreRender, relation, root.id());
        ecs_run_phase_target(EcsOnRender, relation, root.id());
        ecs_run_phase_target(EcsPostRender, relation, root.id());
    };
    pass(ecs::entity::null());
    for (const auto &instance : stack)
        pass(instance.root);
}

void register_owner() {
    ecs::relation<Owner>(
        { .storage = EcsRelationByTarget, .on_delete_target = EcsRemoveRelation, .acyclic = false }
    );
}

void install(Runtime runtime) {
    if (ecs::has_resource<Runtime>())
        throw std::logic_error("Only one scene catalogue may be imported per world");
    register_owner();
    ecs::set_resource(std::move(runtime));
    auto &state = ecs::resource<Runtime>();
    for (auto &definition : state.definitions) {
        for (auto &register_system : definition.systems)
            register_system();
        definition.systems.clear();
    }
    const ecs_phase_desc_t phase{ .name = "scene.Apply",
                                  .after = ECS_PHASE_NONE,
                                  .before = EcsOnLoad };
    auto apply_phase = ecs_phase_init(&phase);
    ecs::system("scene.Apply")
        .phase(apply_phase)
        .immediate()
        .no_defer()
        .each([](ecs::res<Runtime> state) { state->apply(); });
    auto render = ecs::system("scene.Render").phase(EcsOnRender).immediate().no_defer();
    if (auto pipeline = ecs::try_resource<RenderPipeline>(); pipeline && pipeline->after)
        render.after(pipeline->after);
    render.each([](ecs::res<Runtime> state) { state->render(); });
    ecs::at_fini([] { ecs::resource<Runtime>().clear(); });
}

void adopt(ecs::entity root, ecs::entity entity) {
    if (!entity.is_alive() || entity.has<Root>() || entity.id() == root.id())
        throw std::invalid_argument("Cannot adopt a scene root or dead entity");
    // Validate the complete subtree before changing ownership or hierarchy.
    std::vector<ecs::entity> subtree{ entity };
    for (std::size_t i = 0; i < subtree.size(); ++i) {
        auto current = subtree[i];
        if (current.has<Root>() ||
            (current.has_relation<Owner>() && current.target<Owner>().id() != root.id()))
            throw std::invalid_argument("Entity belongs to another scene instance");
        auto children =
            ecs_relation_sources(current.id(), ecs::detail::ecs_cpp_relation_id<ecs::ChildOf>());
        for (std::uint32_t j = 0; j < children.count; ++j)
            subtree.emplace_back(children.entities[j]);
    }
    for (auto current : subtree)
        current.relate<Owner>(root);
    auto parent = entity.target<ecs::ChildOf>();
    if (parent.id() != root.id() && (!parent.is_alive() || !parent.has_relation<Owner>() ||
                                     parent.target<Owner>().id() != root.id()))
        entity.child_of(root);
}

} // namespace scene::detail
