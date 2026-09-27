#include "../includes.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>

namespace {
    // Leave part of a 60 Hz frame for rendering, audio, and the rest of the
    // scheduler. Spending the entire 16.67 ms interval in playback's catch-up
    // loop makes an otherwise 60 FPS frame miss v-sync and present at 30 FPS.
    constexpr auto kPlaybackStepBudget = std::chrono::duration<double, std::milli>(6.0);
    constexpr int kMaxCatchupStepsPerFrame = 12;
}

class $modify(GJBaseGameLayer) {

    void update(float dt) {
        auto& g = Global::get();

        // Run the multi-step physics loop only for explicit fixed-step modes.
        // Plain macro playback uses the game's normal update path so decorated
        // levels do not pay several full physics/object passes per rendered frame.
        bool shouldBypass = (g.tpsEnabled && Global::getTPS() != 240.f) ||
                            (g.lockDelta && g.state != state::none);

        if (!shouldBypass) {
            g.leftOver = 0.0;
            return GJBaseGameLayer::update(dt);
        }
        // Only apply the bypass for the active PlayLayer.  During editor
        // playtesting, both PlayLayer and LevelEditorLayer derive from
        // GJBaseGameLayer and may both receive update() calls.  Letting the
        // bypass run for the LevelEditorLayer would corrupt g.leftOver and
        // cause double-stepping, making the game run at twice the expected speed.
        PlayLayer* pl = PlayLayer::get();
        if (!pl || pl != typeinfo_cast<PlayLayer*>(this))
            return GJBaseGameLayer::update(dt);
        
        const double newDt = 1.0 / static_cast<double>(Global::getTPS());

        double realDt = static_cast<double>(dt) + g.leftOver;
        double maxFrameDt = newDt * static_cast<double>(kMaxCatchupStepsPerFrame);
        if (realDt > maxFrameDt)
            realDt = maxFrameDt;

        const auto deadline = std::chrono::steady_clock::now() + kPlaybackStepBudget;
        // Account for representation error at exact tick boundaries. Without
        // this, identical deltas can produce a different step count depending
        // on the platform's floating-point implementation.
        constexpr double tickBoundaryEpsilon = 1e-6;
        int mult = static_cast<int>(std::floor((realDt / newDt) + tickBoundaryEpsilon));
        if (mult > kMaxCatchupStepsPerFrame)
            mult = kMaxCatchupStepsPerFrame;

        for (int i = 0; i < mult; ++i) {
            GJBaseGameLayer::update(static_cast<float>(newDt));
            // Always complete at least one tick. Afterwards, yield before the
            // playback loop consumes the whole display frame. Unprocessed
            // ticks remain in leftOver, so inputs and frame fixes are applied
            // on their original simulation frames rather than being dropped.
            if (i + 1 < mult && std::chrono::steady_clock::now() >= deadline) {
                mult = i + 1;
                break;
            }
        }

        // Keep the fractional remainder for the next frame.  Using
        // realDt (not just dt) as the base ensures the leftOver stays
        // in [0, newDt) and never grows unboundedly.
        g.leftOver = realDt - newDt * static_cast<double>(mult);
        // Do not carry a tiny negative value created by the boundary epsilon.
        if (g.leftOver < 0.0 && g.leftOver > -(newDt * tickBoundaryEpsilon))
            g.leftOver = 0.0;
        
    }

    float getModifiedDelta(float dt) {
        auto& g = Global::get();
        bool shouldBypass = (g.tpsEnabled && Global::getTPS() != 240.f) ||
                            (g.lockDelta && g.state != state::none);
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
