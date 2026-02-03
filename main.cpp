#include <iostream>

#include "hfsm.hpp"

enum class Event : unsigned { SW_OFF, SW_ON, START, STOP, NEXT };

struct Context {};

struct Sleeping : hfsm::State<Context>, hfsm::Singleton<Sleeping> {
  virtual void onEntry(Context &context, const Event &event) {
    std::cout << "  " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void perform(Context &context, const Event &event) {
    std::cout << "  " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void onExit(Context &context, const Event &event) {
    std::cout << "  " << __PRETTY_FUNCTION__ << std::endl;
  }
};

struct Active : hfsm::CompositeState<Context>, hfsm::Singleton<Active> {
  virtual void onEntry(Context &context, const Event &event) {
    std::cout << "  " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void perform(Context &context, const Event &event) {
    std::cout << "  " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void onExit(Context &context, const Event &event) {
    std::cout << "  " << __PRETTY_FUNCTION__ << std::endl;
  }
};

struct Playing : hfsm::CompositeState<Context>, hfsm::Singleton<Playing> {
  virtual void onEntry(Context &context, const Event &event) {
    std::cout << "    " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void perform(Context &context, const Event &event) {
    std::cout << "    " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void onExit(Context &context, const Event &event) {
    std::cout << "    " << __PRETTY_FUNCTION__ << std::endl;
  }
};

struct Playing1 : hfsm::State<Context>, hfsm::Singleton<Playing1> {
  virtual void onEntry(Context &context, const Event &event) {
    std::cout << "      " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void perform(Context &context, const Event &event) {
    std::cout << "      " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void onExit(Context &context, const Event &event) {
    std::cout << "      " << __PRETTY_FUNCTION__ << std::endl;
  }
};

struct Playing2 : hfsm::State<Context>, hfsm::Singleton<Playing2> {
  virtual void onEntry(Context &context, const Event &event) {
    std::cout << "      " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void perform(Context &context, const Event &event) {
    std::cout << "      " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void onExit(Context &context, const Event &event) {
    std::cout << "      " << __PRETTY_FUNCTION__ << std::endl;
  }
};

struct Paused : hfsm::State<Context>, hfsm::Singleton<Paused> {
  virtual void onEntry(Context &context, const Event &event) {
    std::cout << "    " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void perform(Context &context, const Event &event) {
    std::cout << "    " << __PRETTY_FUNCTION__ << std::endl;
  }
  virtual void onExit(Context &context, const Event &event) {
    std::cout << "    " << __PRETTY_FUNCTION__ << std::endl;
  }
};

int main() {
  hfsm::StateMachine<Context, Sleeping> sm(
      {{Sleeping::getInstance(), Event::SW_ON, Active::getInstance()},
       {Active::getInstance(), Event::SW_OFF, Sleeping::getInstance()}});

  hfsm::CompositeState<Context> *active = Active::getInstance();
  active->set(Paused::getInstance(),
              {{Paused::getInstance(), Event::START, Playing::getInstance()},
               {Playing::getInstance(), Event::STOP, Paused::getInstance()}});

  hfsm::CompositeState<Context> *playing = Playing::getInstance();
  playing->set(
      Playing1::getInstance(),
      {{Playing1::getInstance(), Event::NEXT, Playing2::getInstance()},
       {Playing2::getInstance(), Event::NEXT, Playing1::getInstance()}});

  auto bot = sm.makeInstance();

  std::cout << "SW_ON" << std::endl;
  sm.dispatch(bot, Event::SW_ON);
  std::cout << "START" << std::endl;
  sm.dispatch(bot, Event::START);
  std::cout << "NEXT" << std::endl;
  sm.dispatch(bot, Event::NEXT);
  std::cout << "NEXT" << std::endl;
  sm.dispatch(bot, Event::NEXT);
  std::cout << "STOP" << std::endl;
  sm.dispatch(bot, Event::STOP);
  std::cout << "SW_OFF" << std::endl;
  sm.dispatch(bot, Event::SW_OFF);
}
