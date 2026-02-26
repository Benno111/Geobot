#pragma once

#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include "../includes.hpp"

#include "load_macro_layer.hpp"
#include "render_settings_layer.hpp"
#include "save_macro_layer.hpp"
#include "macro_info_layer.hpp"

enum InputType {
	None,
	Settings,
	Action,
	Autosave,
	Speedhack,
	Seed,
	Respawn,
	Tps,
	FrameOffset,
	FrameFixesLimit,
	Accuracy
};

struct RecordSetting {
	std::string name;
	std::string id;
	InputType input;
	float labelScale = 0.325f;
	cocos2d::SEL_MenuHandler callback = nullptr;
	bool disabled = false;
};

class RecordLayer : public xdb::Popup<>, public TextInputDelegate {
public:
	CCMenuItemToggler* recording = nullptr;
	CCMenuItemToggler* playing = nullptr;
	CCMenuItemToggler* speedhackToggle = nullptr;
	CCMenuItemToggler* trajectoryToggle = nullptr;
	CCMenuItemToggler* noclipToggle = nullptr;
	CCMenuItemToggler* frameStepperToggle = nullptr;
	CCMenuItemToggler* renderToggle = nullptr;
	CCMenuItemToggler* tpsToggle = nullptr;

	CCLabelBMFont* actionsLabel = nullptr;
	CCLabelBMFont* fpsLabel = nullptr;
	CCLabelBMFont* warningLabel = nullptr;

	CCSprite* warningSprite = nullptr;
	CCScale9Sprite* tpsBg = nullptr;

	CCMenuItemSpriteExtra* FPSLeft = nullptr;
	CCMenuItemSpriteExtra* FPSRight = nullptr;

	CCTextInputNode* widthInput = nullptr;
	CCTextInputNode* heightInput = nullptr;
	CCTextInputNode* bitrateInput = nullptr;
	CCTextInputNode* fpsInput = nullptr;
	CCTextInputNode* codecInput = nullptr;
	CCTextInputNode* seedInput = nullptr;
	CCTextInputNode* speedhackInput = nullptr;
	CCTextInputNode* respawnInput = nullptr;
	CCTextInputNode* tpsInput = nullptr;
	CCTextInputNode* frameOffsetInput = nullptr;
	CCTextInputNode* frameFixesLimitInput = nullptr;
	geode::ScrollLayer* settingsScroll = nullptr;
	geode::Scrollbar* settingsScrollbar = nullptr;
	CCMenu* settingsMenu = nullptr;

	std::vector<CCNode*> nodes;

	CCMenu* menu = nullptr;

	Mod* mod = nullptr;

	bool cursorWasHidden = false;

protected:

	bool setup() override;

	~RecordLayer() override {
		cocos2d::CCTouchDispatcher::get()->unregisterForcePrio(this);
	    Global::get().layer = nullptr;
	}

public:

	static std::string getTPSString();
	
	STATIC_CREATE(RecordLayer, 455, 271)
	
	virtual void onClose(cocos2d::CCObject*) override;

	void textChanged(CCTextInputNode* node) override;

	void checkSpeedhack();

	static RecordLayer* openMenu(bool instant = false);

	void openMenu2(CCObject*) {
		RecordLayer::openMenu();
	}

	void moreSettings(CCObject*) {
		geode::openSettingsPopup(mod, false);
	}

	void openLoadMacro(CCObject*);
	void openStarRateOverride(CCObject*);

	void openSaveMacro(CCObject*);
	void clear22Percentage(CCObject*);

	void showCodecPopup(CCObject*);

	void toggleRecording(CCObject*);

	void togglePlaying(CCObject*);

	void toggleRender(CCObject* btn);

	void openPresets(CCObject*);

	void onAutosaves(CCObject*);
	void openMacrosFolder(CCObject*);
	void openAutosavesFolder(CCObject*);
	void openRendersFolder(CCObject*);

	void loadSettingsList();

	void loadSetting(RecordSetting sett, float yPos, CCMenu* targetMenu);

	void setToggleMember(CCMenuItemToggler* toggle, std::string id);

	void onEditMacro(CCObject*);

	void macroInfo(CCObject*);

	void toggleSetting(CCObject* obj);

	void openKeybinds(CCObject*);

	void toggleFPS(bool on);

	void onDiscord(CCObject*);
	void onCycleAccuracy(CCObject*);

	void updateTPS();

	void showKeybindsWarning();

};
