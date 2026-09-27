#include "../includes.hpp"
#include <Geode/modify/PlayLayer.hpp>

class $modify(PlayLayer) {
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto& g = Global::get();
        if (g.state != state::recording || g.macro.inputs.empty()) return;
        if (g.autosaveCheck < g.autosaveInterval) {
            g.autosaveCheck += dt;
            return;
        }
        g.autosaveCheck = 0.f;
        auto now = std::chrono::steady_clock::now();
        auto number = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        Macro::autoSave(m_level, static_cast<int>(number));
    }

    void levelComplete() {
        auto& g = Global::get();
        if (g.state == state::recording)
            Macro::autoSave(nullptr, g.currentSession);
        PlayLayer::levelComplete();
    }
};
