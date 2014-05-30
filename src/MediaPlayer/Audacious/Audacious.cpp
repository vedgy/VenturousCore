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

# include "Audacious.hpp"

# include "PlayerUtilities.hpp"

# include <QtCoreUtilities/String.hpp>

# include <QString>
# include <QStringList>

# include <utility>
# include <chrono>
# include <thread>


namespace
{
/// Number of milliseconds that should be enough for Audacious to start
/// responding to commands after being launched.
constexpr int maxStartMs = 3000;
}


const QString & Audacious::playerName() const
{
    return AudaciousTools::playerName();
}

void Audacious::setEssentialOptions()
{
    AudaciousTools::setEssentialOptions();
}

void MediaPlayer::setPlayerWindowVisible(const bool visible)
{
    AudaciousTools::setMainWindowVisible(visible);
}

bool Audacious::start()
{
    if (AudaciousTools::isPlaying())
        return true;
    return start(playerArguments_);
}

bool Audacious::start(const std::string & pathToItem)
{
    return start(playerArguments_ +
                 QStringList { AudaciousTools::addToTemporaryPlaylistArg(),
                               QtUtilities::toQString(pathToItem)
                             });
}

bool Audacious::start(const std::vector<std::string> & pathsToItems)
{
    QStringList arguments = playerArguments_;
    arguments.reserve(arguments.size() + 1 + int(pathsToItems.size()));
    arguments << AudaciousTools::addToTemporaryPlaylistArg();
    for (const std::string & path : pathsToItems)
        arguments << QtUtilities::toQString(path);
    return start(arguments);
}


Audacious::Audacious(QStringList arguments)
    : playerArguments_(std::move(arguments))
{
    setOptionsTimer_.setInterval(maxStartMs);
    connect(& setOptionsTimer_, SIGNAL(timeout()), SLOT(onSetOptionsTimeout()));
    hideWindowTimer_.setInterval(10);
    connect(& hideWindowTimer_, SIGNAL(timeout()), SLOT(onHideWindowTimeout()));
}

void Audacious::launchedPlayer()
{
    if (autoSetOptions())
        setOptionsTimer_.start();
    if (autoHideWindow()) {
        hideWindowTimer_.start();
        hideWindowTimerStartTime_ = SteadyClock::now();
    }
}

void Audacious::exitingPlayer()
{
    setOptionsTimer_.stop();
    hideWindowTimer_.stop();
}


void Audacious::onSetOptionsTimeout()
{
    setOptionsTimer_.stop();
    AudaciousTools::setEssentialOptions();
}

void Audacious::onHideWindowTimeout()
{
    if (AudaciousTools::isRunning()) {
        hideWindowTimer_.stop();
        AudaciousTools::setMainWindowVisible(false);
    }
    else if (SteadyClock::now() - hideWindowTimerStartTime_ >=
             std::chrono::milliseconds(maxStartMs)) {
        hideWindowTimer_.stop();
    }
}



namespace AudaciousTools
{
namespace
{
AUDACIOUS_TOOLS_STRING_CONSTANT(on, "on\n")
AUDACIOUS_TOOLS_STRING_CONSTANT(off, "off\n")

/// @brief Sets Audacious playlist option to specified value - desiredStatus.
/// @param optionName Playlist option to be set.
void setStatus(const QString & optionName, const QString & desiredStatus)
{
    const QString baseCommand = toolCommand() + " playlist-" + optionName;
    const QString stdOut =
        PlayerUtilities::executeAndGetOutput(baseCommand + "-status");
    if (stdOut != desiredStatus)
        PlayerUtilities::execute(baseCommand + "-toggle");
}

}


bool isRunning()
{
    return ! PlayerUtilities::executeAndGetOutput(
               toolCommand() + " version").isEmpty();
}

bool isPlaying()
{
    return PlayerUtilities::executeAndGetOutput(
               toolCommand() + " playback-status") == "playing\n";
}

void requestQuit()
{
    PlayerUtilities::execute(toolCommand() + " shutdown");
}

void quit()
{
    while (isRunning()) {
        requestQuit();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void setEssentialOptions()
{
    if (isRunning()) {
        setStatus("auto-advance", on());
        setStatus("repeat", off());
        setStatus("shuffle", off());
        setStatus("stop-after", off());
    }
}

void setMainWindowVisible(const bool visible)
{
    PlayerUtilities::execute(toolCommand() + " mainwin-show " +
                             (visible ? "on" : "off"));
}

}
