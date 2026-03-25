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

bool isEditorPlaytestCompat(PlayLayer* pl) {
    if (!pl) return false;
    return LevelEditorLayer::get() != nullptr || pl->m_isTestMode;
}

void resetFramePerfectStats(Global& g) {
    g.framePerfectOverlayFrames = 0;
    g.framePerfectOverlayText.clear();
    g.framePerfectOverlayTypeName.clear();
    g.framePerfectOverlayFpsType.clear();
    g.framePerfectOverlayLeftWiggle = 0;
    g.framePerfectOverlayRightWiggle = 0;
    g.framePerfectOverlayScanning = false;
    g.framePerfectCount = 0;
    g.framePerfectCount60 = 0;
    g.framePerfectCount144 = 0;
    g.framePerfectCount240 = 0;
    g.framePerfectExpected = 0;
    g.framePerfectExpected60 = 0;
    g.framePerfectExpected144 = 0;
    g.framePerfectExpected240 = 0;
    g.lastFramePerfectAction = std::numeric_limits<size_t>::max();
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
            g.heldButtons[i] = false;
            g.wasHolding[i] = false;
        }
        Macro::resetVariables();

        g.macroUsedInAttempt = false;
        resetFramePerfectStats(g);

        int frame = Global::getCurrentFrame();

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
                    resetFramePerfectStats(g);
                    return pl->resetLevelFromStart();
                }
            }

            if (g.previousFrame == frame && frame != 0 && g.macro.geobotMacro)
                return GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (g.state == state::none)
            return;

        if (pl && !m_levelEndAnimationStarted && (g.state == state::playing || g.state == state::recording))
            g.macroUsedInAttempt = true;

        int frame = Global::getCurrentFrame(!pl);
        g.previousFrame = frame;

        if (pl && g.macro.geobotMacro && g.restart && !m_levelEndAnimationStarted) {
            resetFramePerfectStats(g);
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
                resetFramePerfectStats(g);
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
        if (!isTrackedFramePerfectInput(this, currentInput))
            return;

        std::string typeName = getFramePerfectTypeName(currentInput.button, currentInput.down);
        int leftWiggle = findLiveFramePerfectWiggle(g.macro.inputs, actionIndex);

        Global::triggerFramePerfectOverlayProgress(
            currentInput.button,
            currentInput.down,
            typeName,
            std::max(0, leftWiggle),
            0
        );

        auto& pending = m_fields->pendingFramePerfects;
        size_t write = 0;
        for (size_t i = 0; i < pending.size(); i++) {
            auto const& candidate = pending[i];
            input pendingInput(candidate.inputFrame, candidate.button, candidate.player2, candidate.down);

            if (!canInputsFormFramePerfect(pendingInput, currentInput))
                continue;

            Global::triggerFramePerfectOverlayCounted(
                candidate.actionIndex,
                candidate.button,
                candidate.down,
                candidate.typeName,
                candidate.leftWiggle == -1 ? kFramePerfectMaxGap + 1 : candidate.leftWiggle,
                getFramePerfectGap(pendingInput, currentInput)
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
            static_cast<int>(currentInput.frame),
            currentInput.button,
            currentInput.down,
            currentInput.player2,
            leftWiggle,
            typeName
        });
    }

    void trimExpiredPendingFramePerfects(int frame) {
        auto& pending = m_fields->pendingFramePerfects;
        if (pending.empty())
            return;

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

        GJBaseGameLayer::handleButton(hold, button, player2);
    }
};
