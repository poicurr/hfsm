#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include <hfsm/hfsm.hpp>

struct TestContext {
  bool canStart = false;
  int entryCount = 0;
  int exitCount = 0;
  int performCount = 0;
};

enum class ScopedEvent {
  kIdleToRunning = 1,
  kRetry = 2,
};

bool testBuilderWithScopedEventType() {
  hfsm::StateMachineBuilder<TestContext, ScopedEvent> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle idle = builder.addLeaf("Idle");
  const hfsm::StateHandle running = builder.addLeaf("Running");

  builder.setInitial(root, idle);
  builder.addChild(root, running);
  builder.addTransition(idle, ScopedEvent::kIdleToRunning, running);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  hfsm::StateMachine<TestContext, ScopedEvent> sm(
      std::move(*buildResult.definition));
  auto inst = sm.makeInstance();
  TestContext context;
  return sm.dispatch(inst, ScopedEvent::kIdleToRunning, context) &&
         inst.currentLeaf() == running;
}

bool testBuildFailure() {
  hfsm::StateMachineBuilder<TestContext> builder;
  const hfsm::StateHandle leaf = builder.addLeaf("RootAsLeaf");
  auto buildResult = builder.build(leaf);
  if (buildResult.ok())
    return false;
  return buildResult.formattedErrors().find("build: root must be composite") !=
         std::string::npos;
}

bool testGuardPassFail() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle idle = builder.addLeaf("Idle");
  const hfsm::StateHandle running = builder.addLeaf("Running");

  builder.setInitial(root, idle);
  builder.addChild(root, running);
  const hfsm::TransitionHandle toRunning =
      builder.addTransition(idle, 1, running);
  builder.setGuard(toRunning, [](auto &&ctx) { return ctx.context.canStart; });

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  hfsm::StateMachine<TestContext> sm(std::move(*buildResult.definition));
  auto inst = sm.makeInstance();
  TestContext context;

  bool ok = !sm.dispatch(inst, 1, context);
  const bool blocked = (inst.currentLeaf() == idle);
  context.canStart = true;
  ok = ok && sm.dispatch(inst, 1, context);
  const bool accepted = (inst.currentLeaf() == running);
  return ok && blocked && accepted;
}

bool testPriority() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle a = builder.addLeaf("A");
  const hfsm::StateHandle b = builder.addLeaf("B");
  const hfsm::StateHandle c = builder.addLeaf("C");

  builder.setInitial(root, a);
  builder.addChild(root, b);
  builder.addChild(root, c);

  builder.addTransition(a, 1, b, 0);
  builder.addTransition(a, 1, c, 10);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  hfsm::StateMachine<TestContext> sm(std::move(*buildResult.definition));
  auto inst = sm.makeInstance();
  TestContext context;
  const bool ok = sm.dispatch(inst, 1, context);
  return ok && inst.currentLeaf() == c;
}

bool testSamePriorityStableOrder() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle a = builder.addLeaf("A");
  const hfsm::StateHandle b = builder.addLeaf("B");
  const hfsm::StateHandle c = builder.addLeaf("C");

  builder.setInitial(root, a);
  builder.addChild(root, b);
  builder.addChild(root, c);

  builder.addTransition(a, 1, b, 0);
  builder.addTransition(a, 1, c, 0);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  hfsm::StateMachine<TestContext> sm(std::move(*buildResult.definition));
  auto inst = sm.makeInstance();
  TestContext context;
  const bool ok = sm.dispatch(inst, 1, context);
  return ok && inst.currentLeaf() == b;
}

bool testStateMachineRejectsInvalidDefinition() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle child = builder.addLeaf("Child");

  builder.setInitial(root, child);
  builder.addTransition(child, 1, child, 0);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  auto definition = std::move(*buildResult.definition);
  definition.states[hfsm::stateIndex(child)].outgoing.push_back(
      hfsm::TransitionHandle{99});

  hfsm::StateMachine<TestContext> sm(std::move(definition));
  return !sm.isValid();
}

bool testStateMachineRejectsInvalidParentHandle() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle child = builder.addLeaf("Child");

  builder.setInitial(root, child);
  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  auto definition = std::move(*buildResult.definition);
  definition.states[hfsm::stateIndex(child)].parent.value = 999;

  hfsm::StateMachine<TestContext> sm(std::move(definition));
  return !sm.isValid();
}

bool testStateMachineRejectsLeafHasChildren() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle leaf = builder.addLeaf("Leaf");

  builder.setInitial(root, leaf);
  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  auto definition = std::move(*buildResult.definition);
  definition.states[hfsm::stateIndex(leaf)].children.push_back(root);

  hfsm::StateMachine<TestContext> sm(std::move(definition));
  return !sm.isValid();
}

bool testStateMachineRejectsLeafInitialChild() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle leaf = builder.addLeaf("Leaf");

  builder.setInitial(root, leaf);
  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  auto definition = std::move(*buildResult.definition);
  definition.states[hfsm::stateIndex(leaf)].initialChild = root;

  hfsm::StateMachine<TestContext> sm(std::move(definition));
  return !sm.isValid();
}

bool testStateMachineRejectsOutgoingTransitionReferencedTwice() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle child = builder.addLeaf("Child");

  builder.setInitial(root, child);
  const hfsm::TransitionHandle transition =
      builder.addTransition(child, 1, child, 0);
  if (transition == hfsm::INVALID_TRANSITION)
    return false;

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  auto definition = std::move(*buildResult.definition);
  definition.states[hfsm::stateIndex(child)].outgoing.push_back(transition);

  hfsm::StateMachine<TestContext> sm(std::move(definition));
  return !sm.isValid();
}

bool testStateMachineNormalizesCompositeTransitionTarget() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle active = builder.addLeaf("Active");
  const hfsm::StateHandle sub = builder.addComposite("Sub");
  const hfsm::StateHandle subLeaf = builder.addLeaf("SubLeaf");

  builder.setInitial(root, active);
  builder.addChild(root, sub);
  builder.setInitial(sub, subLeaf);
  builder.addTransition(active, 1, sub, 0);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  auto definition = std::move(*buildResult.definition);
  definition.states[hfsm::stateIndex(sub)].initialChild = hfsm::INVALID_STATE;

  hfsm::StateMachine<TestContext> sm(std::move(definition));
  if (!sm.isValid())
    return false;

  auto inst = sm.makeInstance();
  TestContext context;
  if (!sm.dispatch(inst, 1, context))
    return false;

  return inst.currentLeaf() == subLeaf;
}

bool testStateMachineRejectsUnreachableTransitionEndpoint() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle child = builder.addLeaf("Child");

  builder.setInitial(root, child);
  builder.addTransition(child, 1, child, 0);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  auto definition = std::move(*buildResult.definition);
  const auto unreachable = hfsm::stateHandleFromIndex(definition.states.size());
  definition.states.emplace_back();
  definition.transitions[0].to = unreachable;

  hfsm::StateMachine<TestContext> sm(std::move(definition));
  return !sm.isValid();
}

bool testStateMachineRejectsOutgoingTransitionFromMismatch() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle child = builder.addLeaf("Child");

  builder.setInitial(root, child);
  const hfsm::TransitionHandle transition =
      builder.addTransition(child, 1, child, 0);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  auto definition = std::move(*buildResult.definition);
  definition.states[hfsm::stateIndex(root)].outgoing.push_back(transition);

  hfsm::StateMachine<TestContext> sm(std::move(definition));
  if (sm.isValid())
    return false;

  const auto &errors = sm.validationErrors();
  return std::find_if(
             errors.begin(), errors.end(), [](const std::string &error) {
               return error.find("outgoing transition from mismatch") !=
                      std::string::npos;
             }) != errors.end();
}

bool testStateMachineRejectsUnreferencedTransition() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle child = builder.addLeaf("Child");

  builder.setInitial(root, child);
  builder.addTransition(child, 1, child, 0);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  auto definition = std::move(*buildResult.definition);
  definition.states[hfsm::stateIndex(child)].outgoing.clear();

  hfsm::StateMachine<TestContext> sm(std::move(definition));
  return !sm.isValid();
}

bool testInvalidInstanceNoOp() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle leaf = builder.addLeaf("Leaf");

  builder.setInitial(root, leaf);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  hfsm::StateMachine<TestContext> sm(std::move(*buildResult.definition));
  hfsm::StateMachine<TestContext>::Instance instance{};
  TestContext context;

  const int beforeEntry = context.entryCount;
  const int beforeExit = context.exitCount;
  const int beforePerform = context.performCount;

  if (sm.dispatch(instance, 1, context))
    return false;
  sm.tick(instance, 1, context);

  return context.entryCount == beforeEntry && context.exitCount == beforeExit &&
         context.performCount == beforePerform;
}

bool testCycleBuildFailure() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle parent = builder.addComposite("Parent");
  const hfsm::StateHandle child = builder.addComposite("Child");

  builder.setInitial(root, parent);
  builder.addChild(root, parent);
  builder.addChild(parent, child);
  builder.addChild(child, parent);

  auto buildResult = builder.build(root);
  return !buildResult.ok();
}

bool testDuplicateChildIgnored() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle child = builder.addLeaf("Child");

  builder.setInitial(root, child);
  builder.addChild(root, child);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  const auto &states = buildResult.definition->states;
  return states[hfsm::stateIndex(root)].children.size() == 1;
}

bool testParentTransition() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle idle = builder.addLeaf("Idle");
  const hfsm::StateHandle run = builder.addLeaf("Run");

  builder.setInitial(root, idle);
  builder.addChild(root, run);
  builder.addTransition(root, 1, run, 0);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  hfsm::StateMachine<TestContext> sm(std::move(*buildResult.definition));
  auto inst = sm.makeInstance();
  TestContext context;

  if (!sm.dispatch(inst, 1, context))
    return false;

  return inst.currentLeaf() == run;
}

bool testSelfTransitionOrder() {
  hfsm::StateMachineBuilder<TestContext> builder;
  hfsm::Callbacks<TestContext> callbacks;
  callbacks.onEntry = [](TestContext &ctx, hfsm::EventId) { ++ctx.entryCount; };
  callbacks.onExit = [](TestContext &ctx, hfsm::EventId) { ++ctx.exitCount; };
  callbacks.onPerform = [](TestContext &ctx, hfsm::EventId) {
    ++ctx.performCount;
  };

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle leaf = builder.addLeaf("Leaf", callbacks);

  builder.setInitial(root, leaf);
  builder.addTransition(leaf, 1, leaf, 0);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  hfsm::StateMachine<TestContext> sm(std::move(*buildResult.definition));
  auto inst = sm.makeInstance();
  TestContext context;

  if (!sm.dispatch(inst, 1, context))
    return false;

  return context.entryCount == 0 && context.exitCount == 0 &&
         context.performCount == 1 && inst.currentLeaf() == leaf;
}

bool testLifecycleCallbackOrder() {
  hfsm::StateMachineBuilder<TestContext> builder;
  std::vector<std::string> log;

  const auto makeCallbacks = [&](std::string label) {
    hfsm::Callbacks<TestContext> callbacks;
    callbacks.onEntry = [&, label](TestContext &, hfsm::EventId) {
      log.emplace_back(std::string(label) + ":entry");
    };
    callbacks.onExit = [&, label](TestContext &, hfsm::EventId) {
      log.emplace_back(std::string(label) + ":exit");
    };
    callbacks.onPerform = [&, label](TestContext &, hfsm::EventId) {
      log.emplace_back(std::string(label) + ":perform");
    };
    return callbacks;
  };

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle a = builder.addComposite("A", makeCallbacks("A"));
  const hfsm::StateHandle b = builder.addComposite("B", makeCallbacks("B"));
  const hfsm::StateHandle a1 = builder.addLeaf("A1", makeCallbacks("A1"));
  const hfsm::StateHandle b1 = builder.addLeaf("B1", makeCallbacks("B1"));

  builder.setInitial(root, a);
  builder.addChild(root, b);
  builder.setInitial(a, a1);
  builder.setInitial(b, b1);

  builder.addTransition(a1, 1, b1, 0);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  hfsm::StateMachine<TestContext> sm(std::move(*buildResult.definition));
  auto inst = sm.makeInstance();
  TestContext context;
  if (!sm.dispatch(inst, 1, context))
    return false;

  const std::vector<std::string> expected{"A1:exit", "A:exit", "B:entry",
                                          "B1:entry", "B1:perform"};
  return inst.currentLeaf() == b1 && log == expected;
}

bool testTickCallsPerformOnly() {
  hfsm::StateMachineBuilder<TestContext> builder;

  const auto callbacks = []() {
    hfsm::Callbacks<TestContext> cb;
    cb.onEntry = [](TestContext &ctx, hfsm::EventId) { ++ctx.entryCount; };
    cb.onExit = [](TestContext &ctx, hfsm::EventId) { ++ctx.exitCount; };
    cb.onPerform = [](TestContext &ctx, hfsm::EventId) { ++ctx.performCount; };
    return cb;
  }();

  const hfsm::StateHandle root = builder.addComposite("Root");
  const hfsm::StateHandle leaf = builder.addLeaf("Leaf", callbacks);

  builder.setInitial(root, leaf);

  auto buildResult = builder.build(root);
  if (!buildResult.ok())
    return false;

  hfsm::StateMachine<TestContext> sm(std::move(*buildResult.definition));
  auto inst = sm.makeInstance();
  TestContext context;
  sm.tick(inst, 1, context);

  return context.entryCount == 0 && context.exitCount == 0 &&
         context.performCount == 1;
}

int main() {
  bool ok = true;

  ok = ok && testBuildFailure();
  ok = ok && testGuardPassFail();
  ok = ok && testBuilderWithScopedEventType();
  ok = ok && testPriority();
  ok = ok && testSamePriorityStableOrder();
  ok = ok && testStateMachineRejectsInvalidDefinition();
  ok = ok && testStateMachineRejectsInvalidParentHandle();
  ok = ok && testStateMachineRejectsLeafHasChildren();
  ok = ok && testStateMachineRejectsLeafInitialChild();
  ok = ok && testStateMachineRejectsUnreachableTransitionEndpoint();
  ok = ok && testStateMachineRejectsOutgoingTransitionReferencedTwice();
  ok = ok && testStateMachineRejectsOutgoingTransitionFromMismatch();
  ok = ok && testStateMachineRejectsUnreferencedTransition();
  ok = ok && testStateMachineNormalizesCompositeTransitionTarget();
  ok = ok && testInvalidInstanceNoOp();
  ok = ok && testCycleBuildFailure();
  ok = ok && testDuplicateChildIgnored();
  ok = ok && testParentTransition();
  ok = ok && testSelfTransitionOrder();
  ok = ok && testLifecycleCallbackOrder();
  ok = ok && testTickCallsPerformOnly();

  if (!ok) {
    std::cerr << "hfsm unit tests FAILED\n";
    return 1;
  }

  std::cout << "hfsm unit tests PASSED\n";
  return 0;
}
