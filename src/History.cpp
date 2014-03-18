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

# include "History.hpp"

# include <cstddef>
# include <cassert>
# include <cctype>
# include <utility>
# include <algorithm>
# include <deque>
# include <string>
# include <fstream>
# include <iterator>


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
        const auto begin = std::find_if_not(line.begin(), line.end(),
        [](char c) { return std::isspace(c); });

        if (begin == line.begin())
            items_.emplace_back(std::move(line));
        else if (begin != line.end())
            items_.emplace_back(line.substr(begin - line.begin()));
        // skipping lines without non-whitespace characters.
    }

    return is.is_open() && ! is.bad();
}

bool History::save(const std::string & filename) const
{
    std::ofstream os(filename);
    std::copy(items_.begin(), items_.end(),
              std::ostream_iterator<std::string>(os, "\n"));
    return os;
}

void History::setMaxSize(const std::size_t maxSize)
{
    maxSize_ = std::min(maxSize, maxMaxSize());
    if (items_.size() > maxSize_)
        items_.erase(items_.begin() + maxSize_, items_.end());
}

std::string History::getRelativePath(const std::size_t index,
                                     int nHiddenDirs) const
{
    std::string entry = items_.at(index);
    if (nHiddenDirs == 0)
        return entry;
    if (nHiddenDirs < 0)
        throw Error("negative number of hidden directories.");
    // skipping first symbol due to ItemTree's path specifics.
    std::size_t i = 0;
    do {
        i = entry.find('/', i + 1);
        if (i == std::string::npos)
            return std::string();
    }
    while (--nHiddenDirs > 0);

    return std::move(entry).substr(i + 1);
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
    if (indices.back() >= items_.size())
        throw Error(outOfBoundsErrorMessage());

    std::deque<std::string> newItems;
    for (std::size_t i = 0, indicesIndex = 0; i < items_.size(); ++i) {
        while (indicesIndex < indices.size() && indices[indicesIndex] < i)
            ++indicesIndex;
        if (indicesIndex < indices.size() && indices[indicesIndex] == i)
            continue;
        newItems.emplace_back(std::move(items_[i]));
    }
    items_ = newItems;
}
