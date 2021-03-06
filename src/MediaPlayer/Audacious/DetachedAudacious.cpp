/*
 This file is part of VenturousCore.
 Copyright (C) 2014, 2015 Igor Kushnir <igorkuo AT Google mail>

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

# include "DetachedAudacious.hpp"

# include "PlayerUtilities.hpp"

# include <CommonUtilities/ExceptionsToStderr.hpp>

# include <QString>
# include <QStringList>


DetachedAudacious::DetachedAudacious()
    : Audacious( { AudaciousTools::startPlaybackArg() })
{}

DetachedAudacious::~DetachedAudacious() noexcept
{
    CommonUtilities::exceptionsToStderr([this] {
        if (exitExternalPlayerOnQuit())
            exitPlayer();
    }, VENTUROUS_CORE_ERROR_PREFIX "In ~DetachedAudacious(): ");
}

bool DetachedAudacious::isRunning() const
{
    return AudaciousTools::isRunning();
}

MediaPlayer::Status DetachedAudacious::status() const
{
    return AudaciousTools::status();
}

void DetachedAudacious::togglePause()
{
    AudaciousTools::togglePause();
}

void DetachedAudacious::exitPlayer()
{
    exitingPlayer();
    AudaciousTools::requestQuit();
}


bool DetachedAudacious::start(const QStringList & arguments)
{
    PlayerUtilities::startDetached(AudaciousTools::playerCommand(), arguments);
    if (! isRunning())
        launchedPlayer();
    return true;
}
