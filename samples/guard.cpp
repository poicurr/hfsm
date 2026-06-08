#include <iostream>

#include <hfsm/hfsm.hpp>

enum class Event {
  kStart = 1,
};

struct Context {
  bool canStart = false;
};

int main() {
  hfsm::StateMachineBuilder<Context, Event> builder;

  hfsm::Callbacks<Context, Event> noop;
  auto root = builder.addComposite("Root", noop);
  auto stateIdle = builder.addLeaf("Idle", {});
  auto stateWarn = builder.addLeaf("Warn", {});
  auto stateRun = builder.addLeaf("Run", {});

  builder.setInitial(root, stateIdle);
  builder.addChild(root, stateWarn);
  builder.addChild(root, stateRun);

  const hfsm::TransitionHandle toWarn = builder.addTransition(
      stateIdle, Event::kStart, stateWarn, 1);
  builder.setGuard(toWarn, [](auto &&ctx) { return !ctx.context.canStart; });

  const hfsm::TransitionHandle toRun = builder.addTransition(
      stateIdle, Event::kStart, stateRun, 2);
  builder.setGuard(toRun, [](auto &&ctx) { return ctx.context.canStart; });

  auto buildResult = builder.build(root);
  if (!buildResult.ok()) {
    for (const auto &error : buildResult.errors)
      std::cerr << "build error: " << error << '\n';
    return 1;
  }

  hfsm::StateMachine<Context, Event> machine(std::move(*buildResult.definition));
  hfsm::StateMachine<Context, Event>::Instance blockedInst = machine.makeInstance();
  hfsm::StateMachine<Context, Event>::Instance acceptedInst = machine.makeInstance();

  Context blockedContext;
  std::cout << "blocked_initial="
            << std::string(machine.stateName(blockedInst.currentLeaf()))
            << '\n';
  machine.dispatch(blockedInst, Event::kStart, blockedContext);
  std::cout << "blocked="
            << std::string(machine.stateName(blockedInst.currentLeaf()))
            << '\n';

  Context acceptedContext;
  acceptedContext.canStart = true;
  machine.dispatch(
      acceptedInst, Event::kStart, acceptedContext);
  std::cout << "accepted="
            << std::string(machine.stateName(acceptedInst.currentLeaf()))
            << '\n';

  return 0;
}
