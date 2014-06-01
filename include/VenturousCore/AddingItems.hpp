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

# ifndef VENTUROUS_CORE_ADDING_ITEMS_HPP
# define VENTUROUS_CORE_ADDING_ITEMS_HPP

# include <QString>
# include <QStringList>


namespace ItemTree
{
class Tree;
}

namespace AddingItems
{
inline const QStringList & allMetadataPatterns()
{
    static const QStringList l { "*.cue" };
    return l;
}

inline const QStringList & allAudioPatterns()
{
    static const QStringList l {
        "*.mp3",
        "*.m4a", "*.m4b", "*.m4p", "*.m4v", "*.m4r", "*.3gp", "*.mp4", "*.aac",
        "*.ogg", "*.oga",
        "*.flac",
        "*.wv",
        "*.shn",
        "*.mpc", "*.mp+", "*.mpp",
        "*.tta",
        "*.wma"
    };
    return l;
}

struct Policy {
    /// Files that match filePatterns can be added as items in itemTree.
    QStringList filePatterns = allMetadataPatterns();
    /// Directory is considered to be media dir if it contains (as direct
    /// children!) files that match mediaDirFilePatterns.
    QStringList mediaDirFilePatterns = allAudioPatterns();

    /// If true, files that match filePatterns are inserted in itemTree.
    bool addFiles = true;
    /// If true, media dirs are inserted in itemTree.
    bool addMediaDirs = true;

    /// Next 2 flags are considered only if
    /// (addFiles && addDirsWithMedia == true) and files that match
    /// filePatterns were found in media dir. Let's denote such a situation as
    /// 'BothFound'. All 4 combinations of next 2 flags are allowed.

    /// If true, files that match filePatterns are added
    /// in case of BothFound.
    bool ifBothAddFiles = true;
    /// If true, media dir is added in case of BothFound.
    bool ifBothAddMediaDirs = false;
};

bool operator == (const Policy & lhs, const Policy & rhs);

inline bool operator != (const Policy & lhs, const Policy & rhs)
{
    return !(lhs == rhs);
}


/// @brief Scans specified directory and its subdirectories recursively
/// according to policy. Inserts found items in itemTree.
/// @param dirName Absolute path to directory.
void addDir(const QString & dirName, const Policy & policy,
            ItemTree::Tree & itemTree);

}

# endif // VENTUROUS_CORE_ADDING_ITEMS_HPP
