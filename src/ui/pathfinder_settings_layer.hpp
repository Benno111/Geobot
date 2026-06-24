#pragma once

#include "record_layer.hpp"
#include "../macro.hpp"
#include <Geode/binding/LevelEditorLayer.hpp>
#include <fstream>

class PathfinderSettingsLayer : public xdb::Popup<> {
public:
    STATIC_CREATE(PathfinderSettingsLayer, 280, 200)

    void open(CCObject*) {
        create()->show();
    }

    ~PathfinderSettingsLayer() override = default;

private:
    CCMenuItemToggler* modeToggle = nullptr;
    CCLabelBMFont* statusLabel = nullptr;
    CCLabelBMFont* solverLabel = nullptr;
    CCMenuItemSpriteExtra* runButton = nullptr;

    bool setup() override {
        setTitle("Pathfinder");
        adjustForLoadingScreen();
        Utils::setBackgroundColor(m_bgSprite);
        schedule(schedule_selector(PathfinderSettingsLayer::tickState));

        auto& g = Global::get();
        bool featureEnabled = Global::isPathfinderFeatureEnabled();

        CCLabelBMFont* lbl = CCLabelBMFont::create("Enable Pathfinder", "bigFont.fnt");
        lbl->setPosition({ m_size.width / 2.f - 18.f, 149.f });
        lbl->setScale(0.38f);
        m_mainLayer->addChild(lbl);

        modeToggle = CCMenuItemToggler::create(
            CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png"),
            CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"),
            this,
            menu_selector(PathfinderSettingsLayer::onToggle)
        );
        modeToggle->setPosition({ m_size.width / 2.f + 72.f, 149.f });
        modeToggle->setScale(0.5f);
        modeToggle->setID("pathfinder-popup-toggle"_spr);
        modeToggle->toggle(featureEnabled && g.pathfinderMode);
        modeToggle->setEnabled(featureEnabled);
        modeToggle->setOpacity(featureEnabled ? 255 : 110);
        m_buttonMenu->addChild(modeToggle);

        lbl = CCLabelBMFont::create("Current Status", "goldFont.fnt");
        lbl->setPosition({ m_size.width / 2.f, 119.f });
        lbl->setScale(0.46f);
        m_mainLayer->addChild(lbl);

        statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
        statusLabel->setPosition({ m_size.width / 2.f, 102.f });
        statusLabel->setScale(0.72f);
        statusLabel->setOpacity(190);
        m_mainLayer->addChild(statusLabel);

        lbl = CCLabelBMFont::create("Internal search status", "goldFont.fnt");
        lbl->setPosition({ m_size.width / 2.f, 79.f });
        lbl->setScale(0.4f);
        m_mainLayer->addChild(lbl);

        solverLabel = CCLabelBMFont::create("", "chatFont.fnt");
        solverLabel->setPosition({ m_size.width / 2.f, 61.f });
        solverLabel->setScale(0.58f);
        solverLabel->setOpacity(170);
        m_mainLayer->addChild(solverLabel);

        lbl = CCLabelBMFont::create(featureEnabled ? "Uses the live PlayLayer simulation to keep testing" : "Enable the feature flag in mod settings", "chatFont.fnt");
        lbl->setPosition({ m_size.width / 2.f, 38.f });
        lbl->setScale(0.52f);
        lbl->setOpacity(120);
        m_mainLayer->addChild(lbl);

        lbl = CCLabelBMFont::create(featureEnabled ? "candidate macros until it finds a surviving route." : "to use pathfinder controls here.", "chatFont.fnt");
        lbl->setPosition({ m_size.width / 2.f, 25.f });
        lbl->setScale(0.52f);
        lbl->setOpacity(120);
        m_mainLayer->addChild(lbl);

        ButtonSprite* runSpr = ButtonSprite::create("Run Search");
        runSpr->setScale(0.68f);
        runButton = CCMenuItemSpriteExtra::create(runSpr, this, menu_selector(PathfinderSettingsLayer::onRunSolver));
        runButton->setPosition({ m_size.width / 2.f - 58.f, 10.f });
        m_buttonMenu->addChild(runButton);

        ButtonSprite* closeSpr = ButtonSprite::create("Close");
        closeSpr->setScale(0.68f);
        CCMenuItemSpriteExtra* closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(PathfinderSettingsLayer::onClose));
        closeBtn->setPosition({ m_size.width / 2.f + 54.f, 10.f });
        m_buttonMenu->addChild(closeBtn);

        updateStateUI();
        return true;
    }

    void onToggle(CCObject*) {
        bool enabled = modeToggle ? !modeToggle->isToggled() : false;
        RecordLayer::applyPathfinderState(enabled);
        updateStateUI();
    }

    void onRunSolver(CCObject*) {
        if (!Global::isPathfinderFeatureEnabled()) {
            FLAlertLayer::create("Pathfinder", "Enable the <cy>Pathfinder</c> feature flag in mod settings first.", "Ok")->show();
            return;
        }

        PlayLayer* pl = PlayLayer::get();
        if (!pl) {
            FLAlertLayer::create("Pathfinder", "Start a <cy>level</c> first. Internal search needs a live PlayLayer simulation.", "Ok")->show();
            return;
        }

        if (pl->m_levelSettings->m_platformerMode || pl->m_levelSettings->m_twoPlayerMode) {
            FLAlertLayer::create("Pathfinder", "The internal search currently supports <cy>single-player jump pathing</c> only.", "Ok")->show();
            return;
        }

        if (Global::isPathfinderAutoSearchActive()) {
            Global::stopPathfinderAutoSearch(true);
            Global::resetPathfinderState();
            Notification::create("Internal pathfinder search stopped.", NotificationIcon::Warning)->show();
            updateStateUI();
            return;
        }

        if (!Global::startPathfinderAutoSearch()) {
            FLAlertLayer::create("Pathfinder", "Could not start the internal search from the current game state.", "Ok")->show();
            return;
        }
        Notification::create("Internal pathfinder search started.", NotificationIcon::Success)->show();
        updateStateUI();
    }

    void tickState(float) {
        updateStateUI();
    }

    void updateStateUI() {
        auto& g = Global::get();
        if (modeToggle)
            modeToggle->toggle(g.pathfinderMode);
        if (statusLabel) {
            statusLabel->setString(("Pathfinder: " + g.pathfinderStatus).c_str());
            statusLabel->limitLabelWidth(210.f, 0.72f, 0.1f);
            statusLabel->updateLabel();
        }
        if (solverLabel) {
            solverLabel->setString(Global::isPathfinderAutoSearchActive() ? "Search: running" : "Search: idle");
            solverLabel->limitLabelWidth(235.f, 0.58f, 0.1f);
            solverLabel->updateLabel();
        }
    }
};
