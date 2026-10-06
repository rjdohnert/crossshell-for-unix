#include "layout_node.hpp"
#include "pane.hpp"
#include "window.hpp"

Window::Window(int id, const std::string& name) : id(id), name(name) {
        rootLayout = std::make_shared<LayoutNode>();
    }

std::vector<std::shared_ptr<Pane>> Window::GetPanes() {
        std::vector<std::shared_ptr<Pane>> list;
        rootLayout->CollectPanes(list);
        return list;
    }
