#include "includes.hpp"

#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include <array>
#include <chrono>

namespace {
using Clock = std::chrono::steady_clock;
int s_developerKeyProgress = 0;
Clock::time_point s_lastDeveloperKeyAt = {};

bool advanceDeveloperKeySequence(
  int& progress,
  Clock::time_point& lastInputAt,
  enumKeyCodes key
) {
  static constexpr std::array<enumKeyCodes, 3> kSequence = {
    enumKeyCodes::KEY_D,
    enumKeyCodes::KEY_E,
    enumKeyCodes::KEY_V
  };
  constexpr auto kTimeout = std::chrono::milliseconds(1500);
  auto now = Clock::now();

  if (progress < 0 || progress >= static_cast<int>(kSequence.size()) || now - lastInputAt > kTimeout)
    progress = 0;

  if (key == kSequence[progress]) {
    progress++;
  }
  else if (key == kSequence[0]) {
    progress = 1;
  }
  else {
    progress = 0;
  }

  lastInputAt = now;
  if (progress == static_cast<int>(kSequence.size())) {
    progress = 0;
    return true;
  }
  return false;
}

void showDeveloperModeNotification(bool enabled) {
  Notification::create(
    enabled ? "Developer mode enabled" : "Developer mode disabled",
    enabled ? NotificationIcon::Success : NotificationIcon::Warning
  )->show();
}

void toggleDeveloperMode() {
  bool enabled = !Global::isDeveloperModeEnabled();
  Global::setDeveloperModeEnabled(enabled);
  showDeveloperModeNotification(enabled);
}

}

class $modify(CCKeyboardDispatcher) {
  bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double dt) {
    auto& g = Global::get();

    int keyInt = static_cast<int>(key);
    if (g.allKeybinds.contains(keyInt) && !isKeyRepeat) {
      for (size_t i = 0; i < 6; i++) {
        if (std::find(g.keybinds[i].begin(), g.keybinds[i].end(), keyInt) != g.keybinds[i].end()) {
          g.heldButtons[i] = isKeyDown;
        }
      }
    }

    return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, dt);
  }
};
class $modify(MenuLayer) {
  void keyDown(enumKeyCodes key, double timestamp) {
    MenuLayer::keyDown(key, timestamp);
    if (advanceDeveloperKeySequence(s_developerKeyProgress, s_lastDeveloperKeyAt, key))
      toggleDeveloperMode();
  }
};
