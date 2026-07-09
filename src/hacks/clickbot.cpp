#include "../includes.hpp"
#include "clickbot.hpp"

#include <Geode/modify/GJBaseGameLayer.hpp>

void Clickbot::ensureInitialized() {
    auto & g = Global::get();
    if (!g.mod)
        return;

    if (!g.mod->hasSavedValue("clickbot_defaults5")) {
        g.mod->setSavedValue("clickbot_defaults5", true);
        g.mod->setSavedValue("clickbot_holding_only", true);
        g.mod->setSavedValue("clickbot_playing_only", false);
    }

    if (!g.mod->hasSavedValue("clickbot_defaults4")) {
        g.mod->setSavedValue("clickbot_defaults4", true);
        std::filesystem::path dir = g.mod->getResourcesDir();
        ClickSetting setts;

        for (const auto& str : buttonNames) {
            setts.path = dir / fmt::format("default_{}.mp3", str);
            matjson::Value data = matjson::Serialize<ClickSetting>::to_json(setts);
            g.mod->setSavedValue(str, data);
        }

        g.mod->setSavedValue("clickbot_volume", 100);
        g.mod->setSavedValue("clickbot_pitch", 1.f);
    }

    g.clickbotEnabled = g.mod->getSavedValue<bool>("clickbot_enabled");
    g.clickbotOnlyPlaying = g.mod->getSavedValue<bool>("clickbot_playing_only");
    g.clickbotOnlyHolding = g.mod->getSavedValue<bool>("clickbot_holding_only");
}

class $modify(GJBaseGameLayer) {
    
    void handleButton(bool hold, int button, bool player2) {
        GJBaseGameLayer::handleButton(hold, button, player2);
        Clickbot::ensureInitialized();
        auto& g = Global::get();

        if (!g.clickbotEnabled) return;
        if (button > 3 || (!hold && g.clickbotOnlyHolding)) return;

        PlayLayer* pl = PlayLayer::get();

        if (!pl) return;
        if (g.clickbotOnlyPlaying && g.state != state::playing) return;

        std::string btn = button == 1 ? "click" : (button == 2 ? "left" : "right");
        std::string id = (hold ? "hold_" : "release_") + btn;
        Clickbot::playSound(id);
    }

};

FMOD::Sound* Clickbot::getSound(std::string id) {
    auto& c = get();

    if (id == "hold_click")
        return c.holdClick;
    else if (id == "release_click")
        return c.releaseClick;
    else if (id == "hold_left")
        return c.holdLeft;
    else if (id == "release_left")
        return c.releaseLeft;
    else if (id == "hold_right")
        return c.holdRight;
    else if (id == "release_right")
        return c.releaseRight;

    return nullptr;
}

void Clickbot::setSound(std::string id, FMOD::Sound* sound) {
    auto& c = get();

    if (id == "hold_click")
        c.holdClick = sound;
    else if (id == "release_click")
        c.releaseClick = sound;
    else if (id == "hold_left")
        c.holdLeft = sound;
    else if (id == "release_left")
        c.releaseLeft = sound;
    else if (id == "hold_right")
        c.holdRight = sound;
    else if (id == "release_right")
        c.releaseRight = sound;
}

void Clickbot::playSound(std::string id) {
    ensureInitialized();
    auto& c = get();
    if (!c.system) return updateSounds();

    auto& g = Global::get();
    matjson::Value data = g.mod->getSavedValue<matjson::Value>(id);
    ClickSetting settings = matjson::Serialize<ClickSetting>::from_json(data);

    if (settings.disabled) return;

    FMOD::Sound* sound = getSound(id);

    if (!sound) return;

    int masterVol = static_cast<int>(getSavedInt64Safe(g.mod, "clickbot_volume", 100));
    if (settings.volume == 0 || masterVol == 0) return;

    FMOD_RESULT result;

    result = c.system->playSound(sound, nullptr, false, &c.channel);
    if (result != FMOD_OK) return log::debug("Click sound errored. ID: 2");

    result = c.channel->setVolume((settings.volume / 100.f) * (masterVol / 100.f));
    if (result != FMOD_OK) return log::debug("Click sound errored. ID: 3");

    float totalPitch = g.currentPitch * settings.pitch * g.mod->getSavedValue<float>("clickbot_pitch");
    result = c.channel->setPitch(totalPitch);
    if (result != FMOD_OK) return log::debug("Click sound errored. ID: 4");
}

void Clickbot::updateSounds() {
    ensureInitialized();
    auto& c = get();
    FMOD_RESULT result;

    if (!c.system) {
        FMODAudioEngine* fmod = FMODAudioEngine::sharedEngine();
        c.system = fmod->m_system;
    }

    if (!c.system) return;

    for (std::string name : buttonNames) {
        matjson::Value data = Global::get().mod->getSavedValue<matjson::Value>(name);
        ClickSetting settings = matjson::Serialize<ClickSetting>::from_json(data);
        if (!std::filesystem::exists(settings.path)) continue;

        FMOD::Sound* sound = getSound(name);
        result = c.system->createSound(settings.path.string().c_str(), FMOD_DEFAULT, nullptr, &sound);
        if (result != FMOD_OK) {
            log::debug("Click sound errored. ID: 1");
            continue;
        }

        setSound(name, sound);
    }

}
