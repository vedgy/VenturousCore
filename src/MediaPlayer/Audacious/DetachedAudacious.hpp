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

# ifndef VENTUROUS_CORE_DETACHED_AUDACIOUS_HPP
# define VENTUROUS_CORE_DETACHED_AUDACIOUS_HPP

# include "Audacious.hpp"

# include <QString>


class DetachedAudacious : public Audacious
{
public:
    DetachedAudacious();

    ~DetachedAudacious() override;

    /// @return true if external player is running.
    bool isRunning() const override;

    /// @brief Quits external player.
    void exit() final;

private:
    bool start(const QStringList & arguments) override;
};


namespace ConfigureDetachedAudacious
{
/// @brief Sets Audacious settings necessary for DetachedAudacious. Should be
/// called before DetachedAudacious constructor.
/// @return Empty string if settings were set successfully;
/// error message otherwise.
QString setSettings();
/// @brief Cancels modifications made by setSettings() in Audacious settings.
/// Should be called when user switches to another player.
/// @return Empty string if settings were reset successfully;
/// error message otherwise.
QString resetSettings();
}

# endif // VENTUROUS_CORE_DETACHED_AUDACIOUS_HPP
