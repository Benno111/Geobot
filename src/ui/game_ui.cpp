#include "../includes.hpp"
#include "game_ui.hpp"

#include <Geode/modify/PlayLayer.hpp>

#if GEOBOT_ENABLE_FRAMEPERFECT_DETECTION
namespace {
constexpr float kFramePerfectOverlayScale = 0.46f;
constexpr float kFramePerfectOverlayMinWidth = 180.f;
constexpr float kFramePerfectOverlayMaxWidth = 252.f;
constexpr float kFramePerfectOverlayMinHeight = 42.f;
constexpr float kFramePerfectOverlayMaxHeight = 58.f;
constexpr float kFramePerfectOverlayPaddingX = 20.f;
constexpr float kFramePerfectOverlayPaddingY = 13.f;

void updateFramePerfectOverlay(CCLabelBMFont* label, CCScale9Sprite* bg, std::string const& text) {
    if (!label || !bg)
        return;

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    CCPoint position { winSize.width / 2.f, 44.f };
    label->setPosition(position);
    bg->setPosition(position);

    label->setString(text.c_str());
    label->setScale(kFramePerfectOverlayScale);
    label->limitLabelWidth(kFramePerfectOverlayMaxWidth - kFramePerfectOverlayPaddingX, kFramePerfectOverlayScale, 0.28f);
    label->updateLabel();

    auto size = label->getContentSize();
    float scaledWidth = size.width * label->getScale();
    float scaledHeight = size.height * label->getScale();
    float maxWidth = std::max(kFramePerfectOverlayMinWidth, std::min(kFramePerfectOverlayMaxWidth, winSize.width - 16.f));

    bg->setContentSize({
        std::clamp(scaledWidth + kFramePerfectOverlayPaddingX, kFramePerfectOverlayMinWidth, maxWidth),
        std::clamp(scaledHeight + kFramePerfectOverlayPaddingY, kFramePerfectOverlayMinHeight, kFramePerfectOverlayMaxHeight)
    });
}
}
#endif

class $modify(PlayLayer) {

    struct Fields {
        CCLabelBMFont* frameLabel = nullptr;
#if GEOBOT_ENABLE_FRAMEPERFECT_DETECTION
        CCLabelBMFont* framePerfectLabel = nullptr;
        CCScale9Sprite* framePerfectBg = nullptr;
#endif
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto& g = Global::get();

        if (g.state != state::none && g.frameLabel && !g.renderer.recording)
            m_fields->frameLabel->setString(("Frame: " + std::to_string(Global::getCurrentFrame())).c_str());

#if GEOBOT_ENABLE_PATHFINDER
        if (g.pathfinderMode && !g.renderer.recording)
            Interface::updateLabels();
#endif

#if GEOBOT_ENABLE_FRAMEPERFECT_DETECTION
        if (m_fields->framePerfectLabel && m_fields->framePerfectBg) {
            Global::refreshFramePerfectOverlayText();
            std::string mode = Global::getFramePerfectOverlayMode();

            bool labelAllowed = !g.renderer.recording &&
                                !g.mod->getSavedValue<bool>("macro_hide_labels") &&
                                !(g.renderer.recording && g.mod->getSavedValue<bool>("render_hide_labels"));

            bool canShow = false;
            if (mode == "Always")
                canShow = labelAllowed;
            else if (mode == "When")
                canShow = labelAllowed && g.framePerfectOverlayFrames > 0;

            if (canShow && mode == "When") {
                m_fields->framePerfectLabel->setVisible(true);
                m_fields->framePerfectBg->setVisible(true);
                updateFramePerfectOverlay(m_fields->framePerfectLabel, m_fields->framePerfectBg, g.framePerfectOverlayText);
                g.framePerfectOverlayFrames--;
            } else if (canShow && mode == "Always") {
                m_fields->framePerfectLabel->setVisible(true);
                m_fields->framePerfectBg->setVisible(true);
                updateFramePerfectOverlay(
                    m_fields->framePerfectLabel,
                    m_fields->framePerfectBg,
                    g.framePerfectOverlayText.empty() ? "FRAME PERFECT\nWaiting for input\nOverlay armed" : g.framePerfectOverlayText
                );
            } else {
                m_fields->framePerfectLabel->setVisible(false);
                m_fields->framePerfectBg->setVisible(false);
            }
        }
#endif
    }

    bool init(GJGameLevel * level, bool b1, bool b2) {
        if (!PlayLayer::init(level, b1, b2)) return false;

        Interface::addLabels(this);
        Interface::addButtons(this);

        m_fields->frameLabel = static_cast<CCLabelBMFont*>(getChildByID("frame-label"_spr));
#if GEOBOT_ENABLE_FRAMEPERFECT_DETECTION
        m_fields->framePerfectLabel = static_cast<CCLabelBMFont*>(getChildByID("frame-perfect-label"_spr));
        m_fields->framePerfectBg = typeinfo_cast<CCScale9Sprite*>(getChildByID("frame-perfect-bg"_spr));
#endif

        return true;
    }
};

void Interface::addLabels(PlayLayer* pl) {
    CCLabelBMFont* lbl = CCLabelBMFont::create("", "chatFont.fnt");
    lbl->setPosition({ CCDirector::sharedDirector()->getWinSize().width - 6.5f, 12 });
    lbl->setAnchorPoint({ 1, 0.5 });
    lbl->setID("state-label"_spr);
    lbl->setZOrder(300);
    lbl->setScale(0.625f);
    pl->addChild(lbl);

    lbl = CCLabelBMFont::create("", "chatFont.fnt");
    lbl->setPosition({ 6.5f, 12 });
    lbl->setAnchorPoint({ 0, 0.5 });
    lbl->setID("frame-label"_spr);
    lbl->setZOrder(300);
    lbl->setScale(0.625f);
    pl->addChild(lbl);

    lbl = CCLabelBMFont::create("Recording Audio", "bigFont.fnt");
    lbl->setPosition(pl->getContentSize() / 2);
    lbl->setID("recording-audio-label"_spr);
    lbl->setZOrder(300);
    lbl->setOpacity(75);
    lbl->setVisible(false);
    pl->addChild(lbl);

#if GEOBOT_ENABLE_FRAMEPERFECT_DETECTION
    auto bg = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
    bg->setPosition({ CCDirector::sharedDirector()->getWinSize().width / 2.f, 44.f });
    bg->setContentSize({ kFramePerfectOverlayMinWidth, 50.f });
    bg->setOpacity(90);
    bg->setVisible(false);
    bg->setID("frame-perfect-bg"_spr);
    bg->setZOrder(300);
    pl->addChild(bg);

    lbl = CCLabelBMFont::create("", "chatFont.fnt");
    lbl->setPosition({ CCDirector::sharedDirector()->getWinSize().width / 2.f, 44.f });
    lbl->setAnchorPoint({ 0.5f, 0.5f });
    lbl->setID("frame-perfect-label"_spr);
    lbl->setZOrder(301);
    lbl->setScale(kFramePerfectOverlayScale);
    lbl->setVisible(false);
    pl->addChild(lbl);
#endif

    Interface::updateLabels();
}

void Interface::addButtons(PlayLayer* pl) {
    ensureButtonDefaultsInitialized();
    cocos2d::CCSize winSize = CCDirector::sharedDirector()->getWinSize();

    CCMenu* menu = CCMenu::create();
    menu->setZOrder(300);
    menu->setPosition({ 0, 0 });
    menu->setID("button-menu"_spr);
    pl->addChild(menu);

    CCSprite* spr = CCSprite::createWithSpriteFrameName("GJ_arrow_02_001.png");
    spr->setFlipX(true);

    CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(spr, pl, menu_selector(Interface::onFrameStepper));
    btn->setAnchorPoint({ 0, 0 });
    btn->setID("step-frame-btn");
    CCSprite* sprite = btn->getChildByType<CCSprite>(0);
    sprite->setPosition({ 0, 0 });

    menu->addChild(btn);

    spr = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");

    btn = CCMenuItemSpriteExtra::create(spr, pl, menu_selector(Interface::onFrameStepperOff));
    btn->setID("disable-stepper-btn");
    btn->setAnchorPoint({ 0, 0 });
    sprite = btn->getChildByType<CCSprite>(0);
    sprite->setPosition({ 0, 0 });

    menu->addChild(btn);

    spr = CCSprite::createWithSpriteFrameName("GJ_timeIcon_001.png");

    btn = CCMenuItemSpriteExtra::create(spr, pl, menu_selector(Interface::onSpeedhack));
    btn->setAnchorPoint({ 0, 0 });
    btn->setID("speedhack-btn");
    sprite = btn->getChildByType<CCSprite>(0);
    sprite->setPosition({ 0, 0 });

    menu->addChild(btn);

    Interface::updateButtons();
}

void Interface::updateLabels() {
    PlayLayer* pl = PlayLayer::get();
    auto& g = Global::get();

    if (!pl) return;

    if (g.state == state::none || !g.frameLabel)
        static_cast<CCLabelBMFont*>(pl->getChildByID("frame-label"_spr))->setString("");

    CCLabelBMFont* label = typeinfo_cast<CCLabelBMFont*>(pl->getChildByID("state-label"_spr));

    if (!label) return;

    if (g.mod->getSavedValue<bool>("macro_hide_labels"))
        return label->setString("");

    state state = g.state;
    std::string labelText = state == state::none ? "" : "Playing";
    if (state == state::recording)
        labelText = "Recording";

    if (labelText == "Recording" && state == state::recording && g.mod->getSavedValue<bool>("macro_hide_recording_label"))
        labelText = "";

    if (labelText == "Playing" && state == state::playing && g.mod->getSavedValue<bool>("macro_hide_playing_label"))
        labelText = "";

    if (g.renderer.recording && g.mod->getSavedValue<bool>("render_hide_labels")) {
        labelText = "";
        if (CCLabelBMFont* lbl = typeinfo_cast<CCLabelBMFont*>(pl->getChildByID("frame-label"_spr)))
            lbl->setString("");
    }

#if GEOBOT_ENABLE_PATHFINDER
    if (g.pathfinderMode && !g.renderer.recording) {
        std::string pathfinderLabel = "Pathfinder: " + g.pathfinderStatus;
        if (labelText.empty())
            labelText = pathfinderLabel;
        else
            labelText += " | " + pathfinderLabel;
    }
#endif

    label->setString(labelText.c_str());
}

void Interface::updateButtons() {
    PlayLayer* pl = PlayLayer::get();
    if (!pl) return;
    ensureButtonDefaultsInitialized();

    CCNode* menu = pl->getChildByID("button-menu"_spr);
    if (!menu) return;

    auto& g = Global::get();

#ifdef GEODE_IS_WINDOWS
    bool isWindows = true;
#else
    bool isWindows = false;
#endif

    CCNode* disableStepperBtn = menu->getChildByID("disable-stepper-btn");
    CCNode* stepFrameBtn = menu->getChildByID("step-frame-btn");
    CCNode* speedhackBtn = menu->getChildByID("speedhack-btn");

    disableStepperBtn->setPosition(ccp(
        g.mod->getSavedValue<float>("button_off_pos_x"),
        g.mod->getSavedValue<float>("button_off_pos_y")
    ));

    float scale = g.mod->getSavedValue<float>("button_off_scale");

    CCSprite* sprite = disableStepperBtn->getChildByType<CCSprite>(0);
    sprite->setScale(scale);
    sprite->setOpacity(static_cast<int>(g.mod->getSavedValue<float>("button_off_opacity") * 255));
    sprite->setAnchorPoint({ 0, 0 });

    cocos2d::CCSize size = sprite->getContentSize();
    disableStepperBtn->setContentSize({ size.width * scale, size.height * scale });

    stepFrameBtn->setPosition(ccp(
        g.mod->getSavedValue<float>("button_advance_frame_pos_x"),
        g.mod->getSavedValue<float>("button_advance_frame_pos_y")
    ));

    scale = g.mod->getSavedValue<float>("button_advance_frame_scale");

    sprite = stepFrameBtn->getChildByType<CCSprite>(0);
    sprite->setScale(scale);
    sprite->setOpacity(static_cast<int>(g.mod->getSavedValue<float>("button_advance_frame_opacity") * 255));
    sprite->setAnchorPoint({ 0, 0 });

    size = sprite->getContentSize();
    speedhackBtn->setContentSize({ size.width * scale, size.height * scale });

    speedhackBtn->setPosition(ccp(
        g.mod->getSavedValue<float>("button_speedhack_pos_x"),
        g.mod->getSavedValue<float>("button_speedhack_pos_y")
    ));

    scale = g.mod->getSavedValue<float>("button_speedhack_scale");

    sprite = speedhackBtn->getChildByType<CCSprite>(0);
    sprite->setScale(scale);
    sprite->setOpacity(static_cast<int>(g.mod->getSavedValue<float>("button_speedhack_opacity") * 255));
    sprite->setAnchorPoint({ 0, 0 });

    size = sprite->getContentSize();
    speedhackBtn->setContentSize({ size.width * scale, size.height * scale });

    if ((g.state != state::recording && !g.mod->getSavedValue<bool>("macro_always_show_buttons")) || isWindows) {
        disableStepperBtn->setVisible(false);
        stepFrameBtn->setVisible(false);
        speedhackBtn->setVisible(false);

        return;
    }

    speedhackBtn->setVisible(!g.mod->getSavedValue<bool>("macro_hide_speedhack"));

    if (g.mod->getSavedValue<bool>("macro_hide_stepper")) {
        disableStepperBtn->setVisible(false);
        stepFrameBtn->setVisible(false);
    }
    else {
        stepFrameBtn->setVisible(true);
        disableStepperBtn->setVisible(g.frameStepper);
    }
}
