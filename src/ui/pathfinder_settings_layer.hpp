#pragma once

#include "record_layer.hpp"

class PathfinderSettingsLayer : public xdb::Popup<> {
public:
    STATIC_CREATE(PathfinderSettingsLayer, 250, 160)

    void open(CCObject*) {
        create()->show();
    }

private:
    CCMenuItemToggler* modeToggle = nullptr;
    CCLabelBMFont* statusLabel = nullptr;

    bool setup() override {
        setTitle("Pathfinder");
        adjustForLoadingScreen();
        Utils::setBackgroundColor(m_bgSprite);

        auto& g = Global::get();
        bool featureEnabled = Global::isPathfinderFeatureEnabled();

        CCLabelBMFont* lbl = CCLabelBMFont::create("Enable Pathfinder", "bigFont.fnt");
        lbl->setPosition({ m_size.width / 2.f - 12.f, 113.f });
        lbl->setScale(0.38f);
        m_mainLayer->addChild(lbl);

        modeToggle = CCMenuItemToggler::create(
            CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png"),
            CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"),
            this,
            menu_selector(PathfinderSettingsLayer::onToggle)
        );
        modeToggle->setPosition({ m_size.width / 2.f + 68.f, 113.f });
        modeToggle->setScale(0.5f);
        modeToggle->setID("pathfinder-popup-toggle"_spr);
        modeToggle->toggle(featureEnabled && g.pathfinderMode);
        modeToggle->setEnabled(featureEnabled);
        modeToggle->setOpacity(featureEnabled ? 255 : 110);
        m_buttonMenu->addChild(modeToggle);

        lbl = CCLabelBMFont::create("Current Status", "goldFont.fnt");
        lbl->setPosition({ m_size.width / 2.f, 83.f });
        lbl->setScale(0.46f);
        m_mainLayer->addChild(lbl);

        statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
        statusLabel->setPosition({ m_size.width / 2.f, 66.f });
        statusLabel->setScale(0.72f);
        statusLabel->setOpacity(190);
        m_mainLayer->addChild(statusLabel);

        lbl = CCLabelBMFont::create(featureEnabled ? "Use this to arm or disable" : "Enable the feature flag in mod settings", "chatFont.fnt");
        lbl->setPosition({ m_size.width / 2.f, 43.f });
        lbl->setScale(0.58f);
        lbl->setOpacity(120);
        m_mainLayer->addChild(lbl);

        lbl = CCLabelBMFont::create(featureEnabled ? "the pathfinder from settings." : "to use pathfinder controls here.", "chatFont.fnt");
        lbl->setPosition({ m_size.width / 2.f, 29.f });
        lbl->setScale(0.58f);
        lbl->setOpacity(120);
        m_mainLayer->addChild(lbl);

        ButtonSprite* btnSpr = ButtonSprite::create("Ok");
        btnSpr->setScale(0.72f);
        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(btnSpr, this, menu_selector(PathfinderSettingsLayer::onClose));
        btn->setPosition({ m_size.width / 2.f, 14.f });
        m_buttonMenu->addChild(btn);

        updateStateUI();
        return true;
    }

    void onToggle(CCObject*) {
        bool enabled = modeToggle ? !modeToggle->isToggled() : false;
        RecordLayer::applyPathfinderState(enabled);
        updateStateUI();
    }

    void updateStateUI() {
        auto& g = Global::get();
        if (modeToggle)
            modeToggle->toggle(g.pathfinderMode);
        if (statusLabel) {
            statusLabel->setString(("Pathfinder: " + g.pathfinderStatus).c_str());
            statusLabel->limitLabelWidth(180.f, 0.72f, 0.1f);
            statusLabel->updateLabel();
        }
    }
};
