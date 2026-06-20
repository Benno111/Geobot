#pragma once

#include "record_layer.hpp"
#include "../macro.hpp"
#include <Geode/binding/LevelEditorLayer.hpp>

#ifdef GEODE_IS_WINDOWS
#include "../utils/subprocess.hpp"
#endif

#include <ctime>
#include <fstream>
#include <future>
#include <mutex>

namespace {
GJGameLevel* getCurrentPathfinderLevel() {
    if (PlayLayer* pl = PlayLayer::get())
        return pl->m_level;

    if (LevelEditorLayer* lel = LevelEditorLayer::get())
        return lel->m_level;

    return nullptr;
}

std::string sanitizePathfinderName(std::string value) {
    if (value.empty())
        return "pathfinder";

    for (char& ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch)))
            continue;
        ch = '_';
    }

    return value;
}

void replaceAllPathfinder(std::string& source, std::string const& from, std::string const& to) {
    if (from.empty())
        return;

    size_t start = 0;
    while ((start = source.find(from, start)) != std::string::npos) {
        source.replace(start, from.size(), to);
        start += to.size();
    }
}

std::string quotePathfinderArg(std::filesystem::path const& path) {
    std::string raw = path.string();
    replaceAllPathfinder(raw, "\"", "\\\"");
    return "\"" + raw + "\"";
}

bool tryLoadPathfinderMacro(std::filesystem::path const& path, Macro& outMacro) {
    if (!std::filesystem::exists(path))
        return false;

    if (path.extension() == ".xd") {
        outMacro = Macro::XDtoGDR(path);
        return outMacro.description != "fail";
    }

    std::ifstream file(
#ifdef GEODE_IS_WINDOWS
        Utils::widen(path.string()),
#else
        path.string(),
#endif
        std::ios::binary
    );

    if (!file.is_open())
        return false;

    file.seekg(0, std::ios::end);
    std::streampos end = file.tellg();
    if (end < 0)
        return false;
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> macroData(static_cast<size_t>(end));
    file.read(reinterpret_cast<char*>(macroData.data()), static_cast<std::streamsize>(macroData.size()));
    if (!file)
        return false;

#ifdef GEODE_IS_WINDOWS
    __try {
        outMacro = Macro::importData(macroData);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    try {
        outMacro = Macro::importData(macroData);
    }
    catch (...) {
        return false;
    }
#endif

    return true;
}

void applyPathfinderMacro(Macro const& macro) {
    auto& g = Global::get();

    g.macro = macro;
    g.currentAction = 0;
    g.currentFrameFix = 0;
    g.restart = true;
    g.macro.canChangeFPS = false;
    g.macro.geobotMacro = g.macro.botInfo.name == "geobot";

    Global::resetPathfinderState();
    Macro::updateTPS();

    if (g.layer)
        static_cast<RecordLayer*>(g.layer)->updateTPS();

    if (g.state == state::recording) {
        if (g.layer && static_cast<RecordLayer*>(g.layer)->recording)
            static_cast<RecordLayer*>(g.layer)->recording->toggle(false);
        g.state = state::none;
    }

    if (g.state != state::playing) {
        Macro::togglePlaying();
        return;
    }

    PlayLayer* pl = PlayLayer::get();
    if (pl) {
        if (!pl->m_isPaused && !pl->m_levelEndAnimationStarted)
            pl->resetLevelFromStart();
        else
            g.restart = true;
    }

    Interface::updateLabels();
    Interface::updateButtons();
}

#ifdef GEODE_IS_WINDOWS
struct PathfinderSolveState {
    std::atomic_bool cancel = false;
    std::atomic_bool finished = false;
    std::atomic<int> exitCode = { std::numeric_limits<int>::min() };
    std::mutex mutex;
    std::string command;
    std::string status = "Idle";
    std::filesystem::path outputPath;
    std::thread worker;
};
#endif
}

class PathfinderSettingsLayer : public xdb::Popup<> {
public:
    STATIC_CREATE(PathfinderSettingsLayer, 280, 200)

    void open(CCObject*) {
        create()->show();
    }

    ~PathfinderSettingsLayer() override {
#ifdef GEODE_IS_WINDOWS
        if (m_solveState) {
            m_solveState->cancel = true;
            if (m_solveState->worker.joinable())
                m_solveState->worker.join();
        }
#endif
    }

private:
    CCMenuItemToggler* modeToggle = nullptr;
    CCLabelBMFont* statusLabel = nullptr;
    CCLabelBMFont* solverLabel = nullptr;
    CCMenuItemSpriteExtra* runButton = nullptr;
#ifdef GEODE_IS_WINDOWS
    std::shared_ptr<PathfinderSolveState> m_solveState;
#endif
    int spinnerFrame = 0;

    bool setup() override {
        setTitle("Pathfinder");
        adjustForLoadingScreen();
        Utils::setBackgroundColor(m_bgSprite);
        schedule(schedule_selector(PathfinderSettingsLayer::tickSolver));

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

        lbl = CCLabelBMFont::create("External solver status", "goldFont.fnt");
        lbl->setPosition({ m_size.width / 2.f, 79.f });
        lbl->setScale(0.4f);
        m_mainLayer->addChild(lbl);

        solverLabel = CCLabelBMFont::create("", "chatFont.fnt");
        solverLabel->setPosition({ m_size.width / 2.f, 61.f });
        solverLabel->setScale(0.58f);
        solverLabel->setOpacity(170);
        m_mainLayer->addChild(solverLabel);

        lbl = CCLabelBMFont::create(featureEnabled ? "Runs an external Pathfinder-style solver on the" : "Enable the feature flag in mod settings", "chatFont.fnt");
        lbl->setPosition({ m_size.width / 2.f, 38.f });
        lbl->setScale(0.52f);
        lbl->setOpacity(120);
        m_mainLayer->addChild(lbl);

        lbl = CCLabelBMFont::create(featureEnabled ? "current level, then auto-loads and plays the macro." : "to use pathfinder controls here.", "chatFont.fnt");
        lbl->setPosition({ m_size.width / 2.f, 25.f });
        lbl->setScale(0.52f);
        lbl->setOpacity(120);
        m_mainLayer->addChild(lbl);

        ButtonSprite* runSpr = ButtonSprite::create("Run Solver");
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
#ifndef GEODE_IS_WINDOWS
        FLAlertLayer::create("Pathfinder", "External solver integration is currently available on <cy>Windows</c> only in this repo.", "Ok")->show();
        return;
#else
        if (m_solveState && !m_solveState->finished)
            return;

        if (!Global::isPathfinderFeatureEnabled()) {
            FLAlertLayer::create("Pathfinder", "Enable the <cy>Pathfinder</c> feature flag in mod settings first.", "Ok")->show();
            return;
        }

        GJGameLevel* level = getCurrentPathfinderLevel();
        if (!level) {
            FLAlertLayer::create("Pathfinder", "Open a <cy>level</c> or the <cy>editor</c> first.", "Ok")->show();
            return;
        }

        std::filesystem::path executable = Mod::get()->getSettingValue<std::filesystem::path>("pathfinder_solver_executable");
        if (executable.empty() || !std::filesystem::exists(executable)) {
            FLAlertLayer::create("Pathfinder", "Set <cy>Pathfinder Solver Executable</c> in the mod settings first.", "Ok")->show();
            return;
        }

        std::filesystem::path outputFolder = Mod::get()->getSettingValue<std::filesystem::path>("pathfinder_output_folder");
        if (outputFolder.empty())
            outputFolder = Mod::get()->getSaveDir() / "pathfinder";

        std::error_code ec;
        std::filesystem::create_directories(outputFolder, ec);
        if (ec) {
            FLAlertLayer::create("Pathfinder", "Could not create the configured <cy>Pathfinder Output Folder</c>.", "Ok")->show();
            return;
        }

        std::string levelString = ZipUtils::decompressString(level->m_levelString.c_str(), true, 0);
        if (levelString.empty()) {
            FLAlertLayer::create("Pathfinder", "Failed to decompress the current level string.", "Ok")->show();
            return;
        }

        auto timestamp = static_cast<long long>(std::time(nullptr));
        std::string safeLevelName = sanitizePathfinderName(level->m_levelName);
        std::string solverLevelName = level->m_levelName;
        replaceAllPathfinder(solverLevelName, "\"", "'");
        std::filesystem::path inputPath = outputFolder / fmt::format("{}-{}.txt", safeLevelName, timestamp);
        std::filesystem::path outputPath = outputFolder / fmt::format("{}-{}.gdr", safeLevelName, timestamp);

        std::ofstream inputFile(Utils::widen(inputPath.string()));
        if (!inputFile.is_open())
            inputFile.open(inputPath);
        if (!inputFile.is_open()) {
            FLAlertLayer::create("Pathfinder", "Could not write the temporary level export for the solver.", "Ok")->show();
            return;
        }
        inputFile << levelString;
        inputFile.close();

        std::string args = Mod::get()->getSettingValue<std::string>("pathfinder_solver_args");
        if (args.empty()) {
            FLAlertLayer::create("Pathfinder", "Set <cy>Pathfinder Solver Arguments</c> in the mod settings first.", "Ok")->show();
            return;
        }

        replaceAllPathfinder(args, "{input}", inputPath.string());
        replaceAllPathfinder(args, "{output}", outputPath.string());
        replaceAllPathfinder(args, "{level_name}", solverLevelName);

        auto solveState = std::make_shared<PathfinderSolveState>();
        solveState->command = quotePathfinderArg(executable) + " " + args;
        solveState->outputPath = outputPath;
        solveState->status = "Launching solver";

        solveState->worker = std::thread([solveState]() {
            subprocess::Popen process(solveState->command);
            if (!process.valid()) {
                solveState->exitCode = -100;
                solveState->status = "Failed to start solver";
                solveState->finished = true;
                return;
            }

            solveState->status = "Solving level";
            DWORD exitCode = 0;
            while (!solveState->cancel) {
                if (process.wait_for(150, &exitCode)) {
                    process.close(false);
                    solveState->exitCode = static_cast<int>(exitCode);
                    solveState->status = "Solver finished";
                    solveState->finished = true;
                    return;
                }
            }

            process.terminate();
            process.close(false);
            solveState->exitCode = -200;
            solveState->status = "Solver canceled";
            solveState->finished = true;
        });

        m_solveState = solveState;
        spinnerFrame = 0;
        if (solverLabel)
            solverLabel->setString("Solver: running");
        if (runButton)
            runButton->setEnabled(false);
#endif
    }

    void tickSolver(float) {
        updateStateUI();

#ifdef GEODE_IS_WINDOWS
        if (!m_solveState)
            return;

        if (!m_solveState->finished) {
            spinnerFrame = (spinnerFrame + 1) % 4;
            std::string dots(static_cast<size_t>(spinnerFrame), '.');
            if (solverLabel) {
                std::string text = "Solver: running";
                text += dots;
                solverLabel->setString(text.c_str());
                solverLabel->limitLabelWidth(235.f, 0.58f, 0.1f);
                solverLabel->updateLabel();
            }
            return;
        }

        if (m_solveState->worker.joinable())
            m_solveState->worker.join();

        int exitCode = m_solveState->exitCode.load();
        std::filesystem::path outputPath = m_solveState->outputPath;
        m_solveState.reset();
        if (runButton)
            runButton->setEnabled(true);

        if (exitCode == -200) {
            if (solverLabel)
                solverLabel->setString("Solver: canceled");
            return;
        }

        if (exitCode != 0) {
            if (solverLabel)
                solverLabel->setString(fmt::format("Solver failed ({})", exitCode).c_str());
            FLAlertLayer::create(
                "Pathfinder",
                fmt::format(
                    "The external solver exited with code <cr>{}</c>.\nCheck the configured executable and arguments in mod settings.",
                    exitCode
                ),
                "Ok"
            )->show();
            return;
        }

        Macro generatedMacro;
        if (!tryLoadPathfinderMacro(outputPath, generatedMacro)) {
            if (solverLabel)
                solverLabel->setString("Solver finished, macro missing");
            FLAlertLayer::create(
                "Pathfinder",
                "The solver finished, but geobot could not load the generated macro file.",
                "Ok"
            )->show();
            return;
        }

        if (solverLabel)
            solverLabel->setString("Solver finished, macro loaded");

        applyPathfinderMacro(generatedMacro);
        Notification::create("Pathfinder macro loaded and playing.", NotificationIcon::Success)->show();
    #endif
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
        bool solverIdle = true;
#ifdef GEODE_IS_WINDOWS
        solverIdle = !m_solveState || m_solveState->finished;
#endif
        if (solverLabel && solverIdle) {
#ifdef GEODE_IS_WINDOWS
            solverLabel->setString("Solver: idle");
#else
            solverLabel->setString("Solver: unavailable on this platform");
#endif
            solverLabel->limitLabelWidth(235.f, 0.58f, 0.1f);
            solverLabel->updateLabel();
        }
    }
};
