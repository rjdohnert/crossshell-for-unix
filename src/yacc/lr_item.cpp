#include "lr_item.hpp"

bool Item::operator<(const Item& o) const {
        if (rule_id != o.rule_id) return rule_id < o.rule_id;
        return dot < o.dot;
    }

bool Item::operator==(const Item& o) const {
        return rule_id == o.rule_id && dot == o.dot;
    }
