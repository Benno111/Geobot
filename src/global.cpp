#include "ui/record_layer.hpp"
#include "ui/game_ui.hpp"

#include <Geode/modify/CCTextInputNode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

#include <random>
#include <array>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <ctime>

class $modify(CCTextInputNode) {

    bool ccTouchBegan(cocos2d::CCTouch * v1, cocos2d::CCEvent * v2) {
        if (this->getID() == "disabled-input"_spr) return false;

        return CCTextInputNode::ccTouchBegan(v1, v2);
    }
};

namespace {

int monthFromDateAbbrev(std::string_view month) {
  static const std::array<std::string_view, 12> months = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  for (int i = 0; i < static_cast<int>(months.size()); i++) {
    if (months[i] == month) return i;
  }
  return -1;
}

std::time_t getBuildTimestamp() {
  // __DATE__ format: "Mmm dd yyyy"
  std::string date = __DATE__;
  if (date.size() < 11) return static_cast<std::time_t>(-1);

  std::string_view monthStr(date.data(), 3);
  int month = monthFromDateAbbrev(monthStr);
  if (month < 0) return static_cast<std::time_t>(-1);

  int day = 0;
  int year = 0;

  try {
    day = std::stoi(date.substr(4, 2));
    year = std::stoi(date.substr(7, 4));
  } catch (...) {
    return static_cast<std::time_t>(-1);
  }

  std::tm tm = {};
  tm.tm_year = year - 1900;
  tm.tm_mon = month;
  tm.tm_mday = day;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;

  return std::mktime(&tm);
}

bool hasBuildExpiredBy30Days() {
  if (geobotDisableBuildExpiryLock) return false;

  std::time_t build = getBuildTimestamp();
  if (build == static_cast<std::time_t>(-1)) return false;

  constexpr std::time_t kThirtyDays = static_cast<std::time_t>(30 * 24 * 60 * 60);
  std::time_t expiry = build + kThirtyDays;
  return std::time(nullptr) > expiry;
}






}

struct IncompatibleSetting {
  std::string ID;
  bool incompatValue;
  bool isModToggle = false;
  bool isSavedValue = false;
};

struct IncompatibleMod {
  std::string ID;
  bool canBeDisabled;
  std::vector<IncompatibleSetting> incompatSettings;
};

const std::vector<IncompatibleMod> incompatibleMods {
  { "syzzi.click_between_frames", true, { {"soft-toggle", false, true }, { "actual-delta", true } } },
  { "alphalaneous.click_after_frames", true, { { "soft-toggle", false, true } } },
  { "thesillydoggo.qolmod", true, { { "tps-bypass_enabled", true, false, true } } },
  // { "zmx.cbf-lite", false, {  } }
};

bool Global::hasIncompatibleMods() {
  std::vector<std::string> modsToDisable;
  std::vector<std::string> settingsToDisable;

  if (Mod* mod = Loader::get()->getLoadedMod("firee.prism")) {
    auto json = mod->getSavedValue<matjson::Value>("values");
    for (const auto& obj : json.asArray().unwrap()) {

      if (obj["name"].asString().unwrapOrDefault() != "TPS Bypass") continue;

      if (obj["value"].asInt().unwrapOrDefault() != 240)
        settingsToDisable.push_back("<cr>TPS Bypass (Prism Menu)</c>");

      break;

    }
  }

  #ifdef GEODE_IS_WINDOWS

  if (Mod* mod = Loader::get()->getLoadedMod("tobyadd.gdh")) {
    std::filesystem::path configPath = mod->getSaveDir() / "config.json";
	  using namespace nlohmann;

    if (std::filesystem::exists(configPath)) {
      std::ifstream jsonFile(configPath);
      if (jsonFile.is_open()) {
        json jsonData;
        jsonFile >> jsonData;
        if (jsonData.contains("tps_enabled")) {
          if (jsonData["tps_enabled"])
            settingsToDisable.push_back("<cr>TPS Bypass (GDH)</c>");
        }
      }
    }
  }

  #else

  if (Mod* mod = Loader::get()->getLoadedMod("tobyadd.gdh_mobile")) {
    std::filesystem::path configPath = mod->getSaveDir() / "config.json";
	  using namespace nlohmann;
    
    if (std::filesystem::exists(configPath)) {
      std::ifstream jsonFile(configPath);
      if (jsonFile.is_open()) {
        json jsonData;
        jsonFile >> jsonData;
        if (jsonData.contains("fps_value")) {
          if (jsonData["fps_value"] != 240)
            settingsToDisable.push_back("<cr>TPS Bypass (GDH)</c>");
        }
      }
    }
  }

  #endif

  for (IncompatibleMod incompatMod : incompatibleMods) {
    Mod* mod = Loader::get()->getLoadedMod(incompatMod.ID);

    if (!mod) continue;

    std::string modName = mod->getName();

    if (!incompatMod.canBeDisabled) {
      modsToDisable.push_back(modName);
      continue;
    }

    for (IncompatibleSetting sett : incompatMod.incompatSettings) {
      bool value = sett.isSavedValue ? mod->getSavedValue<bool>(sett.ID) : mod->getSettingValue<bool>(sett.ID);

      if (value != sett.incompatValue) continue;

      if (sett.isModToggle)
        modsToDisable.push_back(modName);
      else {
        std::string settName = sett.isSavedValue ? sett.ID : mod->getSetting(sett.ID)->getDisplayName();
        settingsToDisable.push_back(fmt::format("{} ({})", settName, modName));
      }

    }
  }

  if (!modsToDisable.empty()) {
    std::string incompatString = "";

    for (const std::string name : modsToDisable)
      incompatString += fmt::format("<cr>{}</c>{}", name, (name != modsToDisable.back() ? ", " : ""));

    FLAlertLayer::create("Warning", "The following mods are incompatible: \n" + incompatString, "Ok")->show();

  } else if (!settingsToDisable.empty()) {
    std::string incompatString = "";

    for (const std::string name : settingsToDisable)
      incompatString += fmt::format("<cr>{}</c>{}", name, (name != settingsToDisable.back() ? ", " : ""));

    FLAlertLayer::create("Warning", "The following mod settings are incompatible: \n" + incompatString, "Ok")->show();
    
  }

  bool ret = !modsToDisable.empty() || !settingsToDisable.empty();

  if (ret) {
    Global::get().state = state::none;
    Interface::updateLabels();
    Interface::updateButtons();
  }

  return ret;
}

bool Global::isBuildExpired() {
  return Global::get().buildExpired;
}

void Global::showBuildExpiredNotice() {
  auto& g = Global::get();
  if (g.buildExpiryNoticeShown) return;
  g.buildExpiryNoticeShown = true;

  Loader::get()->queueInMainThread([] {
    FLAlertLayer::create(
      "geobot",
      "This build has expired (30-day limit). Please install a newer build.",
      "OK"
    )->show();
  });
}

float Global::getTPS() {
  auto& g = Global::get();
  return g.tpsEnabled ? g.tps : 240.f;
}

int Global::getCurrentFrame(bool editor) {
  PlayLayer* pl = PlayLayer::get();
  GJBaseGameLayer* bgl = pl ? static_cast<GJBaseGameLayer*>(pl) : GJBaseGameLayer::get();
  if (!bgl) return 0;

  auto& g = Global::get();
  int frame;
  const double exactFrame = static_cast<double>(bgl->m_gameState.m_levelTime) *
                            static_cast<double>(getTPS());

  if (!editor && pl) {
    // Use levelTime as the frame source to avoid progress-based jumps that
    // can fast-forward playback and skip actions.
    // Playback advances in fixed-size ticks. Round to the nearest tick instead
    // of truncating so small, platform-specific level-time errors cannot move
    // an input one frame later.
    frame = static_cast<int>(std::llround(exactFrame));
    if (g.macro.geobotMacro)
      frame++;
  } else {
    frame = static_cast<int>(std::llround(exactFrame));
    frame++;
  }

  frame -= g.frameOffset;
  if (frame < 0) return 0;

  return frame;
}

void Global::updateKeybinds() {
  // Legacy custom-keybinds integration is disabled for Geode v5 migration.
}

PauseLayer* Global::getPauseLayer() {
  for (CCNode* child : CCDirector::sharedDirector()->getRunningScene()->getChildrenExt()) {
    if (PauseLayer* pauseLayer = typeinfo_cast<PauseLayer*>(child)) {
      return pauseLayer;
    }
  }

  return nullptr;
}

namespace {
}









std::filesystem::path Global::getFolderSettingPath(std::string const& settingID, bool createIfMissing) {
  auto& g = Global::get();
  auto fallback = [&]() {
    if (settingID == "macros_folder")
      return geode::dirs::getGameDir() / "macros";
    if (settingID == "autosaves_folder")
      return g.mod->getSaveDir() / "autosaves";
    return g.mod->getSaveDir() / settingID;
  };

  auto path = g.mod->getSettingValue<std::filesystem::path>(settingID);
  if (path.empty()) {
    path = fallback();
    g.mod->setSettingValue<std::filesystem::path>(settingID, path);
  }

  std::error_code ec;
  bool validDir = std::filesystem::exists(path, ec) ? std::filesystem::is_directory(path, ec) : true;

  if (!validDir) {
    path = fallback();
    g.mod->setSettingValue<std::filesystem::path>(settingID, path);
  }

  if (createIfMissing && !std::filesystem::exists(path, ec)) {
    std::filesystem::create_directories(path, ec);
    if (ec) {
      auto fb = fallback();
      ec.clear();
      std::filesystem::create_directories(fb, ec);
      if (!ec) {
        path = fb;
        g.mod->setSettingValue<std::filesystem::path>(settingID, path);
      }
    }
  }

  return path;
}

$execute{
  auto & g = Global::get();
  g.buildExpired = hasBuildExpiredBy30Days();
  if (g.buildExpired) {
    Global::showBuildExpiredNotice();
    return;
  }



  if (!g.mod->setSavedValue("defaults_set_12", true)) {
    g.mod->setSettingValue<std::filesystem::path>("macros_folder", Global::getFolderSettingPath("macros_folder"));
    g.mod->setSettingValue<std::filesystem::path>("autosaves_folder", g.mod->getSaveDir() / "autosaves");
  }

  if (!g.mod->setSavedValue("defaults_set_18", true)) {
    std::filesystem::path currentSaveDir = g.mod->getSaveDir();
    std::filesystem::path currentMacros = g.mod->getSettingValue<std::filesystem::path>("macros_folder");
    std::filesystem::path gameMacros = geode::dirs::getGameDir() / "macros";
    std::filesystem::path parent = currentSaveDir.parent_path();

    if (!parent.empty()) {
      std::filesystem::path geobotDefault = currentSaveDir / "macros";
      std::filesystem::path bennoLegacy = parent / "benno111.xdbot" / "macros";
      std::filesystem::path zilkoLegacy = parent / "zilko.xdbot" / "macros";

      if (currentMacros.empty() || currentMacros == geobotDefault || currentMacros == bennoLegacy || currentMacros == zilkoLegacy)
        g.mod->setSettingValue<std::filesystem::path>("macros_folder", gameMacros);
    }
  }



  if (!g.mod->setSavedValue("defaults_set_10", true)) {
    g.mod->setSettingValue("restore_page", true);

    g.mod->setSavedValue("autosave_interval", std::to_string(10));
    g.mod->setSavedValue("autosave_checkpoint_enabled", true);
    g.mod->setSavedValue("autosave_levelend_enabled", true);


    g.mod->setSavedValue("auto_stop_playing", false);
    g.mod->setSavedValue("macro_tps", 240.f);
    g.mod->setSavedValue("macro_tps_enabled", false);



  }



  if (!g.mod->setSavedValue("defaults_set_16", true)) {
    g.mod->setSavedValue("macro_accuracy", std::string("Frame Fixes"));
    g.mod->setSavedValue("frame_offset", 0);
    g.mod->setSavedValue("frame_fixes_limit", 240);
    g.mod->setSavedValue("lock_delta", false);
    g.mod->setSavedValue("auto_stop_playing", false);
  }

  // Hotfix: restore historical playback behavior (do not auto-stop by default).
  if (!g.mod->setSavedValue("defaults_set_17", true))
    g.mod->setSavedValue("auto_stop_playing", false);

  if (!g.mod->hasSavedValue("developer_mode_enabled"))
    g.mod->setSavedValue("developer_mode_enabled", false);



  std::string const currentNoticeVersion = geobotVersion;
  if (!g.mod->hasSavedValue("update_notice_last_seen")) {
    g.mod->setSavedValue("update_notice_last_seen", currentNoticeVersion);
  }
  else {
    std::string lastSeenVersion = g.mod->getSavedValue<std::string>("update_notice_last_seen");
    if (lastSeenVersion != currentNoticeVersion) {
      g.mod->setSavedValue("update_notice_last_seen", currentNoticeVersion);
      Loader::get()->queueInMainThread([currentNoticeVersion] {
        geode::createQuickPopup(
          "Update Available",
          fmt::format(
            "<cl>geobot</c> was updated to <cy>{}</c>.\nOpen mod settings to view changelog and options?",
            currentNoticeVersion
          ),
          "Later", "Open",
          [](auto, bool open) {
            if (open)
              geode::openSettingsPopup(Mod::get(), false);
          }
        );
      });
    }
  }

  // Migrate legacy saved keys to current setting IDs.
  if (!g.mod->hasSavedValue("auto_stop_playing") && g.mod->hasSavedValue("macro_auto_stop_playing"))
    g.mod->setSavedValue("auto_stop_playing", g.mod->getSavedValue<bool>("macro_auto_stop_playing"));

  // Geobot is intentionally a focused macro/click bot. Keep retired utility
  // features off even when an older installation left their values enabled.
  g.frameLabel = g.mod->getSavedValue<bool>("macro_show_frame_label");
  g.tpsEnabled = g.mod->getSavedValue<bool>("macro_tps_enabled");
  g.tps = g.mod->getSavedValue<double>("macro_tps");
  // Autosaving is part of recording/editor playback now, not an optional
  // utility mode. Persist the values so every autosave entry point agrees.
  g.mod->setSavedValue("autosave_interval_enabled", true);
  g.mod->setSavedValue("autosave_checkpoint_enabled", true);
  g.mod->setSavedValue("autosave_levelend_enabled", true);
  g.mod->setSavedValue("macro_auto_save", true);

  g.currentPage = static_cast<int>(getSavedInt64Safe(g.mod, "current_page", 0));

  g.autosaveInterval = (geode::utils::numFromString<float>(g.mod->getSavedValue<std::string>("autosave_interval")).unwrapOr(0.f) * 60);


  g.frameOffset = static_cast<int>(getSavedInt64Safe(g.mod, "frame_offset", 0));
  g.frameFixesLimit = static_cast<int>(getSavedInt64Safe(g.mod, "frame_fixes_limit", 240));
  g.lockDelta = g.mod->getSavedValue<bool>("lock_delta");
  g.stopPlaying = g.mod->getSavedValue<bool>("auto_stop_playing");

  if (g.mod->getSavedValue<std::string>("macro_accuracy") == "Frame Fixes")
    g.frameFixes = true;
  else if (g.mod->getSavedValue<std::string>("macro_accuracy") == "Input Fixes")
    g.inputFixes = true;

  std::string defaultAuthor = "N/A";
  if (auto* account = GJAccountManager::sharedState()) {
    if (!account->m_username.empty())
      defaultAuthor = account->m_username;
  }
  g.macro.author = defaultAuthor;
  g.macro.description = "N/A";
  g.macro.gameVersion = 2.208;
};
