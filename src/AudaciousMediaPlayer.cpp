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
# include <QProcess>

# include <cstddef>
# include <utility>
# include <array>
# include <vector>
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
                  /// These 2 strings must match Audacious error message format.
                  missingFilesAndDirsStart = "Cannot open ",
                  missingFilesAndDirsEnd = ": No such file or directory.";

const QString commandName = "audacious";

/// Start playing current playlist, quit on playback stop.
const QStringList arguments { "-p", "-q" };

const QString addToTemporaryPlaylistArg = "-E";

/// @param errors Audacious stderr.
/// @return Missing files and directories collection.
std::vector<std::string> analyzeErrors(const std::string & errors)
{
    std::vector<std::string> missingFilesAndDirs;
    std::size_t end = 0;
    while (true) {
        std::size_t start = errors.find(missingFilesAndDirsStart, end);
        if (start == std::string::npos)
            break;
        start += missingFilesAndDirsStart.size();

        end = errors.find(missingFilesAndDirsEnd, start);
        if (end == std::string::npos)
            throw MediaPlayer::Error("unexpected Audacious stderr format.");
        const std::string item = errors.substr(start, end - start);
        end += missingFilesAndDirsEnd.size();

        // Audacious duplicates messages sometimes, so this check prevents
        // reporting the same missing item twice.
        if (missingFilesAndDirs.empty() || missingFilesAndDirs.back() != item)
            missingFilesAndDirs.emplace_back(item);
    }

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    if (! missingFilesAndDirs.empty()) {
        std::cout << "Missing files and dirs:\n";
        for (const std::string & s : missingFilesAndDirs)
            std::cout << s << std::endl;
        std::cout << std::endl;
    }
# endif

    return missingFilesAndDirs;
}

}

namespace Audtool
{
const QString commandName = "audtool",
              shutdownCommand = commandName + " shutdown";
const QString on = "on\n", off = "off\n";

/// @return true If Audacious is running, false otherwise.
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

void setRecommendedOptions()
{
    if (isAudaciousRunning()) {
        setStatus("auto-advance", on);
        setStatus("repeat", off);
        setStatus("shuffle", off);
        setStatus("stop-after", off);
    }
}

}

}


class MediaPlayer::Impl : public QObject
{
    Q_OBJECT
public:
    explicit Impl();

    ~Impl() { finishPlayerProcess(); }

    /// @brief Returns true if this Impl controls external player process.
    bool isRunning() const;

    /// @brief Starts player with specified arguments list.
    void start(const QStringList & arguments);

    /// @brief Quits Audacious.
    void quit();

    FinishedSlot finished_ { [](bool, int, std::vector<std::string>) {} };
    ErrorSlot error_ { [](std::string) {} };
    bool autoSetOptions_ = true;

private:
    /// @brief Gracefully finishes playerProcess_ if isRunning().
    void finishPlayerProcess();
    /// @brief Ensures that unmanaged Audacious process is not running if
    /// (! isRunning()).
    void quitUnmanagedAudaciousProcess();
    /// @brief Calls setRecommendedOptions().
    void timerEvent(QTimerEvent *) override;


    QProcess playerProcess_;
    /// Timer is used for auto-setting options. It is necessary, because
    /// Audacious is not ready to accept audtool commands immediately after
    /// start.
    int timerId_ = 0;

private slots:
    /// @brief Calls finished_.
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    /// @brief Calls error_.
    void onError(QProcess::ProcessError error);
};


MediaPlayer::Impl::Impl(): QObject()
{
    connect(& playerProcess_, SIGNAL(finished(int, QProcess::ExitStatus)),
            SLOT(onFinished(int, QProcess::ExitStatus)));
    connect(& playerProcess_, SIGNAL(error(QProcess::ProcessError)),
            SLOT(onError(QProcess::ProcessError)));
}

bool MediaPlayer::Impl::isRunning() const
{
    return playerProcess_.state() != QProcess::NotRunning;
}

void MediaPlayer::Impl::start(const QStringList & arguments)
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
    }

    if (autoSetOptions_) {
        killTimer(timerId_);
        timerId_ = startTimer(3000);
    }

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << QtUtilities::qStringToString(Audacious::commandName);
    for (const QString & arg : arguments)
        std::cout << " \"" << QtUtilities::qStringToString(arg) << '"';
    std::cout << std::endl;
# endif
}

void MediaPlayer::Impl::quit()
{
    finishPlayerProcess();
    quitUnmanagedAudaciousProcess();
}


void MediaPlayer::Impl::finishPlayerProcess()
{
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
                std::cerr << "** VenturousCore CRITICAL ** " +
                          Audacious::playerName +
                          " is not responding. Killing the process..."
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

void MediaPlayer::Impl::quitUnmanagedAudaciousProcess()
{
    if (! isRunning()) {
        while (Audtool::isAudaciousRunning()) {
            execute(Audtool::shutdownCommand);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

void MediaPlayer::Impl::timerEvent(QTimerEvent *)
{
    killTimer(timerId_);
    timerId_ = 0;
    Audtool::setRecommendedOptions();
}


void MediaPlayer::Impl::onFinished(
    const int exitCode, const QProcess::ExitStatus exitStatus)
{
    const std::string errors = playerProcess_.readAllStandardError().data();
    const bool crashExit = (exitStatus == QProcess::CrashExit);

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << "Exit code = " << exitCode
              << ". Crashed = " << std::boolalpha << crashExit
              << ". Errors:\n" << errors;
# endif

    finished_(crashExit, exitCode, Audacious::analyzeErrors(errors));
}

void MediaPlayer::Impl::onError(QProcess::ProcessError error)
{
    if (error == QProcess::Timedout)
        return;
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



std::string MediaPlayer::playerName()
{
    return Audacious::playerName;
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

void MediaPlayer::start()
{
    if (! impl_->isRunning())
        impl_->start(Audacious::arguments);
}

void MediaPlayer::start(const std::string & pathToItem)
{
    impl_->start(Audacious::arguments +
                 QStringList { Audacious::addToTemporaryPlaylistArg,
                               QtUtilities::toQString(pathToItem)
                             });
}

void MediaPlayer::start(const std::vector<std::string> & pathsToItems)
{
    QStringList arguments = Audacious::arguments;
    arguments << Audacious::addToTemporaryPlaylistArg;
    for (const std::string & path : pathsToItems)
        arguments << QtUtilities::toQString(path);
    impl_->start(arguments);
}

void MediaPlayer::setRecommendedOptions()
{
    Audtool::setRecommendedOptions();
}

bool MediaPlayer::getAutoSetOptions() const
{
    return impl_->autoSetOptions_;
}

void MediaPlayer::setAutoSetOptions(const bool autoSetOptions)
{
    impl_->autoSetOptions_ = autoSetOptions;
}

void MediaPlayer::quit()
{
    impl_->quit();
}


# ifdef INCLUDE_MOC
# include "AudaciousMediaPlayer.moc"
# endif
