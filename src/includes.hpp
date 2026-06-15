#pragma once

#include <Geode/Geode.hpp>
// #include <Geode/loader/SettingEvent.hpp>

#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <cmath>
#include <cctype>
#include <vector>
#include <utility>
#include <filesystem>
#include <limits>
#include <charconv>

#include "renderer/renderer.hpp"
#include "macro.hpp"

using namespace geode::prelude;

namespace xdb {
template <class... SetupArgs>
class Popup : public geode::Popup {
protected:
    virtual bool setup(SetupArgs... args) = 0;

    void adjustForLoadingScreen(bool includeTitle = true) {
        cocos2d::CCPoint offset = (cocos2d::CCDirector::sharedDirector()->getWinSize() - m_mainLayer->getContentSize()) / 2;
        m_mainLayer->setPosition(m_mainLayer->getPosition() - offset);
        m_closeBtn->setPosition(m_closeBtn->getPosition() + offset);
        m_bgSprite->setPosition(m_bgSprite->getPosition() + offset);

        if (includeTitle && m_title)
            m_title->setPosition(m_title->getPosition() + offset);
    }

public:
    bool initAnchored(
        float width,
        float height,
        SetupArgs... args,
        char const* bg = "GJ_square01.png",
        cocos2d::CCRect bgRect = {}
    ) {
        if (!this->init(width, height, bg, bgRect)) return false;
        return this->setup(std::forward<SetupArgs>(args)...);
    }
};
}

const int seedAddr = 0x6a4e20;

const int indexButton[6] = { 1, 2, 3, 1, 2, 3 };

const std::map<int, int> buttonIndex[2] = { { {1, 0}, {2, 1}, {3, 2} }, { {1, 3}, {2, 4}, {3, 5} } };

const int sidesButtons[4] = { 1, 2, 4, 5 };

const std::string buttonIDs[6] = {
    "robtop.geometry-dash/jump-p1",
    "robtop.geometry-dash/move-left-p1",
    "robtop.geometry-dash/move-right-p1",
    "robtop.geometry-dash/jump-p2",
    "robtop.geometry-dash/move-left-p2",
    "robtop.geometry-dash/move-right-p2"
};

inline int64_t getSavedInt64Safe(Mod* mod, std::string const& key, int64_t fallback = 0) {
    if (!mod) return fallback;
    auto raw = mod->getSavedValue<std::string>(key);
    if (raw.empty()) return fallback;

    auto begin = raw.data();
    auto end = begin + raw.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(*begin))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) --end;
    if (begin >= end) return fallback;

    int64_t value = 0;
    auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc() || ptr != end) return fallback;
    return value;
}

#define STATIC_CREATE(class, width, height) \
    static class* create() { \
        class* ret = new class(); \
        if (ret->initAnchored(width, height)) { \
            ret->autorelease(); \
            return ret; \
        } \
        delete ret; \
        return nullptr; \
    }

class Global {

    Global() {}

public:

    static auto& get() {
        static Global instance;
        return instance;
    }

    static bool hasIncompatibleMods();
    static bool isBuildExpired();
    static void showBuildExpiredNotice();

    static float getTPS();

    static int getCurrentFrame(bool editor = false);

    static void updateKeybinds();

    static void updateSeed(bool isRestart = false);

    static void updatePitch(float value);

    static void toggleSpeedhack();

    static void frameStep();

    static void toggleFrameStepper();

    static void frameStepperOn();

    static void frameStepperOff();

    static PauseLayer* getPauseLayer();
    static std::filesystem::path getFolderSettingPath(std::string const& settingID, bool createIfMissing = true);
    static void triggerFramePerfectOverlay(int button, bool down);
    static void triggerFramePerfectOverlayProgress(int button, bool down, std::string const& typeName, int leftWiggle, int rightWiggle);
    static void triggerFramePerfectExpected(int leftWiggle, int rightWiggle);
    static void triggerFramePerfectOverlayCounted(size_t actionIndex, int button, bool down, std::string const& typeName, int leftWiggle, int rightWiggle);
    static void refreshFramePerfectOverlayText();
    static void resetFramePerfectStats();
    static bool isDeveloperModeEnabled();
    static void setDeveloperModeEnabled(bool enabled);
    static std::string getFramePerfectOverlayMode();
    static bool isFramePerfectDetectionEnabled();
    static bool isPathfinderFeatureEnabled();

    Mod* mod = Mod::get();
    geode::Popup* layer = nullptr;

    Macro macro;
    Renderer renderer;
    state state = none;

    std::unordered_map<CheckpointObject*, CheckpointData> checkpoints;
    std::unordered_set<int> allKeybinds;
    std::unordered_set<int> playedFrames;
    std::vector<int> keybinds[6];

    int lastAutoSaveFrame = 0;
    std::chrono::time_point<std::chrono::steady_clock> lastAutoSaveMS = std::chrono::steady_clock::now();
    int currentSession = 0;

    bool stepFrame = false;
    bool stepFrameDraw = false;
    int stepFrameDrawMultiple = 0;
    int stepFrameParticle = 0;
    int frameStepperMusicTime = 0;

    bool cancelCheckpoint = false;
    bool ignoreRecordAction = false;
    bool restart = false;
    bool restartLater = false;
    bool creatingTrajectory = false;
    bool firstAttempt = false;
    bool macroUsedInAttempt = false;

    bool disableShaders = false;
    bool safeMode = false;
    bool layoutMode = false;
    bool showTrajectory = false;
    bool coinFinder = false;
    bool frameStepper = false;
    bool speedhackEnabled = false;
    bool speedhackAudio = false;
    bool seedEnabled = false;
    bool clickbotEnabled = false;
    bool clickbotOnlyPlaying = false;
    bool clickbotOnlyHolding = false;
    bool frameLabel = false;
    bool trajectoryBothSides = false;
    bool p2mirror = false;
    bool lockDelta = false;
    bool stopPlaying = false;
    bool pathfinderMode = false;
    bool pathfinderSearching = false;
    bool tpsEnabled = false;
    float tps = 240.f;
    bool previousTpsEnabled = false;
    float previousTps = 0.f;
    bool autoclicker = false;
    bool autoclickerP1 = false;
    bool autoclickerP2 = false;
    int holdFor = 0;
    int releaseFor = 0;
    int holdFor2 = 0;
    int releaseFor2 = 0;
    bool autosaveIntervalEnabled = false;
    int autosaveInterval = 600000;
    float autosaveCheck = 2.f;
    bool autosaveEnabled = false;

    bool ignoreStopDashing[2] = { false, false };
    bool addSideHoldingMembers[2] = { false, false };
    bool wasHolding[6] = { false, false, false, false, false, false };
    bool heldButtons[6] = { false, false, false, false, false, false };

    int delayedFrameRelease[2][2] = { { -1, -1 }, { -1, -1 } };
    int delayedFrameReleaseMain[2] = { -1, -1 };
    int delayedFrameInput[2] = { -1, -1 };
    int ignoreFrame = -1;
    int respawnFrame = -1;
    int clearMovementUntilFrame = -1;
    int ignoreJumpButton = -1;
    int frameOffset = 0;
    int previousFrame = 0;

    size_t currentAction = 0;
    size_t currentFrameFix = 0;
    int frameFixesLimit = 240;
    bool frameFixes = false;
    bool inputFixes = false;
    bool buildExpired = false;
    bool buildExpiryNoticeShown = false;

    int currentPage = 0;
    float currentPitch = 1.f;
    std::string cachedMacroSeedString = "";
    uintptr_t cachedMacroSeedValue = 1;
    uintptr_t latestSeed = 0;
    float leftOver = 0.f;
    int framePerfectOverlayFrames = 0;
    std::string framePerfectOverlayText = "";
    int framePerfectCount = 0;
    int framePerfectCount60 = 0;
    int framePerfectCount144 = 0;
    int framePerfectCount240 = 0;
    int framePerfectExpected = 0;
    int framePerfectExpected60 = 0;
    int framePerfectExpected144 = 0;
    int framePerfectExpected240 = 0;
    std::string framePerfectOverlayTypeName = "";
    std::string framePerfectOverlayFpsType = "";
    int framePerfectOverlayLeftWiggle = 0;
    int framePerfectOverlayRightWiggle = 0;
    bool framePerfectOverlayScanning = false;
    bool framePerfectSfxEnabled = true;
    size_t lastFramePerfectAction = std::numeric_limits<size_t>::max();
    std::string pathfinderStatus = "Idle";
};
