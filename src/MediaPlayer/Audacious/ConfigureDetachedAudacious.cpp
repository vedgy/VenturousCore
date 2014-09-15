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


# include "DetachedAudacious.hpp"

# include <QtCoreUtilities/String.hpp>

# include <CommonUtilities/String.hpp>
# include <CommonUtilities/Streams.hpp>

# define Q_STANDARD_PATHS_AVAILABLE QT_VERSION >= 0x050000

# include <QString>
# include <QObject>
# include <QDir>
# include <QFile>
# include <QFileInfo>

# if Q_STANDARD_PATHS_AVAILABLE
# include <QStandardPaths>
# else // Q_STANDARD_PATHS_AVAILABLE
# ifdef __unix__
# include <QDir>
# else
# include <QFileInfo>
// QtGui dependency! But only for Qt4 and not in Unix.
# include <QDesktopServices>
# endif
# endif // Q_STANDARD_PATHS_AVAILABLE

# include <cstddef>
# include <cassert>
# include <cctype>
# include <utility>
# include <functional>
# include <algorithm>
# include <string>
# include <sstream>
# include <fstream>


namespace ConfigureDetachedAudacious
{
namespace
{
namespace Str = CommonUtilities::String;

QString configuringFailedMessage()
{
    return QObject::tr("Configuring %1 failed: ").
           arg(AudaciousTools::playerName());
}

QString writeContentsToFile(const std::string & filename,
                            std::string & fileContents)
{
    if (! fileContents.empty()) {
        std::ofstream os(filename);
        os << fileContents;
        if (! CommonUtilities::isStreamFine(os)) {
            return configuringFailedMessage() +
                   QObject::tr("writing to file %1 failed.").
                   arg(QtUtilities::toQString(filename));
        }
        fileContents.clear();
    }
    return QString();
}


class Config
{
public:
    explicit Config(const std::string & settingsPath)
        : filename_(settingsPath + "config")
    {}

    /// @brief Parses config and prepares file contents in memory
    /// for DetachedAudacious.
    void prepareSetting();

    /// @brief Parses config and modifies file contents in memory
    /// so that venturous is not called when the end of playlist is reached.
    void prepareResetting();

    /// @brief If there are changes, writes them to config file.
    /// Must be called after prepareSetting() or prepareResetting().
    /// @return empty string if no error has occured; error message otherwise.
    QString writeChangesToFile();

    /// @return true if plugin should be turned off along with resetting;
    /// false if it should not be turned off (because nothing changed in config
    /// or there are [other] registered options/commands for our entry).
    bool isTurningOffPluginNeeded() const { return isTurningOffPluginNeeded_; }

private:
    /// @return true if there are only whitespaces before index on the line that
    /// contains symbol pointed by index.
    /// @param index <= fileContents_.size()
    bool whitespacesOnlyOnTheLineBefore(std::size_t index) const;

    /// @brief Locates heading in fileContents_ starting from index.
    /// Sets index to the beginning of heading name.
    /// @return The position of heading name end.
    /// If no heading was found, index's value becomes undefined,
    /// std::string::npos is returned.
    std::size_t findHeading(std::size_t & index) const;

    std::string getCompleteCommand() const {
        return command1_ + ' ' + command2_;
    }

    std::string getKeyLine() const {
        return key_ + keyCommandSeparator_ + getCompleteCommand() + '\n';
    }

    /// @brief Should be called if command was found in fileContents_.
    /// * entryStart - the position of the first line of sought-for entry
    /// (the position past the end of heading_ line) in fileContents_.
    /// * entryEnd - the position of the EOL just before the first line of
    /// the next entry or fileContents_.size() if sought-for entry is the last.
    /// * lineStart - the position in fileContents_ past the first
    /// keyCommandSeparator_ in the line with key_.
    /// * commandStart - the position in fileContents_ of the first symbol of
    /// command1_.
    /// * commandEnd - the position in fileContents_ past the last symbol of
    /// command2_.
    using OnRegisteredCommand = std::function < void(
                                    std::size_t entryStart,
                                    std::size_t entryEnd,
                                    std::size_t lineStart,
                                    std::size_t commandStart,
                                    std::size_t commandEnd) >;
    /// * lineStart - the same as in OnRegisteredCommand.
    /// * lineEnd - the position of the EOL symbol in the line with key_ or
    /// fileContents_.size() if this line is the last.
    using OnMissingCommand = std::function < void(
                                 std::size_t lineStart, std::size_t lineEnd) >;
    /// * entryStart - the same as in OnRegisteredCommand.
    using OnMissingKey = std::function<void(std::size_t entryStart)>;
    using OnMissingEntry = std::function<void()>;

    struct Handlers {
        OnRegisteredCommand onRegisteredCommand;
        OnMissingCommand onMissingCommand;
        OnMissingKey onMissingKey;
        OnMissingEntry onMissingEntry;
    };

    /// * entryStart - the same as in OnRegisteredCommand.
    /// * entryEnd - the same as in OnRegisteredCommand.
    /// * lineStart - the same as in OnRegisteredCommand.
    /// * lineEnd - the same as in OnMissingCommand.
    void handleKeyLine(const Handlers & handlers, std::size_t entryStart,
                       std::size_t entryEnd, std::size_t lineStart,
                       std::size_t lineEnd);

    /// * entryStart - the same as in OnRegisteredCommand.
    /// * entryEnd - the same as in OnRegisteredCommand.
    void handleEntry(const Handlers & handlers, std::size_t entryStart,
                     std::size_t entryEnd);

    void prepare(const Handlers & handlers);


    const std::string filename_;
    bool isTurningOffPluginNeeded_ = false;
    const char headingStart_ = '[', headingEnd_ = ']',
               keyCommandSeparator_ = '=', commandsSeparator_ = ';';
    const std::string heading_ = "song_change", key_ = "cmd_line_end",
                      command1_ = "ventool", command2_ = "next";
    std::string fileContents_;
};


class PluginRegistry
{
public:
    explicit PluginRegistry(const std::string & settingsPath)
        : filename_(settingsPath + "plugin-registry")
    {}

    /// @brief Parses plugin registry and prepares file contents in memory
    /// for DetachedAudacious.
    /// @return empty string if no error has occured; error message otherwise.
    QString prepareSetting();

    /// @brief Parses plugin registry and modifies file contents in memory
    /// so that Song Change plugin becomes disabled.
    void prepareResetting();

    /// @brief If there are changes, writes them to plugin registry file.
    /// Should be called after prepareSetting() or prepareResetting().
    /// @return empty string if no error has occured; error message otherwise.
    QString writeChangesToFile();

private:
    /// @return Absolute path to shared library or empty string if the library
    /// could not be located.
    static std::string sharedLibrary();

    /// @brief Enables/disables plugin depending on enabled parameter.
    /// @param index Must point to the beginning of plugin_ substring in
    /// fileContents_.
    /// @return true if fileContents_ was changed.
    bool setEnabled(std::size_t index, bool enabled);


    const std::string filename_;
    const std::string pluginName_ = "Song Change",
                      plugin_ = "\nname " + pluginName_ + "\n",
                      stamp_ = "\nstamp", enabled_ = "\nenabled ";
    std::string fileContents_;
};


class Settings
{
public:
    explicit Settings();

    QString set() const;
    QString reset() const;

private:
    static QString getSettingsPath();

    QString configurationCacheError(const QString & action) const {
        return AudaciousTools::playerName() + QObject::tr(
                   " configuration cache error: could not %1 file %2.")
               .arg(action, configuredCache_);
    }

    bool isConfigured() const { return QFileInfo(configuredCache_).exists(); }


    const QString path_, configuredCache_;
};



void Config::prepareSetting()
{
    Handlers handlers;

    handlers.onRegisteredCommand =
    [this](std::size_t, std::size_t, std::size_t, std::size_t, std::size_t) {
        // Command is already registered -> no changes to config are needed.
        fileContents_.clear();
    };

    handlers.onMissingCommand =
    [this](const std::size_t lineStart, const std::size_t lineEnd) {
        // Required command is missing -> add it at the line end.
        const std::size_t lastCommandSymbolPosition =
            Str::Backward::findNonWs(fileContents_, 0, lineEnd);
        std::string separator;
        if (lastCommandSymbolPosition >= lineStart &&
        fileContents_[lastCommandSymbolPosition] != commandsSeparator_) {
            separator = std::string {' '} + commandsSeparator_;
        }
        fileContents_.insert(lineEnd, std::move(separator) + ' ' +
                             getCompleteCommand());
    };

    handlers.onMissingKey =
    [this](std::size_t entryStart) {
        // Add a line with key_ and command to the entry.
        if (fileContents_[entryStart - 1] != '\n') {
            assert(entryStart == fileContents_.size());
            fileContents_.push_back('\n');
            ++entryStart;
        }
        fileContents_.insert(entryStart, getKeyLine());
    };

    handlers.onMissingEntry =
    [this] {
        // Add entire entry with heading_, key_ and command to config.
        fileContents_ += std::string{'\n'} + headingStart_ + heading_ +
        headingEnd_ + '\n' + getKeyLine();
    };

    isTurningOffPluginNeeded_ = false;
    prepare(handlers);
}


void Config::prepareResetting()
{
    Handlers handlers;

    handlers.onRegisteredCommand =
        [this](std::size_t entryStart, const std::size_t entryEnd,
    std::size_t lineStart, std::size_t commandStart, std::size_t commandEnd) {
        // Command is registered -> remove it.
        commandStart =
            Str::Backward::findNonWs(fileContents_, 0, commandStart) + 1;
        Str::skipWsExceptEol(fileContents_, commandEnd);
        if (commandStart == lineStart && (commandEnd == fileContents_.size() ||
        fileContents_[commandEnd] == '\n')) {
            // No other commands -> remove the entire line.
            lineStart = fileContents_.rfind('\n', lineStart);

            // Remove the entire entry if there are no other options here.
            std::size_t firstOption = entryStart;
            Str::skipWs(fileContents_, firstOption);
            if (firstOption >= lineStart) {
                // No options before ours.
                std::size_t nextOption = commandEnd;
                Str::skipWs(fileContents_, nextOption);
                if (nextOption >= entryEnd) {
                    // No options after ours -> remove the entire entry.
                    entryStart = fileContents_.rfind(headingStart_,
                                                     entryStart - 1);
                    entryStart = fileContents_.rfind('\n', entryStart);
                    if (entryStart == std::string::npos ||
                            (entryStart = Str::Backward::findNonWs(
                    fileContents_, 0, entryStart)) == Str::npos()) {
                        // Our entry is the first.
                        entryStart = 0;
                    }
                    else {
                        // There is at least one entry before ours -> leave
                        // preceding entries and one EOL.
                        entryStart =
                            fileContents_.find('\n', entryStart + 1) + 1;
                    }
                    fileContents_.erase(entryStart, entryEnd - entryStart);
                    if (fileContents_.empty()) {
                        // Ensure that fileContents_ is not empty (because
                        // otherwise it won't be written to file).
                        fileContents_.push_back('\n');
                    }
                    isTurningOffPluginNeeded_ = true;
                    return;
                }
            }
            // There are other options in our entry -> remove only one line with
            // our command.
            // EOL before the line is removed. EOL after the line is not
            // removed.
            fileContents_.erase(lineStart, commandEnd - lineStart);
        }
        else {
            // Remove our command and commandsSeparator_ after it (if any).
            if (commandEnd < fileContents_.size() &&
            fileContents_[commandEnd] == commandsSeparator_) {
                ++commandEnd;
            }
            // Leave one whitespace if possible.
            if (CommonUtilities::safeCtype<std::isspace>((unsigned char)
            fileContents_[commandStart])) {
                ++commandStart;
            }
            else if (CommonUtilities::safeCtype<std::isspace>((unsigned char)
            fileContents_[commandEnd - 1])) {
                --commandEnd;
            }
            fileContents_.erase(commandStart, commandEnd - commandStart);
        }
        isTurningOffPluginNeeded_ = false;
    };

    const auto noChanges = [this] {
        // Command is not registered -> no changes to config are needed.
        fileContents_.clear();
        isTurningOffPluginNeeded_ = false;
    };

    handlers.onMissingCommand =
    [& noChanges](std::size_t, std::size_t) { noChanges(); };

    handlers.onMissingKey = [& noChanges](std::size_t) { noChanges(); };

    handlers.onMissingEntry = noChanges;

    prepare(handlers);
}

QString Config::writeChangesToFile()
{
    return writeContentsToFile(filename_, fileContents_);
}


bool Config::whitespacesOnlyOnTheLineBefore(std::size_t index) const
{
    index = Str::Backward::findEolOrNonWs(fileContents_, 0, index);
    return index == Str::npos() || fileContents_[index] == '\n';
}

std::size_t Config::findHeading(std::size_t & index) const
{
    while ((index = fileContents_.find(headingStart_, index)) !=
            std::string::npos) {
        if (! whitespacesOnlyOnTheLineBefore(index++))
            continue;
        std::size_t end =
            fileContents_.find_first_of( {headingEnd_, '\n'}, index);
        if (end == std::string::npos)
            return std::string::npos;
        if (fileContents_[end] != headingEnd_) {
            // Heading is not terminated properly.
            index = end + 1;
            continue;
        }
        Str::skipWs(fileContents_, index);
        return index == end ? end :
               Str::Backward::findNonWs(fileContents_, index, end) + 1;
    }
    return std::string::npos;
}

void Config::handleKeyLine(const Handlers & handlers,
                           const std::size_t entryStart,
                           const std::size_t entryEnd,
                           const std::size_t lineStart,
                           const std::size_t lineEnd)
{
    assert(fileContents_.at(lineStart - 1) == keyCommandSeparator_);
    assert(lineEnd == fileContents_.size() ||
           fileContents_.at(lineEnd) == '\n');
    std::size_t commandIndex = lineStart;
    while ((commandIndex = Str::find(fileContents_, commandIndex, lineEnd,
                                     command1_)) != Str::npos()) {
        std::size_t commandEnd;
        if ((! CommonUtilities::safeCtype<std::isalnum>((unsigned char)
                fileContents_[commandIndex - 1])) &&
                (commandEnd = commandIndex + command1_.size()) < lineEnd &&
                CommonUtilities::safeCtype<std::isspace>((unsigned char)
                        fileContents_[commandEnd])) {
            Str::skipWsExceptEol(fileContents_, commandEnd);
            if (Str::equalSubstr(fileContents_, commandEnd, command2_) &&
                    ((commandEnd += command2_.size()) == fileContents_.size() ||
                     ! CommonUtilities::safeCtype<std::isalnum>((unsigned char)
                             fileContents_[commandEnd]))) {
                handlers.onRegisteredCommand(entryStart, entryEnd, lineStart,
                                             commandIndex, commandEnd);
                return;
            }
            commandIndex = commandEnd;
        }
        else
            commandIndex += command1_.size();
    }
    handlers.onMissingCommand(lineStart, lineEnd);
}

void Config::handleEntry(const Handlers & handlers,
                         const std::size_t entryStart,
                         const std::size_t entryEnd)
{
    assert(entryStart == fileContents_.size() ||
           fileContents_.at(entryStart - 1) == '\n');
    assert(entryEnd == fileContents_.size() ||
           fileContents_.at(entryEnd) == '\n');
    std::size_t keyIndex = entryStart;
    while ((keyIndex = Str::find(fileContents_, keyIndex, entryEnd, key_)) !=
            Str::npos()) {
        keyIndex += key_.size();
        if (! whitespacesOnlyOnTheLineBefore(keyIndex - key_.size()))
            continue;
        Str::skipWsExceptEol(fileContents_, keyIndex);
        if (keyIndex == fileContents_.size() ||
                fileContents_[keyIndex] != keyCommandSeparator_) {
            continue; // Wrong key or invalid line.
        }
        ++keyIndex;
        // keyIndex points to the position in fileContents_ after
        // keyCommandSeparator_.
        std::size_t lineEnd = fileContents_.find('\n', keyIndex);
        if (lineEnd == std::string::npos)
            lineEnd = fileContents_.size();
        handleKeyLine(handlers, entryStart, entryEnd, keyIndex, lineEnd);
        return;
    }
    handlers.onMissingKey(entryStart);
}

void Config::prepare(const Handlers & handlers)
{
    fileContents_ = CommonUtilities::getFileContents(filename_);
    std::size_t index = 0, end;
    while ((end = findHeading(index)) != std::string::npos) {
        if (end - index == heading_.size() &&
                Str::equalSubstr(fileContents_, index, heading_)) {
            // Found heading_.
            index = fileContents_.find('\n', end);
            if (index == std::string::npos)
                index = fileContents_.size();
            else
                ++index;
            end = index;
            if (findHeading(end) == std::string::npos)
                end = fileContents_.size();
            else
                end = fileContents_.rfind('\n', end);
            handleEntry(handlers, index, end);
            return;
        }
    }
    handlers.onMissingEntry();
}



QString PluginRegistry::prepareSetting()
{
    fileContents_ = CommonUtilities::getFileContents(filename_);
    bool contentsChanged = false;
    std::size_t index;
    {
        const std::string formatString = "format";
        if (Str::equalSubstr(fileContents_, 0, formatString))
            index = 0;
        else {
            // Format version is missing.
            std::string format = formatString + " 8\n";
            std::size_t firstNonWs = 0;
            Str::skipWs(fileContents_, firstNonWs);
            fileContents_.replace(0, firstNonWs, format);
            index = format.size();
            contentsChanged = true;
        }
    }

    index = fileContents_.find(plugin_, index);
    if (index == std::string::npos) {
        // Required plugin's description is missing.
        std::string library = sharedLibrary();
        if (library.empty()) {
            fileContents_.clear();
            return configuringFailedMessage() + QObject::tr(
                       "could not locate shared library for %1 plugin.").
                   arg(QtUtilities::toQString(pluginName_));
        }
        Str::trimRight(fileContents_);
        fileContents_ +=
            "\ngeneral " + std::move(library) + stamp_ + " 1" + plugin_ +
            "domain audacious-plugins\npriority 0\nabout 0\nconfig 1"
            + enabled_ + "1\n";
        contentsChanged = true;
    }
    else {
        // Plugin's description is present.
        const bool changed = setEnabled(index, true);
        contentsChanged |= changed;
    }

    if (! contentsChanged)
        fileContents_.clear();
    return QString();
}

void PluginRegistry::prepareResetting()
{
    fileContents_ = CommonUtilities::getFileContents(filename_);

    const std::size_t index = fileContents_.find(plugin_);
    if (index == std::string::npos) {
        // plugin_ is not registered -> nothing to do.
        fileContents_.clear();
    }
    else {
        // Plugin's description is present.
        if (! setEnabled(index, false))
            fileContents_.clear();
    }
}

QString PluginRegistry::writeChangesToFile()
{
    return writeContentsToFile(filename_, fileContents_);
}


std::string PluginRegistry::sharedLibrary()
{
    const QString path = "/usr/lib%1/audacious/General/song_change.so";
    for (QString dir : { "", "/x86_64-linux-gnu", "/i386-linux-gnu" }) {
        const QString filename = path.arg(dir);
        if (QFileInfo(filename).isFile())
            return QtUtilities::qStringToString(filename);
    }
    return std::string();
}

bool PluginRegistry::setEnabled(std::size_t index, const bool enabled)
{
    std::size_t end = fileContents_.find(stamp_, index + plugin_.size());
    if (end == std::string::npos) {
        // Our plugin is the last one.
        end = fileContents_.size();
    }
    else {
        // Set end to the EOL of the last line of our plugin.
        // There is exactly one line that belongs to the plugin before the stamp
        // line.
        end = fileContents_.rfind('\n', end - 1);
    }
    // Set index to the EOL of the penultimate line of our plugin.
    index = fileContents_.rfind('\n', end - 1);

    const char * const enabledString = enabled ? "1" : "0";

    if (Str::equalSubstr(fileContents_, index, enabled_)) {
        // The last line is enabled-line -> ensure that "enabled" value is
        // correct.
        index += enabled_.size();
        std::istringstream istr(fileContents_.substr(index, end - index));
        short value;
        if (!(istr >> value) || value != short(enabled)) {
            // Wrong "enabled" value -> replace it with the correct one.
            fileContents_.replace(index, end - index, enabledString);
            return true;
        }
        return false;
    }
    // The last line is not enabled-line -> add enabled-line at the end.
    if (end == fileContents_.size()) {
        Str::trimRight(fileContents_);
        end = fileContents_.size();
    }
    fileContents_.insert(end, enabled_ + enabledString);
    return true;
}



Settings::Settings()
    : path_(getSettingsPath()),
      configuredCache_(path_ + "ConfiguredForDetachedAudacious.venturous")
{
}

QString Settings::set() const
{
    QString result;
    if (isConfigured())
        return result; // DetachedAudacious is already configured.
    AudaciousTools::quit();
    QDir().mkpath(path_);
    const std::string stdPath = QtUtilities::qStringToString(path_);

    Config config(stdPath);
    config.prepareSetting();
    PluginRegistry pluginRegistry(stdPath);
    result = pluginRegistry.prepareSetting();
    if (! result.isEmpty())
        return result;

    result = config.writeChangesToFile();
    if (! result.isEmpty())
        return result;
    result = pluginRegistry.writeChangesToFile();
    if (! result.isEmpty())
        return result;

    {
        QFile file(configuredCache_);
        if (! file.open(QIODevice::WriteOnly))
            return configurationCacheError(QObject::tr("create"));
    }
    return result;
}

QString Settings::reset() const
{
    QString result;
    if (! isConfigured())
        return result; // DetachedAudacious is not configured.
    AudaciousTools::quit();
    const std::string stdPath = QtUtilities::qStringToString(path_);

    Config config(stdPath);
    config.prepareResetting();

    if (config.isTurningOffPluginNeeded()) {
        PluginRegistry pluginRegistry(stdPath);
        pluginRegistry.prepareResetting();

        result = pluginRegistry.writeChangesToFile();
        if (! result.isEmpty())
            return result;
    }

    result = config.writeChangesToFile();
    if (! result.isEmpty())
        return result;

    if (! QFile::remove(configuredCache_))
        return configurationCacheError(QObject::tr("remove"));
    return result;
}


QString Settings::getSettingsPath()
{
    QString path =
# if Q_STANDARD_PATHS_AVAILABLE
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
# else // Q_STANDARD_PATHS_AVAILABLE
# ifdef __unix__
        QDir::homePath() + "/.config"
# else
        QFileInfo(QDesktopServices::storageLocation(
                      QDesktopServices::DataLocation)).path()
# endif
# endif // Q_STANDARD_PATHS_AVAILABLE
        + "/audacious/";

# ifdef DEBUG_VENTUROUS_MEDIA_PLAYER
    std::cout << "Audacious settings path = "
              << QtUtilities::qStringToString(path) << std::endl;
# endif

    return path;
}

}

QString setSettings()
{
    return Settings().set();
}

QString resetSettings()
{
    return Settings().reset();
}

}
