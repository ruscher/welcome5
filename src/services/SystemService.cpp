#include "SystemService.h"

#include <QColor>
#include <QClipboard>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcessEnvironment>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <memory>
#include <algorithm>

namespace {

QString kdeGlobalsPath()
{
    const QString configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QDir(configHome).filePath(QStringLiteral("kdeglobals"));
}

QString colorFromValue(const QVariant &value)
{
    // QSettings reads Plasma's "61,174,233" as a list of three strings.
    const QString raw = value.metaType().id() == QMetaType::QStringList
        ? value.toStringList().join(QLatin1Char(',')).trimmed()
        : value.toString().trimmed();
    QColor color(raw);
    if (color.isValid()) {
        return color.name(QColor::HexRgb).toLower();
    }

    const QStringList channels = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (channels.size() == 3) {
        bool okRed = false;
        bool okGreen = false;
        bool okBlue = false;
        const int red = channels.at(0).trimmed().toInt(&okRed);
        const int green = channels.at(1).trimmed().toInt(&okGreen);
        const int blue = channels.at(2).trimmed().toInt(&okBlue);
        if (okRed && okGreen && okBlue && red >= 0 && red <= 255 && green >= 0 && green <= 255
            && blue >= 0 && blue <= 255) {
            return QColor(red, green, blue).name(QColor::HexRgb).toLower();
        }
    }

    return {};
}

// KWin's default blur strength (blur.kcfg) and the values used by the styles.
constexpr int kDefaultBlurStrength = 15;
constexpr int kBlurStrength = 15;
constexpr int kGlassStrength = 4;
constexpr int kBlurThreshold = 9;

QString kwinrcPath()
{
    const QString configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QDir(configHome).filePath(QStringLiteral("kwinrc"));
}

QDBusMessage plasmaScript(const QString &script)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell"), QStringLiteral("evaluateScript"));
    message << script;
    return message;
}

bool isDarkScheme(const QString &scheme)
{
    const QString lower = scheme.toLower();
    return lower.contains(QStringLiteral("dark")) || lower.contains(QStringLiteral("breeze-dark"))
        || lower.contains(QStringLiteral("night"));
}

}

SystemService::SystemService(QObject *parent)
    : QObject(parent)
    , m_configPath(kdeGlobalsPath())
    , m_sessionType(qEnvironmentVariable("XDG_SESSION_TYPE", QStringLiteral("unknown")).toLower())
{
    connect(&m_configWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
        readDesktopState();
        updateConfigWatcher();
    });

    updateConfigWatcher();
    refresh();
}

bool SystemService::darkTheme() const
{
    return m_darkTheme;
}

QString SystemService::accentColor() const
{
    return m_accentColor;
}

QString SystemService::visualStyle() const
{
    return m_visualStyle;
}

bool SystemService::visualStyleAvailable() const
{
    return m_visualStyleAvailable;
}

QString SystemService::sessionType() const
{
    return m_sessionType;
}

QString SystemService::plasmaVersion() const
{
    return m_plasmaVersion;
}

QString SystemService::pixKey() const
{
    return m_pixKey;
}

QString SystemService::firewallStatus() const
{
    return m_firewallStatus;
}

QString SystemService::driversStatus() const
{
    return m_driversStatus;
}

bool SystemService::driversAvailable() const
{
    return m_driversAvailable;
}

QString SystemService::lastMessage() const
{
    return m_lastMessage;
}

bool SystemService::busy() const
{
    return m_busy;
}

bool SystemService::isValidAccent(const QString &value)
{
    const QColor color(value.trimmed());
    return color.isValid() && value.trimmed().startsWith(QLatin1Char('#'))
        && (value.trimmed().size() == 7 || value.trimmed().size() == 9);
}

bool SystemService::isAllowedLayout(const QString &value)
{
    static const QStringList allowed = {
        QStringLiteral("plasma-default"), QStringLiteral("panel-top"),
        QStringLiteral("floating"),       QStringLiteral("minimal"),
        QStringLiteral("latte-unity"),    QStringLiteral("tiling")
    };
    return allowed.contains(value);
}

bool SystemService::isAllowedVisualStyle(const QString &value)
{
    return value == QStringLiteral("blur") || value == QStringLiteral("glass") || value == QStringLiteral("solid");
}

QString SystemService::visualStyleFor(bool blurEnabled, int blurStrength, const QStringList &panelOpacities)
{
    const bool allOpaque = !panelOpacities.isEmpty()
        && std::all_of(panelOpacities.cbegin(), panelOpacities.cend(),
                       [](const QString &opacity) { return opacity.trimmed() == QStringLiteral("opaque"); });
    if (allOpaque) {
        return QStringLiteral("solid");
    }
    return blurEnabled && blurStrength >= kBlurThreshold ? QStringLiteral("blur") : QStringLiteral("glass");
}

void SystemService::refresh()
{
    readDesktopState();
    readVisualStyle();
    readPixKey();
    readPlasmaVersion();
    refreshDiagnostics();
}

void SystemService::readDesktopState()
{
    // QSettings maps an INI "[General]" section to top-level keys, so no group here.
    QSettings settings(m_configPath, QSettings::IniFormat);

    // Prefer the window color written by the active scheme; scheme names are a weaker hint.
    const QString windowColor = colorFromValue(settings.value(QStringLiteral("Colors:Window/BackgroundNormal")));
    const QString scheme = settings.value(QStringLiteral("ColorScheme")).toString();
    bool newDark = QGuiApplication::palette().window().color().lightness() < 128;
    if (!windowColor.isEmpty()) {
        newDark = QColor(windowColor).lightness() < 128;
    } else if (!scheme.isEmpty()) {
        newDark = isDarkScheme(scheme);
    }
    if (newDark != m_darkTheme) {
        m_darkTheme = newDark;
        emit darkThemeChanged();
    }

    QString newAccent = colorFromValue(settings.value(QStringLiteral("AccentColor")));
    if (newAccent.isEmpty()) {
        newAccent = QGuiApplication::palette().highlight().color().name(QColor::HexRgb).toLower();
    }
    if (newAccent != m_accentColor) {
        m_accentColor = newAccent;
        emit accentColorChanged();
    }
}

void SystemService::readPixKey()
{
    QString path = QStringLiteral("/etc/mainuan/welcome.conf");
    QSettings systemSettings(path, QSettings::IniFormat);
    systemSettings.beginGroup(QStringLiteral("Contribute"));
    QString newKey = systemSettings.value(QStringLiteral("PixKey")).toString().trimmed();

    if (newKey.isEmpty()) {
        const QString userPath = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
                                     .filePath(QStringLiteral("mainuan/welcome.conf"));
        QSettings userSettings(userPath, QSettings::IniFormat);
        userSettings.beginGroup(QStringLiteral("Contribute"));
        newKey = userSettings.value(QStringLiteral("PixKey")).toString().trimmed();
    }

    if (newKey != m_pixKey) {
        m_pixKey = newKey;
        emit pixKeyChanged();
    }
}

void SystemService::readPlasmaVersion()
{
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("plasmashell"));
    if (executable.isEmpty()) {
        if (!m_plasmaVersion.isEmpty()) {
            m_plasmaVersion.clear();
            emit plasmaVersionChanged();
        }
        return;
    }

    QProcess *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus) {
        const QString output = QString::fromLocal8Bit(process->readAllStandardOutput()).trimmed();
        const QString version = exitCode == 0 ? output : QString();
        if (version != m_plasmaVersion) {
            m_plasmaVersion = version;
            emit plasmaVersionChanged();
        }
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [process](QProcess::ProcessError) {
        process->deleteLater();
    });
    process->start(executable, {QStringLiteral("--version")});
}

void SystemService::refreshDiagnostics()
{
    m_driversAvailable = !QStandardPaths::findExecutable(QStringLiteral("kubuntu-driver-manager")).isEmpty();
    m_driversStatus = m_driversAvailable ? QStringLiteral("Gerenciador disponível")
                                         : QStringLiteral("Gerenciador indisponível");

    // `ufw status` requires root; the boot-time switch in ufw.conf is world-readable.
    QFile ufwConfig(QStringLiteral("/etc/ufw/ufw.conf"));
    if (!ufwConfig.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_firewallStatus.clear();
    } else {
        m_firewallStatus = QStringLiteral("Inativo");
        while (!ufwConfig.atEnd()) {
            const QString line = QString::fromUtf8(ufwConfig.readLine()).trimmed();
            if (line.startsWith(QStringLiteral("ENABLED="))) {
                const QString value = line.mid(8).remove(QLatin1Char('"')).trimmed().toLower();
                m_firewallStatus = value == QStringLiteral("yes") ? QStringLiteral("Ativo") : QStringLiteral("Inativo");
            }
        }
    }
    emit diagnosticsChanged();
}

void SystemService::readVisualStyle()
{
    QSettings kwin(kwinrcPath(), QSettings::IniFormat);
    const bool blurEnabled = kwin.value(QStringLiteral("Plugins/blurEnabled"), true).toString() != QStringLiteral("false");
    bool ok = false;
    int strength = kwin.value(QStringLiteral("Effect-blur/BlurStrength")).toInt(&ok);
    if (!ok) {
        strength = kDefaultBlurStrength;
    }

    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(plasmaScript(QStringLiteral(
            "print(panels().map(function (panel) { return panel.opacity; }).join(','));"))),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, blurEnabled, strength](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QString> reply = *call;
        call->deleteLater();
        const bool available = !reply.isError();
        const QString style = available
            ? visualStyleFor(blurEnabled, strength, reply.value().split(QLatin1Char(','), Qt::SkipEmptyParts))
            : QString();
        if (available != m_visualStyleAvailable || style != m_visualStyle) {
            m_visualStyleAvailable = available;
            m_visualStyle = style;
            emit visualStyleChanged();
        }
    });
}

void SystemService::setMessage(const QString &message, bool error)
{
    const QString prefix = error ? QStringLiteral("Erro: ") : QString();
    const QString newMessage = prefix + message;
    if (newMessage != m_lastMessage) {
        m_lastMessage = newMessage;
        emit lastMessageChanged();
    }
}

void SystemService::setBusy(bool busy)
{
    if (busy != m_busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void SystemService::runCommands(QList<Command> commands, const QString &successMessage, const QString &errorPrefix,
                                std::function<void()> onSuccess)
{
    if (commands.isEmpty()) {
        if (onSuccess) {
            onSuccess();
        } else {
            setMessage(successMessage);
            setBusy(false);
            refresh();
        }
        return;
    }
    if (!m_busy) {
        setBusy(true);
        setMessage(QStringLiteral("Aplicando alteração…"));
    }

    const Command command = commands.takeFirst();
    QProcess *process = new QProcess(this);
    process->setStandardInputFile(QProcess::nullDevice());
    const auto failedToStart = std::make_shared<bool>(false);
    connect(process, &QProcess::errorOccurred, this, [this, process, failedToStart](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            *failedToStart = true;
            setMessage(QStringLiteral("comando não encontrado"), true);
            setBusy(false);
            process->deleteLater();
        }
    });
    connect(process, &QProcess::finished, this,
            [this, process, commands, successMessage, errorPrefix, onSuccess, failedToStart](int exitCode, QProcess::ExitStatus) {
                if (*failedToStart) {
                    return;
                }
                const QString errorOutput = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
                process->deleteLater();
                if (exitCode != 0) {
                    setMessage(errorPrefix + (errorOutput.isEmpty() ? QStringLiteral("código %1").arg(exitCode)
                                                                    : errorOutput), true);
                    setBusy(false);
                    return;
                }
                runCommands(commands, successMessage, errorPrefix, onSuccess);
            });
    process->start(command.program, command.arguments);
}

void SystemService::setTheme(const QString &theme)
{
    if (theme != QStringLiteral("dark") && theme != QStringLiteral("light")) {
        setMessage(QStringLiteral("Tema inválido."), true);
        return;
    }

    const QString executable = QStandardPaths::findExecutable(QStringLiteral("plasma-apply-colorscheme"));
    if (executable.isEmpty()) {
        setMessage(QStringLiteral("plasma-apply-colorscheme não está disponível."), true);
        return;
    }

    const QString scheme = theme == QStringLiteral("dark") ? QStringLiteral("DreamGrayDarkColor")
                                                              : QStringLiteral("DreamGrayLightColor");
    runCommands({{executable, {scheme}}}, QStringLiteral("Tema aplicado."),
                QStringLiteral("Não foi possível aplicar o tema: "));
}

void SystemService::setAccent(const QString &accent)
{
    const QString normalized = accent.trimmed().toLower();
    if (!isValidAccent(normalized)) {
        setMessage(QStringLiteral("Cor de destaque inválida."), true);
        return;
    }

    const QString executable = QStandardPaths::findExecutable(QStringLiteral("plasma-apply-colorscheme"));
    if (executable.isEmpty()) {
        setMessage(QStringLiteral("plasma-apply-colorscheme não está disponível."), true);
        return;
    }

    runCommands({{executable, {QStringLiteral("--accent-color"), normalized}}},
                QStringLiteral("Cor de destaque aplicada."),
                QStringLiteral("Não foi possível aplicar a cor: "));
}

void SystemService::setVisualStyle(const QString &style)
{
    if (!isAllowedVisualStyle(style)) {
        setMessage(QStringLiteral("Estilo visual inválido."), true);
        return;
    }
    if (m_busy) {
        setMessage(QStringLiteral("Aguarde a operação atual terminar."), true);
        return;
    }

    const QString successMessage = QStringLiteral("Estilo visual aplicado.");
    if (style == QStringLiteral("solid")) {
        setBusy(true);
        setMessage(QStringLiteral("Aplicando alteração…"));
        applyPanelOpacity(QStringLiteral("opaque"), successMessage);
        return;
    }

    const QString kwriteconfig = QStandardPaths::findExecutable(QStringLiteral("kwriteconfig6"));
    if (kwriteconfig.isEmpty()) {
        setMessage(QStringLiteral("kwriteconfig6 não está disponível."), true);
        return;
    }
    const QString strength = QString::number(style == QStringLiteral("blur") ? kBlurStrength : kGlassStrength);
    runCommands({{kwriteconfig, {QStringLiteral("--file"), QStringLiteral("kwinrc"), QStringLiteral("--group"),
                                 QStringLiteral("Plugins"), QStringLiteral("--key"), QStringLiteral("blurEnabled"),
                                 QStringLiteral("--type"), QStringLiteral("bool"), QStringLiteral("true")}},
                 {kwriteconfig, {QStringLiteral("--file"), QStringLiteral("kwinrc"), QStringLiteral("--group"),
                                 QStringLiteral("Effect-blur"), QStringLiteral("--key"), QStringLiteral("BlurStrength"),
                                 strength}}},
                successMessage, QStringLiteral("Não foi possível aplicar o estilo: "),
                [this, successMessage]() {
                    // Load the effect if it was disabled and make it re-read its strength.
                    QDBusConnection bus = QDBusConnection::sessionBus();
                    for (const QString &method : {QStringLiteral("loadEffect"), QStringLiteral("reconfigureEffect")}) {
                        QDBusMessage call = QDBusMessage::createMethodCall(
                            QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"),
                            QStringLiteral("org.kde.kwin.Effects"), method);
                        call << QStringLiteral("blur");
                        bus.asyncCall(call);
                    }
                    applyPanelOpacity(QStringLiteral("translucent"), successMessage);
                });
}

void SystemService::applyPanelOpacity(const QString &opacity, const QString &successMessage)
{
    // `opacity` is one of two literals chosen by setVisualStyle; no external text
    // reaches the script. Plasma 6.6.0–6.7.4 ignores this property on panels shown
    // on a screen (upstream bug), so the script reports whether it took effect.
    const QString target = opacity == QStringLiteral("opaque") ? QStringLiteral("opaque")
                                                               : QStringLiteral("translucent");
    const QString script = QStringLiteral(
        "var applied = true;"
        "panels().forEach(function (panel) {"
        "  panel.opacity = '%1';"
        "  if (panel.opacity !== '%1') applied = false;"
        "});"
        "print(applied ? 'applied' : 'unchanged');").arg(target);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(plasmaScript(script)), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, target, successMessage](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QString> reply = *call;
        call->deleteLater();
        if (reply.isError()) {
            setMessage(QStringLiteral("Não foi possível alterar os painéis do Plasma: ") + reply.error().message(), true);
        } else if (reply.value().trimmed() != QStringLiteral("applied")) {
            const QString manual = QStringLiteral("Esta versão do Plasma não permite mudar a opacidade dos painéis "
                                                  "automaticamente; ajuste em Editar painel › Opacidade.");
            if (target == QStringLiteral("opaque")) {
                setMessage(manual, true);
            } else {
                setMessage(QStringLiteral("Desfoque aplicado. ") + manual);
            }
        } else {
            setMessage(successMessage);
        }
        setBusy(false);
        readVisualStyle();
    });
}

void SystemService::performAction(const QString &action)
{
    if (action == QStringLiteral("drivers")) {
        const QString executable = QStandardPaths::findExecutable(QStringLiteral("kubuntu-driver-manager"));
        if (executable.isEmpty()) {
            setMessage(QStringLiteral("O gerenciador de drivers não está instalado."), true);
            return;
        }
        if (!QProcess::startDetached(executable, {})) {
            setMessage(QStringLiteral("Não foi possível abrir o gerenciador de drivers."), true);
        }
        return;
    }

    setMessage(QStringLiteral("Ação desconhecida."), true);
}

void SystemService::applyLayout(const QString &layoutId)
{
    if (!isAllowedLayout(layoutId)) {
        setMessage(QStringLiteral("Layout inválido."), true);
        return;
    }

    if (layoutId == QStringLiteral("plasma-default")) {
        setMessage(QStringLiteral("O layout padrão do Plasma não requer alterações."));
        return;
    }

    setMessage(QStringLiteral("Este layout depende de componentes antigos e não é aplicado no Plasma 6.6."), true);
}

bool SystemService::openUrl(const QString &url)
{
    const QUrl parsed(url);
    if (!parsed.isValid() || (parsed.scheme() != QStringLiteral("https")
                              && parsed.scheme() != QStringLiteral("http")
                              && parsed.scheme() != QStringLiteral("tg"))) {
        setMessage(QStringLiteral("URL bloqueada por segurança."), true);
        return false;
    }
    return QDesktopServices::openUrl(parsed);
}

bool SystemService::copyText(const QString &text)
{
    if (text.isEmpty()) {
        setMessage(QStringLiteral("Não há chave Pix configurada."), true);
        return false;
    }
    if (QGuiApplication::clipboard() == nullptr) {
        setMessage(QStringLiteral("Clipboard indisponível."), true);
        return false;
    }
    QGuiApplication::clipboard()->setText(text);
    setMessage(QStringLiteral("Chave Pix copiada."));
    return true;
}

void SystemService::updateConfigWatcher()
{
    if (m_configWatcher.files().contains(m_configPath)) {
        return;
    }
    if (QFileInfo::exists(m_configPath)) {
        m_configWatcher.addPath(m_configPath);
    }
}
