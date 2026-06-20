#include "includes.hpp"

#include "ui/record_layer.hpp"
#include "practice_fixes/practice_fixes.hpp"
#include "hacks/layout_mode.hpp"

#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

namespace {
constexpr int kFramePerfectMaxGap = 2;
constexpr int kRespawnMovementClearFrames = 5;
constexpr int kPathfinderTolerance = 2;

bool isEditorPlaytestCompat(PlayLayer* pl) {
    if (!pl) return false;
    return LevelEditorLayer::get() != nullptr || pl->m_isTestMode;
}

std::string getFramePerfectTypeName(int button, bool down) {
    if (button == 2)
        return down ? "Left Press" : "Left Release";
    if (button == 3)
        return down ? "Right Press" : "Right Release";
    return down ? "Click Press" : "Click Release";
}

bool isTrackedFramePerfectInput(GJBaseGameLayer* layer, input const& currentInput) {
    return currentInput.button == 1 ||
           (layer->m_levelSettings->m_platformerMode &&
            (currentInput.button == 2 || currentInput.button == 3));
}

bool isFramePerfectForTier(int leftWiggle, int rightWiggle, int maxGap) {
    return leftWiggle <= maxGap || rightWiggle <= maxGap;
}

std::string getFramePerfectTierLabel(int leftWiggle, int rightWiggle) {
    std::string result;

    auto append = [&](int maxGap, char const* label) {
        if (!isFramePerfectForTier(leftWiggle, rightWiggle, maxGap))
            return;
        if (!result.empty())
            result += "/";
        result += label;
    };

    append(2, "60");
    append(1, "144");
    append(0, "240");

    return result.empty() ? "None" : result;
}

int getFramePerfectGap(input const& earlier, input const& later) {
    return std::max(0, static_cast<int>(later.frame) - static_cast<int>(earlier.frame) - 1);
}

bool canInputsFormFramePerfect(input const& earlier, input const& later) {
    if (later.frame <= earlier.frame)
        return false;
    if (earlier.player2 != later.player2)
        return false;
    if (earlier.button != later.button)
        return false;
    if (earlier.down == later.down)
        return false;
    return getFramePerfectGap(earlier, later) <= kFramePerfectMaxGap;
}

int findLiveFramePerfectWiggle(std::vector<input> const& inputs, size_t actionIndex) {
    if (actionIndex >= inputs.size())
        return -1;

    auto const& currentInput = inputs[actionIndex];

    for (size_t i = actionIndex; i-- > 0;) {
        auto const& candidate = inputs[i];
        if (candidate.frame < currentInput.frame - (kFramePerfectMaxGap + 1))
            break;

        if (candidate.player2 != currentInput.player2)
            continue;
        if (candidate.button != currentInput.button)
            continue;
        if (candidate.down == currentInput.down)
            continue;

        return getFramePerfectGap(candidate, currentInput);
    }

    return -1;
}

bool shouldCollectFramePerfectCalibration() {
    return Global::isDeveloperModeEnabled();
}

std::string getPathfinderInputName(input const& action, bool showPlayer) {
    std::string result = getFramePerfectTypeName(action.button, action.down);
    if (showPlayer)
        result += action.player2 ? " P2" : " P1";
    return result;
}

bool shouldShowPlayerForPathfinder(GJBaseGameLayer* layer, input const& action) {
    return (layer && layer->m_levelSettings->m_twoPlayerMode) || action.player2;
}

void syncPathfinderToFrame(int frame) {
    auto& g = Global::get();
    while (g.pathfinderAction < g.macro.inputs.size() &&
           static_cast<int>(g.macro.inputs[g.pathfinderAction].frame) < frame - kPathfinderTolerance) {
        g.pathfinderAction++;
    }
}

void updatePathfinderStatusForFrame(GJBaseGameLayer* layer, int frame) {
    auto& g = Global::get();

    if (!Global::isPathfinderFeatureEnabled()) {
        g.pathfinderSearching = false;
        g.pathfinderStatus = "Disabled";
        return;
    }

    if (!g.pathfinderMode) {
        g.pathfinderSearching = false;
        g.pathfinderStatus = "Idle";
        return;
    }

    if (g.macro.inputs.empty()) {
        g.pathfinderSearching = false;
        g.pathfinderStatus = "No Macro";
        return;
    }

    syncPathfinderToFrame(frame);
    if (g.pathfinderAction >= g.macro.inputs.size()) {
        g.pathfinderSearching = false;
        g.pathfinderStatus = "Complete";
        return;
    }

    auto const& next = g.macro.inputs[g.pathfinderAction];
    g.pathfinderSearching = true;

    int delta = static_cast<int>(next.frame) - frame;
    std::string actionName = getPathfinderInputName(next, shouldShowPlayerForPathfinder(layer, next));
    if (delta > 0)
        g.pathfinderStatus = fmt::format("{} in {}f", actionName, delta);
    else if (delta < 0)
        g.pathfinderStatus = fmt::format("Missed {} by {}f", actionName, -delta);
    else
        g.pathfinderStatus = fmt::format("{} now", actionName);
}

bool matchesPathfinderInput(input const& expected, int frame, int button, bool player2, bool down) {
    bool expectedPlayer2 = Macro::flipControls() ? !expected.player2 : expected.player2;
    return expected.button == button &&
           expected.down == down &&
           expectedPlayer2 == player2 &&
           std::abs(static_cast<int>(expected.frame) - frame) <= kPathfinderTolerance;
}

void advancePathfinderFromInput(GJBaseGameLayer* layer, int frame, int button, bool player2, bool down) {
    auto& g = Global::get();
    if (!g.pathfinderMode || g.macro.inputs.empty())
        return;

    syncPathfinderToFrame(frame);

    for (size_t i = g.pathfinderAction; i < g.macro.inputs.size(); i++) {
        auto const& candidate = g.macro.inputs[i];
        if (static_cast<int>(candidate.frame) > frame + kPathfinderTolerance)
            break;
        if (!matchesPathfinderInput(candidate, frame, button, player2, down))
            continue;

        g.pathfinderAction = i + 1;
        updatePathfinderStatusForFrame(layer, frame);
        return;
    }

    updatePathfinderStatusForFrame(layer, frame);
}

void appendFramePerfectCalibrationRow(
    char const* eventType,
    char const* source,
    size_t actionIndex,
    int frame,
    int button,
    bool player2,
    bool down,
    int leftWiggle,
    int rightWiggle,
    std::string const& typeName,
    size_t partnerActionIndex = std::numeric_limits<size_t>::max(),
    int partnerFrame = -1
) {
    if (!shouldCollectFramePerfectCalibration())
        return;

    auto* mod = Mod::get();
    if (!mod)
        return;

    std::filesystem::path path = mod->getSaveDir() / "frameperfect_calibration.csv";
    bool needsHeader = !std::filesystem::exists(path);

    std::ofstream out(path, std::ios::app);
    if (!out.is_open())
        return;

    if (needsHeader) {
        out << "session,event,source,action_index,partner_action_index,frame,partner_frame,button,player2,down,left_wiggle,right_wiggle,tier,type_name\n";
    }

    auto& g = Global::get();
    out << g.currentSession << ','
        << eventType << ','
        << source << ','
        << actionIndex << ',';

    if (partnerActionIndex == std::numeric_limits<size_t>::max())
        out << -1;
    else
        out << partnerActionIndex;

    out << ','
        << frame << ','
        << partnerFrame << ','
        << button << ','
        << (player2 ? 1 : 0) << ','
        << (down ? 1 : 0) << ','
        << leftWiggle << ','
        << rightWiggle << ','
        << getFramePerfectTierLabel(leftWiggle, rightWiggle) << ','
        << '"' << typeName << "\"\n";
}

void clearMovementStateForRespawnWindow(GJBaseGameLayer* layer) {
    if (!layer) return;

    auto& g = Global::get();
    g.heldButtons[1] = false;
    g.heldButtons[2] = false;
    g.heldButtons[4] = false;
    g.heldButtons[5] = false;
    g.wasHolding[1] = false;
    g.wasHolding[2] = false;
    g.wasHolding[4] = false;
    g.wasHolding[5] = false;

    auto clearPlayer = [](PlayerObject* player) {
        if (!player) return;
        player->m_holdingLeft = false;
        player->m_holdingRight = false;
        player->m_holdingButtons[2] = false;
        player->m_holdingButtons[3] = false;
    };

    clearPlayer(layer->m_player1);
    clearPlayer(layer->m_player2);
}
}

class $modify(PlayLayer) {
    struct Fields {
        int delayedLevelRestart = -1;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects))
            return false;

        auto& g = Global::get();

        if (g.state == state::playing) {
            g.currentAction = 0;
            g.currentFrameFix = 0;
            g.previousFrame = 0;
            g.respawnFrame = -1;
            g.leftOver = 0.f;
            Global::resetPathfinderState();
            Macro::resetVariables();
            if (isEditorPlaytestCompat(this))
                g.restart = true;
        }

        Global::updateKeybinds();

        auto now = std::chrono::system_clock::now();
        g.currentSession = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        g.lastAutoSaveFrame = 0;

        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        auto& g = Global::get();
        if (m_player1) m_player1->releaseAllButtons();
        if (m_player2) m_player2->releaseAllButtons();

        if (m_player1) {
            m_player1->m_holdingLeft = false;
            m_player1->m_holdingRight = false;
            m_player1->m_holdingButtons[1] = false;
            m_player1->m_holdingButtons[2] = false;
            m_player1->m_holdingButtons[3] = false;
        }

        if (m_player2) {
            m_player2->m_holdingLeft = false;
            m_player2->m_holdingRight = false;
            m_player2->m_holdingButtons[1] = false;
            m_player2->m_holdingButtons[2] = false;
            m_player2->m_holdingButtons[3] = false;
        }

        for (int i = 0; i < 6; i++) {
            bool isJumpButton = i == 0 || i == 3;
            if (isJumpButton)
                g.heldButtons[i] = false;
            g.wasHolding[i] = false;
        }
        Macro::resetVariables();

        g.macroUsedInAttempt = false;
        Global::resetFramePerfectStats();
        Global::resetPathfinderState();

        int frame = Global::getCurrentFrame();
        g.clearMovementUntilFrame = frame + (kRespawnMovementClearFrames - 1);

        if (!m_isPracticeMode)
            g.renderer.levelStartFrame = frame;

        if (g.restart && m_levelSettings->m_platformerMode && g.state != state::none)
            m_fields->delayedLevelRestart = frame + 2;

        Global::updateSeed(true);

        g.safeMode = g.layoutMode;
        g.leftOver = 0.f;
        g.currentAction = 0;
        g.currentFrameFix = 0;
        g.restart = false;

        if (g.state == state::recording)
            Macro::updateInfo(this);

        if ((!m_isPracticeMode || frame <= 1 || g.checkpoints.empty()) && g.state == state::recording) {
            g.macro.inputs.clear();
            g.macro.frameFixes.clear();
            g.checkpoints.clear();

            g.macro.framerate = 240.f;
            if (g.layer)
                static_cast<RecordLayer*>(g.layer)->updateTPS();

            PlayerData p1Data = PlayerPracticeFixes::saveData(m_player1);
            PlayerData p2Data = PlayerPracticeFixes::saveData(m_player2);

            InputPracticeFixes::applyFixes(this, p1Data, p2Data, frame);
            Macro::resetVariables();

            m_player1->m_holdingRight = false;
            m_player1->m_holdingLeft = false;
            m_player2->m_holdingRight = false;
            m_player2->m_holdingLeft = false;

            m_player1->m_holdingButtons[2] = false;
            m_player1->m_holdingButtons[3] = false;
            m_player2->m_holdingButtons[2] = false;
            m_player2->m_holdingButtons[3] = false;
        }

        if (!m_levelSettings->m_platformerMode ||
            (!g.mod->getSavedValue<bool>("macro_always_practice_fixes") && g.state != state::recording))
            return;

        g.ignoreRecordAction = true;
        for (int i = 0; i < 4; i++) {
            bool player2 = !(sidesButtons[i] > 2);
            if (g.heldButtons[sidesButtons[i]])
                GJBaseGameLayer::handleButton(true, indexButton[sidesButtons[i]], player2);
        }
        g.ignoreRecordAction = false;
    }
};

class $modify(BGLHook, GJBaseGameLayer) {
    struct Fields {
        bool macroInput = false;

        struct PendingFramePerfect {
            size_t actionIndex = 0;
            int inputFrame = 0;
            int button = 0;
            bool down = false;
            bool player2 = false;
            int leftWiggle = -1;
            std::string typeName;
        };

        std::vector<PendingFramePerfect> pendingFramePerfects;
    };

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto& g = Global::get();
        PlayLayer* pl = PlayLayer::get();

        if (pl && pl != typeinfo_cast<PlayLayer*>(this))
            return GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        Global::updateSeed();

        bool rendering = g.renderer.recording || g.renderer.recordingAudio;
        if (g.state != state::none || rendering) {
            if (!g.firstAttempt) {
                g.renderer.dontRender = false;
                g.renderer.dontRecordAudio = false;
            }

            int frame = Global::getCurrentFrame(!pl);
            if (frame > 2 && g.firstAttempt && g.macro.geobotMacro) {
                g.firstAttempt = false;

                if (pl && !m_levelEndAnimationStarted) {
                    Global::resetFramePerfectStats();
                    return pl->resetLevelFromStart();
                }
            }

            if (g.previousFrame == frame && frame != 0 && g.macro.geobotMacro)
                return GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        int frame = Global::getCurrentFrame(!pl);
        if (pl && frame <= g.clearMovementUntilFrame)
            clearMovementStateForRespawnWindow(this);

        if (pl && !m_levelEndAnimationStarted)
            updatePathfinderStatusForFrame(this, frame);

        if (g.state == state::none)
            return;

        if (pl && !m_levelEndAnimationStarted && (g.state == state::playing || g.state == state::recording))
            g.macroUsedInAttempt = true;

        g.previousFrame = frame;

        if (pl && g.macro.geobotMacro && g.restart && !m_levelEndAnimationStarted) {
            Global::resetFramePerfectStats();
            return pl->resetLevelFromStart();
        }

        if (g.state == state::recording)
            handleRecording(frame);

        if (g.state == state::playing)
            handlePlaying(frame);
    }

    void handleRecording(int frame) {
        auto& g = Global::get();

        if (g.ignoreFrame != -1 && g.ignoreFrame < frame)
            g.ignoreFrame = -1;

        bool twoPlayers = m_levelSettings->m_twoPlayerMode;

        if (g.delayedFrameInput[0] == frame) {
            g.delayedFrameInput[0] = -1;
            GJBaseGameLayer::handleButton(true, 1, true);
        }

        if (g.delayedFrameInput[1] == frame) {
            g.delayedFrameInput[1] = -1;
            GJBaseGameLayer::handleButton(true, 1, false);
        }

        if (frame > g.ignoreJumpButton && g.ignoreJumpButton != -1)
            g.ignoreJumpButton = -1;

        for (int x = 0; x < 2; x++) {
            if (g.delayedFrameReleaseMain[x] == frame) {
                bool player2 = x == 0;
                g.delayedFrameReleaseMain[x] = -1;
                GJBaseGameLayer::handleButton(false, 1, twoPlayers ? player2 : false);
            }

            if (!m_levelSettings->m_platformerMode)
                continue;

            for (int y = 0; y < 2; y++) {
                if (g.delayedFrameRelease[x][y] == frame) {
                    int button = y == 0 ? 2 : 3;
                    bool player2 = x == 0;
                    g.delayedFrameRelease[x][y] = -1;
                    GJBaseGameLayer::handleButton(false, button, player2);
                }
            }
        }

        if (!g.frameFixes || g.macro.inputs.empty())
            return;

        if (!g.macro.frameFixes.empty()) {
            if (1.f / Global::getTPS() * (frame - g.macro.frameFixes.back().frame) < 1.f / g.frameFixesLimit)
                return;
        }

        g.macro.recordFrameFix(frame, m_player1, m_player2);
    }

    void handlePlaying(int frame) {
        auto& g = Global::get();
        if (m_levelEndAnimationStarted)
            return;

        if (m_player1->m_isDead) {
            m_fields->pendingFramePerfects.clear();
            m_player1->releaseAllButtons();
            m_player2->releaseAllButtons();

            if (PlayLayer* pl = PlayLayer::get(); pl && !pl->m_isPracticeMode) {
                Global::resetFramePerfectStats();
                pl->resetLevelFromStart();
            }
            return;
        }

        m_fields->macroInput = true;

        while (g.currentAction < g.macro.inputs.size() && frame >= g.macro.inputs[g.currentAction].frame) {
            size_t actionIndex = g.currentAction;
            auto const& macroInput = g.macro.inputs[g.currentAction];

            if (frame != g.respawnFrame) {
                bool inputPlayer2 = Macro::flipControls() ? !macroInput.player2 : macroInput.player2;
                processRealtimeFramePerfect(actionIndex, macroInput);
                GJBaseGameLayer::handleButton(macroInput.down, macroInput.button, inputPlayer2);
            }

            g.currentAction++;
            g.safeMode = true;
        }

        g.respawnFrame = -1;
        m_fields->macroInput = false;

        trimExpiredPendingFramePerfects(frame);

        if (g.currentAction == g.macro.inputs.size() && g.stopPlaying) {
            Macro::togglePlaying();
            Macro::resetState(true);
            return;
        }

        if (g.frameFixes || g.inputFixes) {
            while (g.currentFrameFix < g.macro.frameFixes.size() &&
                   frame >= g.macro.frameFixes[g.currentFrameFix].frame) {
                auto& fix = g.macro.frameFixes[g.currentFrameFix];

                PlayerObject* p1 = m_player1;
                PlayerObject* p2 = m_player2;

                if (fix.p1.pos.x != 0.f && fix.p1.pos.y != 0.f)
                    p1->setPosition(fix.p1.pos);

                if (fix.p1.rotate && fix.p1.rotation != 0.f)
                    p1->setRotation(fix.p1.rotation);

                if (m_gameState.m_isDualMode) {
                    if (fix.p2.pos.x != 0.f && fix.p2.pos.y != 0.f)
                        p2->setPosition(fix.p2.pos);

                    if (fix.p2.rotate && fix.p2.rotation != 0.f)
                        p2->setRotation(fix.p2.rotation);
                }

                g.currentFrameFix++;
            }
        }
    }

    void processRealtimeFramePerfect(size_t actionIndex, input const& currentInput) {
        auto& g = Global::get();
        if (!Global::isFramePerfectDetectionEnabled()) {
            m_fields->pendingFramePerfects.clear();
            return;
        }
        if (!isTrackedFramePerfectInput(this, currentInput))
            return;

        std::string typeName = getFramePerfectTypeName(currentInput.button, currentInput.down);
        int leftWiggle = findLiveFramePerfectWiggle(g.macro.inputs, actionIndex);
        int currentFrame = static_cast<int>(currentInput.frame);

        Global::triggerFramePerfectOverlayProgress(
            currentInput.button,
            currentInput.down,
            typeName,
            std::max(0, leftWiggle),
            0
        );

        appendFramePerfectCalibrationRow(
            "tracked",
            "current",
            actionIndex,
            currentFrame,
            currentInput.button,
            currentInput.player2,
            currentInput.down,
            leftWiggle,
            -1,
            typeName
        );

        auto& pending = m_fields->pendingFramePerfects;
        size_t write = 0;
        for (size_t i = 0; i < pending.size(); i++) {
            auto const& candidate = pending[i];
            input pendingInput(candidate.inputFrame, candidate.button, candidate.player2, candidate.down);

            if (!canInputsFormFramePerfect(pendingInput, currentInput))
                continue;

            int rightWiggle = getFramePerfectGap(pendingInput, currentInput);
            appendFramePerfectCalibrationRow(
                "matched",
                "pending",
                candidate.actionIndex,
                candidate.inputFrame,
                candidate.button,
                candidate.player2,
                candidate.down,
                candidate.leftWiggle == -1 ? kFramePerfectMaxGap + 1 : candidate.leftWiggle,
                rightWiggle,
                candidate.typeName,
                actionIndex,
                currentFrame
            );

            Global::triggerFramePerfectOverlayCounted(
                candidate.actionIndex,
                candidate.button,
                candidate.down,
                candidate.typeName,
                candidate.leftWiggle == -1 ? kFramePerfectMaxGap + 1 : candidate.leftWiggle,
                rightWiggle
            );
        }

        for (size_t i = 0; i < pending.size(); i++) {
            auto const& candidate = pending[i];
            input pendingInput(candidate.inputFrame, candidate.button, candidate.player2, candidate.down);

            if (canInputsFormFramePerfect(pendingInput, currentInput))
                continue;

            if (write != i)
                pending[write] = candidate;
            write++;
        }
        pending.resize(write);

        if (leftWiggle != -1) {
            appendFramePerfectCalibrationRow(
                "matched",
                "left",
                actionIndex,
                currentFrame,
                currentInput.button,
                currentInput.player2,
                currentInput.down,
                leftWiggle,
                kFramePerfectMaxGap + 1,
                typeName
            );

            Global::triggerFramePerfectOverlayCounted(
                actionIndex,
                currentInput.button,
                currentInput.down,
                typeName,
                leftWiggle,
                kFramePerfectMaxGap + 1
            );
            return;
        }

        pending.push_back({
            actionIndex,
            currentFrame,
            currentInput.button,
            currentInput.down,
            currentInput.player2,
            leftWiggle,
            typeName
        });

        appendFramePerfectCalibrationRow(
            "pending",
            "queued",
            actionIndex,
            currentFrame,
            currentInput.button,
            currentInput.player2,
            currentInput.down,
            leftWiggle,
            -1,
            typeName
        );
    }

    void trimExpiredPendingFramePerfects(int frame) {
        if (!Global::isFramePerfectDetectionEnabled()) {
            m_fields->pendingFramePerfects.clear();
            return;
        }
        auto& pending = m_fields->pendingFramePerfects;
        if (pending.empty())
            return;

        for (auto const& candidate : pending) {
            if (frame <= candidate.inputFrame + kFramePerfectMaxGap + 1)
                continue;

            appendFramePerfectCalibrationRow(
                "expired",
                "timeout",
                candidate.actionIndex,
                candidate.inputFrame,
                candidate.button,
                candidate.player2,
                candidate.down,
                candidate.leftWiggle,
                -1,
                candidate.typeName
            );
        }

        auto it = std::remove_if(
            pending.begin(),
            pending.end(),
            [frame](auto const& candidate) {
                return frame > candidate.inputFrame + kFramePerfectMaxGap + 1;
            }
        );
        pending.erase(it, pending.end());
    }

    void handleButton(bool hold, int button, bool player2) {
        auto& g = Global::get();

        if (g.p2mirror && m_gameState.m_isDualMode && !g.autoclicker) {
            GJBaseGameLayer::handleButton(
                g.mod->getSavedValue<bool>("p2_input_mirror_inverted") ? !hold : hold,
                button,
                !player2
            );
        }

        if (g.state == state::recording &&
            !m_fields->macroInput &&
            !g.ignoreRecordAction &&
            !m_levelEndAnimationStarted) {
            int frame = Global::getCurrentFrame(!PlayLayer::get());
            if (frame != g.ignoreFrame)
                Macro::recordAction(frame, button, player2, hold);
        }

        if (!m_fields->macroInput && !m_levelEndAnimationStarted) {
            int frame = Global::getCurrentFrame(!PlayLayer::get());
            advancePathfinderFromInput(this, frame, button, player2, hold);
        }

        GJBaseGameLayer::handleButton(hold, button, player2);
    }
};
