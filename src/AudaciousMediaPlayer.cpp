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
# include <QtCoreUtilities/String.hpp>
# include <iostream>
# endif


# include "MediaPlayer.hpp"

# include <QtCoreUtilities/String.hpp>

# include <QString>
# include <QStringList>
# include <QObject>
# include <QTimer>
# include <QProcess>

# include <cstddef>
# include <utility>
# include <algorithm>
# include <array>
# include <vector>
# include <set>
# include <string>
# include <chrono>
# include <thread>
# include <iostream>


namespace
{
void execute(const QString & command)
{
    QProcess::execute(command);
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << QtUtilities::qStringToString(command) << std::endl;
# endif
}

/// @brief Executes command and returns its stdout.
QString executeAndGetOutput(const QString & command)
{
    QProcess process;
    process.start(command);
    process.waitForFinished(-1);
    return process.readAllStandardOutput();
}

namespace Audacious
{
const std::string playerName = "Audacious",
                  /// The following strings must match Audacious error message
                  /// format.
                  missingFilesAndDirsStart = "Cannot open ",
                  missingFilesAndDirsEnd = ": No such file or directory",
                  errorStart = " *** ERROR:",
                  libcueFile = "libcue.so";

const QString commandName = "audacious";

/// Start playing current playlist, quit on playback stop.
const QStringList arguments { "-p", "-q" };

const QString addToTemporaryPlaylistArg = "-E";

bool equalSubstr(const std::string & lhs, std::size_t lhsPos,
                 const std::string & rhs)
{
    return lhs.compare(lhsPos, rhs.size(), rhs) == 0;
}

struct StderrInfo {
    std::string errorMessage;
    std::vector<std::string> missingFilesAndDirs;
};

/// @param errors Audacious stderr.
/// @return StderrInfo with information from errors.
StderrInfo analyzeErrors(const std::string & errors)
{
    StderrInfo info;
    std::set<std::string> encounteredNames;
    std::size_t prevEnd = 0;
    while (true) {
        const std::size_t end = errors.find(missingFilesAndDirsEnd, prevEnd);
        if (end == std::string::npos)
            break;
        std::size_t start;
        {
            // Set {start} to the position just after last '\n' character in
            // interval [prevEnd, end).
            const auto rEnd = errors.rend();
            start = rEnd - std::find(rEnd - end, rEnd - prevEnd, '\n');
            if (start == prevEnd && start != 0) { // not found
                std::cerr << VENTUROUS_CORE_ERROR_PREFIX "Unexpected "
                          << playerName << " stderr format. Aborted parsing."
                          << std::endl;
                return info;
            }
        }

        if (equalSubstr(errors, start, errorStart)) {
            if (info.errorMessage.empty()) {
                const auto errorEnd = errors.begin() + end;
                if (std::search(errors.begin() + start, errorEnd,
                                libcueFile.begin(), libcueFile.end())
                        != errorEnd) {
                    info.errorMessage =
                        QtUtilities::qStringToString(
                            QObject::tr(
                                "cue sheet support is not available in %1. "
                                "<i>libcue</i> is most likely not installed."
                            ).arg(QtUtilities::toQString(playerName)));
                }
            }
        }
        else {
            if (equalSubstr(errors, start, missingFilesAndDirsStart)) {
                // If error prefix is present, skip it.
                start += missingFilesAndDirsStart.size();
            }
            std::string item = errors.substr(start, end - start);
            // Audacious duplicates messages sometimes, so this check prevents
            // reporting the same missing item twice.
            if (encounteredNames.insert(item).second)
                info.missingFilesAndDirs.emplace_back(std::move(item));
        }

        prevEnd = end + missingFilesAndDirsEnd.size();
    }

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    if (! info.missingFilesAndDirs.empty()) {
        std::cout << "Missing files and dirs:\n";
        for (const std::string & s : info.missingFilesAndDirs)
            std::cout << s << std::endl;
        std::cout << std::endl;
    }
# endif

    return info;
}

}

namespace Audtool
{
const QString commandName = "audtool",
              shutdownCommand = commandName + " shutdown";
const QString on = "on\n", off = "off\n";

/// @return true if Audacious is running, false otherwise.
bool isAudaciousRunning()
{
    return ! executeAndGetOutput(commandName + " version").isEmpty();
}

/// @brief Sets playlist option to specified value - desiredStatus.
/// @param optionName Playlist option to be set.
void setStatus(const QString & optionName, const QString & desiredStatus)
{
    const QString baseCommand = commandName + " playlist-" + optionName;

    const QString statusCommand = baseCommand + "-status";
    const QString stdOut = executeAndGetOutput(statusCommand);

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << QtUtilities::qStringToString(statusCommand)
              << " = " << QtUtilities::qStringToString(stdOut)
              << "Desired status: "
              << QtUtilities::qStringToString(desiredStatus);
# endif

    if (stdOut != desiredStatus)
        execute(baseCommand + "-toggle");
}

void setEssentialOptions()
{
    if (isAudaciousRunning()) {
        setStatus("auto-advance", on);
        setStatus("repeat", off);
        setStatus("shuffle", off);
        setStatus("stop-after", off);
    }
}

void setMainWindowVisible(bool visible)
{
    execute(commandName + " mainwin-show " + (visible ? "on" : "off"));
}

}

}


class MediaPlayer::Impl : public QObject
{
    Q_OBJECT
public:
    explicit Impl();
    ~Impl();

    /// @return true if this Impl controls external player process.
    bool isRunning() const;

    /// @brief Starts player with specified arguments list.
    /// @return false if player has failed to start or quitted immediatelly;
    /// true otherwise.
    bool start(const QStringList & arguments);

    /// @brief Quits Audacious.
    void quit();

    FinishedSlot finished_ { [](bool, int, std::vector<std::string>) {} };
    ErrorSlot error_ { [](std::string) {} };
    bool autoSetOptions_ = true;
    bool autoHideWindow_ = false;

private:
    /// @brief Gracefully finishes playerProcess_ if isRunning().
    void finishPlayerProcess();
    /// @brief Calls setEssentialOptions().
    void timerEvent(QTimerEvent *) override;
    /// @brief Stops all active timers.
    void stopTimers();


    QProcess playerProcess_;
    /// Timers are necessary because Audacious is not ready to accept audtool
    /// commands immediately after start.
    int setOptionsTimerId_ = 0;
    QTimer hideWindowTimer_;

private slots:
    /// @brief May call error_ and finished_.
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    /// @brief Calls error_.
    void onError(QProcess::ProcessError error);
    void onHideWindowTimeout();
};


MediaPlayer::Impl::Impl(): QObject()
{
    connect(& playerProcess_, SIGNAL(finished(int, QProcess::ExitStatus)),
            SLOT(onFinished(int, QProcess::ExitStatus)));
    connect(& playerProcess_, SIGNAL(error(QProcess::ProcessError)),
            SLOT(onError(QProcess::ProcessError)));
    connect(& hideWindowTimer_, SIGNAL(timeout()), SLOT(onHideWindowTimeout()));
}

MediaPlayer::Impl::~Impl()
{
    playerProcess_.blockSignals(true);
    finishPlayerProcess();
}

bool MediaPlayer::Impl::isRunning() const
{
    return playerProcess_.state() != QProcess::NotRunning;
}

bool MediaPlayer::Impl::start(const QStringList & arguments)
{
    /// If Audacious is already running with proper arguments (isRunning())
    /// AND is ready to accept commands (Audtool::isAudaciousRunning()), there
    /// is no need to restart it.
    if (isRunning() && Audtool::isAudaciousRunning()) {
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
        std::cout << Audacious::playerName << " was running when starting "
                  "playback was requested." << std::endl;
# endif
        QProcess::execute(Audacious::commandName, arguments);
    }
    else {
        quit();
        playerProcess_.start(Audacious::commandName, arguments);

        if (isRunning()) {
            if (autoSetOptions_) {
                killTimer(setOptionsTimerId_);
                setOptionsTimerId_ = startTimer(3000);
            }

            if (autoHideWindow_)
                hideWindowTimer_.start(10);
        }
    }

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << QtUtilities::qStringToString(Audacious::commandName);
    for (const QString & arg : arguments)
        std::cout << " \"" << QtUtilities::qStringToString(arg) << '"';
    std::cout << std::endl;
# endif

    return isRunning();
}

void MediaPlayer::Impl::quit()
{
    finishPlayerProcess();
    while (Audtool::isAudaciousRunning()) {
        execute(Audtool::shutdownCommand);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}


void MediaPlayer::Impl::finishPlayerProcess()
{
    stopTimers();

    if (isRunning()) {
        const bool blocked = playerProcess_.blockSignals(true);

        constexpr int startCheckingAt = 30, considerQuitAt = 40,
                      quitInterval = considerQuitAt - startCheckingAt,
                      forceQuitAt = 100;
        int loopCount = 0;
        int playerIsNotRunningSince = forceQuitAt;

        do {
            execute(Audtool::shutdownCommand);

            ++loopCount;
            if (loopCount == forceQuitAt) {
                std::cerr << VENTUROUS_CORE_ERROR_PREFIX
                          << Audacious::playerName
                          << " is not responding. Killing the process..."
                          << std::endl;
                playerProcess_.kill();
                break;
            }
            if (loopCount >= considerQuitAt &&
                    loopCount - playerIsNotRunningSince >= quitInterval) {
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
                std::cout << "Failed to finish player process properly."
                          " It seems to be in erroneous state." << std::endl;
# endif
                break;
            }
            if (loopCount >= startCheckingAt &&
                    playerIsNotRunningSince > loopCount &&
                    ! Audtool::isAudaciousRunning()) {
                playerIsNotRunningSince = loopCount;
            }
        }
        while (! playerProcess_.waitForFinished(5));

        playerProcess_.blockSignals(blocked);
    }
}

void MediaPlayer::Impl::timerEvent(QTimerEvent *)
{
    killTimer(setOptionsTimerId_);
    setOptionsTimerId_ = 0;
    Audtool::setEssentialOptions();
}

void MediaPlayer::Impl::stopTimers()
{
    if (setOptionsTimerId_ != 0) {
        killTimer(setOptionsTimerId_);
        setOptionsTimerId_ = 0;
    }
    hideWindowTimer_.stop();
}


void MediaPlayer::Impl::onFinished(
    const int exitCode, const QProcess::ExitStatus exitStatus)
{
    stopTimers();

    const std::string errors =
        playerProcess_.readAllStandardError().constData();
    const bool crashExit = (exitStatus == QProcess::CrashExit);

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << "Exit code = " << exitCode
              << ". Crashed = " << std::boolalpha << crashExit
              << ". Errors:\n" << errors;
# endif

    Audacious::StderrInfo info = Audacious::analyzeErrors(errors);
    if (! info.errorMessage.empty())
        error_(std::move(info.errorMessage));
    finished_(crashExit, exitCode, std::move(info.missingFilesAndDirs));
}

void MediaPlayer::Impl::onError(QProcess::ProcessError error)
{
    if (error == QProcess::Timedout)
        return;
    stopTimers();

    QString errorMessage;
    switch (error) {
        case QProcess::FailedToStart:
            errorMessage = tr("the process failed to start. "
                              "Either the invoked program is missing, "
                              "or you may have insufficient permissions "
                              "to invoke the program.");
            break;
        case QProcess::Crashed:
            errorMessage = tr("the process crashed some time after "
                              "starting successfully.");
            break;
        case QProcess::ReadError:
            errorMessage = tr("an error occurred when attempting to "
                              "read from the process.");
            break;
        case QProcess::WriteError:
            errorMessage = tr("an error occurred when attempting to "
                              "write to the process.");
            break;
        case QProcess::UnknownError:
            errorMessage = tr("an unknown error occurred.");
            break;
        default:
            errorMessage = tr("unexpected error has occured.");
    }
    error_(QtUtilities::qStringToString(errorMessage));
}

void MediaPlayer::Impl::onHideWindowTimeout()
{
    if (Audtool::isAudaciousRunning()) {
        hideWindowTimer_.stop();
        Audtool::setMainWindowVisible(false);
    }
}



std::string MediaPlayer::playerName()
{
    return Audacious::playerName;
}

void MediaPlayer::setEssentialOptions()
{
    Audtool::setEssentialOptions();
}

void MediaPlayer::setPlayerWindowVisible(const bool visible)
{
    Audtool::setMainWindowVisible(visible);
}


MediaPlayer::MediaPlayer() : impl_(new Impl)
{
}

MediaPlayer::~MediaPlayer() = default;

bool MediaPlayer::isRunning() const
{
    return impl_->isRunning();
}

void MediaPlayer::setFinishedSlot(FinishedSlot slot)
{
    impl_->finished_ = std::move(slot);
}

void MediaPlayer::setErrorSlot(ErrorSlot slot)
{
    impl_->error_ = std::move(slot);
}

bool MediaPlayer::start()
{
    if (impl_->isRunning())
        return true;
    return impl_->start(Audacious::arguments);
}

bool MediaPlayer::start(const std::string & pathToItem)
{
    return impl_->start(Audacious::arguments +
                        QStringList { Audacious::addToTemporaryPlaylistArg,
                                      QtUtilities::toQString(pathToItem)
                                    });
}

bool MediaPlayer::start(const std::vector<std::string> & pathsToItems)
{
    QStringList arguments = Audacious::arguments;
    arguments.reserve(arguments.size() + 1 + int(pathsToItems.size()));
    arguments << Audacious::addToTemporaryPlaylistArg;
    for (const std::string & path : pathsToItems)
        arguments << QtUtilities::toQString(path);
    return impl_->start(arguments);
}

bool MediaPlayer::getAutoSetOptions() const
{
    return impl_->autoSetOptions_;
}

void MediaPlayer::setAutoSetOptions(const bool autoSetOptions)
{
    impl_->autoSetOptions_ = autoSetOptions;
}

bool MediaPlayer::getAutoHideWindow() const
{
    return impl_->autoHideWindow_;
}

void MediaPlayer::setAutoHideWindow(const bool autoHide)
{
    impl_->autoHideWindow_ = autoHide;
}

void MediaPlayer::quit()
{
    impl_->quit();
}


# include "AudaciousMediaPlayer.moc"
