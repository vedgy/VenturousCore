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

# include <QString>
# include <QStringList>
# include <QObject>

# include <utility>
# include <vector>
# include <string>
# include <memory>


class MediaPlayer : public QObject
{
    Q_OBJECT
public:
    /// @brief If (isRunning() &&
    ///                 (ExitExternalPlayerOnQuit || {process is managed})) {
    /// exits external player process }.
    virtual ~MediaPlayer() = default;

    /// @return Name of the external player.
    virtual const QString & playerName() const = 0;
    /// @brief Sets essential external player options. Should be called while
    /// external player is running; otherwise may have no effect.
    virtual void setEssentialOptions() = 0;
    /// @brief Shows/hides external player window. Should be called while
    /// external player is running; otherwise may have no effect.
    virtual void setPlayerWindowVisible(bool visible) = 0;

    /// @return true if player process is running.
    /// NOTE: if a given MediaPlayer descendant uses managed external player,
    /// this method only checks the state of attached process.
    /// Independent player process is not taken into account in this case.
    virtual bool isRunning() const = 0;


    /// @brief All start() overloads ensure playing state of external player.
    /// Player process may be started/restarted or just commanded to play.
    /// @return false if player process has failed to start or exited
    /// immediatelly; true otherwise.

    /// @brief Starts playback of current external player's playlist.
    virtual bool start() = 0;
    /// @brief Creates external player playlist with specified item and starts
    /// playing it.
    /// @param pathToItem Absolute path to playable item.
    virtual bool start(const std::string & pathToItem) = 0;
    /// @brief Creates external player playlist with specified items and starts
    /// playing it.
    /// @param pathsToItems Absolute paths to playable items.
    virtual bool start(const std::vector<std::string> & pathsToItems) = 0;

    /// @brief Blocks signals and exits external player process.
    /// NOTE: if a given MediaPlayer descendant uses managed external player,
    /// only managed process would be exited.
    virtual void exit() = 0;

    /// @brief If AutoSetOptions property is set to true [default], essential
    /// player options are set each time external player process is
    /// started/restarted. This ensures that correct options are in use even if
    /// user manually set wrong options.
    /// However if user wants manual options handling or never changes them and
    /// wants better performance, this property can be set to false.
    bool autoSetOptions() const { return autoSetOptions_; }
    void setAutoSetOptions(bool autoSet) { autoSetOptions_ = autoSet; }

    /// @brief If AutoHideWindow property is set to true, external player
    /// window is hidden each time external player process is started/restarted.
    bool autoHideWindow() const { return autoHideWindow_; }
    void setAutoHideWindow(bool autoHide) { autoHideWindow_ = autoHide; }

    /// @brief If ExitExternalPlayerOnQuit property is set to true [default],
    /// external player process is finished in destructor if isRunning();
    /// otherwise it would remain running.
    /// NOTE: if player process is managed (attached), it is impossible to
    /// detach and leave it running. In this case the property is ignored.
    bool exitExternalPlayerOnQuit() const { return exitExternalPlayerOnQuit_; }
    void setExitExternalPlayerOnQuit(bool exitOnQuit) {
        exitExternalPlayerOnQuit_ = exitOnQuit;
    }

signals:
    /// @brief Is emitted after managed player process has finished execution.
    /// @param crashExit True if external player crashed.
    /// @param exitCode Exit code of external player. Is valid only if
    /// (crashExit == false).
    /// @param errors List of external player errors.
    /// @param missingFilesAndDirs List of missing items reported by
    /// external player.
    void finished(bool crashExit, int exitCode, QStringList errors,
                  QStringList missingFilesAndDirs);

    /// Is emitted when error happens in managed player process.
    /// @param errorMessage Message that describes error, suitable for
    /// displaying to user.
    /// NOTE: if process fails to start, finished() would not be emitted.
    /// isRunning() would return false in this case (if called from error()
    /// signal handler).
    void error(QString errorMessage);

protected:
    MediaPlayer() = default;

private:
    bool autoSetOptions_ = true;
    bool autoHideWindow_ = false;
    bool exitExternalPlayerOnQuit_ = true;
};


namespace GetMediaPlayer
{
/// @return List of available players.
const QStringList & playerList();

/// @param id Index in [0, playerList().size()).
/// @return true if player with specified id uses detached external player
/// process.
bool isExternalPlayerProcessDetached(int id);

/// @param id Index in [0, playerList().size()).
/// @return pair (MediaPlayer instance with specified id,
/// list of error messages).
/// NOTE: MediaPlayer descendants do not start external player process in
/// constructor, so (instance(id).first->isRunning() == false).
std::pair<std::unique_ptr<MediaPlayer>, QStringList> instance(int id);
}

# endif // VENTUROUS_CORE_MEDIA_PLAYER_HPP
