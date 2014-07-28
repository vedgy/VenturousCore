/*
 This file is part of VenturousCore.
 Copyright (C) 2014 Igor Kushnir <igorkuo AT Google mail>

 VenturousCore is free software: you can redistribute it and/or
 modify it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 VenturousCore is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with
 VenturousCore.  If not, see <http://www.gnu.org/licenses/>.
*/

# ifdef DEBUG_VENTUROUS_HISTORY
# include <iostream>
# endif


# include "History.hpp"

# include <CommonUtilities/String.hpp>
# include <CommonUtilities/Streams.hpp>

# include <cstddef>
# include <cassert>
# include <cctype>
# include <utility>
# include <algorithm>
# include <deque>
# include <string>
# include <iterator>
# include <fstream>


namespace
{
std::string outOfBoundsErrorMessage()
{
    return "index is out of bounds.";
}

}


bool History::load(const std::string & filename)
{
    if (maxSize_ == 0)
        return true;
    clear();

    std::ifstream is(filename);

    std::string line;
    while (items_.size() < maxSize_ && std::getline(is, line)) {
        const auto begin =
            std::find_if_not(line.begin(), line.end(),
                             CommonUtilities::SafeCtype<std::isspace>());
        if (begin != line.end()) {
            if (begin == line.begin())
                items_.emplace_back(std::move(line));
            else
                items_.emplace_back(line.substr(begin - line.begin()));
        }
        // Skipping lines without non-whitespace characters.
    }

    return CommonUtilities::isStreamFine(is);
}

bool History::save(const std::string & filename) const
{
    std::ofstream os(filename);
    std::copy(items_.begin(), items_.end(),
              std::ostream_iterator<std::string>(os, "\n"));
    return CommonUtilities::isStreamFine(os);
}

void History::setMaxSize(const std::size_t maxSize)
{
    maxSize_ = std::min(maxSize, maxMaxSize());
    if (items_.size() > maxSize_)
        items_.erase(items_.begin() + maxSize_, items_.end());
}

void History::push(std::string entry)
{
    if (entry.empty())
        throw Error("empty entry.");
    items_.emplace_front(std::move(entry));
    if (items_.size() > maxSize_)
        items_.pop_back();
}

void History::remove(std::vector<std::size_t> indices)
{
    if (indices.empty())
        return;
    if (indices.size() == 1) {
        if (indices.back() >= items_.size())
            throw Error(outOfBoundsErrorMessage());
        items_.erase(items_.begin() + indices.back());
        return;
    }

    std::sort(indices.begin(), indices.end());

# ifdef DEBUG_VENTUROUS_HISTORY
    std::cout << "Indices of entries, scheduled for removal from history:";
    for (std::size_t i : indices)
        std::cout << ' ' << i;
    std::cout << std::endl;
# endif

    if (indices.back() >= items_.size())
        throw Error(outOfBoundsErrorMessage());
    assert(std::adjacent_find(indices.begin(), indices.end()) == indices.end()
           && "No duplicates are allowed!");

    std::deque<std::string> newItems;
    for (std::size_t i = 0, indicesIndex = 0; i < items_.size(); ++i) {
        if (indicesIndex < indices.size()) {
            if (indices[indicesIndex] < i)
                ++indicesIndex;
            if (indicesIndex < indices.size() && indices[indicesIndex] == i)
                continue;
        }
        newItems.emplace_back(std::move(items_[i]));
    }
    items_ = std::move(newItems);
}
