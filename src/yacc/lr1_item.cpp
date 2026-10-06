#include "lr1_item.hpp"

bool LR1Item::operator<(const LR1Item& o) const {
        if (rule_id != o.rule_id) return rule_id < o.rule_id;
        if (dot != o.dot) return dot < o.dot;
        return lookahead < o.lookahead;
    }

bool LR1Item::operator==(const LR1Item& o) const {
        return rule_id == o.rule_id && dot == o.dot && lookahead == o.lookahead;
    }
