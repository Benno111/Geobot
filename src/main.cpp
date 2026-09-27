#include "includes.hpp"

#include "ui/game_ui.hpp"
#include "ui/record_layer.hpp"
#include "practice_fixes/practice_fixes.hpp"

#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/modify/AppDelegate.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

namespace {
constexpr int kRespawnMovementClearFrames = 5;

bool isEditorPlaytestCompat(PlayLayer* pl) {
    if (!pl) return false;
    return LevelEditorLayer::get() != nullptr || pl->m_isTestMode;
}

#ifdef GEODE_IS_IOS
void clearRuntimeInputState(GJBaseGameLayer* layer) {
    if (!layer) return;

    auto clearPlayer = [](PlayerObject* player) {
        if (!player) return;
        player->releaseAllButtons();
        player->m_holdingLeft = false;
        player->m_holdingRight = false;
        player->m_holdingButtons[1] = false;
        player->m_holdingButtons[2] = false;
        player->m_holdingButtons[3] = false;
    };

    clearPlayer(layer->m_player1);
    clearPlayer(layer->m_player2);

    auto& g = Global::get();
    for (int i = 0; i < 6; i++) {
        g.heldButtons[i] = false;
        g.wasHolding[i] = false;
    }

    g.delayedFrameRelease[0][0] = -1;
    g.delayedFrameRelease[0][1] = -1;
    g.delayedFrameRelease[1][0] = -1;
    g.delayedFrameRelease[1][1] = -1;
    g.delayedFrameReleaseMain[0] = -1;
    g.delayedFrameReleaseMain[1] = -1;
    g.delayedFrameInput[0] = -1;
    g.delayedFrameInput[1] = -1;
    g.ignoreFrame = -1;
    g.ignoreJumpButton = -1;
}

void handleIOSAppInterrupted() {
    auto& g = Global::get();

    if (g.layer)
        detachActiveInputsRecursive(g.layer);
    if (CCScene* scene = CCDirector::sharedDirector()->getRunningScene())
        detachActiveInputsRecursive(scene);
    if (g.layer) {
        if (auto* recordLayer = typeinfo_cast<RecordLayer*>(g.layer))
            recordLayer->onClose(nullptr);
        else
            g.layer->removeFromParentAndCleanup(true);
        g.layer = nullptr;
    }

    PlayLayer* pl = PlayLayer::get();
    LevelEditorLayer* editor = LevelEditorLayer::get();
    clearRuntimeInputState(pl ? static_cast<GJBaseGameLayer*>(pl) : static_cast<GJBaseGameLayer*>(editor));

    bool wasActive = g.state != state::none;

    g.state = state::none;
    g.restart = false;
    g.restartLater = false;
    g.leftOver = 0.f;
    g.currentAction = 0;
    g.currentFrameFix = 0;
    Macro::resetVariables();

    if (pl && !pl->m_isPaused && !pl->m_levelEndAnimationStarted)
        pl->pauseGame(false);

    Interface::updateLabels();
    Interface::updateButtons();

    if (wasActive)
        log::info("Stopped active geobot session for iOS app interruption");
}
#endif






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
                    Macro::resetVariables();
            if (isEditorPlaytestCompat(this))
                g.restart = true;
        }

        Global::updateKeybinds();

        auto now = std::chrono::system_clock::now();
        g.currentSession = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        g.lastAutoSaveFrame = 0;
        g.botUsedInLevelSession = g.state != state::none;

        return true;
    }

    void onExit() {
        PlayLayer::onExit();
        Global::get().botUsedInLevelSession = false;
    }

    void showNewBest(bool po, int p1, int p2, bool p3, bool p4, bool p5) {
        if (!Global::get().botUsedInLevelSession)
            PlayLayer::showNewBest(po, p1, p2, p3, p4, p5);
    }

    void levelComplete() {
        auto& g = Global::get();
        bool wasTestMode = m_isTestMode;
        if (g.botUsedInLevelSession)
            m_isTestMode = true;

        PlayLayer::levelComplete();
        m_isTestMode = wasTestMode;
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

        int frame = Global::getCurrentFrame();
        g.clearMovementUntilFrame = frame + (kRespawnMovementClearFrames - 1);

        if (g.restart && m_levelSettings->m_platformerMode && g.state != state::none)
            m_fields->delayedLevelRestart = frame + 2;


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

#ifdef GEODE_IS_IOS
class $modify(AppDelegate) {
    void applicationDidEnterBackground() {
        handleIOSAppInterrupted();
        AppDelegate::applicationDidEnterBackground();
    }

    void applicationWillEnterForeground() {
        AppDelegate::applicationWillEnterForeground();

        auto& g = Global::get();
        g.leftOver = 0.f;
        if (PlayLayer* pl = PlayLayer::get())
            clearRuntimeInputState(pl);
        else if (LevelEditorLayer* editor = LevelEditorLayer::get())
            clearRuntimeInputState(editor);
    }
};
#endif

class $modify(BGLHook, GJBaseGameLayer) {
    struct Fields {
        bool macroInput = false;

    };

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto& g = Global::get();
        PlayLayer* pl = PlayLayer::get();

        if (pl && pl != typeinfo_cast<PlayLayer*>(this))
            return GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);


        if (g.state != state::none) {

            int frame = Global::getCurrentFrame(!pl);
            if (frame > 2 && g.firstAttempt && g.macro.geobotMacro) {
                g.firstAttempt = false;

                if (pl && !m_levelEndAnimationStarted) {
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


        if (g.state == state::none)
            return;

        if (pl && !m_levelEndAnimationStarted && (g.state == state::playing || g.state == state::recording))
            g.macroUsedInAttempt = true;
        if (pl && (g.state == state::playing || g.state == state::recording))
            g.botUsedInLevelSession = true;

        g.previousFrame = frame;

        if (pl && g.macro.geobotMacro && g.restart && !m_levelEndAnimationStarted) {
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

        if (!g.macro.frameFixes.empty() && g.macro.frameFixes.back().frame == frame)
            return;

        g.macro.recordFrameFix(frame, m_player1, m_player2);
    }

    void handlePlaying(int frame) {
        auto& g = Global::get();
        if (m_levelEndAnimationStarted)
            return;

        if (m_player1->m_isDead) {
            m_player1->releaseAllButtons();
            m_player2->releaseAllButtons();


            if (PlayLayer* pl = PlayLayer::get(); pl && !pl->m_isPracticeMode) {
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
                (void)actionIndex;
                GJBaseGameLayer::handleButton(macroInput.down, macroInput.button, inputPlayer2);
            }

            g.currentAction++;
        }

        g.respawnFrame = -1;
        m_fields->macroInput = false;


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





    void handleButton(bool hold, int button, bool player2) {
        auto& g = Global::get();



        if (g.state == state::recording &&
            !m_fields->macroInput &&
            !g.ignoreRecordAction &&
            !m_levelEndAnimationStarted) {
            int frame = Global::getCurrentFrame(!PlayLayer::get());
            // Respect ignoreFrame for a range: skip recording while
            // frame <= g.ignoreFrame (g.ignoreFrame == -1 means disabled).
            if (g.ignoreFrame == -1 || frame > g.ignoreFrame)
                Macro::recordAction(frame, button, player2, hold);
        }


        GJBaseGameLayer::handleButton(hold, button, player2);
    }
};
