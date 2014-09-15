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

# include "DetachedAudacious.hpp"
# include "ManagedAudacious.hpp"

# include <QString>
# include <QStringList>
# include <QObject>

# include <cstddef>
# include <cassert>
# include <utility>
# include <array>
# include <stdexcept>
# include <memory>


namespace
{
typedef std::unique_ptr<MediaPlayer> MediaPlayerPtr;
}

const QString & MediaPlayer::toString(const Status status)
{
    static const std::array<QString, 3> statuses = {{
            tr("stopped"), tr("paused"), tr("playing")
        }
    };
    const std::size_t index = static_cast<std::size_t>(status);
    assert(index < statuses.size());
    return statuses[index];
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
    return isDetached[std::size_t(id)];
}

std::pair<MediaPlayerPtr, QStringList> instance(const int id)
{
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << "Player with id=" << id << " was requested." << std::endl;
# endif

    MediaPlayerPtr player;
    QStringList errors;

    const auto addErrorIfNotEmpty = [& errors](const QString & errorMessage) {
        if (! errorMessage.isEmpty())
            errors << errorMessage;
    };

    enum : int { detachedAudacious = 0, managedAudacious = 1 };

    if (id != detachedAudacious)
        addErrorIfNotEmpty(ConfigureDetachedAudacious::resetSettings());

    switch (id) {
        case detachedAudacious:
            addErrorIfNotEmpty(ConfigureDetachedAudacious::setSettings());
            player.reset(new DetachedAudacious);
            break;
        case managedAudacious:
            player.reset(new ManagedAudacious);
            break;
        default:
            throw std::out_of_range("id is out of player list bounds.");
    }

    return { std::move(player), std::move(errors) };
}

}
