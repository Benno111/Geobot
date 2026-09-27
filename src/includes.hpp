#pragma once

#include <Geode/Geode.hpp>
// #include <Geode/loader/SettingEvent.hpp>

#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <vector>
#include <utility>
#include <filesystem>
#include <deque>
#include <limits>
#include <charconv>
#include <cstdint>

#include "macro.hpp"

using namespace geode::prelude;

#define WINDOW_BG "GJ_square01.png"

inline void detachInputNodeSafe(CCTextInputNode* input) {
    if (!input) return;

    input->detachWithIME();
    input->onClickTrackNode(false);
    if (input->m_cursor)
        input->m_cursor->setVisible(false);
}

inline void detachActiveInputsRecursive(CCNode* root) {
    if (!root) return;

    if (auto* input = typeinfo_cast<CCTextInputNode*>(root))
        detachInputNodeSafe(input);

    if (auto* input = typeinfo_cast<TextInput*>(root))
        detachInputNodeSafe(input->getInputNode());

    if (CCArray* children = root->getChildren()) {
        for (int i = 0; i < children->count(); ++i) {
            if (auto* child = typeinfo_cast<CCNode*>(children->objectAtIndex(i)))
                detachActiveInputsRecursive(child);
        }
    }
}

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
    void onExit() override {
        detachActiveInputsRecursive(this);
        geode::Popup::onExit();
    }

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





    static PauseLayer* getPauseLayer();
    static std::filesystem::path getFolderSettingPath(std::string const& settingID, bool createIfMissing = true);

    Mod* mod = Mod::get();
    geode::Popup* layer = nullptr;

    Macro macro;
    state state = none;

    std::unordered_map<CheckpointObject*, CheckpointData> checkpoints;
    std::unordered_set<int> allKeybinds;
    std::unordered_set<int> playedFrames;
    std::vector<int> keybinds[6];

    int lastAutoSaveFrame = 0;
    std::chrono::time_point<std::chrono::steady_clock> lastAutoSaveMS = std::chrono::steady_clock::now();
    std::int64_t currentSession = 0;


    bool cancelCheckpoint = false;
    bool ignoreRecordAction = false;
    bool restart = false;
    bool restartLater = false;
    bool firstAttempt = false;
    bool macroUsedInAttempt = false;
    bool botUsedInLevelSession = false;

    bool clickbotEnabled = false;
    bool clickbotOnlyPlaying = false;
    bool clickbotOnlyHolding = false;
    bool frameLabel = false;
    bool lockDelta = false;
    bool stopPlaying = false;
    bool tpsEnabled = false;
    float tps = 240.f;
    bool previousTpsEnabled = false;
    float previousTps = 0.f;
    bool autosaveEnabled = true;
    bool autosaveIntervalEnabled = true;
    int autosaveInterval = 600;
    float autosaveCheck = 2.f;

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
    // Keep the fixed-step accumulator in double precision. A float accumulator
    // can round a nominal 4-step 60 Hz update down to 3 steps on some targets.
    double leftOver = 0.0;
};
