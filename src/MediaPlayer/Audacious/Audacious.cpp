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

# include "Audacious.hpp"

# include "PlayerUtilities.hpp"

# include <QtCoreUtilities/String.hpp>

# include <QString>
# include <QStringList>

# include <cstddef>
# include <utility>
# include <string>
# include <stdexcept>
# include <chrono>
# include <thread>
# include <iostream>


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

void Audacious::setPlayerWindowVisible(const bool visible)
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

/// @brief Assigns value to Audacious playlist option.
/// @param optionName Playlist option to be set.
void setOption(const QString & optionName, const QString & value)
{
    const QString baseCommand = toolCommand() + " playlist-" + optionName;
    const QString stdOut =
        PlayerUtilities::executeAndGetOutput(baseCommand + "-status");
    if (stdOut != value)
        PlayerUtilities::execute(baseCommand + "-toggle");
}


/// @return String returned by audtool playback-status command.
QString statusString()
{
    return PlayerUtilities::executeAndGetOutput(
               toolCommand() + " playback-status");
}

/// Statuses returned by audtool playback-status command.
namespace Status
{
AUDACIOUS_TOOLS_STRING_CONSTANT(paused, "paused\n")
AUDACIOUS_TOOLS_STRING_CONSTANT(playing, "playing\n")
}

}


QString versionString()
{
    const QString output =
        PlayerUtilities::executeAndGetOutput(playerCommand() + " -v");

    int start = playerName().size();
    if (! output.startsWith(playerName()) || output[start] != ' ')
        return QString();
    ++start;
    int end = output.indexOf(' ', start);
    if (end < 0)
        end = output.size();

    return output.mid(start, end - start);
}

Version version()
{
    const std::string str = QtUtilities::qStringToString(versionString());
    if (! str.empty()) {
        try {
            Version result;
            std::size_t pos;
            result.major = std::stoi(str, & pos);
            result.minor = 0;
            if (pos < str.size()) {
                if (str[pos] != '.') {
                    throw std::logic_error(
                        "missing period in the version string");
                }
                if (++pos < str.size())
                    result.minor = std::stoi(str.substr(pos));
            }
            return result;
        }
        catch (const std::logic_error & e) {
            const std::string name = QtUtilities::qStringToString(playerName());
            std::cerr << VENTUROUS_CORE_ERROR_PREFIX "Unexpected " << name
                      << " version format. Assuming that " << name
                      << " is not installed. Error: " << e.what() << std::endl;
        }
    }
    return { -1, -1 };
}

bool isRunning()
{
    return ! PlayerUtilities::executeAndGetOutput(
               toolCommand() + " version").isEmpty();
}

MediaPlayer::Status status()
{
    const QString status = statusString();
    if (status == Status::playing())
        return MediaPlayer::Status::playing;
    if (status == Status::paused())
        return MediaPlayer::Status::paused;
    return MediaPlayer::Status::stopped;
}

bool isPlaying()
{
    return statusString() == Status::playing();
}

void togglePause()
{
    PlayerUtilities::execute(toolCommand() + " playback-pause");
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
        setOption("auto-advance", on());
        setOption("repeat", off());
        setOption("shuffle", off());
        setOption("stop-after", off());
    }
}

void setMainWindowVisible(const bool visible)
{
    PlayerUtilities::execute(toolCommand() + " mainwin-show " +
                             (visible ? "on" : "off"));
}

}
