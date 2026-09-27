
#pragma once

#include "../includes.hpp"
#include "record_layer.hpp"

class Interface {

public:

    static void addLabels(PlayLayer* pl);

    static void addButtons(PlayLayer* pl);

    static void updateLabels();

    static void updateButtons();

};