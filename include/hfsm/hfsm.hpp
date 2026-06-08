#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hfsm {

using EventId = int;

struct StateHandle {
  int value = -1;

  constexpr StateHandle() = default;
  constexpr explicit StateHandle(int value) : value(value) {}

  [[nodiscard]] constexpr bool isValid() const { return value >= 0; }
};

struct TransitionHandle {
  int value = -1;

  constexpr TransitionHandle() = default;
  constexpr explicit TransitionHandle(int value) : value(value) {}

  [[nodiscard]] constexpr bool isValid() const { return value >= 0; }
};

inline constexpr bool operator==(const StateHandle &lhs,
                                 const StateHandle &rhs) {
  return lhs.value == rhs.value;
}

inline constexpr bool operator!=(const StateHandle &lhs,
                                 const StateHandle &rhs) {
  return lhs.value != rhs.value;
}

inline constexpr bool operator==(const TransitionHandle &lhs,
                                 const TransitionHandle &rhs) {
  return lhs.value == rhs.value;
}

inline constexpr bool operator!=(const TransitionHandle &lhs,
                                 const TransitionHandle &rhs) {
  return lhs.value != rhs.value;
}

inline constexpr StateHandle INVALID_STATE{-1};
inline constexpr TransitionHandle INVALID_TRANSITION{-1};

[[nodiscard]] constexpr StateHandle stateHandleFromIndex(std::size_t index) {
  return StateHandle{static_cast<int>(index)};
}

[[nodiscard]] constexpr TransitionHandle
transitionHandleFromIndex(std::size_t index) {
  return TransitionHandle{static_cast<int>(index)};
}

[[nodiscard]] inline std::size_t stateIndex(StateHandle stateHandle) {
  return static_cast<std::size_t>(stateHandle.value);
}

[[nodiscard]] inline std::size_t
transitionIndex(TransitionHandle transitionHandle) {
  return static_cast<std::size_t>(transitionHandle.value);
}

[[nodiscard]] inline bool isValidStateHandle(std::size_t stateCount,
                                             StateHandle stateHandle) {
  return stateHandle.value >= 0 && stateIndex(stateHandle) < stateCount;
}

[[nodiscard]] inline bool
isValidTransitionHandle(std::size_t transitionCount,
                        TransitionHandle transitionHandle) {
  return transitionHandle.value >= 0 &&
         transitionIndex(transitionHandle) < transitionCount;
}

template <class ContextType, class EventType = EventId>
struct GuardContext {
  ContextType &context;
  EventType event;
};

template <class ContextType, class EventType = EventId>
using Guard = std::function<bool(GuardContext<ContextType, EventType> &)>;

template <class ContextType, class EventType = EventId>
using Callback = std::function<void(ContextType &, EventType)>;

template <class ContextType, class EventType = EventId>
struct Callbacks {
  Callback<ContextType, EventType> onEntry;
  Callback<ContextType, EventType> onPerform;
  Callback<ContextType, EventType> onExit;
};

template <class ContextType, class EventType = EventId>
struct TransitionDef {
  StateHandle from = INVALID_STATE;
  StateHandle to = INVALID_STATE;
  EventType event = EventType{};
  int priority = 0;
  int order = 0;
  Guard<ContextType, EventType> guard;
};

template <class ContextType, class EventType = EventId>
struct StateDef {
  std::string name;
  Callbacks<ContextType, EventType> callbacks;
  bool isComposite = false;
  StateHandle parent = INVALID_STATE;
  StateHandle initialChild = INVALID_STATE;
  std::vector<StateHandle> children;
  std::vector<TransitionHandle> outgoing;
};

template <class ContextType, class EventType = EventId>
struct MachineDefinition {
  std::vector<StateDef<ContextType, EventType>> states;
  std::vector<TransitionDef<ContextType, EventType>> transitions;
  StateHandle root = INVALID_STATE;
};

template <class ContextType, class EventType = EventId>
struct BuildResult {
  std::optional<MachineDefinition<ContextType, EventType>> definition;
  std::vector<std::string> errors;

  [[nodiscard]] bool ok() const { return definition.has_value(); }

  [[nodiscard]] std::string
  formattedErrors(std::string_view separator = "\n") const {
    std::string message;
    for (std::size_t i = 0; i < errors.size(); ++i) {
      if (i > 0)
        message += separator;
      message += errors[i];
    }
    return message;
  }
};

//============================================================
// StateMachineBuilder
//============================================================
// 1) addLeaf / addComposite
// 2) setInitial / addChild / addTransition
// 3) build(root) -> BuildResult
//============================================================
template <class ContextType, class EventType = EventId>
class StateMachineBuilder {
public:
  StateMachineBuilder() = default;

  StateHandle addLeaf(std::string name,
                      Callbacks<ContextType, EventType> callbacks = {}) {
    StateDef<ContextType, EventType> state;
    state.name = std::move(name);
    state.callbacks = std::move(callbacks);
    state.isComposite = false;
    m_states.push_back(std::move(state));
    return stateHandleFromIndex(m_states.size() - 1);
  }

  StateHandle addComposite(std::string name,
                           Callbacks<ContextType, EventType> callbacks = {}) {
    StateDef<ContextType, EventType> state;
    state.name = std::move(name);
    state.callbacks = std::move(callbacks);
    state.isComposite = true;
    m_states.push_back(std::move(state));
    return stateHandleFromIndex(m_states.size() - 1);
  }

  void addChild(StateHandle parent, StateHandle child) {
    if (!isValidState(parent) || !isValidState(child)) {
      m_errors.push_back("addChild: invalid state handle");
      return;
    }
    if (!m_states[stateIndex(parent)].isComposite) {
      m_errors.push_back("addChild: parent is not composite");
      return;
    }
    if (m_states[stateIndex(child)].parent != INVALID_STATE &&
        m_states[stateIndex(child)].parent != parent) {
      m_errors.push_back("addChild: child already has different parent");
      return;
    }
    auto &children = m_states[stateIndex(parent)].children;
    if (std::find(children.begin(), children.end(), child) != children.end()) {
      return;
    }
    m_states[stateIndex(child)].parent = parent;
    m_states[stateIndex(parent)].children.push_back(child);
  }

  void setInitial(StateHandle parent, StateHandle child) {
    if (!isValidState(parent) || !isValidState(child)) {
      m_errors.push_back("setInitial: invalid state handle");
      return;
    }
    if (!m_states[stateIndex(parent)].isComposite) {
      m_errors.push_back("setInitial: parent is not composite");
      return;
    }
    const std::size_t errorCount = m_errors.size();
    addChild(parent, child);
    if (m_errors.size() != errorCount)
      return;
    m_states[stateIndex(parent)].initialChild = child;
  }

  TransitionHandle addTransition(StateHandle from, EventType event,
                                 StateHandle to, int priority = 0) {
    if (!isValidState(from) || !isValidState(to)) {
      m_errors.push_back("addTransition: invalid transition endpoint");
      return INVALID_TRANSITION;
    }
    TransitionDef<ContextType, EventType> transition;
    transition.from = from;
    transition.to = to;
    transition.event = event;
    transition.priority = priority;
    transition.order = m_nextOrder++;
    m_transitions.push_back(std::move(transition));

    const TransitionHandle handle =
        transitionHandleFromIndex(m_transitions.size() - 1);
    m_states[stateIndex(from)].outgoing.push_back(handle);
    return handle;
  }

  void setGuard(TransitionHandle handle, Guard<ContextType, EventType> guard) {
    if (!isValidTransitionHandle(m_transitions.size(), handle)) {
      m_errors.push_back("setGuard: invalid transition handle");
      return;
    }
    m_transitions[transitionIndex(handle)].guard = std::move(guard);
  }

  BuildResult<ContextType, EventType> build(StateHandle root) {
    BuildResult<ContextType, EventType> buildResult;
    MachineDefinition<ContextType, EventType> result;
    result.states = m_states;
    result.transitions = m_transitions;
    result.root = root;

    validate(result);
    sortTransitions(result);

    buildResult.errors = std::move(m_errors);
    if (buildResult.errors.empty()) {
      buildResult.definition = std::move(result);
    }

    resetInternal();
    return buildResult;
  }

private:
  bool isValidState(StateHandle handle) const {
    return isValidStateHandle(m_states.size(), handle);
  }

  static void
  dfsVisit(const std::vector<StateDef<ContextType, EventType>> &states,
           StateHandle state, std::vector<char> &visited) {
    if (state == INVALID_STATE)
      return;
    if (visited[stateIndex(state)])
      return;
    visited[stateIndex(state)] = 1;
    for (StateHandle child : states[stateIndex(state)].children)
      dfsVisit(states, child, visited);
  }

  static bool
  hasParentCycle(const std::vector<StateDef<ContextType, EventType>> &states,
                 StateHandle start, std::vector<char> &seen,
                 std::vector<char> &inStack) {
    if (start == INVALID_STATE)
      return false;

    const std::size_t startIndex = stateIndex(start);
    if (inStack[startIndex])
      return true;
    if (seen[startIndex])
      return false;

    inStack[startIndex] = 1;
    const StateHandle parent = states[startIndex].parent;
    const bool cycleFound = hasParentCycle(states, parent, seen, inStack);
    inStack[startIndex] = 0;
    seen[startIndex] = 1;
    return cycleFound;
  }

  static bool isChildOf(const StateDef<ContextType, EventType> &stateDef,
                        StateHandle child) {
    return std::find(stateDef.children.begin(), stateDef.children.end(),
                     child) != stateDef.children.end();
  }

  void validate(MachineDefinition<ContextType, EventType> &result) {
    if (result.root == INVALID_STATE ||
        !isValidStateHandle(result.states.size(), result.root)) {
      m_errors.push_back("build: invalid root handle");
      return;
    }
    if (!result.states[stateIndex(result.root)].isComposite) {
      m_errors.push_back("build: root must be composite");
      return;
    }
    if (result.states[stateIndex(result.root)].parent != INVALID_STATE) {
      m_errors.push_back("build: root has parent");
    }

    std::vector<char> seen(result.states.size(), 0);
    std::vector<char> inStack(result.states.size(), 0);
    for (std::size_t stateIndexValue = 0;
         stateIndexValue < result.states.size(); ++stateIndexValue) {
      const StateHandle state = stateHandleFromIndex(stateIndexValue);
      if (hasParentCycle(result.states, state, seen, inStack)) {
        m_errors.push_back("build: cycle in hierarchy");
        break;
      }
    }

    for (std::size_t parentIndex = 0; parentIndex < result.states.size();
         ++parentIndex) {
      const StateHandle parent = stateHandleFromIndex(parentIndex);
      for (StateHandle child : result.states[stateIndex(parent)].children) {
        if (!isValidStateHandle(result.states.size(), child)) {
          m_errors.push_back("build: children contains invalid handle");
          continue;
        }
        if (result.states[stateIndex(child)].parent != parent) {
          m_errors.push_back("build: parent / child mismatch");
        }
      }
    }

    for (std::size_t stateIndexValue = 0;
         stateIndexValue < result.states.size(); ++stateIndexValue) {
      const StateHandle state = stateHandleFromIndex(stateIndexValue);
      if (!result.states[stateIndex(state)].isComposite)
        continue;
      const std::size_t index = stateIndex(state);
      if (result.states[index].children.empty()) {
        m_errors.push_back("build: composite has no children");
        continue;
      }
      if (result.states[index].initialChild == INVALID_STATE) {
        result.states[index].initialChild =
            result.states[index].children.front();
      } else if (!isChildOf(result.states[index],
                            result.states[index].initialChild)) {
        m_errors.push_back("build: initial state is not direct child");
      }
    }

    std::vector<char> visited(result.states.size(), 0);
    dfsVisit(result.states, result.root, visited);
    for (char reached : visited) {
      if (!reached) {
        m_errors.push_back("build: unreachable state exists");
        break;
      }
    }

    for (const auto &transition : result.transitions) {
      if (transition.from == INVALID_STATE || transition.to == INVALID_STATE) {
        m_errors.push_back("build: transition has invalid endpoint");
        continue;
      }
      if (!isValidStateHandle(result.states.size(), transition.from) ||
          !isValidStateHandle(result.states.size(), transition.to) ||
          !visited[stateIndex(transition.from)] ||
          !visited[stateIndex(transition.to)]) {
        m_errors.push_back("build: transition endpoint not reachable");
        break;
      }
    }
  }

  static void sortTransitions(
      std::vector<StateDef<ContextType, EventType>> &states,
      std::vector<TransitionDef<ContextType, EventType>> &transitions) {
    for (auto &state : states) {
      std::stable_sort(state.outgoing.begin(), state.outgoing.end(),
                       [&](TransitionHandle a, TransitionHandle b) {
                         const auto &ta = transitions[transitionIndex(a)];
                         const auto &tb = transitions[transitionIndex(b)];
                         if (ta.priority != tb.priority)
                           return ta.priority > tb.priority;
                         return ta.order < tb.order;
                       });
    }
  }

  static void
  sortTransitions(MachineDefinition<ContextType, EventType> &result) {
    sortTransitions(result.states, result.transitions);
  }

  void resetInternal() {
    m_states.clear();
    m_transitions.clear();
    m_errors.clear();
    m_nextOrder = 0;
  }

  std::vector<StateDef<ContextType, EventType>> m_states;
  std::vector<TransitionDef<ContextType, EventType>> m_transitions;
  std::vector<std::string> m_errors;
  int m_nextOrder = 0;
};

//============================================================
// StateMachine
//============================================================
template <class ContextType, class EventType = EventId>
class StateMachine {
public:
  class Instance {
    friend class StateMachine;

  public:
    [[nodiscard]] StateHandle previousLeaf() const { return m_prevLeaf; }

    [[nodiscard]] StateHandle currentLeaf() const { return m_currLeaf; }

  private:
    StateHandle m_prevLeaf = INVALID_STATE;
    StateHandle m_currLeaf = INVALID_STATE;
  };

  StateMachine() = default;
  explicit StateMachine(
      MachineDefinition<ContextType, EventType> &&definition) {
    auto normalizedDefinition = normalizeDefinition(std::move(definition));
    if (!validateDefinition(normalizedDefinition, m_validationErrors))
      return;
    m_states = std::move(normalizedDefinition.states);
    m_transitions = std::move(normalizedDefinition.transitions);
    m_root = normalizedDefinition.root;
    sortTransitions(m_states, m_transitions);
  }

  [[nodiscard]] bool isValid() const {
    return m_root != INVALID_STATE &&
           isValidStateHandle(m_states.size(), m_root) &&
           m_states[stateIndex(m_root)].isComposite;
  }

  [[nodiscard]] const std::vector<std::string> &validationErrors() const {
    return m_validationErrors;
  }

  [[nodiscard]] std::string
  formattedValidationErrors(std::string_view separator = "\n") const {
    std::string message;
    for (std::size_t i = 0; i < m_validationErrors.size(); ++i) {
      if (i > 0)
        message += separator;
      message += m_validationErrors[i];
    }
    return message;
  }

  [[nodiscard]] Instance makeInstance() const {
    Instance instance{};
    instance.m_prevLeaf = INVALID_STATE;
    instance.m_currLeaf = descendToLeaf(m_root);
    return instance;
  }

  bool dispatch(Instance &instance, EventType event,
                ContextType &context) const {
    if (!isValid() || !isValidState(instance.m_currLeaf))
      return false;

    TransitionHandle transition =
        findTransition(instance.m_currLeaf, event, context);
    if (transition == INVALID_TRANSITION)
      return false;

    const StateHandle fromLeaf = instance.m_currLeaf;
    const StateHandle toState = m_transitions[transitionIndex(transition)].to;
    const StateHandle toLeaf = descendToLeaf(toState);

    if (fromLeaf == toLeaf) {
      callOnPerform(fromLeaf, event, context);
      return true;
    }

    runLifecycle(fromLeaf, toLeaf, event, context);
    instance.m_prevLeaf = fromLeaf;
    instance.m_currLeaf = toLeaf;
    callOnPerform(toLeaf, event, context);
    return true;
  }

  void tick(Instance &instance, EventType event, ContextType &context) const {
    if (!isValid() || !isValidState(instance.m_currLeaf))
      return;
    callOnPerform(instance.m_currLeaf, event, context);
  }

  [[nodiscard]] std::string_view stateName(StateHandle state) const {
    if (state == INVALID_STATE || !isValidStateHandle(m_states.size(), state))
      return {};
    return m_states[stateIndex(state)].name;
  }

private:
  bool isValidState(StateHandle state) const {
    return isValidStateHandle(m_states.size(), state);
  }

  static void
  dfsVisit(const std::vector<StateDef<ContextType, EventType>> &states,
           StateHandle state, std::vector<char> &visited) {
    if (state == INVALID_STATE)
      return;
    if (visited[stateIndex(state)])
      return;
    visited[stateIndex(state)] = 1;
    for (StateHandle child : states[stateIndex(state)].children) {
      dfsVisit(states, child, visited);
    }
  }

  static bool
  hasParentCycle(const std::vector<StateDef<ContextType, EventType>> &states,
                 StateHandle state, std::vector<char> &seen,
                 std::vector<char> &inStack) {
    if (state == INVALID_STATE)
      return false;
    if (!isValidStateHandle(states.size(), state)) {
      return true;
    }

    const std::size_t index = stateIndex(state);
    if (inStack[index])
      return true;
    if (seen[index])
      return false;

    inStack[index] = 1;
    const bool cycleFound =
        hasParentCycle(states, states[index].parent, seen, inStack);
    inStack[index] = 0;
    seen[index] = 1;
    return cycleFound;
  }

  static bool isChildOf(const StateDef<ContextType, EventType> &stateDef,
                        StateHandle child) {
    return std::find(stateDef.children.begin(), stateDef.children.end(),
                     child) != stateDef.children.end();
  }

  static void sortTransitions(
      std::vector<StateDef<ContextType, EventType>> &states,
      std::vector<TransitionDef<ContextType, EventType>> &transitions) {
    for (auto &state : states) {
      std::stable_sort(state.outgoing.begin(), state.outgoing.end(),
                       [&](TransitionHandle a, TransitionHandle b) {
                         const auto &ta = transitions[transitionIndex(a)];
                         const auto &tb = transitions[transitionIndex(b)];
                         if (ta.priority != tb.priority)
                           return ta.priority > tb.priority;
                         return ta.order < tb.order;
                       });
    }
  }

  static bool validateDefinition(
      const MachineDefinition<ContextType, EventType> &definition,
      std::vector<std::string> &errors) {
    errors.clear();
    const std::size_t stateCount = definition.states.size();
    if (definition.states.empty() || definition.root == INVALID_STATE ||
        !isValidStateHandle(stateCount, definition.root)) {
      errors.push_back("StateMachine validation failed: invalid root handle");
      return false;
    }
    const std::size_t rootIndex = stateIndex(definition.root);
    if (!definition.states[rootIndex].isComposite ||
        definition.states[rootIndex].parent != INVALID_STATE) {
      errors.push_back(
          "StateMachine validation failed: root is not valid composite");
      return false;
    }

    const auto &states = definition.states;
    const auto &transitions = definition.transitions;
    for (const auto &state : states) {
      if (state.parent != INVALID_STATE &&
          !isValidStateHandle(stateCount, state.parent)) {
        errors.push_back(
            "StateMachine validation failed: parent has invalid handle");
        return false;
      }
      if (!state.isComposite) {
        if (!state.children.empty()) {
          errors.push_back("StateMachine validation failed: leaf has children");
          return false;
        }
        if (state.initialChild != INVALID_STATE) {
          errors.push_back(
              "StateMachine validation failed: leaf has initial child");
          return false;
        }
        continue;
      }
      if (state.children.empty()) {
        errors.push_back(
            "StateMachine validation failed: composite has no children");
        return false;
      }
      if (state.initialChild != INVALID_STATE &&
          !isChildOf(state, state.initialChild)) {
        errors.push_back("StateMachine validation failed: initial state is not "
                         "direct child");
        return false;
      }
    }

    for (std::size_t parentIndex = 0; parentIndex < stateCount; ++parentIndex) {
      const StateHandle parent = stateHandleFromIndex(parentIndex);
      for (StateHandle child : states[stateIndex(parent)].children) {
        if (!isValidStateHandle(stateCount, child)) {
          errors.push_back("StateMachine validation failed: children contains "
                           "invalid handle");
          return false;
        }
        if (states[stateIndex(child)].parent != parent) {
          errors.push_back(
              "StateMachine validation failed: parent / child mismatch");
          return false;
        }
      }
    }

    std::vector<char> seen(states.size(), 0);
    std::vector<char> inStack(states.size(), 0);
    for (std::size_t stateIndex = 0; stateIndex < states.size(); ++stateIndex) {
      const StateHandle state = stateHandleFromIndex(stateIndex);
      if (hasParentCycle(states, state, seen, inStack)) {
        errors.push_back("StateMachine validation failed: cycle in hierarchy");
        return false;
      }
    }

    std::vector<char> reachable(states.size(), 0);
    dfsVisit(states, definition.root, reachable);
    for (char reached : reachable) {
      if (!reached) {
        errors.push_back(
            "StateMachine validation failed: unreachable state exists");
        return false;
      }
    }

    for (const auto &transition : transitions) {
      if (transition.from == INVALID_STATE || transition.to == INVALID_STATE) {
        errors.push_back(
            "StateMachine validation failed: transition has invalid endpoint");
        return false;
      }
      if (!isValidStateHandle(stateCount, transition.from) ||
          !isValidStateHandle(stateCount, transition.to)) {
        errors.push_back(
            "StateMachine validation failed: transition has invalid endpoint");
        return false;
      }
      if (!reachable[stateIndex(transition.from)] ||
          !reachable[stateIndex(transition.to)]) {
        errors.push_back("StateMachine validation failed: transition endpoint "
                         "not reachable");
        return false;
      }
    }

    std::vector<std::size_t> outgoingRefCount(transitions.size(), 0);
    for (std::size_t parentIndex = 0; parentIndex < stateCount; ++parentIndex) {
      const StateHandle parent = stateHandleFromIndex(parentIndex);
      const auto &state = states[stateIndex(parent)];
      for (TransitionHandle transitionHandle : state.outgoing) {
        if (!isValidTransitionHandle(transitions.size(), transitionHandle)) {
          errors.push_back("StateMachine validation failed: outgoing contains "
                           "invalid transition handle");
          return false;
        }
        const std::size_t transitionArrayIndex =
            transitionIndex(transitionHandle);
        ++outgoingRefCount[transitionArrayIndex];
        if (transitions[transitionArrayIndex].from != parent) {
          errors.push_back("StateMachine validation failed: outgoing "
                           "transition from mismatch");
          return false;
        }
      }
    }

    for (std::size_t i = 0; i < outgoingRefCount.size(); ++i) {
      if (outgoingRefCount[i] == 0) {
        errors.push_back("StateMachine validation failed: transition is not "
                         "referenced from outgoing");
        return false;
      }
      if (outgoingRefCount[i] > 1) {
        errors.push_back("StateMachine validation failed: transition is "
                         "referenced multiple times");
        return false;
      }
    }

    return true;
  }

  TransitionHandle findTransition(StateHandle leaf, EventType event,
                                  ContextType &context) const {
    StateHandle state = leaf;
    while (state != INVALID_STATE) {
      for (TransitionHandle transitionHandle :
           m_states[stateIndex(state)].outgoing) {
        const auto &transition =
            m_transitions[transitionIndex(transitionHandle)];
        if (transition.event != event)
          continue;
        if (!transition.guard)
          return transitionHandle;
        GuardContext<ContextType, EventType> guardContext{context, event};
        if (transition.guard(guardContext))
          return transitionHandle;
      }
      state = m_states[stateIndex(state)].parent;
    }
    return INVALID_TRANSITION;
  }

  StateHandle descendToLeaf(StateHandle state) const {
    StateHandle current = state;
    while (current != INVALID_STATE &&
           m_states[stateIndex(current)].isComposite) {
      if (m_states[stateIndex(current)].initialChild == INVALID_STATE) {
        break;
      }
      current = m_states[stateIndex(current)].initialChild;
    }
    return current;
  }

  static std::vector<StateHandle>
  parentChain(const std::vector<StateDef<ContextType, EventType>> &states,
              StateHandle leaf) {
    std::vector<StateHandle> chain;
    if (leaf == INVALID_STATE || !isValidStateHandle(states.size(), leaf))
      return {};

    StateHandle current = states[stateIndex(leaf)].parent;
    while (current != INVALID_STATE) {
      chain.push_back(current);
      current = states[stateIndex(current)].parent;
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
  }

  void runLifecycle(StateHandle fromLeaf, StateHandle toLeaf, EventType event,
                    ContextType &context) const {
    auto fromChain = parentChain(m_states, fromLeaf);
    auto toChain = parentChain(m_states, toLeaf);
    fromChain.push_back(fromLeaf);
    toChain.push_back(toLeaf);

    std::size_t prefix = 0;
    const std::size_t shared = std::min(fromChain.size(), toChain.size());
    while (prefix < shared && fromChain[prefix] == toChain[prefix]) {
      ++prefix;
    }

    for (std::size_t i = fromChain.size(); i-- > prefix;) {
      callOnExit(fromChain[i], event, context);
    }
    for (std::size_t i = prefix; i < toChain.size(); ++i) {
      callOnEnter(toChain[i], event, context);
    }
  }

  void callOnEnter(StateHandle state, EventType event,
                   ContextType &context) const {
    const auto &callback = m_states[stateIndex(state)].callbacks.onEntry;
    if (callback)
      callback(context, event);
  }

  void callOnExit(StateHandle state, EventType event,
                  ContextType &context) const {
    const auto &callback = m_states[stateIndex(state)].callbacks.onExit;
    if (callback)
      callback(context, event);
  }

  void callOnPerform(StateHandle state, EventType event,
                     ContextType &context) const {
    const auto &callback = m_states[stateIndex(state)].callbacks.onPerform;
    if (callback)
      callback(context, event);
  }

  std::vector<StateDef<ContextType, EventType>> m_states;
  std::vector<TransitionDef<ContextType, EventType>> m_transitions;
  StateHandle m_root = INVALID_STATE;
  std::vector<std::string> m_validationErrors;

  static MachineDefinition<ContextType, EventType>
  normalizeDefinition(MachineDefinition<ContextType, EventType> definition) {
    for (std::size_t stateIndex = 0; stateIndex < definition.states.size();
         ++stateIndex) {
      auto &stateDef = definition.states[stateIndex];
      if (stateDef.isComposite && stateDef.initialChild == INVALID_STATE &&
          !stateDef.children.empty()) {
        stateDef.initialChild = stateDef.children.front();
      }
    }
    return definition;
  }
};

} // namespace hfsm
