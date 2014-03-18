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

# ifndef VENTUROUS_CORE_ITEM_TREE_INL_HPP
# define VENTUROUS_CORE_ITEM_TREE_INL_HPP

# include "ItemTree.hpp"

# include <TemplateUtilities/Resize.hpp>

# include <cassert>
# include <string>
# include <iterator>


namespace ItemTree
{
template <typename InputStringIterator>
Node * Node::descendant(
    InputStringIterator begin, const InputStringIterator end)
{
    Node * node = this;
    for (; begin != end && node != nullptr; ++begin)
        node = node->child(* begin);
    return node;
}

template <typename InputStringIterator>
std::deque<Node *> Node::descendantPath(
    InputStringIterator begin, InputStringIterator end)
{
    std::deque<Node *> nodes { this };
    for (; begin != end && nodes.back() != nullptr; ++begin)
        nodes.emplace_back(nodes.back()->child(* begin));
    nodes.pop_front();
    return nodes;
}


template<class StringCollection>
StringCollection Node::getAllItems() const
{
    StringCollection result;
    if (name_.empty()) // root
        TemplateUtilities::resize(result, itemCount());
    else
        TemplateUtilities::resize(result, itemCount(), name_ + '/');
    addAllItemsRelative(std::begin(result));
    return result;
}


template<typename ForwardStringIterator>
ForwardStringIterator Node::addAllItems(const ForwardStringIterator begin) const
{
    ForwardStringIterator it = begin;
    const std::string path = name_ + '/';
    // Add "<name_>/" to this node's (if it is an Item) and all descendants'
    // paths.
    for (int i = itemCount(); i > 0; --i, ++it)
        * it += path;
    addAllItemsRelative<ForwardStringIterator>(begin);
    return it;
}

template<typename ForwardStringIterator>
void Node::addAllItemsRelative(ForwardStringIterator begin) const
{
    if (playable_) {
        assert(! begin->empty() && begin->back() == '/');
        // Remove '/' from the end of this Item's path.
        begin->pop_back();
        ++begin;
    }
    for (const Node & child : children_)
        begin = child.addAllItems<ForwardStringIterator>(begin);
}

}

# endif // VENTUROUS_CORE_ITEM_TREE_INL_HPP
