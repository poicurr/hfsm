#pragma once

#include <algorithm>
#include <tuple>
#include <unordered_map>
#include <vector>

// 利用側が定義するイベント型
enum class Event : unsigned;

namespace hfsm {

using ::Event;

template <class T>
struct Singleton {
  static T *getInstance() {
    static T instance;
    return &instance;
  }
};

template <class ContextType>
struct State {
  virtual ~State() = default;
  virtual void onEntry(ContextType &context, const Event &event) = 0;
  virtual void perform(ContextType &context, const Event &event) = 0;
  virtual void onExit(ContextType &context, const Event &event) = 0;
  virtual bool isComposite() { return false; }
};

template <class ContextType>
using Transition =
    std::vector<std::tuple<State<ContextType> *, Event, State<ContextType> *>>;

template <class ContextType>
struct ParentLinker {
  static std::unordered_map<State<ContextType> *, State<ContextType> *> table;
};

template <class ContextType>
std::unordered_map<State<ContextType> *, State<ContextType> *>
    ParentLinker<ContextType>::table{};

template <class ContextType>
struct Resolver {
  Transition<ContextType> transition;

  void set(State<ContextType> *parent,
           const Transition<ContextType> &transitionInput) {
    transition = transitionInput;
    for (auto &&tran : transition) {
      State<ContextType> *from = std::get<0>(tran);
      State<ContextType> *to = std::get<2>(tran);
      ParentLinker<ContextType>::table.emplace(from, parent);
      ParentLinker<ContextType>::table.emplace(to, parent);
    }
  }

  void add(State<ContextType> *parent, State<ContextType> *from,
           const Event &event, State<ContextType> *to) {
    ParentLinker<ContextType>::table.emplace(from, parent);
    ParentLinker<ContextType>::table.emplace(to, parent);
    transition.emplace_back(from, event, to);
  }

  State<ContextType> *resolveFirst() const {
    if (transition.empty())
      return nullptr;
    return std::get<0>(transition[0]);
  }

  State<ContextType> *resolve(State<ContextType> *currState,
                              const Event &receivedEvent) const {
    for (auto &&tran : transition) {
      auto &&state = std::get<0>(tran);
      auto &&event = std::get<1>(tran);
      if (state == currState && event == receivedEvent) {
        return std::get<2>(tran);
      }
    }
    return nullptr;
  }
};

template <class ContextType>
struct CompositeState : State<ContextType> {
  Resolver<ContextType> resolver;
  State<ContextType> *initialState;

  CompositeState() : initialState{nullptr} {}

  void set(State<ContextType> *initial,
           const Transition<ContextType> &transitionInput) {
    initialState = initial;
    resolver.set(this, transitionInput);
  }

  void set(State<ContextType> *initial,
           std::initializer_list<
               std::tuple<State<ContextType> *, Event, State<ContextType> *>>
               transitionInput) {
    set(initial, Transition<ContextType>(transitionInput));
  }

  CompositeState &setInitial(State<ContextType> *initial) {
    initialState = initial;
    return *this;
  }

  CompositeState &addTransition(State<ContextType> *from, const Event &event,
                                State<ContextType> *to) {
    resolver.add(this, from, event, to);
    if (!initialState) {
      initialState = from;
    }
    return *this;
  }

  State<ContextType> *resolveFirst() const {
    if (initialState)
      return initialState;
    return resolver.resolveFirst();
  }

  State<ContextType> *resolve(State<ContextType> *currState,
                              const Event &event) const {
    return resolver.resolve(currState, event);
  }

  bool isComposite() override { return true; }
};

template <class ContextType, class InitialState>
struct StateMachine {
  struct Root : CompositeState<ContextType>, Singleton<Root> {
    void onEntry(ContextType &, const Event &) override {}
    void perform(ContextType &, const Event &) override {}
    void onExit(ContextType &, const Event &) override {}
  };

  struct Instance {
    State<ContextType> *prevState;
    State<ContextType> *currState;
    ContextType context;
  };

  Root root;

  explicit StateMachine(const Transition<ContextType> &transition) {
    root.set(InitialState::getInstance(), transition);
  }

  State<ContextType> *parentOf(State<ContextType> *state) const {
    auto &table = ParentLinker<ContextType>::table;
    auto it = table.find(state);
    if (it == table.end())
      return nullptr;
    return it->second;
  }

  State<ContextType> *resolve(State<ContextType> *currState,
                              const Event &event) const {
    if (!currState)
      return nullptr;
    State<ContextType> *parent = parentOf(currState);
    State<ContextType> *nextState = nullptr;
    while (parent) {
      auto *compositeParent =
          static_cast<CompositeState<ContextType> *>(parent);
      nextState = compositeParent->resolve(currState, event);
      if (nextState)
        break;
      currState = compositeParent;
      parent = parentOf(compositeParent);
    }

    while (nextState && nextState->isComposite()) {
      auto *composite = static_cast<CompositeState<ContextType> *>(nextState);
      nextState = composite->resolveFirst();
    }
    return nextState;
  }

  std::vector<State<ContextType> *>
  parentChain(State<ContextType> *state) const {
    std::vector<State<ContextType> *> chain;
    State<ContextType> *parent = parentOf(state);
    while (parent) {
      chain.push_back(parent);
      parent = parentOf(parent);
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
  }

  void doAction(Instance &instance, const Event &event) const {
    if (instance.currState == instance.prevState)
      return;

    auto prevParents = parentChain(instance.prevState);
    auto currParents = parentChain(instance.currState);

    std::vector<State<ContextType> *> exitList;
    std::vector<State<ContextType> *> entryList;
    size_t len = std::min(prevParents.size(), currParents.size());
    for (size_t i = 0; i < len; ++i) {
      if (prevParents[i] != currParents[i]) {
        exitList.push_back(prevParents[i]);
        entryList.push_back(currParents[i]);
      }
    }

    if (prevParents.size() < currParents.size()) {
      for (size_t i = prevParents.size(); i < currParents.size(); ++i) {
        entryList.push_back(currParents[i]);
      }
    }

    if (prevParents.size() > currParents.size()) {
      for (size_t i = currParents.size(); i < prevParents.size(); ++i) {
        exitList.push_back(prevParents[i]);
      }
    }

    std::reverse(exitList.begin(), exitList.end());

    // onExit
    if (instance.prevState)
      instance.prevState->onExit(instance.context, event);
    for (auto &&elm : exitList) {
      elm->onExit(instance.context, event);
    }

    // onEntry
    for (auto &&elm : entryList) {
      elm->onEntry(instance.context, event);
    }
    instance.currState->onEntry(instance.context, event);

    // Perform
    instance.currState->perform(instance.context, event);
  }

  bool dispatch(Instance &instance, const Event &event) const {
    State<ContextType> *nextState = resolve(instance.currState, event);
    if (!nextState)
      return false;
    if (nextState == instance.currState) {
      instance.prevState = instance.currState;
      instance.currState->perform(instance.context, event);
      return true;
    }
    instance.prevState = instance.currState;
    instance.currState = nextState;
    doAction(instance, event);
    return true;
  }

  Instance makeInstance() const {
    Instance inst{};
    inst.prevState = nullptr;
    inst.currState = InitialState::getInstance();
    return inst;
  }
};

} // namespace hfsm
