#pragma once

#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include "../includes.hpp"

#include "load_macro_layer.hpp"
#include "save_macro_layer.hpp"
#include "macro_info_layer.hpp"

enum InputType {
	None,
	Settings,
	Action,
	Tps,
	FrameOffset,
	FrameFixesLimit,
	Accuracy,
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
	CCMenuItemToggler* tpsToggle = nullptr;

	CCLabelBMFont* actionsLabel = nullptr;
	CCLabelBMFont* warningLabel = nullptr;

	CCSprite* warningSprite = nullptr;
	CCScale9Sprite* tpsBg = nullptr;

	CCMenuItemSpriteExtra* FPSLeft = nullptr;
	CCMenuItemSpriteExtra* FPSRight = nullptr;

	CCTextInputNode* tpsInput = nullptr;
	CCTextInputNode* frameOffsetInput = nullptr;
	CCTextInputNode* frameFixesLimitInput = nullptr;
	geode::ScrollLayer* settingsScroll = nullptr;
	geode::Scrollbar* settingsScrollbar = nullptr;
	CCMenu* settingsMenu = nullptr;
	CCLabelBMFont* settingsSectionLabel = nullptr;
	std::vector<CCMenuItemSpriteExtra*> settingsCategoryButtons;

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
	
	static RecordLayer* create();
	
	virtual void onClose(cocos2d::CCObject*) override;

	void textChanged(CCTextInputNode* node) override;


	static RecordLayer* openMenu(bool instant = false);

	void openMenu2(CCObject*) {
		RecordLayer::openMenu();
	}

	void moreSettings(CCObject*) {
		geode::openSettingsPopup(mod, false);
	}

	void openLoadMacro(CCObject*);

	void openSaveMacro(CCObject*);


	void toggleRecording(CCObject*);

	void togglePlaying(CCObject*);



	void onAutosaves(CCObject*);
	void openMacrosFolder(CCObject*);
	void openAutosavesFolder(CCObject*);

	void loadSettingsList();

	void loadSetting(RecordSetting sett, float yPos, CCMenu* targetMenu);

	void setToggleMember(CCMenuItemToggler* toggle, std::string id);

	void onEditMacro(CCObject*);

	void macroInfo(CCObject*);

	void toggleSetting(CCObject* obj);

	void openKeybinds(CCObject*);


	void onDiscord(CCObject*);
	void onCycleAccuracy(CCObject*);
	void onSelectSettingsCategory(CCObject*);
	void updateSettingsCategoryButtons();
	void selectSettingsCategory(size_t index);

	void updateTPS();

	void showKeybindsWarning();

};
