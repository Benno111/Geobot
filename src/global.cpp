#include "ui/record_layer.hpp"
#include "ui/game_ui.hpp"

#include <Geode/modify/CCTextInputNode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

#include <random>
#include <array>
#include <algorithm>
#include <cctype>
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

bool isValidFFmpegBinaryPath(std::filesystem::path const& path) {
  if (path.empty()) return false;

  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec))
    return false;

  std::string fileName = path.filename().string();
  std::transform(fileName.begin(), fileName.end(), fileName.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return fileName == "ffmpeg" || fileName == "ffmpeg.exe";
}

std::vector<std::filesystem::path> getBundledFFmpegCandidates(Mod* mod) {
  std::filesystem::path resources = mod->getResourcesDir();
  std::vector<std::filesystem::path> candidates = {
    resources / "ffmpeg.exe",
    resources / "ffmpeg",
  };

#ifdef GEODE_IS_WINDOWS
  candidates.push_back(resources / "ffmpeg" / "windows" / "ffmpeg.exe");
#elif defined(GEODE_IS_ANDROID)
  candidates.push_back(resources / "ffmpeg" / "android" / "ffmpeg");
  candidates.push_back(resources / "ffmpeg" / "android" / "ffmpeg.exe");
#elif defined(GEODE_IS_MACOS)
  candidates.push_back(resources / "ffmpeg" / "macos" / "ffmpeg");
#elif defined(GEODE_IS_IOS)
  candidates.push_back(resources / "ffmpeg" / "ios" / "ffmpeg");
#else
  candidates.push_back(resources / "ffmpeg" / "linux" / "ffmpeg");
#endif

  return candidates;
}

std::filesystem::path resolveBundledFFmpegPath(Mod* mod) {
  for (auto const& candidate : getBundledFFmpegCandidates(mod)) {
    if (isValidFFmpegBinaryPath(candidate))
      return candidate;
  }

  std::filesystem::path gameDirFFmpeg = geode::dirs::getGameDir() / "ffmpeg.exe";
  if (isValidFFmpegBinaryPath(gameDirFFmpeg))
    return gameDirFFmpeg;

  gameDirFFmpeg = geode::dirs::getGameDir() / "ffmpeg";
  if (isValidFFmpegBinaryPath(gameDirFFmpeg))
    return gameDirFFmpeg;

  return {};
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

  if (!editor && pl) {
    // Use levelTime as the frame source to avoid progress-based jumps that
    // can fast-forward playback and skip actions.
    frame = static_cast<int>(bgl->m_gameState.m_levelTime * getTPS());
    if (g.macro.geobotMacro)
      frame++;
  } else {
    frame = static_cast<int>(bgl->m_gameState.m_levelTime * getTPS());
    frame++;
  }

  frame -= g.frameOffset;
  if (frame < 0) return 0;

  return frame;
}

void Global::updateKeybinds() {
  // Legacy custom-keybinds integration is disabled for Geode v5 migration.
}

void Global::updateSeed(bool isRestart) {

  auto& g = Global::get();

  if (g.seedEnabled) {
    PlayLayer* pl = PlayLayer::get();
    if (!pl) return;

    unsigned long long ull = std::stoull(g.mod->getSavedValue<std::string>("macro_seed"), nullptr, 0);
    uintptr_t seed = static_cast<uintptr_t>(ull);
    int finalSeed;

    if (!pl->m_player1->m_isDead) {
      std::mt19937 generator(seed + pl->m_gameState.m_currentProgress);
      std::uniform_int_distribution<int> distribution(10000, 999999999);
      finalSeed = distribution(generator);
    }
    else {
      std::random_device rd;
      std::mt19937 generator(rd());
      std::uniform_int_distribution<int> distribution(1000, 999999999);
      finalSeed = distribution(generator);
    }

#ifdef GEODE_IS_WINDOWS
    *(uintptr_t*)((char*)geode::base::get() + seedAddr) = finalSeed;
#else
    GameToolbox::fast_srand(finalSeed);
#endif

    g.safeMode = true;
  }

  if (isRestart && g.state == state::recording) {
#ifdef GEODE_IS_WINDOWS
    g.macro.seed = *(uintptr_t*)((char*)geode::base::get() + seedAddr);
#else
    g.macro.seed = 0;
#endif
  }

}

void Global::updatePitch(float value) {
  auto& g = Global::get();
  if (!g.speedhackAudio) {
    if (g.currentPitch != 1.f) value = 1.f;
    else return;
  }

  FMODAudioEngine* fmod = FMODAudioEngine::sharedEngine();
  FMOD::ChannelGroup* channel = nullptr;
  fmod->m_system->getMasterChannelGroup(&channel);

  if (channel) {
    channel->setPitch(value);
    g.currentPitch = value;
  }
}

void Global::frameStep() {
  auto& g = Global::get();
  if (!PlayLayer::get() || !g.frameStepper) return;

  g.stepFrame = true;
  g.stepFrameDraw = true;
  g.stepFrameParticle = 4;
}

void Global::toggleSpeedhack() {
  auto& g = Global::get();
  g.mod->setSavedValue("macro_speedhack_enabled", !g.mod->getSavedValue<bool>("macro_speedhack_enabled"));
  g.speedhackEnabled = g.mod->getSavedValue<bool>("macro_speedhack_enabled");

  if (g.layer) {
    if (static_cast<RecordLayer*>(g.layer)->speedhackToggle)
      static_cast<RecordLayer*>(g.layer)->speedhackToggle->toggle(g.mod->getSavedValue<bool>("macro_speedhack_enabled"));
  }

  if (!g.mod->getSavedValue<bool>("macro_speedhack_enabled"))
    Global::updatePitch(1.f);
}

void Global::toggleFrameStepper() {
  if (Global::get().frameStepper)
    Global::frameStepperOff();
  else
    Global::frameStepperOn();
}

void Global::frameStepperOn() {
  auto& g = Global::get();

  g.mod->setSavedValue("macro_frame_stepper", true);
  g.frameStepper = true;

  if (g.layer) {
    if (static_cast<RecordLayer*>(g.layer)->frameStepperToggle)
      static_cast<RecordLayer*>(g.layer)->frameStepperToggle->toggle(true);
  }

  if (PlayLayer::get())
    g.frameStepperMusicTime = FMODAudioEngine::sharedEngine()->getMusicTimeMS(0);

  Interface::updateButtons();
}

void Global::frameStepperOff() {
  auto& g = Global::get();

  g.mod->setSavedValue("macro_frame_stepper", false);
  g.stepFrame = false;
  g.stepFrameParticle = false;
  g.frameStepper = false;

  if (PlayLayer::get() && g.frameStepperMusicTime != 0) {
    FMODAudioEngine::sharedEngine()->setMusicTimeMS(g.frameStepperMusicTime, true, 0);
    g.frameStepperMusicTime = 0;
  }

  if (g.layer) {
    if (static_cast<RecordLayer*>(g.layer)->frameStepperToggle)
      static_cast<RecordLayer*>(g.layer)->frameStepperToggle->toggle(false);
  }

  Interface::updateButtons();
}

PauseLayer* Global::getPauseLayer() {
  for (CCNode* child : CCDirector::sharedDirector()->getRunningScene()->getChildrenExt()) {
    if (PauseLayer* pauseLayer = typeinfo_cast<PauseLayer*>(child)) {
      return pauseLayer;
    }
  }

  return nullptr;
}

void Global::triggerFramePerfectOverlay(int button, bool down) {
  auto& g = Global::get();
  const char* buttonName = "Click";
  if (button == 2) buttonName = "Left";
  else if (button == 3) buttonName = "Right";

  g.framePerfectOverlayText = fmt::format("Frame Perfect: {} {}", buttonName, down ? "Press" : "Release");
  g.framePerfectOverlayFrames = 30;
}

void Global::triggerFramePerfectOverlayCounted(size_t actionIndex, int button, bool down, std::string const& typeName) {
  auto& g = Global::get();
  if (g.lastFramePerfectAction == actionIndex)
    return;

  g.lastFramePerfectAction = actionIndex;
  g.framePerfectCount++;

  const char* buttonName = "Click";
  if (button == 2) buttonName = "Left";
  else if (button == 3) buttonName = "Right";

  double tps = std::max(1.0, static_cast<double>(Global::getTPS()));
  int count60 = static_cast<int>(std::lround(static_cast<double>(g.framePerfectCount) * 60.0 / tps));
  int count144 = static_cast<int>(std::lround(static_cast<double>(g.framePerfectCount) * 144.0 / tps));
  int count240 = static_cast<int>(std::lround(static_cast<double>(g.framePerfectCount) * 240.0 / tps));

  g.framePerfectOverlayText = fmt::format(
    "FP {} {} ({}) | 60:{} 144:{} 240:{}",
    buttonName,
    down ? "Press" : "Release",
    typeName,
    count60,
    count144,
    count240
  );
  g.framePerfectOverlayFrames = 45;
}

std::filesystem::path Global::getFolderSettingPath(std::string const& settingID, bool createIfMissing) {
  auto& g = Global::get();
  auto fallback = [&]() {
    if (settingID == "macros_folder")
      return g.mod->getSaveDir() / "macros";
    if (settingID == "autosaves_folder")
      return g.mod->getSaveDir() / "autosaves";
    if (settingID == "render_folder")
      return g.mod->getSaveDir() / "renders";
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

  if (!g.mod->setSavedValue("defaults_set_14", true)) {
    g.mod->setSavedValue("render_fade_in_video", std::to_string(2));
    g.mod->setSavedValue("render_fade_out_video", std::to_string(2));
  }

  if (!g.mod->setSavedValue("defaults_set_12", true)) {
    g.mod->setSettingValue<std::filesystem::path>("macros_folder", g.mod->getSaveDir() / "macros");
    g.mod->setSettingValue<std::filesystem::path>("autosaves_folder", g.mod->getSaveDir() / "autosaves");
  }

  #ifdef GEODE_IS_ANDROID
  
  if (!g.mod->setSavedValue("defaults_set_15", true))
    g.mod->setSavedValue("render_video_args", std::string(""));
  
  if (!g.mod->setSavedValue("defaults_set_11", true))
    g.mod->setSavedValue("render_codec", std::string("libx264"));
    g.mod->setSavedValue("render_hardware_accel", std::string("Off"));
  
  #endif

  if (!g.mod->setSavedValue("defaults_set_10", true)) {
    g.mod->setSettingValue("restore_page", true);

    g.mod->setSavedValue("autosave_interval_enabled", false);
    g.mod->setSavedValue("autosave_interval", std::to_string(10));
    g.mod->setSavedValue("autosave_checkpoint_enabled", true);
    g.mod->setSavedValue("autosave_levelend_enabled", true);
    
    g.mod->setSavedValue("render_fade_in_video", std::to_string(2));
    g.mod->setSavedValue("render_fade_out_video", std::to_string(2));

    g.mod->setSavedValue("auto_stop_playing", false);
    g.mod->setSavedValue("macro_tps", 240.f);
    g.mod->setSavedValue("macro_tps_enabled", false);

    g.mod->setSavedValue("autoclicker_hold_for", 5);
    g.mod->setSavedValue("autoclicker_release_for", 5);
    g.mod->setSavedValue("autoclicker_hold_for2", 5);
    g.mod->setSavedValue("autoclicker_release_for2", 5);
    g.mod->setSavedValue("autoclicker_p1", true);
    g.mod->setSavedValue("autoclicker_p2", true);

    g.mod->setSavedValue("trajectory_color1", ccc3(74, 226, 85));
    g.mod->setSavedValue("trajectory_color2", ccc3(130, 8, 8));
    g.mod->setSavedValue("trajectory_length", std::to_string(240));

  }

  if (!g.mod->setSavedValue("defaults_set3", true)) {
    g.mod->setSettingValue<std::filesystem::path>("render_folder", g.mod->getSaveDir() / "renders");
    g.mod->setSavedValue("render_file_extension", std::string(".mp4"));
    g.mod->setSavedValue("render_sfx_volume", 1.f);
    g.mod->setSavedValue("render_music_volume", 1.f);
    g.mod->setSavedValue("respawn_time", 0.5f);
    g.mod->setSavedValue("render_seconds_after", std::to_string(2));
    g.mod->setSavedValue("render_record_audio", true);
    g.mod->setSavedValue("render_args", std::string("-pix_fmt yuv420p"));
    g.mod->setSavedValue("macro_noclip_p1", true);
    g.mod->setSavedValue("macro_noclip_p2", true);

    g.mod->setSavedValue("render_width2", std::to_string(1920));
    g.mod->setSavedValue("render_height", std::to_string(1080));
    g.mod->setSavedValue("render_bitrate", std::to_string(12));
    g.mod->setSavedValue("render_fps", std::to_string(60));
    g.mod->setSavedValue("render_video_args", std::string("colorspace=all=bt709:iall=bt470bg:fast=1"));

    g.mod->setSavedValue("render_codec", std::string("libx264"));
    auto bundledFFmpeg = resolveBundledFFmpegPath(g.mod);
    if (!bundledFFmpeg.empty())
      g.mod->setSettingValue("ffmpeg_path", bundledFFmpeg);

    g.mod->setSavedValue("render_record_audio", true);
    g.mod->setSavedValue("render_hide_labels", true);

    g.mod->setSavedValue("macro_seed", std::to_string(1));
    g.mod->setSavedValue("macro_speedhack", std::string("0.5"));
    g.mod->setSavedValue("macro_fps", 3);

    g.mod->setSavedValue("macro_ignore_inputs", true);
    g.mod->setSavedValue("macro_auto_safe_mode", true);
    g.mod->setSavedValue("macro_speedhack_audio", true);
    g.mod->setSavedValue("macro_show_frame_label", false);
    g.mod->setSavedValue("macro_hide_playing_label", true);

    g.mod->setSavedValue("menu_show_button", true);
    g.mod->setSavedValue("menu_pause_on_open", false);
    g.mod->setSavedValue("menu_show_cursor", true);

    #ifdef GEODE_IS_ANDROID
    g.mod->setSavedValue("menu_show_cursor", false);
    #endif

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

  if (!g.mod->hasSavedValue("frame_perfect_overlay_mode"))
    g.mod->setSavedValue("frame_perfect_overlay_mode", std::string("When"));

  // Migrate legacy saved keys to current setting IDs.
  if (!g.mod->hasSavedValue("auto_stop_playing") && g.mod->hasSavedValue("macro_auto_stop_playing"))
    g.mod->setSavedValue("auto_stop_playing", g.mod->getSavedValue<bool>("macro_auto_stop_playing"));
  if (!g.mod->hasSavedValue("disable_shaders") && g.mod->hasSavedValue("disableShaders"))
    g.mod->setSavedValue("disable_shaders", g.mod->getSavedValue<bool>("disableShaders"));

  g.showTrajectory = g.mod->getSavedValue<bool>("macro_show_trajectory");
  g.coinFinder = g.mod->getSavedValue<bool>("macro_coin_finder");
  g.frameStepper = g.mod->getSavedValue<bool>("macro_frame_stepper");
  g.seedEnabled = g.mod->getSavedValue<bool>("macro_seed_enabled");
  g.frameLabel = g.mod->getSavedValue<bool>("macro_show_frame_label");
  g.speedhackAudio = g.mod->getSavedValue<bool>("macro_speedhack_audio");
  g.trajectoryBothSides = g.mod->getSavedValue<bool>("macro_trajectory_both_sides");
  g.p2mirror = g.mod->getSavedValue<bool>("p2_input_mirror");
  g.tpsEnabled = g.mod->getSavedValue<bool>("macro_tps_enabled");
  g.tps = g.mod->getSavedValue<double>("macro_tps");
  g.autoclicker = g.mod->getSavedValue<bool>("autoclicker_enabled");
  g.autoclickerP1 = g.mod->getSavedValue<bool>("autoclicker_p1");
  g.autoclickerP2 = g.mod->getSavedValue<bool>("autoclicker_p2");
  g.disableShaders = g.mod->getSavedValue<bool>("disable_shaders");
  g.autosaveIntervalEnabled = g.mod->getSavedValue<bool>("autosave_interval_enabled");
  g.autosaveEnabled = g.mod->getSavedValue<bool>("macro_auto_save");

  g.holdFor = static_cast<int>(getSavedInt64Safe(g.mod, "autoclicker_hold_for", 5));
  g.releaseFor = static_cast<int>(getSavedInt64Safe(g.mod, "autoclicker_release_for", 5));
  g.holdFor2 = static_cast<int>(getSavedInt64Safe(g.mod, "autoclicker_hold_for2", 5));
  g.releaseFor2 = static_cast<int>(getSavedInt64Safe(g.mod, "autoclicker_release_for2", 5));
  g.currentPage = static_cast<int>(getSavedInt64Safe(g.mod, "current_page", 0));

  g.autosaveInterval = (geode::utils::numFromString<float>(g.mod->getSavedValue<std::string>("autosave_interval")).unwrapOr(0.f) * 60);
  
  g.speedhackEnabled = false;
  g.mod->setSavedValue("macro_speedhack_enabled", false);

  g.frameOffset = static_cast<int>(getSavedInt64Safe(g.mod, "frame_offset", 0));
  g.frameFixesLimit = static_cast<int>(getSavedInt64Safe(g.mod, "frame_fixes_limit", 240));
  g.lockDelta = g.mod->getSavedValue<bool>("lock_delta");
  g.stopPlaying = g.mod->getSavedValue<bool>("auto_stop_playing");

  if (g.mod->getSavedValue<std::string>("render_hardware_accel").empty())
    g.mod->setSavedValue("render_hardware_accel", std::string("Off"));

  auto ffmpegPath = g.mod->getSettingValue<std::filesystem::path>("ffmpeg_path");
  bool hasValidFFmpeg = isValidFFmpegBinaryPath(ffmpegPath);
  if (!hasValidFFmpeg) {
    auto bundledFFmpeg = resolveBundledFFmpegPath(g.mod);
    if (!bundledFFmpeg.empty())
      g.mod->setSettingValue("ffmpeg_path", bundledFFmpeg);
  }

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
