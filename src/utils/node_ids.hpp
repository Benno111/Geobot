#pragma once

#include "../includes.hpp"

namespace node_ids {
CCNode* findByIDRecursive(CCNode* root, std::string_view id);
CCNode* getChildByIDOrRecursive(CCNode* root, std::string_view id);
}

