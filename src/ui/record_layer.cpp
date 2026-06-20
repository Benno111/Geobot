#include "record_layer.hpp"
#include "macro_editor.hpp"
#include "game_ui.hpp"
#include "render_presets_layer.hpp"
#include "clickbot_layer.hpp"
#include "noclip_settings_layer.hpp"
#include "autoclicker_settings_layer.hpp"
#include "trajectory_settings_layer.hpp"
#include "pathfinder_settings_layer.hpp"
#include "mirror_settings_layer.hpp"
#include "star_rate_override_layer.hpp"
#include "../hacks/coin_finder.hpp"
#include "../hacks/show_trajectory.hpp"

#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/utils/web.hpp>
#include <array>
#include <charconv>
#include <cctype>
#include <ctime>

namespace {
bool parseU64Safe(std::string const& raw, unsigned long long& out) {
	if (raw.empty()) return false;
	auto begin = raw.data();
	auto end = begin + raw.size();
	while (begin < end && std::isspace(static_cast<unsigned char>(*begin))) ++begin;
	while (end > begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) --end;
	if (begin >= end) return false;

	if (*begin == '+') ++begin;
	else if (*begin == '-') return false;
	if (begin >= end) return false;

	int base = 10;
	if ((end - begin) >= 2 && begin[0] == '0' && (begin[1] == 'x' || begin[1] == 'X')) {
		base = 16;
		begin += 2;
		if (begin >= end) return false;
	}
	else if ((end - begin) > 1 && begin[0] == '0') {
		base = 8;
	}

	auto [ptr, ec] = std::from_chars(begin, end, out, base);
	return ec == std::errc() && ptr == end;
}

bool parseIntSafe(std::string const& raw, int& out) {
	if (raw.empty()) return false;
	auto begin = raw.data();
	auto end = begin + raw.size();
	while (begin < end && std::isspace(static_cast<unsigned char>(*begin))) ++begin;
	while (end > begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) --end;
	if (begin >= end) return false;
	auto [ptr, ec] = std::from_chars(begin, end, out);
	return ec == std::errc() && ptr == end;
}
}

struct SettingsCategory {
    std::string title;
    std::vector<RecordSetting> settings;
};

const std::vector<SettingsCategory> kSettingsCategories {
    {
        "Macro Settings",
        {
            { "Accuracy:", "macro_accuracy", InputType::Accuracy, 0.4f },
            { "FP Overlay:", "frame_perfect_overlay_mode", InputType::FramePerfectMode, 0.4f },
            { "Frame Offset:", "frame_offset", InputType::FrameOffset, 0.4f },
            { "Frame Fix Limit:", "frame_fixes_limit", InputType::FrameFixesLimit, 0.4f },
            { "Lock Delta:", "lock_delta", InputType::None },
            { "Auto Stop Playing:", "auto_stop_playing", InputType::None },
            { "TPS Bypass:", "macro_tps_enabled", InputType::Tps, 0.4f },
            { "Speedhack:", "macro_speedhack_enabled", InputType::Speedhack, 0.4f },
            { "Seed:", "macro_seed_enabled", InputType::Seed, 0.4f },
            { "Enable Noclip:", "macro_noclip", InputType::Settings, 0.325f, menu_selector(NoclipSettingsLayer::open) },
            { "Show Trajectory:", "macro_show_trajectory", InputType::Settings, 0.325f, menu_selector(TrajectorySettingsLayer::open)  },
            { "Enable Frame Stepper:", "macro_frame_stepper", InputType::None },
            { "Instant respawn:", "macro_instant_respawn", InputType::None },
            { "No death effect:", "macro_no_death_effect", InputType::None },
            { "No respawn flash:", "macro_no_respawn_flash", InputType::None },
            { "Enable Coin Finder:", "macro_coin_finder", InputType::None },
            { "Enable Layout Mode:", "macro_layout_mode", InputType::None },
            { "Auto Safe Mode:", "macro_auto_safe_mode", InputType::None }
        }
    },
    {
        "Path Finder Settings",
        {
            { "Pathfinder Mode:", "pathfinder_mode", InputType::Settings, 0.34f, menu_selector(RecordLayer::openPathfinderSettings) }
        }
    },
    {
        "Render Settings",
        {
#ifdef GEODE_IS_WINDOWS
            { "Force cursor on open:", "menu_show_cursor", InputType::None },
            { "Button on pause menu:", "menu_show_button", InputType::None },
            { "Pause on open:", "menu_pause_on_open", InputType::None },
#else
            { "Always show buttons:", "macro_always_show_buttons", InputType::None },
            { "Hide speedhack button:", "macro_hide_speedhack", InputType::None },
            { "Hide Frame Stepper button:", "macro_hide_stepper", InputType::None, 0.3f },
#endif
            { "Hide labels on render:", "render_hide_labels", InputType::None },
            { "Hide playing label:", "macro_hide_playing_label", InputType::None },
            { "Hide recording label:", "macro_hide_recording_label", InputType::None },
            { "Renders Folder:", "render_folder_btn", InputType::Action, 0.325f, menu_selector(RecordLayer::openRendersFolder) }
        }
    },
    {
        "Other Settings",
        {
            { "Enable Clickbot:", "clickbot_enabled", InputType::Settings, 0.325f, menu_selector(ClickbotLayer::open)},
            { "Enable Autoclicker:", "autoclicker_enabled", InputType::Settings, 0.3f, menu_selector(AutoclickerLayer::open) },
            { "Always Practice Fixes:", "macro_always_practice_fixes", InputType::None },
            { "Ignore inputs:", "macro_ignore_inputs", InputType::None },
            { "Show Frame Label:", "macro_show_frame_label", InputType::None },
            { "Speedhack Audio:", "macro_speedhack_audio", InputType::None },
            { "Macros Folder:", "macros_folder_btn", InputType::Action, 0.325f, menu_selector(RecordLayer::openMacrosFolder) },
            { "Autosaves Folder:", "autosaves_folder_btn", InputType::Action, 0.325f, menu_selector(RecordLayer::openAutosavesFolder) },
            { "Respawn Time:", "respawn_time_enabled", InputType::Respawn },
            { "Input Mirror:", "p2_input_mirror", InputType::Settings, 0.325f, menu_selector(MirrorSettingsLayer::open) },
            { "Disable Shaders:", "disable_shaders", InputType::None },
            { "Instant Mirror Portal:", "instant_mirror_portal", InputType::None },
            { "No Mirror Portal:", "no_mirror_portal", InputType::None },
            { "Enable Auto Saving:", "macro_auto_save", InputType::Autosave }
        }
    }
};

namespace {
const std::vector<std::string> kAccuracyModes = {
    "Vanilla",
    "Input Fixes",
    "Frame Fixes"
};

const std::vector<std::string> kFramePerfectOverlayModes = {
    "Never",
    "Always"
};

const std::vector<std::string> kFramePerfectOverlayDeveloperModes = {
    "Never",
    "When",
    "Always"
};

std::string getSavedAccuracyMode(Mod* mod) {
    std::string value = mod->getSavedValue<std::string>("macro_accuracy");
    for (auto const& mode : kAccuracyModes) {
        if (value == mode)
            return value;
    }
    return "Frame Fixes";
}

void applyAccuracyMode(std::string const& value) {
    auto& g = Global::get();
    g.frameFixes = value == "Frame Fixes";
    g.inputFixes = value == "Input Fixes";
}

std::string getSavedFramePerfectOverlayMode(Mod* mod) {
    (void)mod;
    return Global::getFramePerfectOverlayMode();
}

std::vector<std::string> const& getFramePerfectOverlayModes() {
    if (Global::isDeveloperModeEnabled())
        return kFramePerfectOverlayDeveloperModes;
    return kFramePerfectOverlayModes;
}

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

std::time_t getBuildTimestampForExpiry() {
    // __DATE__ format: "Mmm dd yyyy"
    std::string date = __DATE__;
    if (date.size() < 11) return static_cast<std::time_t>(-1);

    int month = monthFromDateAbbrev(std::string_view(date.data(), 3));
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

std::string getBuildExpiryTimerText() {
    std::time_t build = getBuildTimestampForExpiry();
    if (build == static_cast<std::time_t>(-1))
        return "Exp: N/A";

    constexpr std::time_t kThirtyDays = static_cast<std::time_t>(30 * 24 * 60 * 60);
    std::time_t expiry = build + kThirtyDays;
    std::time_t now = std::time(nullptr);

    if (now >= expiry)
        return "Exp: expired";

    std::time_t remaining = expiry - now;
    int days = static_cast<int>(remaining / (24 * 60 * 60));
    int hours = static_cast<int>((remaining % (24 * 60 * 60)) / (60 * 60));

    return fmt::format("Exp: {}d {:02}h", days, hours);
}

GJGameLevel* getCurrentLevelForMenus() {
    if (PlayLayer* pl = PlayLayer::get())
        return pl->m_level;

    if (LevelEditorLayer* lel = LevelEditorLayer::get())
        return lel->m_level;

    return nullptr;
}

class GeobotPauseButtonHandler : public CCObject {
public:
    void onPress(CCObject*) {
        // EditorPauseLayer can conflict with popup input routing; close it
        // first, then open geobot on the next main-thread tick.
        if (CCScene* scene = CCDirector::sharedDirector()->getRunningScene()) {
            if (EditorPauseLayer* editorPause = scene->getChildByType<EditorPauseLayer>(0))
                editorPause->onResume(nullptr);
        }

        Loader::get()->queueInMainThread([] {
            RecordLayer::openMenu(true);
        });
    }

    static GeobotPauseButtonHandler* get() {
        static GeobotPauseButtonHandler* inst = []() {
            auto* obj = new GeobotPauseButtonHandler();
            obj->autorelease();
            obj->retain();
            return obj;
        }();
        return inst;
    }
};

CCNode* findNodeByIDRecursive(CCNode* root, const char* id) {
    if (!root) return nullptr;
    if (root->getID() == id) return root;

    CCArray* children = root->getChildren();
    if (!children) return nullptr;

    for (int i = 0; i < children->count(); i++) {
        CCNode* child = dynamic_cast<CCNode*>(children->objectAtIndex(i));
        if (CCNode* found = findNodeByIDRecursive(child, id))
            return found;
    }
    return nullptr;
}

CCMenu* findSettingsMenu(CCLayer* layer) {
    if (auto settingsBtn = findNodeByIDRecursive(layer, "settings-button")) {
        if (auto menu = typeinfo_cast<CCMenu*>(settingsBtn->getParent()))
            return menu;
    }

    if (CCNode* menu = layer->getChildByID("right-button-menu"))
        return typeinfo_cast<CCMenu*>(menu);
    if (CCNode* menu = layer->getChildByID("left-button-menu"))
        return typeinfo_cast<CCMenu*>(menu);
    if (CCNode* menu = layer->getChildByID("bottom-button-menu"))
        return typeinfo_cast<CCMenu*>(menu);

    // Editor/Pause fallback when node IDs are unavailable.
    if (CCMenu* menu = layer->getChildByType<CCMenu>(1))
        return menu;
    if (CCMenu* menu = layer->getChildByType<CCMenu>(0))
        return menu;

    return nullptr;
}

void addgeobotPauseButton(cocos2d::CCLayer* layer) {
    if (Global::isBuildExpired()) return;

#ifdef GEODE_IS_WINDOWS
    if (!Mod::get()->getSavedValue<bool>("menu_show_button")) return;
#endif

    CCSprite* sprite = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
    sprite->setScale(0.35f);

    CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(
        sprite,
        GeobotPauseButtonHandler::get(),
        menu_selector(GeobotPauseButtonHandler::onPress)
    );
    btn->setID("geobot-button"_spr);

    if (auto settingsBtn = findNodeByIDRecursive(layer, "settings-button")) {
        if (auto settingsMenu = typeinfo_cast<CCMenu*>(settingsBtn->getParent())) {
            btn->setPosition(settingsBtn->getPosition() + ccp(-42.f, 0.f));
            settingsMenu->addChild(btn);
            return;
        }
    }

    if (auto settingsMenu = findSettingsMenu(layer)) {
        if (CCArray* children = settingsMenu->getChildren()) {
            if (children->count() > 0) {
                if (CCNode* first = static_cast<CCNode*>(children->objectAtIndex(0)))
                    btn->setPosition(first->getPosition() + ccp(-42.f, 0.f));
            }
        }

        settingsMenu->addChild(btn);
        settingsMenu->updateLayout();
        return;
    }

    CCMenu* fallbackMenu = CCMenu::create();
    fallbackMenu->setID("button"_spr);
    layer->addChild(fallbackMenu);
    btn->setPosition({214, 88});
    fallbackMenu->addChild(btn);
}
}

class $modify(PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        addgeobotPauseButton(this);
    }
};

class $modify(EditorPauseLayer) {
    void customSetup() {
        EditorPauseLayer::customSetup();
        addgeobotPauseButton(this);
    }
};

$execute{
    geode::listenForSettingChanges<cocos2d::ccColor3B>("background_color", +[](cocos2d::ccColor3B value) {
        auto& g = Global::get();
        if (g.layer) {
            CCArray* children = CCDirector::sharedDirector()->getRunningScene()->getChildren();
            if (FLAlertLayer* layer = typeinfo_cast<FLAlertLayer*>(children->lastObject()))
                layer->removeFromParentAndCleanup(true);

            static_cast<RecordLayer*>(g.layer)->onClose(nullptr);
            RecordLayer::openMenu(true);
        }
  });
};

void RecordLayer::openSaveMacro(CCObject*) {
    SaveMacroLayer::open();
}

void RecordLayer::openLoadMacro(CCObject*) {
    LoadMacroLayer::open(static_cast<geode::Popup*>(this), nullptr);
}

void RecordLayer::openPathfinderSettings(CCObject*) {
    if (auto* layer = PathfinderSettingsLayer::create())
        layer->show();
}

void RecordLayer::openStarRateOverride(CCObject*) {
    if (auto* layer = StarRateOverrideLayer::create())
        layer->show();
}

void RecordLayer::clear22Percentage(CCObject*) {
    GJGameLevel* level = getCurrentLevelForMenus();
    if (!level) {
        FLAlertLayer::create("Clear 2.2 Info", "Open a <cl>level</c> or the <cl>editor</c> first.", "Ok")->show();
        return;
    }

    geode::createQuickPopup(
        "Clear 2.2 Info",
        "Clear this level's <cl>2.2 percentage info</c>?",
        "Cancel", "Yes",
        [level](auto, bool btn2) {
            if (!btn2 || !level) return;

            level->setNewNormalPercent2(0);
            level->m_orbCompletion = 0;
            level->m_attemptTime = 0;
            level->m_bestTime = 0;
            level->m_ticksTime = 0;
            level->m_clicksTime = 0;
            level->m_coinsTime = 0;
            level->m_savedTime = false;

            if (GameLevelManager* glm = GameLevelManager::sharedState()) {
                glm->updateLevel(level);
                glm->saveLevel(level);
            }

            Notification::create("2.2 percentage + time info cleared", NotificationIcon::Success)->show();
        }
    );
}

RecordLayer* RecordLayer::openMenu(bool instant) {
    auto& g = Global::get();
    if (g.buildExpired) {
        Global::showBuildExpiredNotice();
        return nullptr;
    }

    PlayLayer* pl = PlayLayer::get();
    bool cursor = false;

    CCArray* children = CCDirector::sharedDirector()->getRunningScene()->getChildren();
    CCObject* child;

    if (g.layer)
        static_cast<RecordLayer*>(g.layer)->onClose(nullptr);

    if (pl && g.mod->getSavedValue<bool>("menu_pause_on_open")) {
        if (!pl->m_isPaused)
            pl->pauseGame(false);
    }
#ifdef GEODE_IS_WINDOWS
    else if (pl && g.mod->getSavedValue<bool>("menu_show_cursor")) {
        cursor = cocos2d::CCEGLView::sharedOpenGLView()->getShouldHideCursor();
        cocos2d::CCEGLView::sharedOpenGLView()->showCursor(true);
    }
#endif

    RecordLayer* layer = create();
    layer->cursorWasHidden = cursor;
    layer->m_noElasticity = instant || Global::get().speedhackEnabled;
    layer->show();

    g.layer = static_cast<geode::Popup*>(layer);

    return layer;
}

void RecordLayer::checkSpeedhack() {
    std::string speedhackValue = mod->getSavedValue<std::string>("macro_speedhack");

    if (std::count(speedhackValue.begin(), speedhackValue.end(), '.') == 0)
        speedhackValue += ".0";

    if (speedhackValue.back() == '.')
        speedhackValue += "0";

    if (speedhackValue[0] == '0' && speedhackValue[1] != '.')
        speedhackValue.erase(0, 1);

    if (speedhackValue[0] == '.')
        speedhackValue = "0" + speedhackValue;

    mod->setSavedValue("macro_speedhack", speedhackValue);
}

void RecordLayer::onClose(CCObject*) {
    checkSpeedhack();

    PlayLayer* pl = PlayLayer::get();

    if (cursorWasHidden && pl)
        PlatformToolbox::hideCursor();

    Global::get().layer = nullptr;

    this->setKeypadEnabled(false);
    this->setTouchEnabled(false);
    this->removeFromParentAndCleanup(true);
}

void RecordLayer::toggleRecording(CCObject*) {
    auto& g = Global::get();
    if (g.buildExpired) {
        Global::showBuildExpiredNotice();
        return recording->toggle(true);
    }

    if (Global::hasIncompatibleMods())
        return recording->toggle(true);

    if (g.state == state::playing) playing->toggle(false);
    g.state = g.state == state::recording ? state::none : state::recording;

    if (g.state == state::recording) {
        g.currentAction = 0;
        g.currentFrameFix = 0;

        PlayLayer* pl = PlayLayer::get();
        // Restarting from the beginning clears Geometry Dash's practice
        // checkpoint stack. When recording is started mid-practice, keep the
        // existing checkpoints and continue from the current practice state
        // instead of deleting every checkpoint the player has placed.
        g.restart = !(pl && pl->m_isPracticeMode && !g.checkpoints.empty());

        if (pl) {
            if (!pl->m_isPaused)
                pl->pauseGame(false);
        }
    }

    Interface::updateLabels();
    Interface::updateButtons();
    Macro::updateTPS();
    this->updateTPS();

    g.lastAutoSaveMS = std::chrono::steady_clock::now();
}

void RecordLayer::togglePlaying(CCObject*) {
    auto& g = Global::get();
    if (g.buildExpired) {
        Global::showBuildExpiredNotice();
        return playing->toggle(true);
    }

    if (Global::hasIncompatibleMods())
        return playing->toggle(true);

    if (g.state == state::recording)
        recording->toggle(false);

    g.state = g.state == state::playing ? state::none : state::playing;

    if (g.state == state::playing) {
        g.currentAction = 0;
        g.currentFrameFix = 0;

        g.macro.geobotMacro = g.macro.botInfo.name == "geobot";
        
        PlayLayer* pl = PlayLayer::get();

        if (pl) {
            if (!pl->m_isPaused && !pl->m_levelEndAnimationStarted)
                pl->resetLevelFromStart();
            else
                g.restart = true;
        }
    }

    Interface::updateLabels();
    Interface::updateButtons();
    Macro::updateTPS();
    this->updateTPS();
}

void RecordLayer::toggleRender(CCObject* btn) {
    if (!Renderer::toggle())
        static_cast<CCMenuItemToggler*>(btn)->toggle(true);

    if (Global::get().renderer.recordingAudio)
        static_cast<CCMenuItemToggler*>(btn)->toggle(false);
}

void RecordLayer::onEditMacro(CCObject*) {
    MacroEditLayer::open();
}

void RecordLayer::toggleFPS(bool on) { // forgotten
    return;
    float scaleSpr = -0.8, scaleBtn = -1;
    int opacityBtn = 57, opacityLbl = 80;

    if (on) {
        return;
        scaleSpr = 0.8;
        scaleBtn = 1;
        opacityBtn = 230;
        opacityLbl = 255;
    }

    CCSprite* spr = CCSprite::createWithSpriteFrameName("edit_leftBtn_001.png");
    spr->setScale(scaleSpr);
    FPSLeft->setSprite(spr);
    FPSLeft->setScale(scaleBtn);
    FPSLeft->setOpacity(opacityBtn);

    spr = CCSprite::createWithSpriteFrameName("edit_rightBtn_001.png");
    spr->setScale(scaleSpr);
    FPSRight->setSprite(spr);
    FPSRight->setScale(scaleBtn);
    FPSRight->setOpacity(opacityBtn);

    fpsLabel->setOpacity(opacityLbl);
}

void RecordLayer::macroInfo(CCObject*) {
    MacroInfoLayer::create()->show();
}

void RecordLayer::textChanged(CCTextInputNode* node) {
    if (!node) return;

    mod = Mod::get();

    if (node == seedInput) {
        unsigned long long num = 0;
        std::string seedStr = seedInput ? std::string(seedInput->getString()) : std::string();
        if (parseU64Safe(seedStr, num)) {
            mod->setSavedValue("macro_seed", std::to_string(num));
            return;
        }
        else {
            return seedInput->setString(mod->getSavedValue<std::string>("macro_seed").c_str());
        }
    }

    if (node == codecInput)
        mod->setSavedValue("render_codec", std::string(codecInput->getString()));

    if (std::string_view(widthInput->getString()) != "" && node == widthInput)
        mod->setSavedValue("render_width2", std::string(widthInput->getString()));

    if (std::string_view(heightInput->getString()) != "" && node == heightInput)
        mod->setSavedValue("render_height", std::string(heightInput->getString()));

    if (std::string_view(bitrateInput->getString()) != "" && node == bitrateInput)
        mod->setSavedValue("render_bitrate", std::string(bitrateInput->getString()));

    if (std::string_view(fpsInput->getString()) != "" && node == fpsInput) {
        int fpsValue = 0;
        if (!parseIntSafe(std::string(fpsInput->getString()), fpsValue) || fpsValue > 240)
            return fpsInput->setString(mod->getSavedValue<std::string>("render_fps").c_str());
        mod->setSavedValue("render_fps", std::string(fpsInput->getString()));
    }

    if (respawnInput && node == respawnInput) {
        std::string str = respawnInput->getString();
        mod->setSavedValue("respawn_time", numFromString<double>(str).unwrapOr(0.5));
    }

    if (tpsInput && node == tpsInput) {
        float value = geode::utils::numFromString<float>(tpsInput->getString()).unwrapOr(0.f);
        if (std::string_view(tpsInput->getString()) != "" && value < 999999 && value >= 0.f) {
            mod->setSavedValue("macro_tps", value);
            Global::get().tps = value;
            Global::get().leftOver = 0.f;
        }
    }

    if (frameOffsetInput && node == frameOffsetInput) {
        auto value = geode::utils::numFromString<int>(frameOffsetInput->getString());
        if (!value) {
            frameOffsetInput->setString(std::to_string(Global::get().frameOffset).c_str());
            return;
        }
        int parsed = value.unwrap();
        if (parsed < -100) parsed = -100;
        if (parsed > 100) parsed = 100;
        mod->setSavedValue("frame_offset", parsed);
        Global::get().frameOffset = parsed;
        warningLabel->setString(("WARNING: Currently recording / playing macros with a frame offset of " + std::to_string(parsed)).c_str());
        warningLabel->setVisible(parsed != 0);
        warningSprite->setVisible(parsed != 0);
    }

    if (frameFixesLimitInput && node == frameFixesLimitInput) {
        auto value = geode::utils::numFromString<int>(frameFixesLimitInput->getString());
        if (!value) {
            frameFixesLimitInput->setString(std::to_string(Global::get().frameFixesLimit).c_str());
            return;
        }
        int parsed = std::max(1, value.unwrap());
        mod->setSavedValue("frame_fixes_limit", parsed);
        Global::get().frameFixesLimit = parsed;
    }

    if (!speedhackInput || node != speedhackInput) return;

    if (std::string_view(speedhackInput->getString()) != "" && node == speedhackInput) {
        std::string value = speedhackInput->getString();

        if (value == ".")
            speedhackInput->setString("0.");
        else if (std::count(value.begin(), value.end(), '.') == 2 || std::stof(value) > 10)
            return speedhackInput->setString(mod->getSavedValue<std::string>("macro_speedhack").c_str());
    }

    if (std::string_view(speedhackInput->getString()) != "")
        mod->setSavedValue("macro_speedhack", std::string(speedhackInput->getString()));
}

void RecordLayer::toggleSetting(CCObject* obj) {
    CCMenuItemToggler* toggle = static_cast<CCMenuItemToggler*>(obj);
    std::string id = toggle->getID();
    auto& g = Global::get();

    bool value = !toggle->isToggled(); 

    if (id == "pathfinder_mode" && !Global::isPathfinderFeatureEnabled())
        value = false;

    g.mod->setSavedValue(id, value);

    // Some of these get checked every frame so idk i didnt want to do mod->getSavedValue<bool> every time
    if (id == "macro_seed_enabled") g.seedEnabled = value;
    if (id == "macro_speedhack_enabled") g.speedhackEnabled = value;
    if (id == "macro_speedhack_audio") g.speedhackAudio = value;
    if (id == "p2_input_mirror") g.p2mirror = value;
    if (id == "clickbot_enabled") g.clickbotEnabled = value;
    if (id == "clickbot_playing_only") g.clickbotOnlyPlaying = value;
    if (id == "clickbot_holding_only") g.clickbotOnlyHolding = value;
    if (id == "macro_tps_enabled") g.tpsEnabled = value;
    if (id == "autoclicker_enabled") g.autoclicker = value;
    if (id == "disable_shaders") g.disableShaders = value;
    if (id == "macro_auto_save") g.autosaveEnabled = value;
    if (id == "lock_delta") g.lockDelta = value;
    if (id == "auto_stop_playing") g.stopPlaying = value;
    if (id == "pathfinder_mode") applyPathfinderState(value, menu);

    if (id == "macro_show_trajectory") {
        g.showTrajectory = value;
        if (!value) ShowTrajectory::trajectoryOff();
    }

    if (id == "macro_coin_finder") {
        g.coinFinder = value;
        if (!value) CoinFinder::finderOff();
    }

    if (id == "macro_show_trajectory") {
        g.showTrajectory = value;
        if (!value) ShowTrajectory::trajectoryOff();
    }

    if (id == "macro_show_frame_label") {
        g.frameLabel = value;
        Interface::updateLabels();
    }

    if (id == "macro_frame_stepper") {
        g.frameStepper = value;
        Interface::updateButtons();
    }

    if (id == "clickbot_enabled" || id == "clickbot_playing_only")
        Clickbot::updateSounds();

    if (id == "macro_hide_recording_label" || id == "macro_hide_playing_label" || id == "render_hide_labels")
        Interface::updateLabels();

    if (id == "macro_hide_speedhack" || id == "macro_hide_stepper" || id == "macro_always_show_buttons")
        Interface::updateButtons();

    if (id == "render_only_song" && value) {
        CCScene* scene = CCDirector::sharedDirector()->getRunningScene();
        if (RenderSettingsLayer* layer = scene->getChildByType<RenderSettingsLayer>(0)) {
            if (!layer->recordAudioToggle) return;
            layer->recordAudioToggle->toggle(false);
            g.mod->setSavedValue("render_record_audio", false);
        }
    }

    if (id == "render_record_audio" && value) {
        CCScene* scene = CCDirector::sharedDirector()->getRunningScene();
        if (RenderSettingsLayer* layer = scene->getChildByType<RenderSettingsLayer>(0)) {
            if (!layer->onlySongToggle) return;
            layer->onlySongToggle->toggle(false);
            g.mod->setSavedValue("render_only_song", false);
        }
    }

    if (id == "menu_show_button") {
        PlayLayer* pl = PlayLayer::get();

        if (!pl) return;
        if (!pl->m_isPaused) return;

        if (PauseLayer* layer = Global::getPauseLayer()) {
            layer->onResume(nullptr);
            PlayLayer::get()->pauseGame(false);

            this->onClose(nullptr);
            RecordLayer::openMenu(true);
        }

        if (!value)
            Notification::create("geobot Button is disabled.", NotificationIcon::Warning)->show();
    }
}

void RecordLayer::showKeybindsWarning() {
    if (!mod->setSavedValue("opened_keybinds", true))
        FLAlertLayer::create(
            "Warning",
            "Scroll down to find geobot's keybinds",
            "Ok"
        )->show();
}

void RecordLayer::openKeybinds(CCObject*) {
#ifdef GEODE_IS_WINDOWS

    MoreOptionsLayer::create()->onKeybindings(nullptr);
    CCScene* scene = CCDirector::get()->getRunningScene();

    FLAlertLayer* layer = typeinfo_cast<FLAlertLayer*>(scene->getChildren()->lastObject());
    if (!layer) return showKeybindsWarning();

    CCLayer* mainLayer = layer->getChildByType<CCLayer>(0);
    if (!mainLayer) return showKeybindsWarning();

    CCNode* scrollLayer = mainLayer->getChildByID("ScrollLayer");
    if (!scrollLayer) return showKeybindsWarning();

    CCNode* contentLayer = scrollLayer->getChildByID("content-layer");
    if (!contentLayer) return showKeybindsWarning();

    CCNode* geobot = contentLayer->getChildByID("geobot");
    if (!geobot) return showKeybindsWarning();

    contentLayer->setPositionY(geobot->getPositionY() - 118);

#else

    Interface::openButtonEditor();

#endif
}

void RecordLayer::openPresets(CCObject*) {
    RenderPresetsLayer::create()->show();
}

void RecordLayer::onAutosaves(CCObject*) {
    std::filesystem::path path = Global::getFolderSettingPath("autosaves_folder");

    if (std::filesystem::exists(path))
        LoadMacroLayer::open(static_cast<geode::Popup*>(this), nullptr, true);
    else {
        FLAlertLayer::create("Error", "There was an error getting the folder. ID: 5", "Ok")->show();
    }
}

void RecordLayer::showCodecPopup(CCObject*) {
    FLAlertLayer::create("Codec", "<cr>AMD:</c> h264_amf\n<cg>NVIDIA:</c> h264_nvenc\n<cl>INTEL:</c> h264_qsv\nI don't know: libx264", "Ok")->show();
}

void RecordLayer::openMacrosFolder(CCObject*) {
    geode::createQuickPopup(
        "Macros Folder",
        "Open the current macros folder or change its path in mod settings?",
        "Open", "Change",
        [this](auto, bool btn2) {
            if (btn2)
                geode::openSettingsPopup(mod, false);
            else
                file::openFolder(Global::getFolderSettingPath("macros_folder"));
        }
    );
}

void RecordLayer::openAutosavesFolder(CCObject*) {
    file::openFolder(Global::getFolderSettingPath("autosaves_folder"));
}

void RecordLayer::openRendersFolder(CCObject*) {
    file::openFolder(Global::getFolderSettingPath("render_folder"));
}

bool RecordLayer::setup() {
    auto& g = Global::get();
    mod = g.mod;

    Utils::setBackgroundColor(m_bgSprite);
    
    cocos2d::CCPoint offset = (CCDirector::sharedDirector()->getWinSize() - m_mainLayer->getContentSize()) / 2;
    m_mainLayer->setPosition(m_mainLayer->getPosition() - offset);
    m_closeBtn->setPosition(m_closeBtn->getPosition() + offset);
    m_bgSprite->setPosition(m_bgSprite->getPosition() + offset);

    m_closeBtn->setPosition(m_closeBtn->getPosition() + ccp(-6.75, 6.75));
    m_closeBtn->getNormalImage()->setScale(0.575f);

    menu = CCMenu::create();
    m_mainLayer->addChild(menu);

    warningSprite = CCSprite::createWithSpriteFrameName("geode.loader/info-alert.png");
    warningSprite->setScale(0.675f);
    warningSprite->setPosition({ 82, 307 });
    m_mainLayer->addChild(warningSprite);

    warningLabel = CCLabelBMFont::create(("WARNING: Currently recording / playing macros with a frame offset of " + std::to_string(g.frameOffset)).c_str(), "bigFont.fnt");
    warningLabel->setAnchorPoint({ 0, 0.5 });
    warningLabel->setPosition({ 92, 307 });
    warningLabel->setScale(0.275f);
    m_mainLayer->addChild(warningLabel);

    warningSprite->setVisible(g.frameOffset != 0);
    warningLabel->setVisible(g.frameOffset != 0);

    CCSprite* spriteOn = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
    CCSprite* spriteOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");

    CCLabelBMFont* versionLabel = CCLabelBMFont::create(("geobot " + geobotVersion).c_str(), "chatFont.fnt");
    versionLabel->setOpacity(63);
    versionLabel->setPosition(ccp(-217, -125));
    versionLabel->setAnchorPoint({ 0, 0.5 });
    versionLabel->setScale(0.4f);
    versionLabel->setSkewX(4);
    menu->addChild(versionLabel);

    if (!geobotDisableBuildExpiryLock) {
        auto timerText = getBuildExpiryTimerText();
        CCLabelBMFont* expiryLabel = CCLabelBMFont::create(timerText.c_str(), "chatFont.fnt");
        expiryLabel->setOpacity(63);
        expiryLabel->setAnchorPoint({ 0, 0.5 });
        expiryLabel->setScale(0.4f);
        expiryLabel->setSkewX(4);

        float versionWidth = versionLabel->getContentSize().width * versionLabel->getScaleX();
        expiryLabel->setPosition(ccp(versionLabel->getPositionX() + versionWidth + 8.f, -125));
        menu->addChild(expiryLabel);
    }

#ifdef GEODE_IS_WINDOWS

    CCLabelBMFont* codecBtnLbl = CCLabelBMFont::create("?", "chatFont.fnt");
    codecBtnLbl->setOpacity(148);
    codecBtnLbl->setScale(0.65f);

    CCMenuItemSpriteExtra* codecBtn = CCMenuItemSpriteExtra::create(codecBtnLbl, this, menu_selector(RecordLayer::showCodecPopup));
    codecBtn->setPosition({ -26, -49 });

    menu->addChild(codecBtn);

#endif

    CCScale9Sprite* bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    bg->setScale(0.7f);
    bg->setColor({ 0,0,0 });
    bg->setOpacity(75);
    bg->setPosition(ccp(-212, 121));
    bg->setAnchorPoint({ 0, 1 });
    bg->setContentSize({ 275, 151 });
    menu->addChild(bg);

    bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    bg->setScale(0.7f);
    bg->setColor({ 0,0,0 });
    bg->setOpacity(75);
    bg->setPosition(ccp(-212, 0));
    bg->setAnchorPoint({ 0, 1 });
    bg->setContentSize({ 275, 169 });
    menu->addChild(bg);

    bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    bg->setScale(0.7f);
    bg->setColor({ 0,0,0 });
    bg->setOpacity(75);
    bg->setPosition(ccp(103, 2));
    bg->setContentSize({ 313, 339 });
    menu->addChild(bg);

    recording = CCMenuItemToggler::create(spriteOff, spriteOn, this, menu_selector(RecordLayer::toggleRecording));
    recording->toggle(g.state == state::recording);

    playing = CCMenuItemToggler::create(spriteOff, spriteOn, this, menu_selector(RecordLayer::togglePlaying));
    playing->toggle(g.state == state::playing);

    recording->setPosition(ccp(-161.5, 78));
    recording->setScale(0.775);
    playing->setPosition(ccp(-74.5, 78));
    playing->setScale(0.775);

    menu->addChild(recording);
    menu->addChild(playing);

    actionsLabel = CCLabelBMFont::create(("Actions: " + std::to_string(g.macro.inputs.size())).c_str(), "chatFont.fnt");
    actionsLabel->limitLabelWidth(57.f, 0.6f, 0.01f);
    actionsLabel->updateLabel();
    actionsLabel->setAnchorPoint({ 0, 0.5 });
    actionsLabel->setOpacity(83);
    actionsLabel->setPosition(ccp(-201, 110));
    menu->addChild(actionsLabel);

    CCLabelBMFont* pathfinderLabel = CCLabelBMFont::create(
        ("Pathfinder: " + g.pathfinderStatus).c_str(),
        "chatFont.fnt"
    );
    pathfinderLabel->limitLabelWidth(94.f, 0.5f, 0.01f);
    pathfinderLabel->updateLabel();
    pathfinderLabel->setAnchorPoint({ 0, 0.5 });
    pathfinderLabel->setOpacity(83);
    pathfinderLabel->setPosition(ccp(-201, 96));
    pathfinderLabel->setID("pathfinder-status-label"_spr);
    menu->addChild(pathfinderLabel);

    CCLabelBMFont* lbl = CCLabelBMFont::create("Macro", "goldFont.fnt");
    lbl->setPosition(ccp(-116.5, 112));
    lbl->setScale(0.575f);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("Render", "goldFont.fnt");
    lbl->setScale(0.6f);
    lbl->setPosition(ccp(-116.5, -9));
    menu->addChild(lbl);



    lbl = CCLabelBMFont::create("Left Settings", "goldFont.fnt");
    lbl->setPosition(ccp(12, 111));
    lbl->setScale(0.56f);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("Right Settings", "goldFont.fnt");
    lbl->setPosition(ccp(156, 111));
    lbl->setScale(0.56f);
    menu->addChild(lbl);

    CCScale9Sprite* settingsBg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    settingsBg->setScale(0.7f);
    settingsBg->setColor({ 0,0,0 });
    settingsBg->setOpacity(90);
    settingsBg->setPosition({ -20.f, -85.f });
    settingsBg->setAnchorPoint({ 0.f, 0.f });
    settingsBg->setContentSize({ 100.f, 181.f });
    menu->addChild(settingsBg);

    settingsBg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    settingsBg->setScale(0.7f);
    settingsBg->setColor({ 0,0,0 });
    settingsBg->setOpacity(90);
    settingsBg->setPosition({ 91.f, -85.f });
    settingsBg->setAnchorPoint({ 0.f, 0.f });
    settingsBg->setContentSize({ 190.f, 181.f });
    menu->addChild(settingsBg);

    settingsSectionLabel = CCLabelBMFont::create("", "goldFont.fnt");
    settingsSectionLabel->setPosition({ 186.f, 95.f });
    settingsSectionLabel->setScale(0.42f);
    menu->addChild(settingsSectionLabel);

    settingsScroll = geode::ScrollLayer::create(cocos2d::CCSize { 190.f, 150.f });
    settingsScroll->setPosition({ 91.f, -72.f });
    settingsScroll->setTouchEnabled(true);
    settingsScroll->enableScrollWheel(true);
    menu->addChild(settingsScroll);

    settingsScrollbar = geode::Scrollbar::create(settingsScroll);
    settingsScrollbar->setPosition({ 186.f, 0.f });
    menu->addChild(settingsScrollbar);

    settingsCategoryButtons.clear();
    std::array<char const*, 4> categoryTitles = {
        "Macro Settings",
        "Path Finder\nSettings",
        "Render Settings",
        "Other Settings"
    };
    std::array<CCPoint, 4> categoryPositions = {
        CCPoint { 30.f, 70.f },
        CCPoint { 30.f, 32.f },
        CCPoint { 30.f, -7.f },
        CCPoint { 30.f, -48.f }
    };
    std::array<CCSize, 4> categorySizes = {
        CCSize { 92.f, 31.f },
        CCSize { 92.f, 25.f },
        CCSize { 92.f, 25.f },
        CCSize { 92.f, 30.f }
    };

    for (size_t i = 0; i < categoryTitles.size(); i++) {
        auto* cardBg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        cardBg->setContentSize(categorySizes[i]);
        cardBg->setOpacity(135);
        cardBg->setColor({ 255, 255, 255 });

        auto* cardLabel = CCLabelBMFont::create(categoryTitles[i], "chatFont.fnt");
        cardLabel->setScale(i == 1 ? 0.42f : 0.5f);
        cardLabel->setPosition({
            categorySizes[i].width / 2.f,
            categorySizes[i].height / 2.f
        });
        cardBg->addChild(cardLabel);

        auto* cardBtn = CCMenuItemSpriteExtra::create(
            cardBg,
            this,
            menu_selector(RecordLayer::onSelectSettingsCategory)
        );
        cardBtn->setTag(static_cast<int>(i));
        cardBtn->setPosition(categoryPositions[i]);
        menu->addChild(cardBtn);
        settingsCategoryButtons.push_back(cardBtn);
    }

    lbl = CCLabelBMFont::create("Record", "bigFont.fnt");
    lbl->setPosition(ccp(-161.5, 60));
    lbl->setScale(0.325f);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("Play", "bigFont.fnt");
    lbl->setPosition(ccp(-74, 60));
    lbl->setScale(0.325f);
    menu->addChild(lbl);

    

    lbl = CCLabelBMFont::create("X", "chatFont.fnt");
    lbl->setPosition(ccp(-114.5, -31));
    lbl->setScale(0.7f);
    menu->addChild(lbl);



    lbl = CCLabelBMFont::create("M", "chatFont.fnt");
    lbl->setPosition(ccp(-164, -59));
    lbl->setScale(0.7f);
    menu->addChild(lbl);



    lbl = CCLabelBMFont::create("FPS", "chatFont.fnt");
    lbl->setPosition(ccp(-108.5, -59));
    lbl->setScale(0.49f);
    menu->addChild(lbl);



    ButtonSprite* btnSprite = ButtonSprite::create("Save");
    btnSprite->setScale(0.54f);

    CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(btnSprite,
        this,
        menu_selector(RecordLayer::openSaveMacro));

    btn->setPosition(ccp(-162, 34));
    menu->addChild(btn);

#ifdef GEODE_IS_WINDOWS
    btnSprite = ButtonSprite::create("Keybinds");
#else
    btnSprite = ButtonSprite::create("Buttons");
#endif
    btnSprite->setScale(0.54f);

    btn = CCMenuItemSpriteExtra::create(btnSprite,
        this,
        menu_selector(RecordLayer::openKeybinds));

    btn->setPosition(ccp(40, -100));
    menu->addChild(btn);


    btnSprite = ButtonSprite::create("Rate");
    btnSprite->setScale(0.54f);

    btn = CCMenuItemSpriteExtra::create(btnSprite,
        this,
        menu_selector(RecordLayer::openStarRateOverride));

    btn->setPosition(ccp(148, -100));
    menu->addChild(btn);

    btnSprite = ButtonSprite::create("Load");
    btnSprite->setScale(0.54f);

    btn = CCMenuItemSpriteExtra::create(btnSprite,
        this,
        menu_selector(RecordLayer::openLoadMacro));

    btn->setPosition(ccp(-106, 34));
    menu->addChild(btn);

    btnSprite = ButtonSprite::create("Edit");
    btnSprite->setScale(0.54f);

    btn = CCMenuItemSpriteExtra::create(btnSprite,
        this,
        menu_selector(RecordLayer::onEditMacro));

    btn->setPosition(ccp(-50, 34));
    menu->addChild(btn);

    widthInput = CCTextInputNode::create(150, 30, "Width", "chatFont.fnt");
    widthInput->m_textField->setAnchorPoint({ 0.5f, 0.5f });
    widthInput->ignoreAnchorPointForPosition(true);
    widthInput->setPosition(ccp(-157, -31));
    widthInput->setMaxLabelScale(0.7f);
    widthInput->setMouseEnabled(true);
    widthInput->setContentSize({ 60, 20 });
    widthInput->setTouchEnabled(true);
    widthInput->setAllowedChars("0123456789");
    widthInput->setString(mod->getSavedValue<std::string>("render_width2").c_str());
    widthInput->setDelegate(this);
    widthInput->setID("render-input");
    menu->addChild(widthInput);

    heightInput = CCTextInputNode::create(150, 30, "Height", "chatFont.fnt");
    heightInput->m_textField->setAnchorPoint({ 0.5f, 0.5f });
    heightInput->ignoreAnchorPointForPosition(true);
    heightInput->setPosition(ccp(-72.5, -31));
    heightInput->setMaxLabelScale(0.7f);
    heightInput->setMouseEnabled(true);
    heightInput->setContentSize({ 60, 20 });
    heightInput->setTouchEnabled(true);
    heightInput->setAllowedChars("0123456789");
    heightInput->setString(mod->getSavedValue<std::string>("render_height").c_str());
    heightInput->setDelegate(this);
    heightInput->setID("render-input");
    menu->addChild(heightInput);

    bitrateInput = CCTextInputNode::create(150, 30, "br", "chatFont.fnt");
    bitrateInput->m_textField->setAnchorPoint({ 0.5f, 0.5f });
    bitrateInput->ignoreAnchorPointForPosition(true);
    bitrateInput->setPosition(ccp(-185.5, -59));
    bitrateInput->setMaxLabelScale(0.7f);
    bitrateInput->setMouseEnabled(true);
    bitrateInput->setContentSize({ 32, 20 });
    bitrateInput->setTouchEnabled(true);
    bitrateInput->setAllowedChars("0123456789");
    bitrateInput->setString(mod->getSavedValue<std::string>("render_bitrate").c_str());
    bitrateInput->setDelegate(this);
    menu->addChild(bitrateInput);

    CCSprite* emptyBtn = CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
    emptyBtn->setScale(0.67f);

    CCSprite* folderIcon = CCSprite::createWithSpriteFrameName("folderIcon_001.png");
    folderIcon->setPosition(emptyBtn->getContentSize() / 2);
    folderIcon->setScale(0.7f);

    emptyBtn->addChild(folderIcon);
    btn = CCMenuItemSpriteExtra::create(
        emptyBtn,
        this,
        menu_selector(RecordLayer::openPresets)
    );
    btn->setPosition(ccp(-177.5, -97));

    menu->addChild(btn);

    CCSprite* spr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
    spr->setScale(0.65f);

    btn = CCMenuItemSpriteExtra::create(
        spr,
        this,
        menu_selector(RenderSettingsLayer::open)
    );
    btn->setPosition(ccp(-129.5, -97));
    menu->addChild(btn);


    codecInput = CCTextInputNode::create(150, 30, "Codec", "chatFont.fnt");
    codecInput->m_textField->setAnchorPoint({ 0.5f, 0.5f });
    codecInput->ignoreAnchorPointForPosition(true);
    codecInput->setPosition(ccp(-70.5, -62));
    codecInput->setMouseEnabled(true);
    codecInput->setTouchEnabled(true);
    codecInput->setContentSize({ 79, 20 });
    codecInput->setScale(0.75);
    codecInput->setString(mod->getSavedValue<std::string>("render_codec").c_str());
    codecInput->setDelegate(this);
    codecInput->setAllowedChars("0123456789abcdefghijklmnopqrstuvwxyz-_.\"\\/");
    codecInput->setMaxLabelWidth(74.f);
    menu->addChild(codecInput);

    fpsInput = CCTextInputNode::create(150, 30, "FPS", "chatFont.fnt");
    fpsInput->m_textField->setAnchorPoint({ 0.5f, 0.5f });
    fpsInput->ignoreAnchorPointForPosition(true);
    fpsInput->setPosition(ccp(-133, -59));
    fpsInput->setMaxLabelScale(0.7f);
    fpsInput->setMouseEnabled(true);
    fpsInput->setTouchEnabled(true);
    fpsInput->setContentSize({ 32, 20 });
    fpsInput->setAllowedChars("0123456789");
    fpsInput->setString(mod->getSavedValue<std::string>("render_fps").c_str());
    fpsInput->setDelegate(this);
    menu->addChild(fpsInput);

    bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    bg->setScale(0.375f);
    bg->setColor({ 0,0,0 });
    bg->setOpacity(75);
    bg->setPosition(ccp(-103.5, -21));
    bg->setAnchorPoint({ 0, 1 });
    bg->setContentSize({ 162, 55 });
    bg->setZOrder(29);
    menu->addChild(bg);

    bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    bg->setScale(0.375f);
    bg->setColor({ 0,0,0 });
    bg->setOpacity(75);
    bg->setPosition(ccp(-188, -21));
    bg->setAnchorPoint({ 0, 1 });
    bg->setContentSize({ 162, 55 });
    bg->setZOrder(29);
    menu->addChild(bg);

    bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    bg->setScale(0.375f);
    bg->setColor({ 0,0,0 });
    bg->setOpacity(75);
    bg->setPosition(ccp(-201.5, -49));
    bg->setAnchorPoint({ 0, 1 });
    bg->setContentSize({ 82, 55 });
    bg->setZOrder(29);
    menu->addChild(bg);

    bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    bg->setScale(0.375f);
    bg->setColor({ 0,0,0 });
    bg->setOpacity(75);
    bg->setPosition(ccp(-148.5, -49));
    bg->setAnchorPoint({ 0, 1 });
    bg->setContentSize({ 82, 55 });
    bg->setZOrder(29);
    menu->addChild(bg);

    bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
    bg->setScale(0.375f);
    bg->setColor({ 0,0,0 });
    bg->setOpacity(75);
    bg->setPosition(ccp(-92, -49));
    bg->setAnchorPoint({ 0, 1 });
    bg->setContentSize({ 167, 55 });
    bg->setZOrder(29);
    menu->addChild(bg);

    ButtonSprite* spriteOn2 = ButtonSprite::create("Stop");
    spriteOn2->setScale(0.74f);
    ButtonSprite* spriteOff2 = ButtonSprite::create("Start");
    spriteOff2->setScale(0.74f);

    renderToggle = CCMenuItemToggler::create(spriteOff2, spriteOn2, this, menu_selector(RecordLayer::toggleRender));
    renderToggle->toggle(g.renderer.recording || g.renderer.recordingAudio);

    renderToggle->setPosition(ccp(-65.5, -100));
    menu->addChild(renderToggle);

    spr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
    spr->setScale(0.65f);
    btn = CCMenuItemSpriteExtra::create(
        spr,
        this,
        menu_selector(RecordLayer::macroInfo)
    );
    btn->setPosition(ccp(-36, 107));
    menu->addChild(btn);

    spr = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");
    spr->setScale(0.5f);
    btn = CCMenuItemSpriteExtra::create(
        spr,
        this,
        menu_selector(RecordLayer::clear22Percentage)
    );
    btn->setPosition(ccp(-20, 107));
    menu->addChild(btn);

    if (g.currentPage < 0 || static_cast<size_t>(g.currentPage) >= kSettingsCategories.size())
        g.currentPage = 0;

    updateSettingsCategoryButtons();
    loadSettingsList();

    return true;
}

void RecordLayer::onCycleAccuracy(CCObject*) {
    auto& g = Global::get();
    std::string current = getSavedAccuracyMode(mod);
    size_t index = 0;
    for (size_t i = 0; i < kAccuracyModes.size(); i++) {
        if (kAccuracyModes[i] == current) {
            index = i;
            break;
        }
    }

    index = (index + 1) % kAccuracyModes.size();
    std::string next = kAccuracyModes[index];
    mod->setSavedValue("macro_accuracy", next);
    applyAccuracyMode(next);

    if (settingsMenu)
        loadSettingsList();
}

void RecordLayer::onCycleFramePerfectMode(CCObject*) {
    std::string current = getSavedFramePerfectOverlayMode(mod);
    auto const& modes = getFramePerfectOverlayModes();
    size_t index = 0;
    for (size_t i = 0; i < modes.size(); i++) {
        if (modes[i] == current) {
            index = i;
            break;
        }
    }

    index = (index + 1) % modes.size();
    mod->setSavedValue("frame_perfect_overlay_mode", modes[index]);

    if (settingsMenu)
        loadSettingsList();
}

void RecordLayer::onSelectSettingsCategory(CCObject* sender) {
    auto* node = static_cast<CCNode*>(sender);
    if (!node)
        return;

    selectSettingsCategory(static_cast<size_t>(std::max(0, node->getTag())));
}

void RecordLayer::updateSettingsCategoryButtons() {
    for (size_t i = 0; i < settingsCategoryButtons.size(); i++) {
        auto* button = settingsCategoryButtons[i];
        if (!button)
            continue;

        bool selected = static_cast<size_t>(Global::get().currentPage) == i;
        button->setScale(selected ? 1.04f : 1.f);

        if (auto* bg = typeinfo_cast<CCScale9Sprite*>(button->getNormalImage())) {
            bg->setOpacity(selected ? 185 : 135);
            bg->setColor(selected ? ccColor3B { 230, 213, 147 } : ccColor3B { 255, 255, 255 });
        }
    }
}

void RecordLayer::selectSettingsCategory(size_t index) {
    auto& g = Global::get();
    if (index >= kSettingsCategories.size())
        index = 0;

    g.currentPage = static_cast<int>(index);
    if (mod)
        mod->setSavedValue("current_page", std::to_string(g.currentPage));

    updateSettingsCategoryButtons();
    loadSettingsList();
}

void RecordLayer::applyPathfinderState(bool enabled, CCMenu* rootMenu) {
    auto& g = Global::get();
    if (!Global::isPathfinderFeatureEnabled())
        enabled = false;
    if (g.mod)
        g.mod->setSavedValue("pathfinder_mode", enabled);

    g.pathfinderMode = enabled;
    g.pathfinderSearching = false;
    g.pathfinderStatus = !Global::isPathfinderFeatureEnabled() ? "Disabled" : (enabled ? "Armed" : "Idle");

    CCNode* searchRoot = rootMenu
        ? static_cast<CCNode*>(rootMenu)
        : static_cast<CCNode*>(CCDirector::get()->getRunningScene());

    if (searchRoot) {
        if (CCMenuItemToggler* toggle = typeinfo_cast<CCMenuItemToggler*>(findNodeByIDRecursive(searchRoot, "pathfinder_mode")))
            toggle->toggle(enabled);

        if (CCLabelBMFont* statusLabel = typeinfo_cast<CCLabelBMFont*>(findNodeByIDRecursive(searchRoot, "pathfinder-status-label"_spr))) {
            statusLabel->setString(("Pathfinder: " + g.pathfinderStatus).c_str());
            statusLabel->limitLabelWidth(94.f, 0.5f, 0.01f);
            statusLabel->updateLabel();
        }
    }

    Interface::updateLabels();
}

void RecordLayer::setToggleMember(CCMenuItemToggler* toggle, std::string id) {
    if (id == "macro_speedhack_enabled") speedhackToggle = toggle;
    if (id == "macro_show_trajectory") trajectoryToggle = toggle;
    if (id == "macro_noclip") noclipToggle = toggle;
    if (id == "macro_frame_stepper") frameStepperToggle = toggle;
    if (id == "macro_tps_enabled") tpsToggle = toggle;
}

void RecordLayer::loadSetting(RecordSetting sett, float yPos, CCMenu* targetMenu) {
    if (sett.id == "pathfinder_mode" && !Global::isPathfinderFeatureEnabled())
        sett.disabled = true;

    float targetWidth = targetMenu ? targetMenu->getContentSize().width : 190.f;
    float labelX = 10.f;
    float toggleX = targetWidth - 14.f;
    float optionButtonX = targetWidth - 46.f;
    float actionButtonX = targetWidth - 28.f;
    float compactInputAnchorX = targetWidth - 76.f;
    float compactInputCenterX = targetWidth - 58.f;
    float seedInputAnchorX = targetWidth - 128.f;
    float seedInputCenterX = targetWidth - 82.f;
    float cycleButtonX = targetWidth - 56.f;

    CCLabelBMFont* lbl = CCLabelBMFont::create(sett.name.c_str(), "bigFont.fnt");
    lbl->setPosition(ccp(labelX, yPos));
    lbl->setAnchorPoint({ 0, 0.5 });
    lbl->setOpacity(200);
    lbl->setScale(sett.labelScale);
    lbl->limitLabelWidth(targetWidth - 92.f, sett.labelScale, 0.1f);
    lbl->updateLabel();

    nodes.push_back(static_cast<CCNode*>(lbl));
    targetMenu->addChild(lbl);

    CCSprite* spriteOn = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
    CCSprite* spriteOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
    float toggleScale = 0.555f;

    if (sett.disabled) {
        lbl->setOpacity(110);
    }

    if (sett.input != InputType::Action) {
        CCMenuItemToggler* toggle = CCMenuItemToggler::create(spriteOff, spriteOn, this, menu_selector(RecordLayer::toggleSetting));
        toggle->setPosition(ccp(toggleX, yPos));
        toggle->setScale(toggleScale);
        bool toggled = mod->getSavedValue<bool>(sett.id);
        if (sett.id == "pathfinder_mode" && !Global::isPathfinderFeatureEnabled())
            toggled = false;
        toggle->toggle(toggled);
        toggle->setID(sett.id.c_str());
        toggle->setEnabled(!sett.disabled);
        toggle->setOpacity(sett.disabled ? 110 : 255);

        nodes.push_back(static_cast<CCNode*>(toggle));
        targetMenu->addChild(toggle);

        setToggleMember(toggle, sett.id);
    }

    if (sett.input == InputType::None) return;

    if (sett.input == InputType::Action) {
        CCSprite* emptyBtn = CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
        emptyBtn->setScale(0.469f);

        CCSprite* folderIcon = CCSprite::createWithSpriteFrameName("folderIcon_001.png");
        folderIcon->setPosition(emptyBtn->getContentSize() / 2);
        folderIcon->setScale(0.7f);
        emptyBtn->addChild(folderIcon);

        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(
            emptyBtn,
            this,
            sett.callback
        );
        btn->setPosition(ccp(actionButtonX, yPos));

        nodes.push_back(static_cast<CCNode*>(btn));
        targetMenu->addChild(btn);
        return;
    }

    if (sett.input == InputType::Settings) {
        CCSprite* spr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        spr->setScale(0.41f);
        spr->setOpacity(215);

        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(
            spr,
            this,
            sett.callback
        );
        btn->setPosition(ccp(optionButtonX, yPos));
        btn->setEnabled(!sett.disabled);
        btn->setOpacity(sett.disabled ? 110 : 255);

        nodes.push_back(static_cast<CCNode*>(btn));
        targetMenu->addChild(btn);
    }

    if (sett.input == InputType::Autosave) {
        CCSprite* emptyBtn = CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
        emptyBtn->setScale(0.469f);

        CCSprite* folderIcon = CCSprite::createWithSpriteFrameName("folderIcon_001.png");
        folderIcon->setPosition(emptyBtn->getContentSize() / 2);
        folderIcon->setScale(0.7f);
        emptyBtn->addChild(folderIcon);

        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(
            emptyBtn,
            this,
            menu_selector(RecordLayer::onAutosaves)
        );
        btn->setPosition(ccp(actionButtonX, yPos));

        nodes.push_back(static_cast<CCNode*>(btn));
        targetMenu->addChild(btn);
    }

    if (sett.input == InputType::Speedhack) {
        CCScale9Sprite* bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        bg->setPosition(ccp(compactInputAnchorX, yPos + 10));
        bg->setScale(0.355f);
        bg->setColor({ 0,0,0 });
        bg->setOpacity(75);
        bg->setAnchorPoint({ 0, 1 });
        bg->setContentSize({ 88, 55 });
        bg->setZOrder(29);
        nodes.push_back(static_cast<CCNode*>(bg));
        targetMenu->addChild(bg);

        speedhackInput = CCTextInputNode::create(150, 30, "SH", "chatFont.fnt");
        speedhackInput->setPosition(ccp(compactInputCenterX, yPos));
        speedhackInput->m_textField->setAnchorPoint({ 0.5f, 0.5f });
        speedhackInput->ignoreAnchorPointForPosition(true);
        speedhackInput->setMaxLabelScale(0.7f);
        speedhackInput->setMouseEnabled(true);
        speedhackInput->setTouchEnabled(true);
        speedhackInput->setContentSize({ 32, 20 });
        speedhackInput->setAllowedChars("0123456789.");
        speedhackInput->setString(mod->getSavedValue<std::string>("macro_speedhack").c_str());
        speedhackInput->setMaxLabelWidth(30.f);
        speedhackInput->setDelegate(this);
        speedhackInput->setMaxLabelLength(6);

        nodes.push_back(static_cast<CCNode*>(speedhackInput));
        targetMenu->addChild(speedhackInput);
    }

    if (sett.input == InputType::Tps) {
        tpsBg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        tpsBg->setPosition(ccp(compactInputAnchorX, yPos + 10));
        tpsBg->setScale(0.355f);
        tpsBg->setColor({ 0,0,0 });
        tpsBg->setOpacity(75);
        tpsBg->setAnchorPoint({ 0, 1 });
        tpsBg->setContentSize({ 88, 55 });
        tpsBg->setZOrder(29);
        nodes.push_back(static_cast<CCNode*>(tpsBg));
        targetMenu->addChild(tpsBg);

        tpsInput = CCTextInputNode::create(150, 30, "tps", "chatFont.fnt");
        tpsInput->setPosition(ccp(compactInputCenterX, yPos));
        tpsInput->m_textField->setAnchorPoint({ 0.5f, 0.5f });
        tpsInput->ignoreAnchorPointForPosition(true);
        tpsInput->setMaxLabelScale(0.7f);
        tpsInput->setMouseEnabled(true);
        tpsInput->setTouchEnabled(true);
        tpsInput->setContentSize({ 32, 20 });
        tpsInput->setAllowedChars("0123456789.");
        tpsInput->setString(Utils::getSimplifiedString(fmt::format("{:.3f}", Mod::get()->getSavedValue<double>("macro_tps"))).c_str());
        tpsInput->setMaxLabelWidth(30.f);
        tpsInput->setDelegate(this);
        tpsInput->setMaxLabelLength(9);

        nodes.push_back(static_cast<CCNode*>(tpsInput));
        targetMenu->addChild(tpsInput);
    }

    if (sett.input == InputType::Seed) {
        CCScale9Sprite* bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        bg->setPosition(ccp(seedInputAnchorX, yPos + 10));
        bg->setScale(0.355f);
        bg->setColor({ 0,0,0 });
        bg->setOpacity(75);
        bg->setAnchorPoint({ 0, 1 });
        bg->setContentSize({ 170, 55 });
        bg->setZOrder(29);
        nodes.push_back(static_cast<CCNode*>(bg));
        targetMenu->addChild(bg);

        seedInput = CCTextInputNode::create(150, 30, "Seed", "chatFont.fnt");
        seedInput->setPosition(ccp(seedInputCenterX, yPos));
        seedInput->m_textField->setAnchorPoint({ 0.5f, 0.5f });
        seedInput->ignoreAnchorPointForPosition(true);
        seedInput->setMaxLabelScale(0.7f);
        seedInput->setMouseEnabled(true);
        seedInput->setTouchEnabled(true);
        seedInput->setContentSize({ 85, 20 });
        seedInput->setAllowedChars("0123456789");
        seedInput->setString(mod->getSavedValue<std::string>("macro_seed").c_str());
        seedInput->setMaxLabelWidth(70.f);
        seedInput->setDelegate(this);
        seedInput->setMaxLabelLength(20);

        nodes.push_back(static_cast<CCNode*>(seedInput));
        targetMenu->addChild(seedInput);
    }

    if (sett.input == InputType::Respawn) {
        CCScale9Sprite* bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        bg->setPosition(ccp(compactInputAnchorX, yPos + 10));
        bg->setScale(0.355f);
        bg->setColor({ 0,0,0 });
        bg->setOpacity(75);
        bg->setAnchorPoint({ 0, 1 });
        bg->setContentSize({ 88, 55 });
        bg->setZOrder(29);
        nodes.push_back(static_cast<CCNode*>(bg));
        targetMenu->addChild(bg);

        respawnInput = CCTextInputNode::create(150, 30, "sec", "chatFont.fnt");
        respawnInput->setPosition(ccp(compactInputCenterX, yPos));
        respawnInput->m_textField->setAnchorPoint({ 0.5f, 0.5f });
        respawnInput->ignoreAnchorPointForPosition(true);
        respawnInput->setMaxLabelScale(0.7f);
        respawnInput->setMouseEnabled(true);
        respawnInput->setTouchEnabled(true);
        respawnInput->setContentSize({ 32.f, 20.f });
        respawnInput->setAllowedChars("0123456789.");
        respawnInput->setString(fmt::format("{:.2}", mod->getSavedValue<double>("respawn_time")).c_str());
        respawnInput->setMaxLabelWidth(30.f);
        respawnInput->setDelegate(this);
        respawnInput->setMaxLabelLength(4);

        nodes.push_back(static_cast<CCNode*>(respawnInput));
        targetMenu->addChild(respawnInput);
    }

    if (sett.input == InputType::FrameOffset) {
        CCScale9Sprite* bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        bg->setPosition(ccp(compactInputAnchorX, yPos + 10));
        bg->setScale(0.355f);
        bg->setColor({ 0,0,0 });
        bg->setOpacity(75);
        bg->setAnchorPoint({ 0, 1 });
        bg->setContentSize({ 88, 55 });
        bg->setZOrder(29);
        nodes.push_back(static_cast<CCNode*>(bg));
        targetMenu->addChild(bg);

        frameOffsetInput = CCTextInputNode::create(150, 30, "offset", "chatFont.fnt");
        frameOffsetInput->setPosition(ccp(compactInputCenterX, yPos));
        frameOffsetInput->m_textField->setAnchorPoint({ 0.5f, 0.5f });
        frameOffsetInput->ignoreAnchorPointForPosition(true);
        frameOffsetInput->setMaxLabelScale(0.7f);
        frameOffsetInput->setMouseEnabled(true);
        frameOffsetInput->setTouchEnabled(true);
        frameOffsetInput->setContentSize({ 32, 20 });
        frameOffsetInput->setAllowedChars("0123456789-");
        frameOffsetInput->setString(std::to_string(Global::get().frameOffset).c_str());
        frameOffsetInput->setMaxLabelWidth(30.f);
        frameOffsetInput->setDelegate(this);
        frameOffsetInput->setMaxLabelLength(4);

        nodes.push_back(static_cast<CCNode*>(frameOffsetInput));
        targetMenu->addChild(frameOffsetInput);
    }

    if (sett.input == InputType::FrameFixesLimit) {
        CCScale9Sprite* bg = CCScale9Sprite::create("square02b_001.png", { 0, 0, 80, 80 });
        bg->setPosition(ccp(compactInputAnchorX, yPos + 10));
        bg->setScale(0.355f);
        bg->setColor({ 0,0,0 });
        bg->setOpacity(75);
        bg->setAnchorPoint({ 0, 1 });
        bg->setContentSize({ 88, 55 });
        bg->setZOrder(29);
        nodes.push_back(static_cast<CCNode*>(bg));
        targetMenu->addChild(bg);

        frameFixesLimitInput = CCTextInputNode::create(150, 30, "fps", "chatFont.fnt");
        frameFixesLimitInput->setPosition(ccp(compactInputCenterX, yPos));
        frameFixesLimitInput->m_textField->setAnchorPoint({ 0.5f, 0.5f });
        frameFixesLimitInput->ignoreAnchorPointForPosition(true);
        frameFixesLimitInput->setMaxLabelScale(0.7f);
        frameFixesLimitInput->setMouseEnabled(true);
        frameFixesLimitInput->setTouchEnabled(true);
        frameFixesLimitInput->setContentSize({ 32, 20 });
        frameFixesLimitInput->setAllowedChars("0123456789");
        frameFixesLimitInput->setString(std::to_string(Global::get().frameFixesLimit).c_str());
        frameFixesLimitInput->setMaxLabelWidth(30.f);
        frameFixesLimitInput->setDelegate(this);
        frameFixesLimitInput->setMaxLabelLength(6);

        nodes.push_back(static_cast<CCNode*>(frameFixesLimitInput));
        targetMenu->addChild(frameFixesLimitInput);
    }

    if (sett.input == InputType::Accuracy) {
        ButtonSprite* btnSpr = ButtonSprite::create(getSavedAccuracyMode(mod).c_str());
        btnSpr->setScale(0.4f);
        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(
            btnSpr,
            this,
            menu_selector(RecordLayer::onCycleAccuracy)
        );
        btn->setPosition(ccp(cycleButtonX, yPos));

        nodes.push_back(static_cast<CCNode*>(btn));
        targetMenu->addChild(btn);
    }

    if (sett.input == InputType::FramePerfectMode) {
        ButtonSprite* btnSpr = ButtonSprite::create(getSavedFramePerfectOverlayMode(mod).c_str());
        btnSpr->setScale(0.4f);
        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(
            btnSpr,
            this,
            menu_selector(RecordLayer::onCycleFramePerfectMode)
        );
        btn->setPosition(ccp(cycleButtonX, yPos));

        nodes.push_back(static_cast<CCNode*>(btn));
        targetMenu->addChild(btn);
    }
}

void RecordLayer::loadSettingsList() {
    checkSpeedhack();

    auto& g = Global::get();
    nodes.clear();

    speedhackToggle = nullptr;
    frameStepperToggle = nullptr;
    trajectoryToggle = nullptr;
    noclipToggle = nullptr;
    tpsToggle = nullptr;

    speedhackInput = nullptr;
    respawnInput = nullptr;
    seedInput = nullptr;
    tpsInput = nullptr;
    frameOffsetInput = nullptr;
    frameFixesLimitInput = nullptr;

    tpsBg = nullptr;

    if (!settingsScroll) return;

    if (settingsMenu) {
        settingsMenu->removeFromParentAndCleanup(true);
        settingsMenu = nullptr;
    }

    constexpr float rowSpacing = 29.f;
    constexpr float topPadding = 16.f;
    constexpr float bottomPadding = 12.f;
    size_t categoryIndex = static_cast<size_t>(std::clamp(g.currentPage, 0, static_cast<int>(kSettingsCategories.size()) - 1));
    auto const& category = kSettingsCategories[categoryIndex];
    size_t settingCount = category.settings.size();

    float viewWidth = settingsScroll->getContentSize().width;
    float viewHeight = settingsScroll->getContentSize().height;
    float contentHeight = std::max(viewHeight, topPadding + bottomPadding + settingCount * rowSpacing);

    if (settingsSectionLabel) {
        settingsSectionLabel->setString(category.title.c_str());
        settingsSectionLabel->limitLabelWidth(120.f, 0.42f, 0.1f);
        settingsSectionLabel->updateLabel();
    }

    settingsScroll->m_contentLayer->setAnchorPoint({ 0.f, 0.f });
    settingsScroll->m_contentLayer->setPosition({ 0.f, 0.f });
    settingsScroll->m_contentLayer->setContentSize({ viewWidth, contentHeight });

    settingsMenu = CCMenu::create();
    settingsMenu->setPosition({ 0.f, 0.f });
    settingsMenu->setAnchorPoint({ 0.f, 0.f });
    settingsMenu->setContentSize({ viewWidth, contentHeight });
    // Keep settings controls above the root menu in touch routing so
    // toggles/inputs inside the scroll area are reliably clickable.
    settingsMenu->setTouchPriority(menu ? menu->getTouchPriority() - 1 : -129);
    settingsScroll->m_contentLayer->addChild(settingsMenu);

    for (size_t i = 0; i < category.settings.size(); i++) {
        float yPos = contentHeight - topPadding - (static_cast<float>(i) * rowSpacing);
        loadSetting(category.settings[i], yPos, settingsMenu);
    }

    settingsScroll->scrollToTop();
    updateTPS();
}

void RecordLayer::onDiscord(CCObject*) {
    geode::createQuickPopup(
        "Discord",
        "Join the <cb>Discord</c> server?\n(<cl>discord.gg/w6yvdzVzBd</c>).",
        "No", "Yes",
        [](auto, bool btn2) {
        	if (btn2)
				geode::utils::web::openLinkInBrowser("https://discord.gg/w6yvdzVzBd");
        }
    );
}

void RecordLayer::updateTPS() {
    if (!tpsInput || !tpsToggle || !tpsBg) return;
    auto& g = Global::get();

    tpsToggle->toggle(g.tpsEnabled);
    tpsInput->setString(Utils::getSimplifiedString(fmt::format("{:.3f}", Mod::get()->getSavedValue<double>("macro_tps"))).c_str());

    if (g.state == state::none || g.macro.inputs.empty()) {
        if (CCMenuItemSpriteExtra* btn = tpsToggle->getChildByType<CCMenuItemSpriteExtra>(0))
            if (CCSprite* spr = btn->getChildByType<CCSprite>(0))
                spr->setOpacity(255);
        if (CCMenuItemSpriteExtra* btn = tpsToggle->getChildByType<CCMenuItemSpriteExtra>(1))
            if (CCSprite* spr = btn->getChildByType<CCSprite>(0))
                spr->setOpacity(255);

        tpsInput->setID("");
        tpsBg->setOpacity(75);
        tpsToggle->setEnabled(true);

        tpsInput->detachWithIME();
        tpsInput->onClickTrackNode(false);
        tpsInput->m_cursor->setVisible(false);
    } else {
        if (CCMenuItemSpriteExtra* btn = tpsToggle->getChildByType<CCMenuItemSpriteExtra>(0))
            if (CCSprite* spr = btn->getChildByType<CCSprite>(0))
                spr->setOpacity(120);
        if (CCMenuItemSpriteExtra* btn = tpsToggle->getChildByType<CCMenuItemSpriteExtra>(1))
            if (CCSprite* spr = btn->getChildByType<CCSprite>(0))
                spr->setOpacity(120);

        tpsInput->setID("disabled-input"_spr);
        tpsBg->setOpacity(30);
        tpsToggle->setEnabled(false);

        tpsInput->detachWithIME();
        tpsInput->onClickTrackNode(false);
        tpsInput->m_cursor->setVisible(false);
    }
}
