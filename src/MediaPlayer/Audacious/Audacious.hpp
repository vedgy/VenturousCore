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

# ifndef VENTUROUS_CORE_AUDACIOUS_HPP
# define VENTUROUS_CORE_AUDACIOUS_HPP

# include "MediaPlayer.hpp"

# include <QString>
# include <QStringList>
# include <QTimer>

# include <chrono>


class Audacious : public MediaPlayer
{
    Q_OBJECT
public:
    const QString & playerName() const override;
    void setEssentialOptions() override;
    void setPlayerWindowVisible(bool visible) override;

    bool start() override;
    bool start(const std::string & pathToItem) override;
    bool start(const std::vector<std::string> & pathsToItems) override;

protected:
    explicit Audacious(QStringList playerArguments);

    /// @brief Passes arguments to Audacious and ensures its playing status.
    /// @return false if player process has failed to start or exited
    /// immediatelly; true otherwise.
    virtual bool start(const QStringList & arguments) = 0;

    /// @brief Must be called after successful player process launch.
    void launchedPlayer();
    /// @brief Must be called just before or just after exiting player process.
    void exitingPlayer();

    /// Default arguments for player command.
    const QStringList playerArguments_;

private:
    /// Timers are necessary because Audacious is not ready to accept audtool
    /// commands immediately after start.

    QTimer setOptionsTimer_;

    typedef std::chrono::steady_clock SteadyClock;
    typedef std::chrono::time_point<SteadyClock> SteadyTimePoint;
    SteadyTimePoint hideWindowTimerStartTime_;
    QTimer hideWindowTimer_;

private slots:
    void onSetOptionsTimeout();
    void onHideWindowTimeout();
};


namespace AudaciousTools
{
# define AUDACIOUS_TOOLS_STRING_CONSTANT(NAME, VALUE)    \
    inline const QString & NAME() {                      \
    static const QString value{ VALUE }; return value; }

AUDACIOUS_TOOLS_STRING_CONSTANT(playerName, "Audacious")
AUDACIOUS_TOOLS_STRING_CONSTANT(playerCommand, "audacious")
AUDACIOUS_TOOLS_STRING_CONSTANT(startPlaybackArg, "-p")
AUDACIOUS_TOOLS_STRING_CONSTANT(addToTemporaryPlaylistArg, "-E")
AUDACIOUS_TOOLS_STRING_CONSTANT(toolCommand, "audtool")

/// @return true if Audacious process is running.
bool isRunning();

/// @return Status of Audacious as reported by statusString().
MediaPlayer::Status status();

/// @return true if Audacious has playing status.
bool isPlaying();

/// @brief Pauses/unpauses playback.
void togglePause();

/// @brief Issues a command to quit Audacious.
void requestQuit();

/// @brief Calls requestQuit() while isRunning().
void quit();

/// @brief Sets essential Audacious options.
void setEssentialOptions();

/// @brief Sets Audacious main window visibility to visible.
void setMainWindowVisible(bool visible);

}

# endif // VENTUROUS_CORE_AUDACIOUS_HPP
