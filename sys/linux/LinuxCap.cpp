#include "LinuxCap.h"

#include <QDebug>
#include <QProcess>
#include <QStandardPaths>
#include <QFileInfo>

#define EXIT_CODE(p) (p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1)

QString Linux_GetCapString(const QString &path) {
    QProcess p;
    p.setProgram(Linux_FindCapProgsExec("getcap"));
    p.setArguments({path});
    p.start();
    p.waitForFinished(500);
    return p.readAllStandardOutput();
}

int Linux_Pkexec_SetCapString(const QString &path, const QString &cap) {
    QProcess p;
    p.setProgram("pkexec");
    p.setArguments({Linux_FindCapProgsExec("setcap"), cap, path});
    p.start();
    p.waitForFinished(-1);
    return EXIT_CODE(p);
}

bool Linux_HavePkexec() {
    QProcess p;
    p.setProgram("pkexec");
    p.setArguments({"--help"});
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.start();
    p.waitForFinished(500);
    return EXIT_CODE(p) == 0;
}

bool Linux_HaveSetcap() {
    const auto setcapExec = Linux_FindCapProgsExec("setcap");
    return QFileInfo::exists(setcapExec) && QFileInfo(setcapExec).isExecutable();
}

bool Linux_HaveTunDevice(QString *details) {
    const QFileInfo fi("/dev/net/tun");
    if (!fi.exists()) {
        if (details != nullptr) *details = QStringLiteral("/dev/net/tun does not exist");
        return false;
    }
    if (!fi.isReadable() || !fi.isWritable()) {
        if (details != nullptr) {
            *details = QStringLiteral("/dev/net/tun exists but has insufficient access (%1)")
                           .arg(static_cast<int>(fi.permissions()));
        }
        return false;
    }
    if (details != nullptr) {
        *details = QStringLiteral("/dev/net/tun is available");
    }
    return true;
}

QString Linux_FindCapProgsExec(const QString &name) {
    QString exec = QStandardPaths::findExecutable(name);
    if (exec.isEmpty())
        exec = QStandardPaths::findExecutable(name, {"/usr/sbin", "/sbin"});

    if (exec.isEmpty())
        qDebug() << "Executable" << name << "could not be resolved";
    else
        qDebug() << "Found exec" << name << "at" << exec;

    return exec.isEmpty() ? name : exec;
}
