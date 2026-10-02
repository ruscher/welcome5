#pragma once

#include <QFileSystemWatcher>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <functional>

class SystemService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool darkTheme READ darkTheme NOTIFY darkThemeChanged)
    Q_PROPERTY(QString accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QString visualStyle READ visualStyle NOTIFY visualStyleChanged)
    Q_PROPERTY(bool visualStyleAvailable READ visualStyleAvailable NOTIFY visualStyleChanged)
    Q_PROPERTY(QString sessionType READ sessionType CONSTANT)
    Q_PROPERTY(QString plasmaVersion READ plasmaVersion NOTIFY plasmaVersionChanged)
    Q_PROPERTY(QString pixKey READ pixKey NOTIFY pixKeyChanged)
    Q_PROPERTY(QString firewallStatus READ firewallStatus NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString driversStatus READ driversStatus NOTIFY diagnosticsChanged)
    Q_PROPERTY(bool driversAvailable READ driversAvailable NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit SystemService(QObject *parent = nullptr);

    bool darkTheme() const;
    QString accentColor() const;
    QString visualStyle() const;
    bool visualStyleAvailable() const;
    QString sessionType() const;
    QString plasmaVersion() const;
    QString pixKey() const;
    QString firewallStatus() const;
    QString driversStatus() const;
    bool driversAvailable() const;
    QString lastMessage() const;
    bool busy() const;

    static bool isValidAccent(const QString &value);
    static bool isAllowedLayout(const QString &value);
    static bool isAllowedVisualStyle(const QString &value);
    // "blur", "glass" or "solid" from the KWin blur settings and panel opacities.
    static QString visualStyleFor(bool blurEnabled, int blurStrength, const QStringList &panelOpacities);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setTheme(const QString &theme);
    Q_INVOKABLE void setAccent(const QString &accent);
    Q_INVOKABLE void setVisualStyle(const QString &style);
    Q_INVOKABLE void performAction(const QString &action);
    Q_INVOKABLE void applyLayout(const QString &layoutId);
    Q_INVOKABLE bool openUrl(const QString &url);
    Q_INVOKABLE bool copyText(const QString &text);

signals:
    void darkThemeChanged();
    void accentColorChanged();
    void visualStyleChanged();
    void plasmaVersionChanged();
    void pixKeyChanged();
    void diagnosticsChanged();
    void lastMessageChanged();
    void busyChanged();

private:
    struct Command
    {
        QString program;
        QStringList arguments;
    };

    void readDesktopState();
    void readVisualStyle();
    void readPixKey();
    void refreshDiagnostics();
    void readPlasmaVersion();
    void setMessage(const QString &message, bool error = false);
    void setBusy(bool busy);
    void runCommands(QList<Command> commands, const QString &successMessage, const QString &errorPrefix,
                     std::function<void()> onSuccess = {});
    void applyPanelOpacity(const QString &opacity, const QString &successMessage);
    void updateConfigWatcher();

    QFileSystemWatcher m_configWatcher;
    QString m_configPath;
    QString m_accentColor;
    QString m_visualStyle;
    QString m_plasmaVersion;
    QString m_pixKey;
    QString m_firewallStatus;
    QString m_driversStatus;
    QString m_lastMessage;
    QString m_sessionType;
    bool m_darkTheme = false;
    bool m_visualStyleAvailable = false;
    bool m_driversAvailable = false;
    bool m_busy = false;
};
