#include "../includes.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>

class $modify(GJBaseGameLayer) {

    void update(float dt) {
        auto& g = Global::get();

        // Run the multi-step physics loop when:
        //   1. Custom TPS bypass is enabled (non-240 TPS), OR
        //   2. Lock Delta is enabled while playing/recording (ensures one physics step
        //      per 1/TPS slice, which is required for correct macro playback physics
        //      even at the default 240 TPS when the render framerate is lower), OR
        //   3. A macro is actively being played back (ensures the fixed step loop runs
        //      regardless of user TPS/lockDelta settings, so playback speed is
        //      identical in normal play and in editor test mode).
        bool shouldBypass = (g.tpsEnabled && Global::getTPS() != 240.f) ||
                            (g.lockDelta && g.state != state::none) ||
                            g.state == state::playing;

        if (!shouldBypass) return GJBaseGameLayer::update(dt);
        // Only apply the bypass for the active PlayLayer.  During editor
        // playtesting, both PlayLayer and LevelEditorLayer derive from
        // GJBaseGameLayer and may both receive update() calls.  Letting the
        // bypass run for the LevelEditorLayer would corrupt g.leftOver and
        // cause double-stepping, making the game run at twice the expected speed.
        PlayLayer* pl = PlayLayer::get();
        if (!pl || pl != typeinfo_cast<PlayLayer*>(this))
            return GJBaseGameLayer::update(dt);
        
        float newDt = 1.f / Global::getTPS();

        if (g.frameStepper) return GJBaseGameLayer::update(newDt);

        float realDt = dt + g.leftOver;

        auto startTime = std::chrono::high_resolution_clock::now();
        int mult = static_cast<int>(realDt / newDt);

        for (int i = 0; i < mult; ++i) {
            GJBaseGameLayer::update(newDt);
            if (std::chrono::high_resolution_clock::now() - startTime > std::chrono::duration<double, std::milli>(16.666f)) {
                mult = i + 1;
                break;
            }
        }

        // Keep the fractional remainder for the next frame.  Using
        // realDt (not just dt) as the base ensures the leftOver stays
        // in [0, newDt) and never grows unboundedly.
        g.leftOver = realDt - newDt * mult;
        
    }

    float getModifiedDelta(float dt) {
        auto& g = Global::get();
        bool shouldBypass = (g.tpsEnabled && Global::getTPS() != 240.f) ||
                            (g.lockDelta && g.state != state::none) ||
                            g.state == state::playing;
        if (!shouldBypass) return GJBaseGameLayer::getModifiedDelta(dt);
        PlayLayer* pl = PlayLayer::get();
        if (!pl || pl != typeinfo_cast<PlayLayer*>(this))
            return GJBaseGameLayer::getModifiedDelta(dt);

        double dVar1;
        float fVar2;
        float fVar3;
        double dVar4;

        float newDt = 1.f / Global::getTPS();
        
        if (0 < m_resumeTimer) {
            // cocos2d::CCDirector::sharedDirector();
            m_resumeTimer--;
            dt = 0.0;
        }

        fVar2 = 1.0;
        if (m_gameState.m_timeWarp <= 1.0) {
            fVar2 = m_gameState.m_timeWarp;
        }

        dVar1 = dt + m_extraDelta;
        fVar3 = std::round(dVar1 / (fVar2 * newDt));
        dVar4 = fVar3 * fVar2 * newDt;
        m_extraDelta = dVar1 - dVar4;

        return dVar4;
    }

};