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

# ifndef VENTUROUS_CORE_HISTORY_HPP
# define VENTUROUS_CORE_HISTORY_HPP

# include <cstddef>
# include <vector>
# include <deque>
# include <string>
# include <stdexcept>


/// Stores most recent entries at the front.
/// Removes oldest entries if items().size() exceeds maxSize() so that
/// condition items().size() <= maxSize() is always true.
/// Entries are guaranteed to be erased only from the back. The only exception:
/// remove() method erases arbitrary entries.
/// Enties are gauranteed to be added only at the front.
class History
{
public:
    class Error : public std::runtime_error
    {
    public:
        explicit Error(const std::string & sWhat) : std::runtime_error(sWhat) {}
    };

    /// @brief Clears history and loads entries from file. Not more than
    /// maxSize() entries will be read.
    /// @return true if loading was successful.
    bool load(const std::string & filename);

    /// @brief Saves history to file.
    /// @return true if saving was successful.
    bool save(const std::string & filename) const;

    const std::deque<std::string> & items() const { return items_; }

    /// @return Maximum allowed value of maxSize.
    std::size_t maxMaxSize() const { return items_.max_size() - 1; }

    std::size_t maxSize() const { return maxSize_; }
    /// @brief Sets maxSize and removes entries from the back until
    /// items().size() <= maxSize().
    void setMaxSize(std::size_t maxSize);

    /// @return items().at(index) with first nHiddenDirs directories removed
    /// from the path. If nHiddenDirs exceeds number of directories in the
    /// entry, filename is returned.
    /// @throw Error If nHiddenDirs < 0.
    std::string getRelativePath(std::size_t index, int nHiddenDirs) const;

    /// @brief Adds entry to the history.
    /// @throw Error If entry.empty() == true.
    void push(std::string entry);

    /// @brief Removes specified indices from the history.
    /// @param indices Collection of indices, may be not sorted.
    /// @throw Error If at least one index is bigger or equal to items().size().
    void remove(std::vector<std::size_t> indices);

    /// @brief Clears the history.
    void clear() { items_.clear(); }

private:
    std::deque<std::string> items_;
    std::size_t maxSize_ = 100;
};

# endif // VENTUROUS_CORE_HISTORY_HPP
