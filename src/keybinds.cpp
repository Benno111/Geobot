#include "includes.hpp"

#ifndef GEODE_IS_IOS
#include <Geode/modify/CCKeyboardDispatcher.hpp>

class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double dt) {
        auto& g = Global::get();
        int keyCode = static_cast<int>(key);
        if (g.allKeybinds.contains(keyCode) && !isKeyRepeat) {
            for (size_t i = 0; i < 6; i++) {
                if (std::find(g.keybinds[i].begin(), g.keybinds[i].end(), keyCode) != g.keybinds[i].end())
                    g.heldButtons[i] = isKeyDown;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, dt);
    }
};
#endif
