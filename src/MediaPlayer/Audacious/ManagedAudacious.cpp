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


# include "ManagedAudacious.hpp"

# include "PlayerUtilities.hpp"

# include <QtCoreUtilities/String.hpp>

# include <QString>
# include <QStringList>
# include <QObject>
# include <QProcess>

# include <cstddef>
# include <utility>
# include <algorithm>
# include <set>
# include <string>
# include <iostream>


namespace AudaciousTools
{
namespace
{
/// @return true if (lhs.substr(lhsPos, rhs.size()) == rhs).
/// NOTE: is safe to call if lhs.size() < lhsPos + rhs.size(). Returns false in
/// this case.
bool equalSubstr(const std::string & lhs, std::size_t lhsPos,
                 const std::string & rhs)
{
    return lhs.compare(lhsPos, rhs.size(), rhs) == 0;
}

struct StderrInfo {
    QString errorMessage;
    QStringList missingFilesAndDirs;
};

/// @param errors Audacious stderr.
/// @return StderrInfo extracted from errors.
StderrInfo analyzeErrors(const std::string & errors)
{
    static const std::string missingFilesAndDirsStart = "Cannot open ",
                             missingFilesAndDirsEnd =
                                 ": No such file or directory",
                                 errorStart = " *** ERROR:",
                                 libcueFile = "libcue.so";

    StderrInfo info;
    std::set<std::string> encounteredItems;
    std::size_t prevEnd = 0;
    while (true) {
        const std::size_t end = errors.find(missingFilesAndDirsEnd, prevEnd);
        if (end == std::string::npos)
            break;
        std::size_t start;
        {
            // Set {start} to the position just after the last '\n' character in
            // interval [prevEnd, end).
            const auto rEnd = errors.rend();
            start = rEnd - std::find(rEnd - end, rEnd - prevEnd, '\n');
            if (start == prevEnd && prevEnd != 0) { // not found
                std::cerr << VENTUROUS_CORE_ERROR_PREFIX "Unexpected "
                          << QtUtilities::qStringToString(playerName())
                          << " stderr format. Aborted parsing." << std::endl;
                return info;
            }
        }

        if (equalSubstr(errors, start, errorStart)) {
            if (info.errorMessage.isEmpty()) {
                const auto errorEnd = errors.begin() + end;
                if (std::search(errors.begin() + start + errorStart.size(),
                                errorEnd,
                                libcueFile.begin(), libcueFile.end())
                        != errorEnd) {
                    info.errorMessage =
                        QObject::tr("cue sheet support is not available in %1. "
                                    "<i>libcue</i> is most likely not "
                                    "installed.").arg(playerName());
                }
            }
        }
        else {
            if (equalSubstr(errors, start, missingFilesAndDirsStart)) {
                // If error prefix is present, skip it.
                start += missingFilesAndDirsStart.size();
            }
            const std::string item = errors.substr(start, end - start);
            // Audacious duplicates messages sometimes, so this check prevents
            // reporting the same missing item more than once.
            if (encounteredItems.insert(item).second)
                info.missingFilesAndDirs << QtUtilities::toQString(item);
        }

        prevEnd = end + missingFilesAndDirsEnd.size();
    }

    return info;
}

}

}


ManagedAudacious::ManagedAudacious()
    : Audacious( { AudaciousTools::startPlaybackArg(), "-q" })
{
    connect(& playerProcess_, SIGNAL(finished(int, QProcess::ExitStatus)),
            SLOT(onFinished(int, QProcess::ExitStatus)));
    connect(& playerProcess_, SIGNAL(error(QProcess::ProcessError)),
            SLOT(onError(QProcess::ProcessError)));
}

ManagedAudacious::~ManagedAudacious()
{
    playerProcess_.blockSignals(true);
    exit();
}

bool ManagedAudacious::isRunning() const
{
    return playerProcess_.state() != QProcess::NotRunning;
}

bool ManagedAudacious::start()
{
    if (isRunning())
        return Audacious::start();
    return start(playerArguments_);
}

void ManagedAudacious::exit()
{
    exitingPlayer();

    if (isRunning()) {
        const bool blocked = playerProcess_.blockSignals(true);

        constexpr int startCheckingAt = 30, considerQuitAt = 40,
                      quitInterval = considerQuitAt - startCheckingAt,
                      forceQuitAt = 100;
        int loopCount = 0;
        int playerIsNotRunningSince = forceQuitAt;

        do {
            AudaciousTools::requestQuit();
            ++loopCount;

            if (loopCount == forceQuitAt) {
                std::cerr << VENTUROUS_CORE_ERROR_PREFIX
                          << QtUtilities::qStringToString(playerName())
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
                    ! AudaciousTools::isRunning()) {
                playerIsNotRunningSince = loopCount;
            }
        }
        while (! playerProcess_.waitForFinished(5));

        playerProcess_.blockSignals(blocked);
    }
}


bool ManagedAudacious::start(const QStringList & arguments)
{
    /// If Audacious is already running with proper arguments (isRunning())
    /// AND is ready to accept commands (AudaciousTools::isRunning()), there
    /// is no need to restart it.
    if (isRunning() && AudaciousTools::isRunning()) {
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
        std::cout << QtUtilities::qStringToString(playerName())
                  << " was running when starting playback was requested."
                  << std::endl;
# endif
        PlayerUtilities::execute(AudaciousTools::playerCommand(), arguments);
    }
    else {
        exit();
        AudaciousTools::quit();
        PlayerUtilities::start(playerProcess_, AudaciousTools::playerCommand(),
                               arguments);
        if (isRunning())
            launchedPlayer();
    }

    return isRunning();
}


void ManagedAudacious::onFinished(const int exitCode,
                                  const QProcess::ExitStatus exitStatus)
{
    exitingPlayer();

    const std::string errors =
        playerProcess_.readAllStandardError().constData();
    const bool crashExit = (exitStatus == QProcess::CrashExit);

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << "Player process ";
    if (crashExit)
        std::cout << "crashed";
    else
        std::cout << "exit code = " << exitCode;
    std::cout << ". Errors:\n" << errors;
# endif

    AudaciousTools::StderrInfo info = AudaciousTools::analyzeErrors(errors);
    emit finished(crashExit, exitCode, { std::move(info.errorMessage) },
                  std::move(info.missingFilesAndDirs));
}

void ManagedAudacious::onError(const QProcess::ProcessError error)
{
    if (error == QProcess::Timedout)
        return;
    if (! isRunning())
        exitingPlayer();

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
    emit this->error(std::move(errorMessage));
}
