// SPDX-License-Identifier: MIT
#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include "parser/earley_things.h"

namespace Elica::Parsing::Earley::Chart {

class ChartState {
private:
    std::vector<std::unique_ptr<::Elica::Parsing::Earley::Things::Item>> items;

    bool compareItems(
        const ::Elica::Parsing::Earley::Things::Item &item1,
        const ::Elica::Parsing::Earley::Things::Item &item2
    ) const {
        return item1.rule == item2.rule &&
               item1.dotIndex == item2.dotIndex &&
               item1.startPosition == item2.startPosition;
    }

public:
    ChartState() = default;

    // returns raw ptrs
    std::vector<::Elica::Parsing::Earley::Things::Item*> getItems() {
        std::vector<::Elica::Parsing::Earley::Things::Item*> rawItems;
        rawItems.reserve(items.size());
        for (auto &item : items) {
            rawItems.push_back(item.get());
        }
        return rawItems;
    }

    const std::vector<::Elica::Parsing::Earley::Things::Item*> getItems() const {
        std::vector<::Elica::Parsing::Earley::Things::Item*> rawItems;
        rawItems.reserve(items.size());
        for (const auto &item : items) {
            rawItems.push_back(item.get());
        }
        return rawItems;
    }

    // add
    ::Elica::Parsing::Earley::Things::Item* addItem(
        const ::Elica::Parsing::Earley::Things::Rule* rule,
        size_t dotIndex,
        size_t startPosition,
        std::vector<const ::Elica::Parsing::Earley::Things::Item*> backPointers = {}
    ) {
        for (const auto &existing : items) {
            if (existing->rule == rule &&
                existing->dotIndex == dotIndex &&
                existing->startPosition == startPosition
            ) {
                return existing.get(); 
            }
        }

        auto newItem = std::make_unique<::Elica::Parsing::Earley::Things::Item>(
            rule, dotIndex, startPosition, std::move(backPointers)
        );
        ::Elica::Parsing::Earley::Things::Item* ptr = newItem.get();
        items.push_back(std::move(newItem));
        return ptr;
    }

    size_t size() const {
        return items.size();
    }
};

class Chart {
private:
    std::vector<ChartState> states;

public:
    Chart() = default;

    explicit Chart(size_t size) : states(size) {}

    void addState() {
        states.emplace_back();
    }

    ChartState &getState(size_t index) {
        return states.at(index);
    }

    const ChartState &getState(size_t index) const {
        return states.at(index);
    }

    size_t size() const {
        return states.size();
    }

    void clear() {
        states.clear();
    }

    void resize(size_t newSize) {
        states.resize(newSize);
    }
};

}