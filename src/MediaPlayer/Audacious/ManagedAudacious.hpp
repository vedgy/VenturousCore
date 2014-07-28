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

# ifndef VENTUROUS_CORE_MANAGED_AUDACIOUS_HPP
# define VENTUROUS_CORE_MANAGED_AUDACIOUS_HPP

# include "Audacious.hpp"

# include <QProcess>


class ManagedAudacious : public Audacious
{
    Q_OBJECT
public:
    ManagedAudacious();

    ~ManagedAudacious() override;

    /// @return true if this Impl controls external player process.
    bool isRunning() const final;

    bool start() override;

    /// @brief If isRunning(), blocks signals and finishes
    /// playerProcess_ gracefully.
    void exit() final;

private:
    bool start(const QStringList & arguments) override;

    QProcess playerProcess_;

private slots:
    /// @brief Emits finished().
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    /// @brief Emits error().
    void onError(QProcess::ProcessError error);
};

# endif // VENTUROUS_CORE_MANAGED_AUDACIOUS_HPP
