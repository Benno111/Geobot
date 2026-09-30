#include "record_layer.hpp"
#include "macro_editor.hpp"
#include "game_ui.hpp"
#include "clickbot_layer.hpp"

#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
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
        "Macro",
        {
            { "Accuracy:", "macro_accuracy", InputType::Accuracy, 0.4f },
            { "Frame Offset:", "frame_offset", InputType::FrameOffset, 0.4f },
            { "Frame Fix Limit:", "frame_fixes_limit", InputType::FrameFixesLimit, 0.4f },
            { "Lock Delta:", "lock_delta", InputType::None },
            { "Auto Stop Playing:", "auto_stop_playing", InputType::None },
            { "TPS Bypass:", "macro_tps_enabled", InputType::Tps, 0.4f },
            { "Enable Clickbot:", "clickbot_enabled", InputType::Settings, 0.325f, menu_selector(ClickbotLayer::open)},
            { "Ignore inputs:", "macro_ignore_inputs", InputType::None },
            { "Macros Folder:", "macros_folder_btn", InputType::Action, 0.325f, menu_selector(RecordLayer::openMacrosFolder) },
            { "Hide playing label:", "macro_hide_playing_label", InputType::None },
            { "Hide recording label:", "macro_hide_recording_label", InputType::None },
            { "Autosaves Folder:", "autosaves_folder_btn", InputType::Action, 0.325f, menu_selector(RecordLayer::openAutosavesFolder) }
        }
    }
};

namespace {
bool isMacroMenuRewriteEnabled() {
    return false;
}

bool shouldShowAutoSafeModeNotice() {
    return Global::get().botUsedInLevelSession;
}

void addRewardDisabledWatermark(CCLayer* layer) {
    if (!layer || !shouldShowAutoSafeModeNotice())
        return;

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto* label = CCLabelBMFont::create("geobot active - rewards disabled", "chatFont.fnt");
    label->setID("geobot-rewards-disabled-label"_spr);
    label->setAnchorPoint({ 0.5f, 0.5f });
    label->setPosition({ winSize.width / 2.f, 18.f });
    label->setScale(0.55f);
    label->setOpacity(180);
    label->setZOrder(200);
    layer->addChild(label);
}

std::string getSettingsCategoryButtonTitle(std::string const& title) {
    return title;
}

CCScale9Sprite* createSettingsChoiceSprite(std::string const& text) {
    const CCSize buttonSize { 82.f, 28.f };

    auto* bg = CCScale9Sprite::create("GJ_button_01.png", { 0, 0, 40, 40 });
    bg->setContentSize(buttonSize);

    auto* label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    label->setPosition(buttonSize / 2.f);
    label->setScale(0.34f);
    label->limitLabelWidth(buttonSize.width - 10.f, label->getScale(), 0.16f);
    label->updateLabel();
    bg->addChild(label);

    return bg;
}

std::vector<RecordSetting> getLegacySettingsList() {
    std::vector<RecordSetting> settings;
    for (auto const& category : kSettingsCategories) {
        settings.insert(settings.end(), category.settings.begin(), category.settings.end());
    }
    return settings;
}

const std::vector<std::string> kAccuracyModes = {
    "Vanilla",
    "Input Fixes",
    "Frame Fixes"
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

void stopMacroOnEndscreen() {
    auto& g = Global::get();
    if (g.state != state::playing && g.state != state::recording)
        return;

    Macro::resetState(true);
    Macro::updateTPS();

    if (auto* recordLayer = typeinfo_cast<RecordLayer*>(g.layer)) {
        if (recordLayer->recording)
            recordLayer->recording->toggle(false);
        if (recordLayer->playing)
            recordLayer->playing->toggle(false);
        recordLayer->updateTPS();
    }
}

void addAutoSafeModeEndscreenLabel(EndLevelLayer* layer) {
    if (!layer || !shouldShowAutoSafeModeNotice())
        return;

    auto* label = CCLabelBMFont::create("Auto-safe-mode", "goldFont.fnt");
    label->setPosition({3.5f, 10.f});
    label->setOpacity(155);
    label->setID("safe-mode-label"_spr);
    label->setScale(0.55f);
    label->setAnchorPoint({0.f, 0.5f});
    layer->addChild(label);
}
}

class $modify(PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        addgeobotPauseButton(this);
        addRewardDisabledWatermark(this);
    }
};

class $modify(EditorPauseLayer) {
    void customSetup() {
        EditorPauseLayer::customSetup();
        addgeobotPauseButton(this);
    }
};

class $modify(EndLevelLayer) {
    void customSetup() {
        EndLevelLayer::customSetup();
        stopMacroOnEndscreen();
        addgeobotPauseButton(this);
        addAutoSafeModeEndscreenLabel(this);
    }

    void onHideLayer(CCObject* obj) {
        EndLevelLayer::onHideLayer(obj);

        if (CCNode* label = getChildByID("safe-mode-label"_spr))
            label->setVisible(!label->isVisible());
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
    layer->m_noElasticity = instant;
    layer->show();

    g.layer = static_cast<geode::Popup*>(layer);

    return layer;
}



void RecordLayer::onClose(CCObject*) {
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
        g.botUsedInLevelSession = PlayLayer::get() != nullptr;
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
    g.autosaveCheck = 0.f;
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
        g.botUsedInLevelSession = PlayLayer::get() != nullptr;
        Macro::preparePlayback();

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

void RecordLayer::onEditMacro(CCObject*) {
    MacroEditLayer::open();
}

void RecordLayer::macroInfo(CCObject*) {
    MacroInfoLayer::create()->show();
}

void RecordLayer::textChanged(CCTextInputNode* node) {
    if (!node) return;

    mod = Mod::get();







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


}

void RecordLayer::toggleSetting(CCObject* obj) {
    auto* toggle = static_cast<CCMenuItemToggler*>(obj);
    std::string id = toggle->getID();
    bool value = !toggle->isToggled();
    auto& g = Global::get();
    g.mod->setSavedValue(id, value);

    if (id == "clickbot_enabled") {
        g.clickbotEnabled = value;
        Clickbot::updateSounds();
    }
    else if (id == "macro_tps_enabled") g.tpsEnabled = value;
    else if (id == "lock_delta") g.lockDelta = value;
    else if (id == "auto_stop_playing") g.stopPlaying = value;
    else if (id == "macro_hide_recording_label" || id == "macro_hide_playing_label")
        Interface::updateLabels();
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

    Notification::create("Configure controls in Geometry Dash settings", NotificationIcon::Info)->show();

#endif
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

RecordLayer* RecordLayer::create() {
    auto* ret = new RecordLayer();
    if (ret->initAnchored(455.f, 271.f)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

bool RecordLayer::setup() {
    auto& g = Global::get();
    mod = g.mod;

    Utils::setBackgroundColor(m_bgSprite);
    
    adjustForLoadingScreen(false);
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

    CCScale9Sprite* bg = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
    bg->setScale(0.7f);
    bg->setColor({ 0,0,0 });
    bg->setOpacity(75);
    bg->setPosition(ccp(-212, 121));
    bg->setAnchorPoint({ 0, 1 });
    bg->setContentSize({ 275, 151 });
    menu->addChild(bg);

    bg = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
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


    CCLabelBMFont* lbl = CCLabelBMFont::create("Macro", "goldFont.fnt");
    lbl->setPosition(ccp(-116.5, 112));
    lbl->setScale(0.575f);
    menu->addChild(lbl);

    bool macroMenuRewrite = isMacroMenuRewriteEnabled();

    if (macroMenuRewrite) {
        lbl = CCLabelBMFont::create("Settings", "goldFont.fnt");
        lbl->setPosition(ccp(159.f, 111.f));
        lbl->setScale(0.56f);
        menu->addChild(lbl);
    }
    else {
        lbl = CCLabelBMFont::create("Settings", "goldFont.fnt");
        lbl->setPosition(ccp(130, 111));
        lbl->setScale(0.56f);
        menu->addChild(lbl);
    }

    if (macroMenuRewrite) {
        CCScale9Sprite* settingsBg = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
        settingsBg->setScale(0.7f);
        settingsBg->setColor({ 0,0,0 });
        settingsBg->setOpacity(90);
        settingsBg->setPosition({ -20.f, -85.f });
        settingsBg->setAnchorPoint({ 0.f, 0.f });
        settingsBg->setContentSize({ 52.f, 181.f });
        menu->addChild(settingsBg);

        settingsBg = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
        settingsBg->setScale(0.7f);
        settingsBg->setColor({ 0,0,0 });
        settingsBg->setOpacity(90);
        settingsBg->setPosition({ 38.f, -85.f });
        settingsBg->setAnchorPoint({ 0.f, 0.f });
        settingsBg->setContentSize({ 243.f, 181.f });
        menu->addChild(settingsBg);
    }
    else {
        CCScale9Sprite* settingsBg = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
        settingsBg->setScale(0.7f);
        settingsBg->setColor({ 0,0,0 });
        settingsBg->setOpacity(90);
        settingsBg->setPosition({ -20.f, -85.f });
        settingsBg->setAnchorPoint({ 0.f, 0.f });
        settingsBg->setContentSize({ 301.f, 181.f });
        menu->addChild(settingsBg);
    }

    settingsSectionLabel = CCLabelBMFont::create("", "goldFont.fnt");
    settingsSectionLabel->setPosition(macroMenuRewrite ? CCPoint { 159.5f, 95.f } : CCPoint { 130.f, 95.f });
    settingsSectionLabel->setScale(0.42f);
    menu->addChild(settingsSectionLabel);

    settingsScroll = geode::ScrollLayer::create(macroMenuRewrite ? cocos2d::CCSize { 243.f, 150.f } : cocos2d::CCSize { 301.f, 150.f });
    settingsScroll->setPosition(macroMenuRewrite ? CCPoint { 38.f, -72.f } : CCPoint { -20.f, -72.f });
    settingsScroll->setTouchEnabled(true);
    settingsScroll->enableScrollWheel(true);
    menu->addChild(settingsScroll);

    settingsScrollbar = geode::Scrollbar::create(settingsScroll);
    settingsScrollbar->setPosition(macroMenuRewrite ? CCPoint { 274.f, 0.f } : CCPoint { 274.f, 0.f });
    menu->addChild(settingsScrollbar);

    settingsCategoryButtons.clear();
    if (macroMenuRewrite) {
        constexpr float categoryTopY = 56.f;
        constexpr float categoryBottomY = 12.f;
        constexpr float categoryWidth = 44.f;
        constexpr float categoryHeight = 38.f;
        size_t categoryCount = kSettingsCategories.size();
        settingsCategoryButtons.reserve(categoryCount);

        for (size_t i = 0; i < categoryCount; i++) {
            std::string categoryTitle = getSettingsCategoryButtonTitle(kSettingsCategories[i].title);
            bool multiline = categoryTitle.find('\n') != std::string::npos;
            float yPos = categoryCount <= 1
                ? categoryTopY
                : categoryTopY - ((categoryTopY - categoryBottomY) * static_cast<float>(i) / static_cast<float>(categoryCount - 1));

            auto* cardBg = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
            cardBg->setContentSize({ categoryWidth, categoryHeight });
            cardBg->setOpacity(135);
            cardBg->setColor({ 255, 255, 255 });

            auto* cardLabel = CCLabelBMFont::create(categoryTitle.c_str(), "chatFont.fnt");
            cardLabel->setScale(multiline ? 0.34f : 0.38f);
            cardLabel->setPosition({
                categoryWidth / 2.f,
                categoryHeight / 2.f
            });
            cardLabel->limitLabelWidth(categoryWidth - 6.f, cardLabel->getScale(), 0.16f);
            cardLabel->updateLabel();
            cardLabel->setID("category-label"_spr);
            cardBg->addChild(cardLabel);

            auto* cardBtn = CCMenuItemSpriteExtra::create(
                cardBg,
                this,
                menu_selector(RecordLayer::onSelectSettingsCategory)
            );
            cardBtn->setTag(static_cast<int>(i));
            cardBtn->setID(fmt::format("settings-category-{}", i).c_str());
            cardBtn->setPosition({ 6.f, yPos });
            menu->addChild(cardBtn);
            settingsCategoryButtons.push_back(cardBtn);
        }
    }

    lbl = CCLabelBMFont::create("Record", "bigFont.fnt");
    lbl->setPosition(ccp(-161.5, 60));
    lbl->setScale(0.325f);
    menu->addChild(lbl);

    lbl = CCLabelBMFont::create("Play", "bigFont.fnt");
    lbl->setPosition(ccp(-74, 60));
    lbl->setScale(0.325f);
    menu->addChild(lbl);

    

    CCMenuItemSpriteExtra* btn = nullptr;
    CCSprite* spr = nullptr;

    ButtonSprite* btnSprite = ButtonSprite::create("Save");
    btnSprite->setScale(0.54f);
    btn = CCMenuItemSpriteExtra::create(btnSprite, this, menu_selector(RecordLayer::openSaveMacro));
    btn->setPosition(ccp(-162, 34));
    menu->addChild(btn);

    btnSprite = ButtonSprite::create("Load");
    btnSprite->setScale(0.54f);
    btn = CCMenuItemSpriteExtra::create(btnSprite, this, menu_selector(RecordLayer::openLoadMacro));
    btn->setPosition(ccp(-106, 34));
    menu->addChild(btn);

    btnSprite = ButtonSprite::create("Edit");
    btnSprite->setScale(0.54f);
    btn = CCMenuItemSpriteExtra::create(btnSprite, this, menu_selector(RecordLayer::onEditMacro));
    btn->setPosition(ccp(-50, 34));
    menu->addChild(btn);

#ifdef GEODE_IS_WINDOWS
    btnSprite = ButtonSprite::create("Keybinds");
#else
    btnSprite = ButtonSprite::create("Buttons");
#endif
    btnSprite->setScale(0.54f);
    btn = CCMenuItemSpriteExtra::create(btnSprite, this, menu_selector(RecordLayer::openKeybinds));
    btn->setPosition(ccp(-116, -32));
    menu->addChild(btn);

    spr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
    spr->setScale(0.65f);
    btn = CCMenuItemSpriteExtra::create(
        spr,
        this,
        menu_selector(RecordLayer::macroInfo)
    );
    btn->setPosition(ccp(-36, 107));
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

void RecordLayer::onSelectSettingsCategory(CCObject* sender) {
    auto* node = static_cast<CCNode*>(sender);
    if (!node)
        return;

    selectSettingsCategory(static_cast<size_t>(std::max(0, node->getTag())));
}

void RecordLayer::updateSettingsCategoryButtons() {
    if (settingsCategoryButtons.empty())
        return;

    int selectedIndex = std::clamp(
        Global::get().currentPage,
        0,
        static_cast<int>(settingsCategoryButtons.empty() ? 0 : settingsCategoryButtons.size() - 1)
    );

    for (size_t i = 0; i < settingsCategoryButtons.size(); i++) {
        auto* button = settingsCategoryButtons[i];
        if (!button)
            continue;

        bool selected = selectedIndex == static_cast<int>(i);
        button->setScale(selected ? 1.04f : 1.f);
        button->setEnabled(!selected);

        if (auto* bg = typeinfo_cast<CCScale9Sprite*>(button->getNormalImage())) {
            bg->setOpacity(selected ? 205 : 120);
            bg->setColor(selected ? ccColor3B { 244, 221, 142 } : ccColor3B { 214, 229, 241 });

            if (auto* label = typeinfo_cast<CCLabelBMFont*>(bg->getChildByID("category-label"_spr))) {
                label->setOpacity(selected ? 255 : 175);
                label->setColor(selected ? ccColor3B { 92, 65, 23 } : ccColor3B { 255, 255, 255 });
            }
        }
    }
}

void RecordLayer::selectSettingsCategory(size_t index) {
    if (!isMacroMenuRewriteEnabled())
        return;

    auto& g = Global::get();
    if (index >= kSettingsCategories.size())
        index = 0;

    if (g.currentPage == static_cast<int>(index)) {
        updateSettingsCategoryButtons();
        return;
    }

    g.currentPage = static_cast<int>(index);
    if (mod)
        mod->setSavedValue("current_page", std::to_string(g.currentPage));

    updateSettingsCategoryButtons();
    loadSettingsList();
}

void RecordLayer::setToggleMember(CCMenuItemToggler* toggle, std::string id) {
    if (id == "macro_tps_enabled") tpsToggle = toggle;
}

void RecordLayer::loadSetting(RecordSetting sett, float yPos, CCMenu* targetMenu) {

    float targetWidth = targetMenu ? targetMenu->getContentSize().width : 190.f;
    float labelX = 10.f;
    float toggleX = targetWidth - 14.f;
    float optionButtonX = targetWidth - 46.f;
    float actionButtonX = targetWidth - 28.f;
    float compactInputAnchorX = targetWidth - 76.f;
    float compactInputCenterX = targetWidth - 58.f;
    float cycleButtonX = targetWidth - 56.f;
    float labelWidth = targetWidth - 92.f;
    if (sett.input == InputType::Action)
        labelWidth = targetWidth - 58.f;
    else if (sett.input == InputType::Accuracy)
        labelWidth = targetWidth - 118.f;

    CCLabelBMFont* lbl = CCLabelBMFont::create(sett.name.c_str(), "bigFont.fnt");
    lbl->setPosition(ccp(labelX, yPos));
    lbl->setAnchorPoint({ 0, 0.5 });
    lbl->setOpacity(200);
    lbl->setScale(sett.labelScale);
    lbl->limitLabelWidth(labelWidth, sett.labelScale, 0.1f);
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
        btn->setID(sett.id.c_str());

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
        btn->setID((sett.id + "_settings").c_str());
        btn->setEnabled(!sett.disabled);
        btn->setOpacity(sett.disabled ? 110 : 255);

        nodes.push_back(static_cast<CCNode*>(btn));
        targetMenu->addChild(btn);
    }





    if (sett.input == InputType::Tps) {
        tpsBg = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
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





    if (sett.input == InputType::FrameOffset) {
        CCScale9Sprite* bg = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
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
        CCScale9Sprite* bg = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
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
        CCScale9Sprite* btnSpr = createSettingsChoiceSprite(getSavedAccuracyMode(mod));
        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(
            btnSpr,
            this,
            menu_selector(RecordLayer::onCycleAccuracy)
        );
        btn->setPosition(ccp(cycleButtonX, yPos));
        btn->setID("macro_accuracy_cycle"_spr);

        nodes.push_back(static_cast<CCNode*>(btn));
        targetMenu->addChild(btn);
    }


}

void RecordLayer::loadSettingsList() {
    auto& g = Global::get();
    nodes.clear();

    tpsToggle = nullptr;

    tpsInput = nullptr;
    frameOffsetInput = nullptr;
    frameFixesLimitInput = nullptr;

    tpsBg = nullptr;

    if (!settingsScroll) return;

    bool macroMenuRewrite = isMacroMenuRewriteEnabled();
    std::vector<RecordSetting> legacySettings;
    SettingsCategory legacyCategory;
    SettingsCategory const* categoryPtr = nullptr;

    if (macroMenuRewrite) {
        size_t categoryIndex = static_cast<size_t>(std::clamp(g.currentPage, 0, static_cast<int>(kSettingsCategories.size()) - 1));
        categoryPtr = &kSettingsCategories[categoryIndex];
    }
    else {
        legacySettings = getLegacySettingsList();
        legacyCategory = { "Settings", std::move(legacySettings) };
        categoryPtr = &legacyCategory;
    }

    if (settingsMenu) {
        detachActiveInputsRecursive(settingsMenu);
        settingsMenu->removeFromParentAndCleanup(true);
        settingsMenu = nullptr;
    }

    constexpr float rowSpacing = 29.f;
    constexpr float topPadding = 16.f;
    constexpr float bottomPadding = 12.f;
    auto const& category = *categoryPtr;
    size_t settingCount = category.settings.size();

    float viewWidth = settingsScroll->getContentSize().width;
    float viewHeight = settingsScroll->getContentSize().height;
    float contentHeight = std::max(viewHeight, topPadding + bottomPadding + settingCount * rowSpacing);
    bool canScroll = contentHeight > viewHeight + 1.f;

    if (settingsScrollbar)
        settingsScrollbar->setVisible(canScroll);

    if (settingsSectionLabel) {
        settingsSectionLabel->setString(category.title.c_str());
        settingsSectionLabel->limitLabelWidth(macroMenuRewrite ? 190.f : 120.f, 0.42f, 0.1f);
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
