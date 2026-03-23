#include "includes.hpp"
#include "ui/record_layer.hpp"
#include "ui/game_ui.hpp"
#include "ui/clickbot_layer.hpp"
#include "ui/macro_editor.hpp"
#include "ui/render_settings_layer.hpp"
#include "hacks/layout_mode.hpp"
#include "hacks/show_trajectory.hpp"

#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include <array>
#include <chrono>

namespace {
using Clock = std::chrono::steady_clock;

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

bool advanceDeveloperTapSequence(
  int& progress,
  Clock::time_point& lastTapAt
) {
  constexpr int kTapCount = 5;
  constexpr auto kTimeout = std::chrono::milliseconds(1250);
  auto now = Clock::now();

  if (progress <= 0 || now - lastTapAt > kTimeout)
    progress = 1;
  else
    progress++;

  lastTapAt = now;
  if (progress >= kTapCount) {
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
  struct Fields {
    int developerKeyProgress = 0;
    int developerTapProgress = 0;
    Clock::time_point lastDeveloperKeyAt = {};
    Clock::time_point lastDeveloperTapAt = {};
  };

  bool init() {
    if (!MenuLayer::init())
      return false;

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto touchMenu = CCMenu::create();
    touchMenu->setPosition({ 0.f, 0.f });
    touchMenu->setID("developer-mode-unlock-menu"_spr);

    auto touchTarget = CCLayerColor::create({ 0, 0, 0, 0 }, 150, 70);
    auto unlockButton = CCMenuItemSpriteExtra::create(
      touchTarget,
      this,
      menu_selector(MenuLayer::onDeveloperModeTap)
    );
    unlockButton->setPosition({ winSize.width / 2.f, winSize.height - 52.f });
    unlockButton->setID("developer-mode-unlock-button"_spr);
    touchMenu->addChild(unlockButton);

    addChild(touchMenu, 999);
    return true;
  }

  void keyDown(enumKeyCodes key) {
    MenuLayer::keyDown(key);
    if (advanceDeveloperKeySequence(m_fields->developerKeyProgress, m_fields->lastDeveloperKeyAt, key)) {
      bool enabled = !Global::isDeveloperModeEnabled();
      Global::setDeveloperModeEnabled(enabled);
      showDeveloperModeNotification(enabled);
    }
  }

  void onDeveloperModeTap(CCObject*) {
    if (!advanceDeveloperTapSequence(m_fields->developerTapProgress, m_fields->lastDeveloperTapAt))
      return;

    bool enabled = !Global::isDeveloperModeEnabled();
    Global::setDeveloperModeEnabled(enabled);
    showDeveloperModeNotification(enabled);
  }
};

$execute {
#ifdef GEODE_IS_WINDOWS
  // geode.custom-keybinds is pre-v5 and does not compile with Geode v5 headers.
  // Keybind registration is temporarily disabled pending migration to the v5 keybind API.
#endif
}
