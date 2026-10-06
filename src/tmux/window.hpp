#pragma once

#include "layout_node.hpp"
#include "pane.hpp"
#include "tmux.hpp"

struct Window {
    int id;
    std::string name;
    std::shared_ptr<LayoutNode> rootLayout;
    std::shared_ptr<Pane> activePane;
    bool isZoomed = false;

    Window(int id, const std::string& name);

    std::vector<std::shared_ptr<Pane>> GetPanes();
};
