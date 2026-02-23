#include <QCoreApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

QTextStream qout(stdout);
QTextStream qerr(stderr);

struct CommandResult {
    bool started = false;
    int exitCode = -1;
    QString out;
    QString err;
};

QString decodeBytes(const QByteArray &bytes) {
    if (bytes.isEmpty()) return {};
    const auto utf8 = QString::fromUtf8(bytes.constData(), bytes.size());
    if (!utf8.contains(QChar::ReplacementCharacter)) return utf8;
    const auto local = QString::fromLocal8Bit(bytes.constData(), bytes.size());
    if (!local.isEmpty()) return local;
    return utf8;
}

CommandResult runCommand(const QString &program, const QStringList &args, int timeoutMs = 15000) {
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    CommandResult r;
    p.start();
    r.started = p.waitForStarted(3000);
    if (!r.started) {
        r.err = p.errorString();
        return r;
    }
    p.waitForFinished(timeoutMs);
    if (p.state() != QProcess::NotRunning) {
        p.kill();
        p.waitForFinished(1000);
    }
    r.exitCode = p.exitCode();
    r.out = decodeBytes(p.readAllStandardOutput()).trimmed();
    r.err = decodeBytes(p.readAllStandardError()).trimmed();
    return r;
}

bool interfaceOk(const QString &name) {
#ifdef Q_OS_WIN
    // Windows interface aliases may include spaces and '*' (e.g. "Local Area Connection* 12").
    if (name.trimmed().isEmpty() || name.size() > 128) return false;
    return !name.contains('\r') && !name.contains('\n');
#else
    static const QRegularExpression re("^[A-Za-z0-9_:\\.-]{1,64}$");
    return re.match(name).hasMatch();
#endif
}

bool cidrOk(const QString &cidr) {
    static const QRegularExpression re("^([0-9]{1,3}\\.){3}[0-9]{1,3}/([0-9]|[12][0-9]|3[0-2])$");
    return re.match(cidr).hasMatch();
}

QString argValue(const QStringList &args, const QString &name) {
    const int idx = args.indexOf(name);
    if (idx < 0 || idx + 1 >= args.size()) return {};
    return args.at(idx + 1).trimmed();
}

int runOrPrint(const QString &program, const QStringList &args, bool failOnError, int timeoutMs = 15000) {
    const auto r = runCommand(program, args, timeoutMs);
    if (r.exitCode != 0 && failOnError) {
        qerr << program << " " << args.join(" ") << "\n";
        if (!r.err.isEmpty()) qerr << r.err << "\n";
        if (!r.out.isEmpty()) qerr << r.out << "\n";
    }
    return r.exitCode;
}

#ifdef Q_OS_LINUX
int applyLinux(const QString &apIf, const QString &apCidr, const QString &tunIf) {
    if (!interfaceOk(apIf) || !interfaceOk(tunIf) || !cidrOk(apCidr)) {
        qerr << "invalid arguments\n";
        return 2;
    }
    if (geteuid() != 0) {
        qerr << "must run as root\n";
        return 3;
    }

    // Best-effort cleanup before apply (idempotent)
    runOrPrint("iptables", {"-t", "mangle", "-D", "PREROUTING", "-i", apIf, "-s", apCidr, "-j", "MARK", "--set-mark", "0x1"}, false);
    runOrPrint("iptables", {"-t", "nat", "-D", "POSTROUTING", "-o", tunIf, "-s", apCidr, "-j", "MASQUERADE"}, false);
    runOrPrint("ip", {"rule", "del", "fwmark", "0x1", "lookup", "100"}, false);
    runOrPrint("ip", {"route", "flush", "table", "100"}, false);

    if (runOrPrint("sysctl", {"-w", "net.ipv4.ip_forward=1"}, true) != 0) return 10;
    if (runOrPrint("iptables", {"-t", "mangle", "-A", "PREROUTING", "-i", apIf, "-s", apCidr, "-j", "MARK", "--set-mark", "0x1"}, true) != 0) return 11;
    if (runOrPrint("ip", {"rule", "add", "fwmark", "0x1", "lookup", "100"}, true) != 0) return 12;
    if (runOrPrint("ip", {"route", "replace", "default", "dev", tunIf, "table", "100"}, true) != 0) return 13;
    if (runOrPrint("iptables", {"-t", "nat", "-A", "POSTROUTING", "-o", tunIf, "-s", apCidr, "-j", "MASQUERADE"}, true) != 0) return 14;

    qout << "OK\n";
    return 0;
}

int clearLinux(const QString &apIf, const QString &apCidr, const QString &tunIf) {
    if (!interfaceOk(apIf) || !interfaceOk(tunIf) || !cidrOk(apCidr)) {
        qerr << "invalid arguments\n";
        return 2;
    }
    if (geteuid() != 0) {
        qerr << "must run as root\n";
        return 3;
    }
    runOrPrint("iptables", {"-t", "mangle", "-D", "PREROUTING", "-i", apIf, "-s", apCidr, "-j", "MARK", "--set-mark", "0x1"}, false);
    runOrPrint("iptables", {"-t", "nat", "-D", "POSTROUTING", "-o", tunIf, "-s", apCidr, "-j", "MASQUERADE"}, false);
    runOrPrint("ip", {"rule", "del", "fwmark", "0x1", "lookup", "100"}, false);
    runOrPrint("ip", {"route", "flush", "table", "100"}, false);
    qout << "OK\n";
    return 0;
}
#endif

#ifdef Q_OS_WIN
bool isAdmin() {
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = nullptr;
    if (!AllocateAndInitializeSid(&ntAuthority,
                                  2,
                                  SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0,
                                  &adminGroup)) {
        return false;
    }
    BOOL isMember = FALSE;
    const BOOL ok = CheckTokenMembership(nullptr, adminGroup, &isMember);
    FreeSid(adminGroup);
    return ok == TRUE && isMember == TRUE;
}

int runPowerShell(const QString &script, const QStringList &args) {
    const QString wrappedScript =
        QStringLiteral("[Console]::InputEncoding=[System.Text.Encoding]::UTF8;")
        + QStringLiteral("[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;")
        + QStringLiteral("$OutputEncoding=[System.Text.Encoding]::UTF8;")
        + script;
    QStringList cmdArgs = {"-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", wrappedScript};
    cmdArgs.append(args);
    const auto r = runCommand("powershell", cmdArgs, 25000);
    if (r.exitCode != 0) {
        if (!r.err.isEmpty()) qerr << r.err << "\n";
        if (!r.out.isEmpty()) qerr << r.out << "\n";
    } else if (!r.out.isEmpty()) {
        qout << r.out << "\n";
    }
    return r.exitCode;
}

int applyWindowsIcs(const QString &publicIf, const QString &privateIf) {
    if (!interfaceOk(publicIf) || !interfaceOk(privateIf)) {
        qerr << "invalid arguments\n";
        return 2;
    }
    if (!isAdmin()) {
        qerr << "must run as admin\n";
        return 3;
    }
    const QString script =
        "$ErrorActionPreference='Stop';"
        "$publicIf=$args[0];$privateIf=$args[1];"
        "$m=New-Object -ComObject HNetCfg.HNetShare;"
        "$all=@($m.EnumEveryConnection());"
        "function Resolve-IcsConnection([string]$name){"
        "  if([string]::IsNullOrWhiteSpace($name)){ return $null };"
        "  $cands = New-Object System.Collections.Generic.List[string];"
        "  $cands.Add($name) | Out-Null;"
        "  try {"
        "    $a = Get-NetAdapter -IncludeHidden | Where-Object { $_.Name -eq $name -or $_.InterfaceAlias -eq $name } | Select-Object -First 1;"
        "    if($a){"
        "      if($a.InterfaceDescription){ $cands.Add([string]$a.InterfaceDescription) | Out-Null };"
        "      if($a.InterfaceGuid){ $cands.Add([string]$a.InterfaceGuid.Guid) | Out-Null };"
        "    }"
        "  } catch {};"
        "  foreach($c in $all){"
        "    $p=$m.NetConnectionProps($c);"
        "    foreach($n in $cands){"
        "      if($p.Name -eq $n -or $p.DeviceName -eq $n -or $p.Guid -eq $n){ return $c }"
        "    }"
        "  }"
        "  foreach($c in $all){"
        "    $p=$m.NetConnectionProps($c);"
        "    foreach($n in $cands){"
        "      if(($p.Name -like \"*$n*\") -or ($p.DeviceName -like \"*$n*\")){ return $c }"
        "    }"
        "  }"
        "  return $null;"
        "};"
        "$pub=Resolve-IcsConnection $publicIf;"
        "$priv=Resolve-IcsConnection $privateIf;"
        "if(-not $pub){"
        "  $known=@(); foreach($c in $all){ $p=$m.NetConnectionProps($c); $known += $p.Name };"
        "  throw \"Public interface not found: $publicIf`nKnown: $($known -join ', ')\""
        "};"
        "if(-not $priv){"
        "  $known=@(); foreach($c in $all){ $p=$m.NetConnectionProps($c); $known += $p.Name };"
        "  throw \"Private interface not found: $privateIf`nKnown: $($known -join ', ')\""
        "};"
        "foreach($c in $all){$cfg=$m.INetSharingConfigurationForINetConnection($c); if($cfg.SharingEnabled){$cfg.DisableSharing()}};"
        "$pubCfg=$m.INetSharingConfigurationForINetConnection($pub);"
        "$privCfg=$m.INetSharingConfigurationForINetConnection($priv);"
        "$pubCfg.EnableSharing(0);"
        "$privCfg.EnableSharing(1);"
        "Write-Output 'OK';";
    return runPowerShell(script, {publicIf, privateIf});
}

int clearWindowsIcs(const QString &publicIf, const QString &privateIf) {
    if (!interfaceOk(publicIf) || !interfaceOk(privateIf)) {
        qerr << "invalid arguments\n";
        return 2;
    }
    if (!isAdmin()) {
        qerr << "must run as admin\n";
        return 3;
    }
    const QString script =
        "$ErrorActionPreference='Stop';"
        "$publicIf=$args[0];$privateIf=$args[1];"
        "$m=New-Object -ComObject HNetCfg.HNetShare;"
        "$all=@($m.EnumEveryConnection());"
        "function Resolve-IcsConnection([string]$name){"
        "  if([string]::IsNullOrWhiteSpace($name)){ return $null };"
        "  foreach($c in $all){"
        "    $p=$m.NetConnectionProps($c);"
        "    if($p.Name -eq $name -or $p.DeviceName -eq $name -or $p.Guid -eq $name){ return $c }"
        "  }"
        "  return $null;"
        "};"
        "$pub=Resolve-IcsConnection $publicIf;"
        "$priv=Resolve-IcsConnection $privateIf;"
        "if($pub){$cfg=$m.INetSharingConfigurationForINetConnection($pub); if($cfg.SharingEnabled){$cfg.DisableSharing()}};"
        "if($priv){$cfg=$m.INetSharingConfigurationForINetConnection($priv); if($cfg.SharingEnabled){$cfg.DisableSharing()}};"
        "Write-Output 'OK';";
    return runPowerShell(script, {publicIf, privateIf});
}

int diagWindowsIcs(const QString &publicIf, const QString &privateIf) {
    if (!interfaceOk(publicIf) || !interfaceOk(privateIf)) {
        qerr << "invalid arguments\n";
        return 2;
    }
    if (!isAdmin()) {
        qerr << "must run as admin\n";
        return 3;
    }
    const QString script =
        "$ErrorActionPreference='Stop';"
        "$publicIf=$args[0];$privateIf=$args[1];"
        "$m=New-Object -ComObject HNetCfg.HNetShare;"
        "$all=@($m.EnumEveryConnection());"
        "function Resolve-IcsConnection([string]$name){"
        "  if([string]::IsNullOrWhiteSpace($name)){ return $null };"
        "  foreach($c in $all){"
        "    $p=$m.NetConnectionProps($c);"
        "    if($p.Name -eq $name -or $p.DeviceName -eq $name -or $p.Guid -eq $name){ return $c }"
        "  }"
        "  return $null;"
        "};"
        "$pub=Resolve-IcsConnection $publicIf;"
        "$priv=Resolve-IcsConnection $privateIf;"
        "if(-not $pub -or -not $priv){throw 'interface not found'};"
        "$pubCfg=$m.INetSharingConfigurationForINetConnection($pub);"
        "$privCfg=$m.INetSharingConfigurationForINetConnection($priv);"
        "if(-not $pubCfg.SharingEnabled -or $pubCfg.SharingConnectionType -ne 0){throw 'public sharing mismatch'};"
        "if(-not $privCfg.SharingEnabled -or $privCfg.SharingConnectionType -ne 1){throw 'private sharing mismatch'};"
        "Write-Output 'OK';";
    return runPowerShell(script, {publicIf, privateIf});
}
#endif

void printUsage(const QString &exe) {
    qerr << "Usage:\n";
#ifdef Q_OS_LINUX
    qerr << "  " << exe << " apply-linux --ap-if <if> --ap-cidr <cidr> --tun-if <if>\n";
    qerr << "  " << exe << " clear-linux --ap-if <if> --ap-cidr <cidr> --tun-if <if>\n";
#endif
#ifdef Q_OS_WIN
    qerr << "  " << exe << " apply-windows-ics --public-if <if> --private-if <if>\n";
    qerr << "  " << exe << " clear-windows-ics --public-if <if> --private-if <if>\n";
    qerr << "  " << exe << " diag-windows-ics --public-if <if> --private-if <if>\n";
#endif
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() < 2) {
        printUsage(args.value(0, "cofebox-net-helper"));
        return 1;
    }

    const auto cmd = args.at(1).trimmed();

#ifdef Q_OS_LINUX
    if (cmd == "apply-linux") {
        return applyLinux(argValue(args, "--ap-if"),
                          argValue(args, "--ap-cidr"),
                          argValue(args, "--tun-if"));
    }
    if (cmd == "clear-linux") {
        return clearLinux(argValue(args, "--ap-if"),
                          argValue(args, "--ap-cidr"),
                          argValue(args, "--tun-if"));
    }
#endif

#ifdef Q_OS_WIN
    if (cmd == "apply-windows-ics") {
        return applyWindowsIcs(argValue(args, "--public-if"), argValue(args, "--private-if"));
    }
    if (cmd == "clear-windows-ics") {
        return clearWindowsIcs(argValue(args, "--public-if"), argValue(args, "--private-if"));
    }
    if (cmd == "diag-windows-ics") {
        return diagWindowsIcs(argValue(args, "--public-if"), argValue(args, "--private-if"));
    }
#endif

    printUsage(args.value(0, "cofebox-net-helper"));
    return 1;
}
