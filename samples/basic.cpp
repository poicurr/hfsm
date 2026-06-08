#include <iostream>
#include <string>
#include <string_view>

#include <hfsm/hfsm.hpp>

enum class Event {
  kPowerOff = 0,
  kPowerOn = 1,
  kResume = 2,
  kPause = 3,
  kNext = 4,
  kStop = 5,
};

struct Context {
  bool allowResume = false;
};

void logState(std::string_view phase,
              const hfsm::StateMachine<Context, Event>::Instance &inst,
              const hfsm::StateMachine<Context, Event> &machine) {
  std::cout << "  " << phase << ": "
            << std::string(machine.stateName(inst.currentLeaf()))
            << '\n';
}

void dispatchAndLog(hfsm::StateMachine<Context, Event> &machine,
                    hfsm::StateMachine<Context, Event>::Instance &state,
                    Event event, Context &context, const char *label) {
  const bool ok = machine.dispatch(state, event, context);
  std::cout << label << " -> " << (ok ? "ok" : "ignore");
  if (ok)
    logState("next", state, machine);
  else
    std::cout << '\n';
}

int main() {
  hfsm::StateMachineBuilder<Context, Event> builder;

  hfsm::Callbacks<Context, Event> rootCb;
  const hfsm::StateHandle root = builder.addComposite("PlayerRoot", rootCb);

  auto makeLeaf = [](std::string_view name) {
    hfsm::Callbacks<Context, Event> cb;
    cb.onEntry = [name](Context &, Event) {
      std::cout << "    onEntry " << name << '\n';
    };
    cb.onPerform = [name](Context &, Event) {
      std::cout << "    onPerform " << name << '\n';
    };
    cb.onExit = [name](Context &, Event) {
      std::cout << "    onExit " << name << '\n';
    };
    return cb;
  };

  const hfsm::StateHandle off = builder.addLeaf("Off", makeLeaf("Off"));
  const hfsm::StateHandle active =
      builder.addComposite("Active", hfsm::Callbacks<Context, Event>{});
  const hfsm::StateHandle paused =
      builder.addLeaf("Paused", makeLeaf("Paused"));
  const hfsm::StateHandle running =
      builder.addComposite("Running", hfsm::Callbacks<Context, Event>{});
  const hfsm::StateHandle runIdle =
      builder.addLeaf("RunIdle", makeLeaf("RunIdle"));
  const hfsm::StateHandle runFast =
      builder.addLeaf("RunFast", makeLeaf("RunFast"));

  builder.setInitial(root, off);
  builder.addChild(root, active);
  builder.setInitial(active, paused);
  builder.setInitial(running, runIdle);
  builder.addChild(active, running);
  builder.addChild(running, runFast);

  builder.addTransition(off, Event::kPowerOn,
                        active);

  const hfsm::TransitionHandle resumeHandle = builder.addTransition(
      paused, Event::kResume, running, 1);
  builder.setGuard(resumeHandle,
                   [](auto &&ctx) { return ctx.context.allowResume; });

  builder.addTransition(runIdle, Event::kPause,
                        paused);
  builder.addTransition(runFast, Event::kPause,
                        paused);
  builder.addTransition(runIdle, Event::kNext,
                        runFast);
  builder.addTransition(runFast, Event::kNext,
                        runIdle);
  builder.addTransition(runIdle, Event::kStop, off);
  builder.addTransition(runFast, Event::kStop, off);
  builder.addTransition(running, Event::kPowerOff,
                        off);
  builder.addTransition(paused, Event::kPowerOff,
                        off);
  builder.addTransition(off, Event::kPowerOff, off);

  auto buildResult = builder.build(root);
  if (!buildResult.ok()) {
    for (const auto &error : buildResult.errors)
      std::cerr << "build error: " << error << '\n';
    return 1;
  }

  hfsm::StateMachine<Context, Event> machine(std::move(*buildResult.definition));
  hfsm::StateMachine<Context, Event>::Instance state = machine.makeInstance();
  Context context;

  std::cout << "initial: "
            << std::string(machine.stateName(state.currentLeaf()))
            << '\n';

  dispatchAndLog(machine, state, Event::kPowerOn,
                 context, "power on");
  dispatchAndLog(machine, state, Event::kResume,
                 context, "resume before allow");

  context.allowResume = true;
  dispatchAndLog(machine, state, Event::kResume,
                 context, "resume");
  dispatchAndLog(machine, state, Event::kNext,
                 context, "next");
  dispatchAndLog(machine, state, Event::kPause,
                 context, "pause");
  dispatchAndLog(machine, state, Event::kResume,
                 context, "resume from pause");
  dispatchAndLog(machine, state, Event::kPowerOff,
                 context, "power off");
}
