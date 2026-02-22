#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace AppRoutingRules {

enum class MatchType {
    ProcessName,
    ProcessPath
};

struct Entry {
    QString displayName;
    MatchType matchType = MatchType::ProcessName;
    QString value;
};

QList<Entry> Parse(const QString &raw);

QString Serialize(const QList<Entry> &entries);

QStringList CollectValues(const QList<Entry> &entries, MatchType matchType);

QString MatchTypeKey(MatchType matchType);

QString MatchTypeLabel(MatchType matchType);

QString SuggestDisplayName(const Entry &entry);

Entry MakeNameEntry(const QString &processName, const QString &displayName = {});

Entry MakePathEntry(const QString &processPath, const QString &displayName = {});

} // namespace AppRoutingRules
