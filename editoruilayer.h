﻿/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "uilayer.h"
#include "editoruipanels.h"

namespace Scene {

class basic_node;

}

class editor_ui : public ui_layer {

public:
// constructors
    editor_ui();
// methods
    // potentially processes provided input key. returns: true if the input was processed, false otherwise
    bool
        on_key( int const Key, int const Action ) override;
    // updates state of UI elements
    void
        update() override;
    void
        set_node( Scene::basic_node * Node );

private:
// members
    itemproperties_panel m_itempropertiespanel { "Node Properties", true };
    Scene::basic_node * m_node { nullptr }; // currently bound scene node, if any
};
