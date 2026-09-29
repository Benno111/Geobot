#include "load_macro_layer.hpp"
#include "macro_editor.hpp"

#include <Geode/modify/CCMenu.hpp>
#include <sstream>
#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

namespace {
	CCSprite* createFavoriteSprite(bool active);

	constexpr size_t kMacroListBatchSize = 12;
	constexpr float kMacroRowHeight = 35.f;
	constexpr float kMacroListWidth = 323.f;
	constexpr float kMacroListHeight = 180.f;
	constexpr char const* FAVORITE_MACROS_KEY = "favorite_macros";

	bool isMacroFile(std::filesystem::path const& path) {
		auto ext = path.extension();
		return ext == ".gdr" || ext == ".xd" || ext == ".json";
	}

	std::string macroDisplayName(std::filesystem::path const& path) {
		std::string filename = path.filename().string();
		std::string name = filename.substr(0, filename.find_last_of('.'));
		if (path.extension() == ".json")
			name = name.substr(0, name.find_last_of('.'));
		return name;
	}

	std::string macroPathID(std::filesystem::path const& path) {
		return path.lexically_normal().generic_string();
	}

	bool favoriteListContains(std::string const& favoritesRaw, std::filesystem::path const& path) {
		std::istringstream favorites(favoritesRaw);
		std::string favorite;
		std::string id = macroPathID(path);
		while (std::getline(favorites, favorite)) {
			if (favorite == id)
				return true;
		}
		return false;
	}
}

class $modify(CCMenu) {
	virtual bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
		CCScene* scene = CCDirector::sharedDirector()->getRunningScene();
		LoadMacroLayer* layer = scene->getChildByType<LoadMacroLayer>(0);

        if (!layer) return CCMenu::ccTouchBegan(touch, event);

        cocos2d::CCPoint pos = touch->getLocation();
		float yCenter = CCDirector::sharedDirector()->getWinSize().height / 2.f;

		if (pos.y > yCenter - 100) return CCMenu::ccTouchBegan(touch, event);

		for (MacroCell* cell : layer->allMacros) {
			if (cell->menu == this)
			 	return false;
		}

		return CCMenu::ccTouchBegan(touch, event);
	}
};

void LoadMacroLayer::open(geode::Popup* layer, geode::Popup* layer2, bool autosaves) {
	std::filesystem::path path = Global::getFolderSettingPath("macros_folder");
	if (!std::filesystem::exists(path))
		return FLAlertLayer::create("Error", "There was an error getting the folder. ID: 6", "Ok")->show();

	path = Global::getFolderSettingPath("autosaves_folder");
	if (!std::filesystem::exists(path))
		return FLAlertLayer::create("Error", "There was an error getting the folder. ID: 61", "Ok")->show();

	LoadMacroLayer* layerReal = create(layer, layer2, autosaves);
	layerReal->m_noElasticity = true;
	layerReal->show();
}

void LoadMacroLayer::textChanged(CCTextInputNode* node) {
	search = Utils::toLower(node->getString());
	if (search != "") {
		searchOff->setVisible(true);
		searchOff->setOpacity(184);
	}
	else
		searchOff->setVisible(false);

	reloadList(0);
}

void LoadMacroLayer::reloadList(int amount) {
	CCNode* listLayer = m_buttonMenu->getChildByID("list-layer");
	if (!listLayer) {
		addList();
		return;
	}

	int childrenCount = 0;
	float posY = 0.f;
	if (macroScroll && macroScroll->m_contentLayer) {
		childrenCount = static_cast<int>(allMacros.size());
		posY = macroScroll->m_contentLayer->getPositionY();
	}

	listLayer->removeFromParentAndCleanup(true);
	if (CCNode* bg = m_buttonMenu->getChildByID("background"))
		bg->removeFromParentAndCleanup(true);

	selectedMacros.clear();
	allMacros.clear();

	if (!isMerge)
		selectAllToggle->toggle(false);

	addList(childrenCount > 7 && amount != 0, posY + (35.f * amount));
}

void LoadMacroLayer::showLoadingScreen() {
	if (!loadingOverlay) {
		CCSize layerSize = m_mainLayer->getContentSize();

		CCNode* dim = CCNode::create();
		dim->setContentSize(layerSize);
		dim->setAnchorPoint({ 0.f, 0.f });
		dim->setPosition({ 0, 0 });

		loadingLabel = CCLabelBMFont::create(isAutosaves ? "Loading Autosaves..." : "Loading Macros...", "bigFont.fnt");
		loadingLabel->setScale(0.32f);
		loadingLabel->setOpacity(150);
		loadingLabel->setAnchorPoint({ 1.f, 0.5f });
		loadingLabel->setPosition({ layerSize.width - 18.f, 33.f });
		dim->addChild(loadingLabel);

		loadingOverlay = dim;
		loadingOverlay->setID("loading-overlay");
		m_mainLayer->addChild(loadingOverlay, 300);
	}

	if (loadingLabel)
		loadingLabel->setString((isAutosaves ? "Loading Autosaves..." : "Loading Macros..."));

	if (loadingOverlay)
		loadingOverlay->setVisible(true);
}

void LoadMacroLayer::hideLoadingScreen() {
	if (loadingOverlay)
		loadingOverlay->setVisible(false);
}

void LoadMacroLayer::deleteSelected(CCObject*) {
	int amount = selectedMacros.size();
	if (amount < 1) return;

	geode::createQuickPopup(
		"Warning",
		"Are you sure you want to <cr>delete</c> <cy>" + std::to_string(amount) + "</c> " + (isAutosaves ? "autosave" : "macro") + "(s)?",
		"Cancel", "Yes",
		[this, amount](auto, bool btn2) {
			if (btn2) {
				for (size_t i = 0; i < this->selectedMacros.size(); i++)
					this->selectedMacros[i]->deleteMacro(false);

				this->reloadList(amount);
				Notification::create("Macros Deleted", NotificationIcon::Success)->show();
			}
		}
	);

}

void LoadMacroLayer::onSelectAll(CCObject* obj) {
	bool on = !static_cast<CCMenuItemToggler*>(obj)->isToggled();

	for (size_t i = 0; i < allMacros.size(); i++) {
		CCMenuItemToggler* toggle = allMacros[i]->toggler;
		if (toggle->isToggled() == on) continue;

		toggle->toggle(on);
		allMacros[i]->selectMacro(false);
	}
}

LoadMacroLayer* LoadMacroLayer::create(geode::Popup* layer, geode::Popup* layer2, bool autosaves) {
	LoadMacroLayer* ret = new LoadMacroLayer();
	std::string texture = Utils::getTexture();
	if (ret->initAnchored(385, 291, layer, layer2, autosaves, texture.c_str())) {
		ret->autorelease();
		return ret;
	}

	delete ret;
	return nullptr;
}

void LoadMacroLayer::onImportMacro(CCObject*) {
	FLAlertLayer::create(
		"Notice",
		"Import via file picker is temporarily disabled on this Geode v5 migration branch.",
		"Ok"
	)->show();
}

bool LoadMacroLayer::setup(geode::Popup* layer, geode::Popup* layer2, bool autosaves) {

	#ifdef GEODE_IS_ANDROID
	invertSort = true;
	#endif

	menu = CCMenu::create();
	menu->setZOrder(110);
	m_mainLayer->addChild(menu);

	Utils::setBackgroundColor(m_bgSprite);

	menuLayer = layer;
	mergeLayer = layer2;
	isAutosaves = autosaves;
	isMerge = mergeLayer != nullptr;

	setTitle(isMerge ? "Merge Macro" : "Load Macro");
	m_title->setPositionY(m_title->getPositionY() + 5);
	m_closeBtn->getNormalImage()->setScale(0.6f);
	//adjustForLoadingScreen(); //fixes bugs

	if (!isMerge) {
		CCSprite* icon = CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
		icon->setScale(0.585f);
		CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(
			icon,
			this,
			menu_selector(LoadMacroLayer::onImportMacro)
		);
		btn->setPosition(ccp(165, -121));

		menu->addChild(btn);

		searchInput = TextInput::create(235, "Search Macro", "bigFont.fnt");
		searchInput->setPositionY(100);
		searchInput->setDelegate(this);
		menu->addChild(searchInput);

		CCSprite* emptyBtn = CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
		emptyBtn->setScale(0.585f);
		CCSprite* folderIcon = CCSprite::createWithSpriteFrameName("folderIcon_001.png");
		folderIcon->setPosition(emptyBtn->getContentSize() / 2);
		folderIcon->setScale(0.7f);
		emptyBtn->addChild(folderIcon);
		btn = CCMenuItemSpriteExtra::create(
			emptyBtn,
			this,
			menu_selector(LoadMacroLayer::openFolder)
		);
		btn->setPosition(ccp(115, -121));

		menu->addChild(btn);

		CCSprite* spr = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");
		spr->setScale(0.585f);
		btn = CCMenuItemSpriteExtra::create(
			spr,
			this,
			menu_selector(LoadMacroLayer::deleteSelected)
		);
		btn->setPosition(ccp(65, -121));

		menu->addChild(btn);


	}

	CCSprite* spr1 = CCSprite::create("GJ_button_01.png");
	CCSprite* spr2 = CCSprite::createWithSpriteFrameName("GJ_sortIcon_001.png");
	spr2->setPosition({20, 20});
	spr1->addChild(spr2);

	CCSprite* spr3 = CCSprite::create("GJ_button_02.png");
	CCSprite* spr4 = CCSprite::createWithSpriteFrameName("GJ_sortIcon_001.png");
	spr4->setPosition({20, 20});
	spr3->addChild(spr4);

	sortToggle = CCMenuItemToggler::create(spr1, spr3, this, menu_selector(LoadMacroLayer::updateSort));
	sortToggle->setPosition({-145, 100});
	sortToggle->setScale(0.55f);
	sortToggle->toggle(false);
	menu->addChild(sortToggle);

	CCSprite* favoriteOff = createFavoriteSprite(false);
	CCSprite* favoriteOn = createFavoriteSprite(true);
	favoritesToggle = CCMenuItemToggler::create(
		favoriteOff,
		favoriteOn,
		this,
		menu_selector(LoadMacroLayer::updateFavoritesFilter)
	);
	favoritesToggle->setPosition({ -172, 70 });
	favoritesToggle->setScale(0.48f);
	favoritesToggle->setID("favorites-filter-toggle");
	menu->addChild(favoritesToggle);

	CCLabelBMFont* favoritesLabel = CCLabelBMFont::create("Favorites", "bigFont.fnt");
	favoritesLabel->setAnchorPoint({ 0.f, 0.5f });
	favoritesLabel->setPosition({ -157.f, 70.f });
	favoritesLabel->setScale(0.32f);
	favoritesLabel->setID("favorites-filter-label");
	menu->addChild(favoritesLabel);

	CCSprite* spriteOn = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
	CCSprite* spriteOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");

	selectAllToggle = CCMenuItemToggler::create(spriteOff, spriteOn, this, menu_selector(LoadMacroLayer::onSelectAll));
	selectAllToggle->setScale(0.585f);
	selectAllToggle->setPosition({ -165, -121 });

	if (!isMerge)
		menu->addChild(selectAllToggle);

	CCLabelBMFont* lbl = CCLabelBMFont::create("Select all", "bigFont.fnt");
	lbl->setScale(0.4f);
	lbl->setPosition({ -110, -121 });

	if (!isMerge)
		menu->addChild(lbl);

	CCSprite* spr = CCSprite::createWithSpriteFrameName("gj_findBtnOff_001.png");
	spr->setScale(0.685f);
	searchOff = CCMenuItemSpriteExtra::create(
		spr,
		this,
		menu_selector(LoadMacroLayer::clearSearch)
	);
	searchOff->setPosition(ccp(137, 100));
	searchOff->setVisible(false);
	menu->addChild(searchOff);

	macroCountLbl = CCLabelBMFont::create("13 Macros", "chatFont.fnt");
	macroCountLbl->setOpacity(108);
	macroCountLbl->setScale(0.55f);
	macroCountLbl->setAnchorPoint({1.f, 0.5f});
	macroCountLbl->setPosition({180, 130});
	menu->addChild(macroCountLbl);

	if (isMerge) {
		p1Toggle = CCMenuItemToggler::create(spriteOff, spriteOn, this, nullptr);
		p1Toggle->setID("p1-toggle");
		p1Toggle->setScale(0.675f);
		p1Toggle->setPosition({ -23, -121 });
		menu->addChild(p1Toggle);

		p2Toggle = CCMenuItemToggler::create(spriteOff, spriteOn, this, nullptr);
		p2Toggle->setID("p2-toggle");
		p2Toggle->setScale(0.675f);
		p2Toggle->setPosition({ 98, -121 });
		menu->addChild(p2Toggle);

		owToggle = CCMenuItemToggler::create(spriteOff, spriteOn, this, nullptr);
		owToggle->setID("ow-toggle");
		owToggle->setScale(0.675f);
		owToggle->setPosition({ -166, -121 });
		owToggle->toggle(true);
		menu->addChild(owToggle);

		lbl = CCLabelBMFont::create("Overwrite", "bigFont.fnt");
		lbl->setPosition({ -111, -121 });
		lbl->setScale(0.44f);
		menu->addChild(lbl);

		lbl = CCLabelBMFont::create("P1 only", "bigFont.fnt");
		lbl->setPosition({ 21, -121 });
		lbl->setScale(0.44f);
		menu->addChild(lbl);

		lbl = CCLabelBMFont::create("P2 only", "bigFont.fnt");
		lbl->setPosition({ 144, -121 });
		lbl->setScale(0.44f);
		menu->addChild(lbl);
	}

	addList();
	
	return true;
}

void LoadMacroLayer::clearSearch(CCObject*) {
	searchOff->setVisible(false);
	searchInput->setString("");
	search = "";

	reloadList(0);
}

void LoadMacroLayer::updateSort(CCObject*) {
	if (!sortToggle) return;

	invertSort = !sortToggle->isToggled();

	#ifdef GEODE_IS_ANDROID
	invertSort = !invertSort;
	#endif

	reloadList(0);
}

namespace {
	CCSprite* createFavoriteSprite(bool active) {
		CCSprite* sprite = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
		sprite->setColor(active ? ccc3(255, 220, 70) : ccc3(120, 120, 120));
		sprite->setOpacity(active ? 255 : 170);
		return sprite;
	}
}

bool LoadMacroLayer::isFavorite(std::filesystem::path const& path) const {
	std::istringstream favorites(Mod::get()->getSavedValue<std::string>(FAVORITE_MACROS_KEY));
	std::string favorite;
	std::string id = macroPathID(path);
	while (std::getline(favorites, favorite)) {
		if (favorite == id)
			return true;
	}
	return false;
}

void LoadMacroLayer::setFavorite(std::filesystem::path const& path, bool favorite) {
	std::istringstream saved(Mod::get()->getSavedValue<std::string>(FAVORITE_MACROS_KEY));
	std::vector<std::string> favorites;
	std::string entry;
	std::string id = macroPathID(path);
	while (std::getline(saved, entry)) {
		if (!entry.empty() && entry != id)
			favorites.push_back(entry);
	}
	if (favorite)
		favorites.push_back(id);

	std::string serialized;
	for (auto const& value : favorites)
		serialized += value + "\n";
	Mod::get()->setSavedValue(FAVORITE_MACROS_KEY, serialized);
}

void LoadMacroLayer::updateFavoritesFilter(CCObject*) {
	favoritesOnly = !favoritesToggle->isToggled();
	reloadList(0);
}

void LoadMacroLayer::addList(bool refresh, float prevScroll) {
	startBackgroundListLoad(refresh, prevScroll);
}

void LoadMacroLayer::performQueuedListLoad() {
	drainPendingListEntries();
}

void LoadMacroLayer::populateList(bool refresh, float prevScroll) {
	rebuildListFromLoaded(refresh, prevScroll);
}

void LoadMacroLayer::cancelBackgroundListLoad() {
	if (listLoadCancel)
		listLoadCancel->store(true);
	listLoadCancel.reset();
	listLoadInProgress = false;

	std::lock_guard<std::mutex> lock(listLoadMutex);
	pendingMacroEntries.clear();
}

void LoadMacroLayer::clearListNodes() {
	if (CCNode* scrollbar = m_buttonMenu->getChildByID("scrollbar"))
		scrollbar->removeFromParentAndCleanup(true);

	if (CCNode* lbl = menu->getChildByID("no-macros-label"))
		lbl->removeFromParentAndCleanup(true);

	if (CCNode* listLayer = m_buttonMenu->getChildByID("list-layer"))
		listLayer->removeFromParentAndCleanup(true);

	if (CCNode* bg = m_buttonMenu->getChildByID("background"))
		bg->removeFromParentAndCleanup(true);

	macroScroll = nullptr;
	macroScrollbar = nullptr;
	macroListMenu = nullptr;
}

void LoadMacroLayer::startBackgroundListLoad(bool refresh, float prevScroll) {
	cancelBackgroundListLoad();
	clearListNodes();

	queuedRefresh = refresh;
	queuedScroll = prevScroll;
	loadedMacroEntries.clear();
	selectedMacros.clear();
	allMacros.clear();
	if (!isMerge && selectAllToggle)
		selectAllToggle->toggle(false);

	listLoadInProgress = true;
	showLoadingScreen();
	rebuildListFromLoaded(false, 0.f);

	auto cancel = std::make_shared<std::atomic_bool>(false);
	listLoadCancel = cancel;
	int generation = ++listLoadGeneration;

	std::filesystem::path folder = Global::getFolderSettingPath(isAutosaves ? "autosaves_folder" : "macros_folder");
	std::string searchSnapshot = search;
	bool favoritesOnlySnapshot = favoritesOnly;
	bool invertSortSnapshot = invertSort;
	std::string favoritesSnapshot = Mod::get()->getSavedValue<std::string>(FAVORITE_MACROS_KEY);

	retain();
	std::thread([this, cancel, generation, folder, searchSnapshot, favoritesOnlySnapshot, invertSortSnapshot, favoritesSnapshot] {
		std::vector<std::filesystem::path> paths;
		std::error_code ec;
		std::filesystem::directory_iterator it(folder, ec);
		std::filesystem::directory_iterator end;
		while (!ec && it != end) {
			if (cancel->load())
				break;
			auto const& entry = *it;
			if (!entry.is_regular_file(ec))
				ec.clear();
			else
				paths.push_back(entry.path());
			it.increment(ec);
		}

		if (invertSortSnapshot)
			std::reverse(paths.begin(), paths.end());

		std::vector<MacroListEntry> batch;
		batch.reserve(kMacroListBatchSize);

		auto flushBatch = [&] {
			if (batch.empty())
				return;

			{
				std::lock_guard<std::mutex> lock(listLoadMutex);
				pendingMacroEntries.insert(pendingMacroEntries.end(), batch.begin(), batch.end());
			}
			batch.clear();

			Loader::get()->queueInMainThread([this, cancel, generation] {
				if (listLoadCancel == cancel && listLoadGeneration == generation)
					drainPendingListEntries();
			});
		};

		for (auto const& macroPath : paths) {
			if (cancel->load())
				break;
			if (!isMacroFile(macroPath))
				continue;

			std::string name = macroDisplayName(macroPath);
			if (!searchSnapshot.empty() && Utils::toLower(name).find(searchSnapshot) == std::string::npos)
				continue;
			if (favoritesOnlySnapshot && !favoriteListContains(favoritesSnapshot, macroPath))
				continue;

			MacroListEntry info;
			info.path = macroPath;
			info.name = name;
#ifdef GEODE_IS_WINDOWS
			info.date = Utils::getFileCreationTime(macroPath);
#endif
			batch.push_back(std::move(info));

			if (batch.size() >= kMacroListBatchSize)
				flushBatch();
		}

		flushBatch();

		Loader::get()->queueInMainThread([this, cancel, generation] {
			if (listLoadCancel == cancel && listLoadGeneration == generation)
				finishBackgroundListLoad();
			release();
		});
	}).detach();
}

void LoadMacroLayer::drainPendingListEntries() {
	if (!listLoadCancel)
		return;

	std::vector<MacroListEntry> batch;
	{
		std::lock_guard<std::mutex> lock(listLoadMutex);
		batch.swap(pendingMacroEntries);
	}

	if (batch.empty())
		return;

	loadedMacroEntries.insert(
		loadedMacroEntries.end(),
		std::make_move_iterator(batch.begin()),
		std::make_move_iterator(batch.end())
	);
	appendLoadedListEntries();
}

void LoadMacroLayer::finishBackgroundListLoad() {
	drainPendingListEntries();
	listLoadInProgress = false;
	hideLoadingScreen();
	updateDynamicListLayout(queuedScroll, queuedRefresh);
	if (loadedMacroEntries.empty()) {
		cocos2d::CCSize winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
		CCLabelBMFont* lbl = CCLabelBMFont::create(isAutosaves ? "No Autosaves" : "No Macros", "bigFont.fnt");
		lbl->setPosition(winSize / 2);
		lbl->setScale(0.5f);
		lbl->setOpacity(100);
		lbl->setID("no-macros-label");
		menu->addChild(lbl);
	}
}

void LoadMacroLayer::onExit() {
	cancelBackgroundListLoad();
	xdb::Popup<geode::Popup*, geode::Popup*, bool>::onExit();
}

void LoadMacroLayer::rebuildListFromLoaded(bool refresh, float prevScroll) {
	cocos2d::CCSize winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();

	clearListNodes();
	allMacros.clear();
	selectedMacros.clear();

	cocos2d::ccColor3B color = Mod::get()->getSettingValue<cocos2d::ccColor3B>("background_color");

	CCNode* listLayer = CCNode::create();
	listLayer->setContentSize({ kMacroListWidth, kMacroListHeight });
	listLayer->setPosition((winSize / 2) - (listLayer->getContentSize() / 2) + ccp(0, 1));
	listLayer->setZOrder(1);
	listLayer->setID("list-layer");
	m_buttonMenu->addChild(listLayer);

	macroScroll = geode::ScrollLayer::create({ kMacroListWidth, kMacroListHeight });
	macroScroll->setPosition({ 0.f, 0.f });
	macroScroll->setTouchEnabled(true);
	macroScroll->enableScrollWheel(true);
	listLayer->addChild(macroScroll);

	macroListMenu = CCMenu::create();
	macroListMenu->setPosition({ 0.f, 0.f });
	macroListMenu->setAnchorPoint({ 0.f, 0.f });
	macroListMenu->setContentSize({ kMacroListWidth, kMacroListHeight });
	macroListMenu->setTouchPriority(menu ? menu->getTouchPriority() - 1 : -129);
	macroScroll->m_contentLayer->addChild(macroListMenu);

	CCScale9Sprite* listBackground = CCScale9Sprite::create(WINDOW_BG, { 0, 0, 80, 80 });
	listBackground->setScale(0.7f);
	listBackground->setColor({ 0,0,0 });
	listBackground->setOpacity(75);
	listBackground->setPosition(winSize / 2 + ccp(-0.11f, -10.5f));
	listBackground->setContentSize({ 461.1f, 255.1f });
	listBackground->setID("background");
	m_buttonMenu->addChild(listBackground);

	macroScrollbar = Scrollbar::create(macroScroll);
	macroScrollbar->setPosition({ (winSize.width / 2) + (listLayer->getScaledContentSize().width / 2) + 4, winSize.height / 2 });
	macroScrollbar->setID("scrollbar");
	m_buttonMenu->addChild(macroScrollbar);

	appendLoadedListEntries();
	updateDynamicListLayout(prevScroll, refresh);
}

void LoadMacroLayer::appendLoadedListEntries() {
	if (!macroScroll || !macroListMenu)
		rebuildListFromLoaded(queuedRefresh, queuedScroll);
	if (!macroScroll || !macroListMenu)
		return;

	bool selectAllWasOn = !isMerge && selectAllToggle && selectAllToggle->isToggled();
	cocos2d::ccColor3B color = Mod::get()->getSettingValue<cocos2d::ccColor3B>("background_color");
	cocos2d::ccColor3B color1 = ccc3(std::max(0, color.r - 70), std::max(0, color.g - 70), std::max(0, color.b - 70));
	cocos2d::ccColor3B color2 = ccc3(std::max(0, color.r - 55), std::max(0, color.g - 55), std::max(0, color.b - 55));

	while (allMacros.size() < loadedMacroEntries.size()) {
		size_t index = allMacros.size();
		auto const& macro = loadedMacroEntries[index];

		CCLayerColor* rowBg = CCLayerColor::create(ccc4(0, 0, 0, 95), kMacroListWidth, kMacroRowHeight);
		rowBg->setColor((index % 2 == 0) ? color1 : color2);
		rowBg->setAnchorPoint({ 0.f, 0.f });
		rowBg->setID(fmt::format("macro-row-bg-{}", index).c_str());
		macroListMenu->addChild(rowBg, -1);

		MacroCell* cell = MacroCell::create(macro.path, macro.name, macro.date, menuLayer, mergeLayer, static_cast<CCLayer*>(this));
		cell->setContentSize({ kMacroListWidth, kMacroRowHeight });
		cell->setAnchorPoint({ 0.f, 0.f });
		cell->setID(fmt::format("macro-cell-{}", index).c_str());
		macroListMenu->addChild(cell);

		if (!isMerge && selectAllWasOn) {
			cell->toggler->toggle(true);
			selectedMacros.push_back(cell);
		}

		allMacros.push_back(cell);
	}

	updateDynamicListLayout(queuedScroll, queuedRefresh);
}

void LoadMacroLayer::updateDynamicListLayout(float prevScroll, bool restoreScroll) {
	if (!macroScroll || !macroScroll->m_contentLayer || !macroListMenu)
		return;

	float viewHeight = macroScroll->getContentSize().height;
	float previousContentHeight = macroScroll->m_contentLayer->getContentSize().height;
	float previousScroll = macroScroll->m_contentLayer->getPositionY();
	float contentHeight = std::max(viewHeight, kMacroRowHeight * static_cast<float>(allMacros.size()));
	macroScroll->m_contentLayer->setAnchorPoint({ 0.f, 0.f });
	macroScroll->m_contentLayer->setContentSize({ kMacroListWidth, contentHeight });
	macroListMenu->setContentSize({ kMacroListWidth, contentHeight });

	if (macroCountLbl)
		macroCountLbl->setString(fmt::format("{} Macros", allMacros.size()).c_str());

	for (size_t i = 0; i < allMacros.size(); i++) {
		float y = contentHeight - kMacroRowHeight * static_cast<float>(i + 1);
		allMacros[i]->setPosition({ 0.f, y });
		if (auto* rowBg = typeinfo_cast<CCLayerColor*>(macroListMenu->getChildByID(fmt::format("macro-row-bg-{}", i).c_str())))
			rowBg->setPosition({ 0.f, y });
	}

	if (macroScrollbar)
		macroScrollbar->setVisible(contentHeight > viewHeight + 1.f);

	if (restoreScroll)
		macroScroll->m_contentLayer->setPositionY(prevScroll);
	else if (previousContentHeight > 0.f)
		macroScroll->m_contentLayer->setPositionY(previousScroll - (contentHeight - previousContentHeight));
}

MacroCell* MacroCell::create(std::filesystem::path path, std::string name, std::time_t date, geode::Popup* menuLayer, geode::Popup* mergeLayer, CCLayer* loadLayer) {
	MacroCell* ret = new MacroCell();
	if (!ret->init(path, name, date, menuLayer, mergeLayer, loadLayer)) {
		delete ret;
		return nullptr;
	}

	ret->autorelease();
	return ret;
}

bool MacroCell::init(std::filesystem::path path, std::string name, std::time_t date, geode::Popup* menuLayer, geode::Popup* mergeLayer, CCLayer* loadLayer) {

	this->path = path;
	this->date = date;
	this->name = name;
	this->menuLayer = menuLayer;
	this->mergeLayer = mergeLayer;
	this->loadLayer = loadLayer;
	this->isMerge = mergeLayer != nullptr;

	bool autosave = false;

	size_t pos = name.find('_');
	if (pos != std::string::npos) {
		std::string firstPart = name.substr(0, pos);
		std::string secondPart = name.substr(pos + 1);
		if (firstPart == "autosave") {
			pos = secondPart.find('_');
			if (pos != std::string::npos) {
				std::string str = secondPart.substr(pos + 1);

				if (std::all_of(str.begin(), str.end(), ::isdigit)) {
					autosave = true;
					this->name = secondPart.substr(0, pos);
				}
			}
		}
	}

	menu = CCMenu::create();
	menu->setPosition({0, 0});
	addChild(menu);

	CCLabelBMFont* lbl = CCLabelBMFont::create(this->name.c_str(), "chatFont.fnt");
	lbl->limitLabelWidth(isMerge ? 194.f : 174.f, 0.8f, 0.01f);
	lbl->setAnchorPoint({ 0, 0.5 });
	lbl->updateLabel();
	addChild(lbl);

	lbl->setPosition({ 10, 23 });

#ifdef GEODE_IS_WINDOWS
	std::string subText = Utils::formatTime(date) + " | ";

	subText += autosave ? "Auto Save" : path.extension().string();

	lbl = CCLabelBMFont::create(subText.c_str(), "chatFont.fnt");
#else
	std::string subText = autosave ? "Auto Save" : path.extension().string();

	lbl = CCLabelBMFont::create(subText.c_str(), "chatFont.fnt");
#endif

	lbl->setPosition({ 10, 9 });
	lbl->setScale(0.55f);
	lbl->setSkewX(2);
	lbl->setAnchorPoint({ 0, 0.5 });
	lbl->setOpacity(80);
	addChild(lbl);

	std::string btnText = isMerge ? "Merge" : "Load";

	ButtonSprite* spr = ButtonSprite::create(btnText.c_str());
	spr->setScale(isMerge ? 0.5425f : 0.62f);
	CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MacroCell::onLoad));
	btn->setPosition(ccp(isMerge ? 277.26f : 288.26f, 17.5f));
	menu->addChild(btn);

	CCSprite* spr2 = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");
	spr2->setScale(0.485f);
	btn = CCMenuItemSpriteExtra::create(
		spr2,
		this,
		menu_selector(MacroCell::onDelete)
	);
	btn->setPosition(ccp(246, 17.5f));

	if (!isMerge)
		menu->addChild(btn);

	CCSprite* spriteOn = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
	CCSprite* spriteOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");

	toggler = CCMenuItemToggler::create(spriteOff, spriteOn, this, menu_selector(MacroCell::onSelect));
	toggler->setScale(0.485f);
	toggler->setPosition({ 220, 17.5 });

	if (!isMerge)
		menu->addChild(toggler);

	if (!isMerge) {
		LoadMacroLayer* layer = static_cast<LoadMacroLayer*>(loadLayer);
		favoriteToggle = CCMenuItemToggler::create(
			createFavoriteSprite(false),
			createFavoriteSprite(true),
			this,
			menu_selector(MacroCell::onFavorite)
		);
		favoriteToggle->setScale(0.42f);
		favoriteToggle->setPosition({ 196, 17.5f });
		favoriteToggle->setID("favorite-toggle");
		favoriteToggle->toggle(layer->isFavorite(path));
		menu->addChild(favoriteToggle);
	}

	return true;
}

void MacroCell::handleLoad() {
	auto& g = Global::get();
	
	Macro newMacro;
	Macro oldMacro = g.macro;

	if (path.extension() == ".xd") {
		if (!Macro::loadXDFile(path)) {
			if (!isMerge)
				return FLAlertLayer::create("Error", "There was an error loading this macro. ID: 45", "Ok")->show();
			else
				return;
		}

		newMacro = g.macro;

		if (isMerge)
			g.macro = oldMacro;
	}
	else {
		std::ifstream f(path.string(), std::ios::binary);
		if (!f.is_open()) {
			if (!isMerge)
				return FLAlertLayer::create("Error", "There was an error loading this macro. ID: 451", "Ok")->show();
			return;
		}

		f.seekg(0, std::ios::end);
		std::streamoff end = f.tellg();
		if (end <= 0) {
			f.close();
			if (!isMerge)
				return FLAlertLayer::create("Error", "There was an error loading this macro. ID: 452", "Ok")->show();
			return;
		}
		f.seekg(0, std::ios::beg);

		size_t fileSize = static_cast<size_t>(end);
		std::vector<std::uint8_t> macroData(fileSize);

		f.read(reinterpret_cast<char*>(macroData.data()), static_cast<std::streamsize>(fileSize));
		if (!f) {
			f.close();
			if (!isMerge)
				return FLAlertLayer::create("Error", "There was an error loading this macro. ID: 453", "Ok")->show();
			return;
		}
		f.close();

#ifdef GEODE_IS_WINDOWS
		// Catch hard parser faults on malformed data in the load path itself.
		__try {
			newMacro = Macro::importData(macroData);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			if (!isMerge)
				return FLAlertLayer::create("Error", "There was an error loading this macro. ID: 455", "Ok")->show();
			return;
		}
#else
		try {
			newMacro = Macro::importData(macroData);
		}
		catch (...) {
			if (!isMerge)
				return FLAlertLayer::create("Error", "There was an error loading this macro. ID: 454", "Ok")->show();
			return;
		}
#endif
	}

	if (isMerge) {
		bool players[2] = { true, true };
		bool p1 = static_cast<LoadMacroLayer*>(loadLayer)->p1Toggle->isToggled();
		bool p2 = static_cast<LoadMacroLayer*>(loadLayer)->p2Toggle->isToggled();

		if (p1)
			players[1] = false;
		else if (p2)
			players[0] = false;

		if (mergeLayer) {
			typeinfo_cast<MacroEditLayer*>(mergeLayer)->mergeMacro(newMacro.inputs, players, static_cast<LoadMacroLayer*>(loadLayer)->owToggle->isToggled());
			loadLayer->keyBackClicked();
		}

		return;
	}

	g.macro = newMacro;
	Macro::preparePlayback();
	g.restart = true;
	g.macro.canChangeFPS = false;

    g.macro.geobotMacro = g.macro.botInfo.name == "geobot";

	// Capture member variables before keyBackClicked() frees 'this' via recursive child cleanup.
	geode::Popup* capturedMenuLayer = menuLayer;
	bool isXdMacro = path.extension() == ".xd";

	loadLayer->keyBackClicked();

	RecordLayer* newLayer = nullptr;

	if (RecordLayer* layer = typeinfo_cast<RecordLayer*>(capturedMenuLayer)) {
		layer->onClose(nullptr);
		newLayer = RecordLayer::openMenu(true);
	}

	if (!newLayer) newLayer = g.layer != nullptr ? static_cast<RecordLayer*>(g.layer) : nullptr;
	if (newLayer) newLayer->updateTPS();

	if (!PlayLayer::get() && g.state != state::playing)
		Macro::togglePlaying();
	else if (g.state == state::recording) {
		if (newLayer) {
			newLayer->recording->toggle(Global::get().state != state::recording);
			newLayer->toggleRecording(nullptr);
		}
		else {
			RecordLayer* layer = RecordLayer::create();
			layer->toggleRecording(nullptr);
			layer->onClose(nullptr);
		}
	}

	if (isXdMacro)
		FLAlertLayer::create("Warning", "<cl>.xd</c> extension macros may not function correctly in this version.", "Ok")->show();

	Notification::create("Macro Loaded", NotificationIcon::Success)->show();
}

void MacroCell::onLoad(CCObject*) {
	if (Global::get().macro.inputs.empty() || isMerge)
		return handleLoad();

	geode::createQuickPopup(
		"Warning",
		"Replace the current <cy>" + std::to_string(Global::get().macro.inputs.size()) + "</c> macro actions?",
		"Cancel", "Yes",
		[this](auto, bool btn2) {
			if (btn2) {
				this->handleLoad();
			}
		}
	);

}

void MacroCell::onDelete(CCObject*) {
	geode::createQuickPopup(
		"Warning",
		"Are you sure you want to <cr>delete</c> this macro? (\"<cl>" + name + "</c>\")",
		"Cancel", "Yes",
		[this](auto, bool btn2) {
			if (btn2) {
				this->deleteMacro(true);
			}
		}
	);
}

void MacroCell::deleteMacro(bool reload) {
	std::error_code ec;
	std::filesystem::remove(path, ec);
	if (ec) {
		return FLAlertLayer::create("Error", "There was an error deleting this macro. ID: 7", "Ok")->show();
	}
	else {
		static_cast<LoadMacroLayer*>(loadLayer)->setFavorite(path, false);
		if (reload) {
			static_cast<LoadMacroLayer*>(loadLayer)->reloadList();
			Notification::create("Macro Deleted", NotificationIcon::Success)->show();
		}
		this->removeFromParentAndCleanup(true);
	}
}

void MacroCell::onSelect(CCObject*) {
	selectMacro(true);
}

void MacroCell::onFavorite(CCObject*) {
	LoadMacroLayer* layer = static_cast<LoadMacroLayer*>(loadLayer);
	bool favorite = !favoriteToggle->isToggled();
	layer->setFavorite(path, favorite);

	// Removing a favorite while the filter is active should remove it from the list.
	if (!favorite && layer->favoritesOnly)
		layer->reloadList(0);
}

void MacroCell::selectMacro(bool single) {
	LoadMacroLayer* layer = static_cast<LoadMacroLayer*>(loadLayer);
	std::vector<MacroCell*>& selectedMacros = layer->selectedMacros;

	auto it = std::remove(selectedMacros.begin(), selectedMacros.end(), this);

	if (it != selectedMacros.end()) {
		selectedMacros.erase(it, selectedMacros.end());
		if (single) layer->selectAllToggle->toggle(false);
	}
	else
		selectedMacros.push_back(this);

	if (selectedMacros.size() == layer->allMacros.size() && single)
		layer->selectAllToggle->toggle(true);
}
