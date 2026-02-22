#include "main/ProcessRoutingRules.hpp"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace {

QString normalizePath(QString value) {
    value = value.trimmed().replace("\\", "/");
    while (value.contains("//")) {
        value.replace("//", "/");
    }
    return value;
}

QString normalizeValueForKey(const AppRoutingRules::Entry &entry) {
    if (entry.matchType == AppRoutingRules::MatchType::ProcessPath) {
        return normalizePath(entry.value).toLower();
    }
#ifdef Q_OS_WIN
    return entry.value.trimmed().toLower();
#else
    return entry.value.trimmed();
#endif
}

bool parseJsonLine(const QString &line, AppRoutingRules::Entry *entry) {
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(line.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;

    const auto object = document.object();
    auto match = object.value("match").toString().trimmed().toLower();
    auto value = object.value("value").toString().trimmed();
    if (match == "process_path" && value.isEmpty()) value = object.value("path").toString().trimmed();
    if (match == "process_name" && value.isEmpty()) value = object.value("name").toString().trimmed();
    if (match.isEmpty()) {
        if (object.contains("path")) match = "process_path";
        if (object.contains("name")) match = "process_name";
    }

    if (value.isEmpty()) return false;

    entry->matchType = (match == "process_path")
                           ? AppRoutingRules::MatchType::ProcessPath
                           : AppRoutingRules::MatchType::ProcessName;
    entry->value = value;
    entry->displayName = object.value("display").toString().trimmed();
    if (entry->displayName.isEmpty()) entry->displayName = object.value("displayName").toString().trimmed();
    return true;
}

AppRoutingRules::Entry parseLine(QString line) {
    AppRoutingRules::Entry entry;
    line = line.trimmed();

    if (line.startsWith('{')) {
        if (parseJsonLine(line, &entry)) return entry;
    }

    if (line.startsWith("path:", Qt::CaseInsensitive)) {
        entry.matchType = AppRoutingRules::MatchType::ProcessPath;
        entry.value = line.mid(5).trimmed();
        return entry;
    }
    if (line.startsWith("name:", Qt::CaseInsensitive)) {
        entry.matchType = AppRoutingRules::MatchType::ProcessName;
        entry.value = line.mid(5).trimmed();
        return entry;
    }

    entry.matchType = AppRoutingRules::MatchType::ProcessName;
    entry.value = line;
    return entry;
}

bool isValidEntry(const AppRoutingRules::Entry &entry) {
    if (entry.value.trimmed().isEmpty()) return false;
    if (entry.matchType == AppRoutingRules::MatchType::ProcessPath) {
        return !normalizePath(entry.value).isEmpty();
    }
    return true;
}

QStringList splitNonEmptyLines(const QString &raw) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return raw.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
#else
    return raw.split(QRegularExpression("[\r\n]"), QString::SkipEmptyParts);
#endif
}

} // namespace

namespace AppRoutingRules {

QList<Entry> Parse(const QString &raw) {
    QList<Entry> entries;
    QSet<QString> dedupe;

    const auto lines = splitNonEmptyLines(raw);
    for (auto line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        auto entry = parseLine(line);
        if (!isValidEntry(entry)) continue;

        if (entry.matchType == MatchType::ProcessPath) {
            entry.value = normalizePath(entry.value);
        } else {
            entry.value = entry.value.trimmed();
        }

        if (entry.displayName.isEmpty()) {
            entry.displayName = SuggestDisplayName(entry);
        }

        const auto key = MatchTypeKey(entry.matchType) + ":" + normalizeValueForKey(entry);
        if (dedupe.contains(key)) continue;
        dedupe.insert(key);
        entries << entry;
    }

    return entries;
}

QString Serialize(const QList<Entry> &entries) {
    QStringList lines;
    lines.reserve(entries.size());

    for (auto entry : entries) {
        if (!isValidEntry(entry)) continue;
        if (entry.matchType == MatchType::ProcessPath) {
            entry.value = normalizePath(entry.value);
        } else {
            entry.value = entry.value.trimmed();
        }
        if (entry.displayName.isEmpty()) {
            entry.displayName = SuggestDisplayName(entry);
        }

        QJsonObject object{
            {"display", entry.displayName},
            {"match", MatchTypeKey(entry.matchType)},
            {"value", entry.value},
        };
        lines << QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }

    return lines.join('\n');
}

QStringList CollectValues(const QList<Entry> &entries, MatchType matchType) {
    QStringList values;
    QSet<QString> dedupe;
    for (const auto &entry : entries) {
        if (entry.matchType != matchType) continue;
        Entry normalized = entry;
        if (normalized.matchType == MatchType::ProcessPath) {
            normalized.value = normalizePath(normalized.value);
        } else {
            normalized.value = normalized.value.trimmed();
        }
        if (!isValidEntry(normalized)) continue;
        const auto key = normalizeValueForKey(normalized);
        if (dedupe.contains(key)) continue;
        dedupe.insert(key);
        values << normalized.value;
    }
    return values;
}

QString MatchTypeKey(MatchType matchType) {
    return (matchType == MatchType::ProcessPath) ? "process_path" : "process_name";
}

QString MatchTypeLabel(MatchType matchType) {
    return (matchType == MatchType::ProcessPath) ? QStringLiteral("process_path") : QStringLiteral("process_name");
}

QString SuggestDisplayName(const Entry &entry) {
    if (entry.matchType == MatchType::ProcessPath) {
        const auto fileName = QFileInfo(entry.value).fileName().trimmed();
        if (!fileName.isEmpty()) return fileName;
    }
    return entry.value.trimmed();
}

Entry MakeNameEntry(const QString &processName, const QString &displayName) {
    Entry entry;
    entry.matchType = MatchType::ProcessName;
    entry.value = processName.trimmed();
    entry.displayName = displayName.trimmed();
    if (entry.displayName.isEmpty()) entry.displayName = SuggestDisplayName(entry);
    return entry;
}

Entry MakePathEntry(const QString &processPath, const QString &displayName) {
    Entry entry;
    entry.matchType = MatchType::ProcessPath;
    entry.value = normalizePath(processPath);
    entry.displayName = displayName.trimmed();
    if (entry.displayName.isEmpty()) entry.displayName = SuggestDisplayName(entry);
    return entry;
}

} // namespace AppRoutingRules
