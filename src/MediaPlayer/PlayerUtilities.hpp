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

# ifndef VENTUROUS_CORE_PLAYER_UTILITIES_HPP
# define VENTUROUS_CORE_PLAYER_UTILITIES_HPP

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
# include <QtCoreUtilities/String.hpp>
# include <iostream>
# endif


# include <QString>
# include <QStringList>
# include <QProcess>


namespace PlayerUtilities
{
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
inline void writeLine(const QString & str)
{
    std::cout << QtUtilities::qStringToString(str) << std::endl;
}

inline void writeCommandAndArgs(const QString & command,
                                const QStringList & arguments)
{
    std::cout << QtUtilities::qStringToString(command);
    for (const QString & arg : arguments)
        std::cout << " \"" << QtUtilities::qStringToString(arg) << '"';
    std::cout << std::endl;
}
# endif

inline void execute(const QString & command)
{
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    writeLine(command);
# endif
    QProcess::execute(command);
}

inline void execute(const QString & command, const QStringList & arguments)
{
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    writeCommandAndArgs(command, arguments);
# endif
    QProcess::execute(command, arguments);
}

inline void start(QProcess & process, const QString & command,
                  const QStringList & arguments)
{
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    writeCommandAndArgs(command, arguments);
# endif
    process.start(command, arguments);
}

/// @brief Executes command and returns its stdout.
inline QString executeAndGetOutput(const QString & command)
{
# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    writeLine(command);
# endif
    QProcess process;
    process.start(command);
    process.waitForFinished(-1);
    return process.readAllStandardOutput();
}

}

# endif // VENTUROUS_CORE_PLAYER_UTILITIES_HPP
