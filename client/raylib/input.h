#pragma once

#include "ray.h"
#include <array>
#include <bitset>
#include <cstddef>
#include <initializer_list>

namespace ray {

template <typename A> inline constexpr std::size_t action_count = static_cast<std::size_t>(A::count);

/** Resource: keys bound to each action of the enum `A` (terminated by `A::count`). */
template <typename A> struct Bindings {
    static constexpr std::size_t max_keys = 3;
    std::array<std::array<KeyboardKey, max_keys>, action_count<A>> keys{};

    /** Replace the keys of `action` (extra keys beyond max_keys are ignored). */
    Bindings &bind(A action, std::initializer_list<KeyboardKey> list) {
        auto &slot = keys[static_cast<std::size_t>(action)];
        slot.fill(KEY_NULL);
        std::size_t i = 0;
        for (auto key : list)
            if (i < max_keys)
                slot[i++] = key;
        return *this;
    }
};

/** Resource: action state for the current frame. */
template <typename A> struct Input {
    std::bitset<action_count<A>> current;
    std::bitset<action_count<A>> previous;

    bool down(A action) const { return current[index(action)]; }
    bool pressed(A action) const { return current[index(action)] && !previous[index(action)]; }
    bool released(A action) const { return !current[index(action)] && previous[index(action)]; }
    float axis(A negative, A positive) const { return float(down(positive)) - float(down(negative)); }

  private:
    static constexpr std::size_t index(A action) { return static_cast<std::size_t>(action); }
};

/** Module: maps the keyboard to the typed actions `A` every frame. */
template <typename A> struct Inputs {
    static void import(const Bindings<A> &bindings) {
        ecs::set_resource(bindings);
        ecs::set_resource(Input<A>{});

        ecs::system("ray.Input")
            .phase(EcsOnLoad)
            .immediate()
            .each([](ecs::res<const Bindings<A>> bindings, ecs::res<Input<A>> input) {
                input->previous = input->current;
                for (std::size_t action = 0; action < action_count<A>; action++) {
                    bool down = false;
                    for (auto key : bindings->keys[action])
                        down |= key != KEY_NULL && IsKeyDown(key);
                    input->current[action] = down;
                }
            });
    }
};

} // namespace ray
