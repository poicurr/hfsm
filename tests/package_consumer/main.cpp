#include <hfsm/hfsm.hpp>

enum class Event {
  kStart,
  kFrame,
};

struct Context {
  int performed = 0;
};

int main() {
  hfsm::StateMachineBuilder<Context, Event> builder;

  hfsm::Callbacks<Context, Event> callbacks;
  callbacks.onPerform = [](Context &ctx, Event) { ++ctx.performed; };

  const auto root = builder.addComposite("Root");
  const auto idle = builder.addLeaf("Idle", std::move(callbacks));
  builder.setInitial(root, idle);

  auto result = builder.build(root);
  if (!result.ok()) {
    return 1;
  }

  hfsm::StateMachine<Context, Event> machine(std::move(*result.definition));
  auto instance = machine.makeInstance();

  if (instance.currentLeaf() != idle) {
    return 2;
  }

  Context context;
  machine.tick(instance, Event::kFrame, context);

  return context.performed == 1 ? 0 : 3;
}
