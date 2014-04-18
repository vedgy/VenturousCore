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

# ifndef VENTUROUS_CORE_MEDIA_PLAYER_HPP
# define VENTUROUS_CORE_MEDIA_PLAYER_HPP

# include <functional>
# include <vector>
# include <string>
# include <stdexcept>
# include <memory>


class MediaPlayer
{
public:
    class Error : public std::runtime_error
    {
    public:
        explicit Error(const std::string & sWhat) : std::runtime_error(sWhat) {}
    };

    /// This type is used for external function, which is called after task is
    /// finished.
    /// @param crashExit True if external player crashed.
    /// @param exitCode Exit code of external player.
    /// @param missingFilesAndDirs If this vector is not empty,
    /// there were errors in the last task. This collection contains all
    /// missing items reported by external player.
    typedef std::function < void(bool crashExit, int exitCode,
                                 std::vector<std::string> missingFilesAndDirs) >
    FinishedSlot;
    /// This type is used for external function, which is called after
    /// external process error happens.
    /// @param errorMessage Message that describes error, suitable for
    /// displaying to user.
    typedef std::function<void(std::string errorMessage)> ErrorSlot;

    /// @return Name of the external player.
    static std::string playerName();

    /// @brief Performs quick initialization.
    /// Does not start media player process.
    explicit MediaPlayer();

    /// @brief Ensures that external player quits gracefully if isRunning().
    ~MediaPlayer();

    /// @brief Returns true if player process that was started by this
    /// instance of MediaPlayer is running.
    bool isRunning() const;

    /// @brief Replaces current finishedSlot with specified value.
    void setFinishedSlot(FinishedSlot slot);
    /// @brief Replaces current errorSlot with specified value.
    void setErrorSlot(ErrorSlot slot);

    /// @brief If (isRunning() == true) does nothing. Otherwise, starts
    /// (or restarts) external player.
    /// Playback of current external player's playlist will be started.
    void start();

    /// @brief Starts player with specified path to item.
    /// @param pathToItem Absolute path to playable item.
    void start(const std::string & pathToItem);

    /// @brief Starts player with specified paths to items.
    /// @param pathsToItems Absolute paths to playable items.
    void start(const std::vector<std::string> & pathsToItems);

    /// @brief Sets recommended external player options. Should be called while
    /// external player is running. Otherwise, may have no effect.
    void setRecommendedOptions();

    /// @brief If AutoSetOptions property is set to true [default], recommended
    /// player options are set each time start() is called. This ensures that
    /// correct options are in use even if user manually changed them before.
    /// However if user wants manual options handling or never changes them and
    /// wants better performance, this property can be set to false.

    /// @return Current AutoSetOptions value.
    bool getAutoSetOptions() const;
    /// @param autoSet Desired AutoSetOptions value.
    void setAutoSetOptions(bool autoSet);

    /// @brief Blocks FinishedSlot and quits external player.
    /// If external player is not running, call has no effect.
    void quit();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

# endif // VENTUROUS_CORE_MEDIA_PLAYER_HPP
