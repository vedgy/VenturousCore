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

# include <QtCoreUtilities/String.hpp>

# include <QString>
# include <QStringList>
# include <QObject>
# include <QProcess>
# include <QTimer>

# include <cstddef>
# include <utility>
# include <vector>
# include <string>
# include <chrono>
# include <thread>


namespace
{
void execute(const QString & command)
{
    QProcess::execute(command);
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << QtUtilities::qStringToString(command) << std::endl;
# endif
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
            missingFilesAndDirs.push_back(item);
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

/// @brief Sets playlist option to specified value - desiredStatus.
/// @param optionName Playlist option to be set.
void setStatus(const QString & optionName, const QString & desiredStatus)
{
    const QString baseCommand = commandName + " playlist-" + optionName;

    const QString statusCommand = baseCommand + "-status";
    QProcess process;
    process.start(statusCommand);
    process.waitForFinished(-1);
    const QString stdOut = process.readAllStandardOutput();

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << QtUtilities::qStringToString(statusCommand)
              << " = " << QtUtilities::qStringToString(stdOut)
              << "Desired status: "
              << QtUtilities::qStringToString(desiredStatus);
# endif

    if (stdOut != desiredStatus)
        execute(baseCommand + "-toggle");
}

}

}


class MediaPlayer::Impl : public QObject
{
    Q_OBJECT
public:
    explicit Impl();

    /// @brief Ensures that external player quits gracefully if isRunning().
    ~Impl();

    /// @brief Returns true if this Impl controls external player process.
    bool isRunning() const;

    /// @brief Requests starting player with specified arguments list.
    void start(const QStringList & arguments);

    /// @brief Requests setting recommended options.
    void setAudtoolOptions();

    /// @brief Requests quitting audacious.
    void quit();

    FinishedSlot finished_ { [](bool, int, std::vector<std::string>) {} };
    bool autoSetOptions_ = true;
    /// Audacious timeout in ms.
    int timeout_ = 2000;

private:
    void startImmediately(const QStringList & arguments);
    void quitImmediately();
    void setAudtoolOptionsImmediately();

    QProcess playerProcess_;
    /// Determines if audacious is ready to accept audtool commands. Depends on
    /// how long ago audacious was started.
    bool audtoolReady_ = true;
    /// Holds arguments for next start() call. If empty, no start() was
    /// requested.
    QStringList queuedStartArguments_;
    /// If true, quit was requested.
    bool queuedQuit_ = false;
    /// If true, setting audtool options was requested.
    bool queuedSettingAudtoolOptions_ = false;

private slots:
    /// @brief Calls finished_.
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    /// @brief Executes queued tasks.
    void onTimeoutPassed();
};


MediaPlayer::Impl::Impl(): QObject()
{
    connect(& playerProcess_, SIGNAL(finished(int, QProcess::ExitStatus)),
            SLOT(onFinished(int, QProcess::ExitStatus)));
}

MediaPlayer::Impl::~Impl()
{
    if (isRunning()) {
        if (! audtoolReady_)
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout_));
        quitImmediately();
    }
}

bool MediaPlayer::Impl::isRunning() const
{
    return playerProcess_.state() != QProcess::NotRunning;
}

void MediaPlayer::Impl::start(const QStringList & arguments)
{
    queuedQuit_ = false;
    if (audtoolReady_)
        startImmediately(arguments);
    else
        queuedStartArguments_ = arguments;
}

void MediaPlayer::Impl::quit()
{
    queuedStartArguments_.clear();
    if (audtoolReady_)
        quitImmediately();
    else
        queuedQuit_ = true;
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

void MediaPlayer::Impl::setAudtoolOptions()
{
    if (audtoolReady_)
        setAudtoolOptionsImmediately();
    else
        queuedSettingAudtoolOptions_ = true;
}


void MediaPlayer::Impl::startImmediately(const QStringList & arguments)
{
    quitImmediately();
    playerProcess_.start(Audacious::commandName, arguments);

    if (autoSetOptions_)
        queuedSettingAudtoolOptions_ = true;

    audtoolReady_ = false;
    QTimer::singleShot(timeout_, this, SLOT(onTimeoutPassed()));

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << QtUtilities::qStringToString(Audacious::commandName);
    for (const QString & arg : arguments)
        std::cout << " \"" << QtUtilities::qStringToString(arg) << '"';
    std::cout << std::endl;
# endif

    queuedStartArguments_.clear();
}

void MediaPlayer::Impl::quitImmediately()
{
    if (queuedSettingAudtoolOptions_)
        setAudtoolOptionsImmediately();

    const bool blocked = playerProcess_.blockSignals(true);
    execute(Audtool::shutdownCommand);
    playerProcess_.waitForFinished(-1);
    playerProcess_.blockSignals(blocked);

    queuedQuit_ = false;
}

void MediaPlayer::Impl::setAudtoolOptionsImmediately()
{
    using namespace Audtool;
    setStatus("auto-advance", on);
    setStatus("repeat", off);
    setStatus("shuffle", off);
    setStatus("stop-after", off);

    queuedSettingAudtoolOptions_ = false;
}


void MediaPlayer::Impl::onTimeoutPassed()
{
    audtoolReady_ = true;

    if (queuedQuit_)
        quitImmediately();
    else if (! queuedStartArguments_.empty())
        startImmediately(queuedStartArguments_);
    else if (queuedSettingAudtoolOptions_)
        setAudtoolOptionsImmediately();
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

int MediaPlayer::getExternalPlayerTimeout() const
{
    return impl_->timeout_;
}

void MediaPlayer::setExternalPlayerTimeout(const int milliseconds)
{
    if (milliseconds < 0)
        throw Error("negative timeout.");
    impl_->timeout_ = milliseconds;
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
    impl_->setAudtoolOptions();
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
