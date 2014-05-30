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

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
# include <iostream>
# endif


# include "MediaPlayer.hpp"

# include <QString>
# include <QStringList>
# include <QObject>

# include <cstddef>
# include <cassert>
# include <array>
# include <stdexcept>
# include <memory>


namespace
{
typedef std::unique_ptr<MediaPlayer> MediaPlayerPtr;
}

namespace GetMediaPlayer
{
const QStringList & playerList()
{
    static const QStringList l = { QObject::tr("Audacious (detached)"),
                                   QObject::tr("Audacious (managed)")
                                 };
    return l;
}

bool isExternalPlayerProcessDetached(const int id)
{
    constexpr std::array<bool, 2> isDetached = {{ true, false }};
    assert(id >= 0 && std::size_t(id) < isDetached.size());
    return isDetached[id];
}

MediaPlayerPtr instance(const int id)
{
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << "Player with id=" << id << " was requested." << std::endl;
# endif
    if (id != 0)
        ;/// TODO: reset Audacious settings.

    switch (id) {
        case 0:
            /// TODO: set Audacious settings.
            return MediaPlayerPtr();
        case 1:
            return MediaPlayerPtr();
        default:
            throw std::out_of_range("id is out of player list bounds.");
    }
}

}
