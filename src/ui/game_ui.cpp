#include "../includes.hpp"
#include "game_ui.hpp"

#include <Geode/modify/PlayLayer.hpp>

class $modify(PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        Interface::addLabels(this);
        return true;
    }
};

void Interface::addLabels(PlayLayer* pl) {
    auto* label = CCLabelBMFont::create("", "chatFont.fnt");
    label->setPosition({ CCDirector::sharedDirector()->getWinSize().width - 6.5f, 12.f });
    label->setAnchorPoint({ 1.f, 0.5f });
    label->setID("state-label"_spr);
    label->setZOrder(300);
    label->setScale(0.625f);
    pl->addChild(label);
    Interface::updateLabels();
}

void Interface::addButtons(PlayLayer*) {}

void Interface::updateLabels() {
    auto* pl = PlayLayer::get();
    if (!pl) return;
    auto* label = typeinfo_cast<CCLabelBMFont*>(pl->getChildByID("state-label"_spr));
    if (!label) return;

    auto state = Global::get().state;
    label->setString(state == state::recording ? "Recording" : state == state::playing ? "Playing" : "");
}

void Interface::updateButtons() {}
