#pragma once

#include <QString>

QString Linux_GetCapString(const QString &path);

int Linux_Pkexec_SetCapString(const QString &path, const QString &cap);

bool Linux_HavePkexec();
bool Linux_HaveSetcap();
bool Linux_HaveTunDevice(QString *details = nullptr);

QString Linux_FindCapProgsExec(const QString &name);
