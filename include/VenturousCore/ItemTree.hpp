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

# ifndef VENTUROUS_CORE_ITEM_TREE_HPP
# define VENTUROUS_CORE_ITEM_TREE_HPP

# include <cstdint>
# include <vector>
# include <deque>
# include <string>
# include <stdexcept>
# include <random>


namespace ItemTree
{
class Error : public std::runtime_error
{
public:
    explicit Error(const std::string & sWhat) : std::runtime_error(sWhat) {}
};


/// @brief Playable entity (directory or file) is called "Item".\n
/// Node can be either Item or directory, which contains Items at some nesting
/// level.
class Node
{
public:
    const std::string & name() const { return name_; }
    bool isPlayable() const { return playable_; }

    const std::vector<Node> & children() const { return children_; }
    /// NOTE (2).
    std::vector<Node> & children() { return children_; }

    /// @return Number of playable descendants. If this node is playable, it
    /// is included in itemCount() too.
    int itemCount() const;

    /// NOTE (1).
    void setPlayable(bool playable) { playable_ = playable; }

    /// @return Pointer to child with specified name. If there is no such child,
    /// nullptr is returned.
    /// NOTE (2).
    Node * child(const std::string & name);

    /// @return Pointer to descendant with path, specified by [begin, end).
    /// If there is no such child, nullptr is returned. In this case not all
    /// iterators in the specified range may be reached.
    /// NOTE (2).
    template <typename InputStringIterator>
    Node * descendant(InputStringIterator begin, InputStringIterator end);

    /// @return Collection of pointers to successive descendants with path,
    /// specified by [begin, end). If there is no child with specified name at
    /// some point, nullptr is placed at the corresponding collection's position
    /// and collection ends on this position. In this case returned collection's
    /// size can be smaller than std::distance(begin, end). Also in this case
    /// not all iterators in the specified range may be reached.
    /// NOTE (2).
    template <typename InputStringIterator>
    std::deque<Node *> descendantPath(
        InputStringIterator begin, InputStringIterator end);

    /// @tparam StringCollection std::string collection.
    /// Must have STL-style (resize() or push_back() method) and iterators.
    /// @return StringCollection with paths to all Items - descendants of this
    /// node. If this node is playable, its path is added too. All paths start
    /// with this->name().
    template<class StringCollection = std::vector<std::string>>
    StringCollection getAllItems() const;

private:
    friend class Tree;

    friend bool operator == (const Node &, const Node &);

    explicit Node(std::string name, bool playable);

    /// @param relativeId Id shift relative to this node.
    /// @throw Error If there are not enough children.
    /// @return Specified child Item's path relative to this node.
    /// NOTE: this method is called for root node to avoid extra '/'.
    /// This method is also called by getChildItemPath().
    std::string getRelativeChildItemPath(int relativeId) const;

    /// @param relativeId Id shift relative to this node.
    /// @throw Error If there are not enough children.
    /// @return Specified child Item's path, which includes this node's name.
    std::string getChildItemPath(int relativeId) const;

    /// @brief Appends all playable descendants' paths (including this node)
    /// to ends of strings, starting from begin. this->itemCount() paths will be
    /// appended, each path will start with this->name().
    /// @return Iterator past the last of the modified strings (equal to
    /// std::next(begin, this->itemCount())).
    template<typename ForwardStringIterator>
    ForwardStringIterator addAllItems(ForwardStringIterator begin) const;

    /// @brief Appends all playable descendants' paths, relative to this node,
    /// to ends of strings, starting from begin.
    /// If this node is an Item, fixes its path.
    template<typename ForwardStringIterator>
    void addAllItemsRelative(ForwardStringIterator begin) const;

    /// @brief Inserts new Item as a descendant. If descendant with specified
    /// name already exists, it becomes (or remains) an Item.
    /// @param relativePath Path to the new Item relative to this node.
    /// NOTE (1).
    void insertItem(std::string relativePath);

    /// @brief Recalculates accumulatedItemCount_ for current node and its
    /// descendants.
    /// @param precedingCount Accumulated Item count before this node.
    void recalculateItemCount(int precedingCount);

    /// @brief Removes nodes, which are not Items and have no playable
    /// descendants.
    void cleanUp();

    /// @throw Error If this node is invalid (has descendants with empty or
    /// duplicate names, children() are not sorted properly).
    void validate() const;


    /// Name of file or directory.
    std::string name_;
    /// Specifies whether this node is an Item or just an intermediate
    /// directory (or even unplayable [meaningless] file).
    bool playable_;
    /// Number of Items before {next node on the same level as this node}.
    int accumulatedItemCount_;
    /// Collection of nodes, which are contained in this node's directory.
    /// This collection is always sorted by name­_, is empty for file-nodes.
    std::vector<Node> children_;
};

inline bool operator == (const Node & lhs, const Node & rhs)
{
    return lhs.name_ == rhs.name_ && lhs.playable_ == rhs.playable_
           && lhs.accumulatedItemCount_ == rhs.accumulatedItemCount_
           && lhs.children_ == rhs.children_;
}

inline bool operator != (const Node & lhs, const Node & rhs)
{
    return !(lhs == rhs);
}


class Tree
{
public:
    /// @brief Constructs empty tree.
    explicit Tree();

    /// @brief Removes all existing nodes and loads tree from file.
    /// @return Empty string if loading was successful. Error message otherwise.
    /// NOTE: if error occurs, this tree will be in undefined (maybe invalid)
    /// state.
    /// NOTE (1).
    std::string load(const std::string & filename);

    /// @brief Saves tree to file.
    /// @return true if saving was successful.
    bool save(const std::string & filename) const;

    int itemCount() const { return root_.accumulatedItemCount_; }

    /// @return {subdirectories of root directory} or {disks}, which were
    /// added to this tree.
    const std::vector<Node> & topLevelNodes() const { return root_.children(); }
    /// NOTE (2).
    std::vector<Node> & topLevelNodes() { return root_.children(); }

    /// NOTE (2).
    Node * child(const std::string & name) { return root_.child(name); }

    /// NOTE (2).
    template <typename InputStringIterator>
    Node * descendant(InputStringIterator begin, InputStringIterator end)
    { return root_.descendant(begin, end); }

    /// NOTE (2).
    template <typename InputStringIterator>
    std::deque<Node *> descendantPath(
        InputStringIterator begin, InputStringIterator end)
    { return root_.descendantPath(begin, end); }

    template<class StringCollection = std::vector<std::string>>
    StringCollection getAllItems() const {
        return root_.getAllItems<StringCollection>();
    }

    /// @param itemId Sequence number of required Item (starting from 0).
    /// @return Specified Item's absolute path.
    std::string getItemAbsolutePath(int itemId) const;

    /// @brief Inserts new Item in the tree. If node with specified name is
    /// already present in this tree, it becomes (or remains) an Item.
    /// @param absolutePath Path to the new Item.
    /// NOTE (1).
    void insertItem(std::string absolutePath);

    /// @brief This method must be called after one or more calls of
    /// non-const Tree's or Node's methods; before itemCount(), getAllItems(),
    /// getItemAbsolutePath(), cleanUp(), comparing nodes or trees.
    void nodesChanged();

    /// @brief Removes non-playable nodes with no playable descendants.
    void cleanUp();

    /// @throw Error If this tree is invalid (empty or duplicate names,
    /// not sorted properly children).
    /// NOTE: Tree should never enter invalid state. If this method throws, it
    /// means improper usage.
    void validate() const;

private:
    friend bool operator == (const Tree &, const Tree &);

    /// Although some systems do not have common root, this field is used
    /// to simplify code. root_ always has empty name and is not playable.
    /// Absolute paths, which don't start with '/', are also supported.
    Node root_ { std::string(), false };
};

inline bool operator == (const Tree & lhs, const Tree & rhs)
{
    return lhs.root_ == rhs.root_;
}

inline bool operator != (const Tree & lhs, const Tree & rhs)
{
    return !(lhs == rhs);
}


class RandomItemChooser
{
public:
    typedef std::uint_fast32_t Seed;

    /// @brief Constructs random engine using current time as a seed.
    explicit RandomItemChooser();

    /// @brief Constructs random engine using parameter value as a seed.
    explicit RandomItemChooser(Seed seed);

    /// @brief Returns random itemId in the tree.
    /// @throw Error If there are no Items in the tree.
    int randomItemId(const Tree & tree);

    /// @brief Returns absolute path to next random Item in the tree.
    /// @throw Error If there are no Items in the tree.
    std::string randomPath(const Tree & tree) {
        return tree.getItemAbsolutePath(randomItemId(tree));
    }

private:
    typedef std::uniform_int_distribution<int> Distribution;

    /// NOTE: distribution bounds are determined by tree.itemCount(), which is
    /// not expected to change often. So the distribution is cached.
    /// Primary reason for caching is not performance, but possible dependency
    /// between subsequent values, so for some Distribution implementation
    /// caching might lead to better randomness.
    Distribution distribution_ { 0, 0 };

    std::mt19937 engine_;
};


/// FOOTNOTES:
/// 1. Tree::nodesChanged() must be called after calling this method.
/// 2. Tree::nodesChanged() must be called after modifying tree.

}

# endif // VENTUROUS_CORE_ITEM_TREE_HPP
