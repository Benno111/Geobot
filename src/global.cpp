
ADD THIS FUNCTION TO src/global.cpp

void Global::triggerFramePerfectExpected(int leftWiggle, int rightWiggle) {
    auto& g = Global::get();

    double tps = std::max(1.0, static_cast<double>(Global::getTPS()));

    auto isFramePerfectForFPS = [&](double targetFps) {
        double stepFrames = tps / targetFps;
        return static_cast<double>(leftWiggle) < stepFrames ||
               static_cast<double>(rightWiggle) < stepFrames;
    };

    if (isFramePerfectForFPS(60.0))
        g.framePerfectExpected60++;

    if (isFramePerfectForFPS(144.0))
        g.framePerfectExpected144++;

    if (isFramePerfectForFPS(240.0))
        g.framePerfectExpected240++;

    g.framePerfectExpected = g.framePerfectExpected240;
}


REPLACE triggerFramePerfectOverlayCounted WITH:

void Global::triggerFramePerfectOverlayCounted(
    size_t actionIndex,
    int button,
    bool down,
    std::string const& typeName,
    int leftWiggle,
    int rightWiggle
) {
    auto& g = Global::get();

    if (g.lastFramePerfectAction == actionIndex)
        return;

    g.lastFramePerfectAction = actionIndex;

    Global::triggerFramePerfectExpected(leftWiggle, rightWiggle);

    if (g.framePerfectSfxEnabled) {
        const char* sfx = down ? "default_hold_click.mp3" : "default_release_click.mp3";

        if (button == 2)
            sfx = down ? "default_hold_left.mp3" : "default_release_left.mp3";
        else if (button == 3)
            sfx = down ? "default_hold_right.mp3" : "default_release_right.mp3";

        FMODAudioEngine::sharedEngine()->playEffect(
            (Mod::get()->getResourcesDir() / sfx).string()
        );
    }

    double tps = std::max(1.0, static_cast<double>(Global::getTPS()));

    auto isFramePerfectForFPS = [&](double targetFps) {
        double stepFrames = tps / targetFps;
        return static_cast<double>(leftWiggle) < stepFrames ||
               static_cast<double>(rightWiggle) < stepFrames;
    };

    if (isFramePerfectForFPS(60.0))
        g.framePerfectCount60++;

    if (isFramePerfectForFPS(144.0))
        g.framePerfectCount144++;

    if (isFramePerfectForFPS(240.0))
        g.framePerfectCount240++;

    g.framePerfectCount = g.framePerfectCount240;

    const char* buttonName = "Click";
    if (button == 2) buttonName = "Left";
    else if (button == 3) buttonName = "Right";

    g.framePerfectOverlayText = fmt::format(
        "FP {} {} ({}) | Wiggle L:{} R:{} | 60:{}/{} 144:{}/{} 240:{}/{}",
        buttonName,
        down ? "Press" : "Release",
        typeName,
        leftWiggle,
        rightWiggle,
        g.framePerfectCount60,
        g.framePerfectExpected60,
        g.framePerfectCount144,
        g.framePerfectExpected144,
        g.framePerfectCount240,
        g.framePerfectExpected240
    );

    g.framePerfectOverlayFrames = 45;
}
