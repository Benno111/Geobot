#pragma once

#include "../includes.hpp"
#include <Geode/binding/LevelEditorLayer.hpp>
#include <algorithm>

class StarRateOverrideLayer : public xdb::Popup<>, public TextInputDelegate {
private:
    CCTextInputNode* starsInput = nullptr;

    static GJGameLevel* getCurrentLevel() {
        if (PlayLayer* pl = PlayLayer::get())
            return pl->m_level;

        if (LevelEditorLayer* lel = LevelEditorLayer::get())
            return lel->m_level;

        return nullptr;
    }

    bool setup() override {
        setTitle("Star Rate Override");

        cocos2d::CCPoint offset = (CCDirector::sharedDirector()->getWinSize() - m_mainLayer->getContentSize()) / 2;
        m_mainLayer->setPosition(m_mainLayer->getPosition() - offset);
        m_closeBtn->setPosition(m_closeBtn->getPosition() + offset);
        m_bgSprite->setPosition(m_bgSprite->getPosition() + offset);
        m_title->setPosition(m_title->getPosition() + offset);

        Utils::setBackgroundColor(m_bgSprite);

        CCMenu* menu = CCMenu::create();
        m_mainLayer->addChild(menu);

        CCLabelBMFont* lbl = CCLabelBMFont::create("Stars (0-10):", "bigFont.fnt");
        lbl->setScale(0.35f);
        lbl->setAnchorPoint({ 0.f, 0.5f });
        lbl->setPosition({ 18.f, 32.f });
        menu->addChild(lbl);

        CCScale9Sprite* bg = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
        bg->setColor({ 0, 0, 0 });
        bg->setOpacity(80);
        bg->setScale(0.45f);
        bg->setAnchorPoint({ 0.f, 1.f });
        bg->setPosition({ 95.f, 45.f });
        bg->setContentSize({ 130.f, 55.f });
        menu->addChild(bg);

        starsInput = CCTextInputNode::create(120, 30, "0", "chatFont.fnt");
        starsInput->setPosition({ 129.f, 31.f });
        starsInput->setContentSize({ 45.f, 20.f });
        starsInput->setAllowedChars("0123456789");
        starsInput->setMaxLabelLength(2);
        starsInput->setMaxLabelScale(0.8f);
        starsInput->setDelegate(this);
        starsInput->setTouchEnabled(true);
        starsInput->setMouseEnabled(true);
        menu->addChild(starsInput);

        int startStars = 0;
        if (GJGameLevel* level = getCurrentLevel())
            startStars = level->m_rateStars != 0 ? static_cast<int>(level->m_rateStars) : static_cast<int>(level->m_stars);
        starsInput->setString(std::to_string(startStars).c_str());

        ButtonSprite* spr = ButtonSprite::create("Apply");
        spr->setScale(0.65f);
        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(StarRateOverrideLayer::onApply));
        btn->setPosition({ -45.f, -44.f });
        menu->addChild(btn);

        spr = ButtonSprite::create("Cancel");
        spr->setScale(0.65f);
        btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(StarRateOverrideLayer::onClose));
        btn->setPosition({ 45.f, -44.f });
        menu->addChild(btn);

        return true;
    }

public:
    STATIC_CREATE(StarRateOverrideLayer, 260, 150)

    void textChanged(CCTextInputNode*) override {}

    void onApply(CCObject*) {
        GJGameLevel* level = getCurrentLevel();
        if (!level)
            return FLAlertLayer::create("Star Rate Override", "Open a <cl>level</c> first.", "Ok")->show();

        int stars = geode::utils::numFromString<int>(starsInput ? starsInput->getString() : "").unwrapOr(0);
        stars = std::clamp(stars, 0, 10);

        level->m_rateStars = stars;
        level->m_stars = stars;

        if (GameLevelManager* glm = GameLevelManager::sharedState()) {
            glm->updateLevel(level);
            glm->saveLevel(level);
        }

        Notification::create("Star rate override applied", NotificationIcon::Success)->show();
        onClose(nullptr);
    }
};
