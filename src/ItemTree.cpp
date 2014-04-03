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

# include "ItemTree.hpp"

# include <cstddef>
# include <cassert>
# include <utility>
# include <functional>
# include <algorithm>
# include <vector>
# include <string>
# include <chrono>
# include <fstream>


namespace ItemTree
{
namespace
{
struct CompNodesByName {
    bool operator()(const Node & lhs, const Node & rhs) const {
        return lhs.name() < rhs.name();
    }
};

struct EqualNodeNames {
    bool operator()(const Node & lhs, const Node & rhs) const {
        return lhs.name() == rhs.name();
    }
};

// NOTE: in Tree::topLevelItems() extra '/' is stored for
// {systems with root directory '/'}. Other nodes' names don't contain '/'.

constexpr char unplayableSymbol = '-', itemSymbol = '*', indentSymbol = '\t';
/// @brief Prints node and all its descendants to os.
/// @param indent Determines level of node.
void print(std::ostream & os, const Node & node, int indent = 0)
{
    for (int i = 0; i < indent; ++i)
        os << indentSymbol;
    os << (node.isPlayable() ? itemSymbol : unplayableSymbol)
       << node.name() << '\n';
    ++indent;
    for (const Node & child : node.children())
        print(os, child, indent);
}

std::string getInvalidStateMessage(const std::string & name)
{
    return "Node " + name + " is invalid.";
}

const std::string wrongFileFormatMessage = "wrong file format.";

}



int Node::itemCount() const
{
    return children_.empty() ? (playable_ ? 1 : 0)
               : children_.back().accumulatedItemCount_;
}

Node * Node::child(const std::string & name)
{
    const auto range = std::equal_range(children_.begin(), children_.end(),
                                        Node(name, false), CompNodesByName());
    if (range.first == range.second)
        return nullptr;
    return & *range.first;
}


Node::Node(std::string name, const bool playable)
    : name_(std::move(name)), playable_(playable)
{
}

std::string Node::getRelativeChildItemPath(int relativeId) const
{
    if (relativeId == 0 && playable_)
        return std::string();

    std::vector<Node>::const_iterator it;
    {
        Node fake(std::string(), false);
        fake.accumulatedItemCount_ = relativeId;
        // Find a child that contains (or is itself) the necessary Item.
        it = std::upper_bound(children_.cbegin(), children_.cend(), fake,
                              [](const Node & lhs, const Node & rhs)
        { return lhs.accumulatedItemCount_ < rhs.accumulatedItemCount_; });
    }

    if (it == children_.cend())
        throw Error("no such child.");
    if (it == children_.cbegin()) {
        if (playable_)
            --relativeId;
    }
    else
        relativeId -= (it - 1)->accumulatedItemCount_;
    return it->getChildItemPath(relativeId);
}

std::string Node::getChildItemPath(const int relativeId) const
{
    return '/' + name_ + getRelativeChildItemPath(relativeId);
}

void Node::insertItem(std::string relativePath)
{
    // Skipping first symbol because root can have '/' as its first symbol.
    // Empty names are not allowed, so this is fine.
    const std::size_t separatorPos = relativePath.find('/', 1);
    std::string firstDir, residue;
    if (separatorPos == std::string::npos) {
        firstDir = std::move(relativePath);
    }
    else {
        firstDir = relativePath.substr(0, separatorPos);
        // Skipping separator '/'.
        residue = std::move(relativePath).substr(separatorPos + 1);
        if (residue.empty())
            throw Error("path ends with '/'.");
    }

    // This node is playable if it is at the end of inserted path, which is
    // equivalent to (residue.empty() == true).
    Node newNode(std::move(firstDir), residue.empty());
    auto range = std::equal_range(children_.begin(), children_.end(),
                                  newNode, CompNodesByName());

    if (range.first == range.second)
        range.first = children_.insert(range.first, newNode);
    else if (newNode.playable_)
        range.first->playable_ = true;

    if (! residue.empty())
        range.first->insertItem(std::move(residue));
}

void Node::recalculateItemCount(int precedingCount)
{
    accumulatedItemCount_ = precedingCount;
    precedingCount = playable_ ? 1 : 0;
    for (Node & child : children_) {
        child.recalculateItemCount(precedingCount);
        precedingCount = child.accumulatedItemCount_;
    }
    accumulatedItemCount_ += precedingCount;
}

void Node::cleanUp()
{
    int prev = playable_ ? 1 : 0;
    for (Node & child : children_) {
        int & cur = child.accumulatedItemCount_;
        if (cur == prev)
            // Schedule empty child for removal.
            cur = 0;
        else
            prev = cur;
    }

    children_.erase(std::remove_if(children_.begin(), children_.end(),
    [](const Node & node) {
        return node.accumulatedItemCount_ == 0;
    }),
                    children_.end());

    std::for_each(children_.begin(), children().end(),
                  std::bind(& Node::cleanUp, std::placeholders::_1));
}

void Node::validate() const
{
    if (children_.empty())
        return;
    if (! std::is_sorted(children_.cbegin(), children_.cend(),
                         CompNodesByName())) {
        throw Error(getInvalidStateMessage(name_) + " Children are not sorted "
                    "properly.");
    }
    {
        auto it = std::adjacent_find(children_.cbegin(), children_.cend(),
                                     EqualNodeNames());
        if (it != children_.cend()) {
            throw Error(getInvalidStateMessage(name_) +
                        " Duplicate children with name \"" + it->name_ + "\".");
        }
    }
    if (children_.front().name_.empty())
        throw Error(getInvalidStateMessage(name_) + " Child with empty name.");

    std::for_each(children_.cbegin(), children().cend(),
                  std::bind(& Node::validate, std::placeholders::_1));
}



Tree::Tree()
{
    root_.accumulatedItemCount_ = 0;
}

std::string Tree::load(const std::string & filename)
{
    root_.children_.clear();

    std::ifstream is(filename);
    /// Holds pointers to last node on each currently open level.
    std::vector<Node *> nodeStack { & root_ };

    std::string line;
    while (std::getline(is, line)) {
        const std::size_t indent = line.find_first_not_of(indentSymbol);
        if (indent == std::string::npos ||
                (line[indent] != unplayableSymbol &&
                 line[indent] != itemSymbol)) {
            // Invalid line detected -> end of parsing.
            break;
        }
        if (indent == line.size() - 1)
            return wrongFileFormatMessage + " Empty name.";
        if (indent > nodeStack.size() - 1)
            return wrongFileFormatMessage + " Unexpectedly large indent.";

        // Node's level is determined by indent.
        nodeStack.erase(nodeStack.begin() + 1 + indent, nodeStack.end());

        const bool playable = (line[indent] == itemSymbol);
        std::string name = std::move(line).substr(indent + 1);
        nodeStack.back()->children_.push_back(Node(std::move(name), playable));

        nodeStack.emplace_back(& nodeStack.back()->children_.back());
    }

    if (is.is_open() && ! is.bad()) {
        try {
            validate();
        }
        catch (Error & e) {
            return e.what();
        }
        return std::string();
    }
    else
        return "reading file failed.";
}

bool Tree::save(const std::string & filename) const
{
    std::ofstream os(filename);
    for (const Node & topNode : root_.children())
        print(os, topNode);
    return os;
}

std::string Tree::getItemAbsolutePath(const int itemId) const
{
    std::string path = root_.getRelativeChildItemPath(itemId);
    assert(path.at(0) == '/');
    // remove extra '/'.
    path.erase(0, 1);
    return path;
}

void Tree::insertItem(std::string absolutePath)
{
    root_.insertItem(std::move(absolutePath));
}

void Tree::nodesChanged()
{
    root_.recalculateItemCount(0);
}

void Tree::cleanUp()
{
    root_.cleanUp();
}

void Tree::validate() const
{
    root_.validate();
}



RandomItemChooser::RandomItemChooser()
    : RandomItemChooser(
        std::chrono::system_clock::now().time_since_epoch().count())
{
}

RandomItemChooser::RandomItemChooser(const Seed seed) : engine_(seed)
{
}

int RandomItemChooser::randomItemId(const Tree & tree)
{
    if (tree.itemCount() == 0)
        throw Error("can not choose random Item from tree without Items.");
    const int maxId = tree.itemCount() - 1;
    // If tree was changed since the last call to this method, update cached
    // distribution_.
    if (distribution_.b() != maxId)
        distribution_ = Distribution(0, maxId);
    return distribution_(engine_);
}

}
