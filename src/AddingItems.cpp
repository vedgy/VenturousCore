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

# ifdef DEBUG_VENTUROUS_ADDING_ITEMS
# include <QtCoreUtilities/String.hpp>
# include <iostream>
# endif


# include "AddingItems.hpp"

# include "ItemTree.hpp"

# include <QtCoreUtilities/String.hpp>

# include <QString>
# include <QStringList>
# include <QDir>

# include <cstddef>
# include <algorithm>
# include <string>


namespace AddingItems
{
bool operator == (const Policy & lhs, const Policy & rhs)
{
    return lhs.filePatterns == rhs.filePatterns &&
           lhs.mediaDirFilePatterns == rhs.mediaDirFilePatterns &&
           lhs.addFiles == rhs.addFiles &&
           lhs.addMediaDirs == rhs.addMediaDirs &&
           lhs.ifBothAddFiles == rhs.ifBothAddFiles &&
           lhs.ifBothAddMediaDirs == rhs.ifBothAddMediaDirs;
}


namespace
{
class ItemAdder
{
public:
    explicit ItemAdder(const QString & dirName, const Policy & policy,
                       ItemTree::Tree & itemTree);

    void addItems();

private:
    /// @brief Recursively adds items from dir_ to itemTree_. Starts with adding
    /// files, then considers adding media dirs.
    void addFilesFirst();
    /// @brief Recursively adds items from dir_ to itemTree_. Considers adding
    /// media dirs first; if some dir isn't media dir, considers adding files
    /// from it.
    void addMediaDirFirst();

    /// @return List of all files in dir_ that match policy.filePatterns.
    QStringList getFileList() const;
    /// @return true if dir_ is media dir.
    bool isMediaDir() const;
    /// @brief Adds all suitable files from dir_ to itemTree_.
    /// @return true if at least one file was added.
    bool addFiles();
    /// @brief Adds dir_ to itemTree_.
    void addMediaDir();

    /// @brief Enters dir_'s subdirectory named subdirName;
    /// calls (this->*method)(); restores dir_ and curPath_ to original state.
    template <typename AddMethod>
    void addSubdir(AddMethod method, const QString & subdirName);

    /// @brief Calls addSubdir(method, <name>) for all subdirectories of dir_.
    template <typename AddMethod>
    void addSubdirs(AddMethod method);

    /// Holds current directory.
    QDir dir_;
    /// Holds path to current directory.
    std::string curPath_;

    const Policy & policy_;
    ItemTree::Tree & itemTree_;
};


ItemAdder::ItemAdder(const QString & dirName, const Policy & policy,
                     ItemTree::Tree & itemTree)
    : dir_(dirName), curPath_(QtUtilities::qStringToString(dir_.path())),
      policy_(policy), itemTree_(itemTree)
{
    dir_.setFilter(QDir::Files | QDir::Readable | QDir::Hidden);
    if (policy_.addMediaDirs)
        dir_.setNameFilters(policy_.mediaDirFilePatterns);
}

void ItemAdder::addItems()
{
    if (policy_.addFiles &&
            (! policy_.addMediaDirs || policy_.ifBothAddFiles)) {
        addFilesFirst();
    }
    else
        addMediaDirFirst();
}

void ItemAdder::addFilesFirst()
{
    // This method is only called if files should be added regardless of
    // media dirs.
    const bool filesFound = addFiles();
    if (policy_.addMediaDirs && (! filesFound || policy_.ifBothAddMediaDirs)
            && isMediaDir()) {
        addMediaDir();
    }
    addSubdirs(& ItemAdder::addFilesFirst);
}

void ItemAdder::addMediaDirFirst()
{
    if (isMediaDir()) {
        if (! policy_.addFiles || policy_.ifBothAddMediaDirs ||
                getFileList().empty()) {
            addMediaDir();
        }
        // This method is only called if playable files from media dir
        // should not be added.
    }
    else {
        if (policy_.addFiles)
            addFiles();
    }
    addSubdirs(& ItemAdder::addMediaDirFirst);
}

QStringList ItemAdder::getFileList() const
{
    return dir_.entryList(policy_.filePatterns);
}

bool ItemAdder::isMediaDir() const
{
    // dir_'s name filters are set to mediaDirFilePatterns if
    // (policy.addMediaDirs == true). This is obviously a prerequisite to
    // calling this method.
    return dir_.count() > 0;
}

bool ItemAdder::addFiles()
{
    const QStringList items = getFileList();
    if (items.empty())
        return false;

    std::for_each(items.begin(), items.end(),
    [this](const QString & filename) {
        itemTree_.insertItem(curPath_ + '/' +
                             QtUtilities::qStringToString(filename));

# ifdef DEBUG_VENTUROUS_ADDING_ITEMS
        std::cout << "Added file "
        << QtUtilities::qStringToString(filename) << std::endl;
# endif
    });

    return true;
}

void ItemAdder::addMediaDir()
{
    itemTree_.insertItem(curPath_);

# ifdef DEBUG_VENTUROUS_ADDING_ITEMS
    std::cout << "Added media dir." << std::endl;
# endif
}

template <typename AddMethod>
void ItemAdder::addSubdir(const AddMethod method, const QString & subdirName)
{
    dir_.cd(subdirName);
    const std::size_t prevSize = curPath_.size();
    curPath_ += '/' + QtUtilities::qStringToString(subdirName);

# ifdef DEBUG_VENTUROUS_ADDING_ITEMS
    std::cout << "Entered " << curPath_ << std::endl;
# endif

    (this->*method)();

    dir_.cdUp();
    curPath_.erase(prevSize);
}

template <typename AddMethod>
void ItemAdder::addSubdirs(const AddMethod method)
{
    const QStringList items = dir_.entryList(
                                  QDir::AllDirs | QDir::NoDotAndDotDot);

    std::for_each(items.begin(), items.end(),
    [&](const QString & subdirName) {
        addSubdir(method, subdirName);
    });
}

}


void addDir(const QString & dirName, const Policy & policy,
            ItemTree::Tree & itemTree)
{
    if (! policy.addFiles && ! policy.addMediaDirs)
        return;

    ItemAdder adder(dirName, policy, itemTree);
    adder.addItems();
}

}
