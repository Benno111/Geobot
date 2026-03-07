#include "node_ids.hpp"

namespace node_ids {
CCNode* findByIDRecursive(CCNode* root, std::string_view id) {
    if (!root) return nullptr;
    if (root->getID() == id) return root;

    CCArray* children = root->getChildren();
    if (!children) return nullptr;

    for (int i = 0; i < children->count(); i++) {
        CCNode* child = typeinfo_cast<CCNode*>(children->objectAtIndex(i));
        if (!child) continue;
        if (CCNode* found = findByIDRecursive(child, id))
            return found;
    }
    return nullptr;
}

CCNode* getChildByIDOrRecursive(CCNode* root, std::string_view id) {
    if (!root) return nullptr;
    if (CCNode* node = root->getChildByID(id.data()))
        return node;
    return findByIDRecursive(root, id);
}
}

