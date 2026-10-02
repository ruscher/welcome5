#include "models/ApplicationModel.h"
#include "models/VideoModel.h"
#include "services/InstallProgressParser.h"
#include "services/InstallService.h"
#include "services/PackageService.h"
#include "services/StartupPreference.h"
#include "services/SystemService.h"

#include <QtTest>

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <memory>
#include <unistd.h>

namespace {

QString fixture(const QString &name)
{
    return QStringLiteral(MAINUAN_TEST_FIXTURES "/") + name;
}

InstallService::Backend mockBackend()
{
    return {fixture(QStringLiteral("pkexec")), fixture(QStringLiteral("helper")),
            fixture(QStringLiteral("dpkg-query")), QStandardPaths::findExecutable(QStringLiteral("true"))};
}

const QStringList kClamAv = {QStringLiteral("clamav"), QStringLiteral("clamav-daemon"),
                             QStringLiteral("clamav-freshclam")};
const QString kClamUi = QStringLiteral("io.github.linx_systems.ClamUI");

// A simulated system: installed Debian packages and Flatpak applications.
class MockSystem
{
public:
    MockSystem(const QStringList &debPackages, const QStringList &flatpaks)
    {
        writeLines(QStringLiteral("deb"), debPackages);
        QStringList lines;
        for (const QString &flatpak : flatpaks) {
            lines << flatpak + QStringLiteral("\tsystem");
        }
        writeLines(QStringLiteral("flatpak"), lines);
        qputenv("MOCK_STATE_DIR", m_dir.path().toLocal8Bit());
    }

    QStringList lines(const QString &name) const
    {
        QFile file(m_dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

private:
    void writeLines(const QString &name, const QStringList &lines)
    {
        QFile file(m_dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        for (const QString &line : lines) {
            file.write(line.toUtf8() + '\n');
        }
    }

    QTemporaryDir m_dir;
};

struct Services
{
    std::unique_ptr<PackageService> packages;
    std::unique_ptr<InstallService> installer;
};

Services makeServices(InstallService::Backend backend = mockBackend())
{
    Services services;
    services.packages = std::make_unique<PackageService>(nullptr, fixture(QStringLiteral("flatpak")));
    services.installer = std::make_unique<InstallService>(services.packages.get(), std::move(backend));
    return services;
}

QVariantMap stateOf(const InstallService &installer, const QString &profile)
{
    return installer.profiles().value(profile).toMap();
}

bool waitForDetection(const InstallService &installer)
{
    return QTest::qWaitFor([&installer]() {
        return stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")) != QStringLiteral("checking");
    }, 5000);
}

bool runToEnd(InstallService &installer, const QString &profile)
{
    QSignalSpy finished(&installer, &InstallService::finished);
    if (!installer.start(profile)) {
        return false;
    }
    return finished.count() > 0 || finished.wait(15000);
}

} // namespace

class MainuanTest final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    void acceptsOnlyHexAccentColors();
    void rejectsUnexpectedPackageIds();
    void acceptsOnlyKnownLayouts();
    void mapsVisualStyles();
    void exposesModelRolesAndLocalVideoCatalog();

    void parsesHelperProtocol();
    void parsesFlatpakProgress();
    void realHelperRejectsUntrustedInput();

    void installsMissingComponentsWithRealProgress();   // A
    void reportsAlreadyInstalledProfiles();             // B
    void treatsDismissedAuthenticationAsCancelled();    // C
    void reportsPackageManagerErrors();                 // D
    void explainsNetworkAndLockFailures();              // E + lock
    void rejectsConcurrentStarts();                     // F
    void detectsStateAgainOnRestart();                  // G
    void installsOnlyClamUiWhenClamAvExists();          // H
    void installsOnlyClamAvWhenClamUiExists();          // I
    void completesFirewallWithPlasmaIntegration();      // J
    void treatsDeniedFlatpakAuthorizationAsCancelled();
    void disablesAptProfilesOnNonDebianSystems();
    void rejectsUnknownProfiles();

    void startupPreferenceDefaultsToShown();
    void startupPreferenceIsStoredPerUser();
    void startupPreferenceReadsPlasmaOverrides();
};

void MainuanTest::cleanup()
{
    qunsetenv("MOCK_SCENARIO");
    qunsetenv("MOCK_STATE_DIR");
}

void MainuanTest::acceptsOnlyHexAccentColors()
{
    QVERIFY(SystemService::isValidAccent(QStringLiteral("#3daee9")));
    QVERIFY(SystemService::isValidAccent(QStringLiteral("#AABBCC")));
    QVERIFY(!SystemService::isValidAccent(QStringLiteral("red")));
    QVERIFY(!SystemService::isValidAccent(QStringLiteral("#12345")));
    QVERIFY(!SystemService::isValidAccent(QStringLiteral("#123456789")));
}

void MainuanTest::rejectsUnexpectedPackageIds()
{
    QVERIFY(PackageService::isAllowedApplication(QStringLiteral("org.libreoffice.LibreOffice")));
    QVERIFY(PackageService::isAllowedApplication(QStringLiteral("com.brave.Browser")));
    QVERIFY(PackageService::isAllowedApplication(kClamUi));
    QVERIFY(!PackageService::isAllowedApplication(QStringLiteral("webapp.gdocs")));
    QVERIFY(!PackageService::isAllowedApplication(QStringLiteral("../../etc/passwd")));
    QVERIFY(!PackageService::isAllowedApplication(QStringLiteral("org.example.Unknown")));
}

void MainuanTest::acceptsOnlyKnownLayouts()
{
    QVERIFY(SystemService::isAllowedLayout(QStringLiteral("plasma-default")));
    QVERIFY(SystemService::isAllowedLayout(QStringLiteral("tiling")));
    QVERIFY(!SystemService::isAllowedLayout(QStringLiteral("../../tmp")));
}

void MainuanTest::mapsVisualStyles()
{
    QVERIFY(SystemService::isAllowedVisualStyle(QStringLiteral("blur")));
    QVERIFY(SystemService::isAllowedVisualStyle(QStringLiteral("glass")));
    QVERIFY(SystemService::isAllowedVisualStyle(QStringLiteral("solid")));
    QVERIFY(!SystemService::isAllowedVisualStyle(QStringLiteral("'; panels().forEach(p => p.remove()); '")));

    const QStringList translucent = {QStringLiteral("adaptive"), QStringLiteral("translucent")};
    QCOMPARE(SystemService::visualStyleFor(true, 15, translucent), QStringLiteral("blur"));
    QCOMPARE(SystemService::visualStyleFor(true, 4, translucent), QStringLiteral("glass"));
    QCOMPARE(SystemService::visualStyleFor(false, 15, translucent), QStringLiteral("glass"));
    QCOMPARE(SystemService::visualStyleFor(true, 15, {QStringLiteral("opaque"), QStringLiteral("opaque")}),
             QStringLiteral("solid"));
    QCOMPARE(SystemService::visualStyleFor(true, 15, {}), QStringLiteral("blur"));
}

void MainuanTest::exposesModelRolesAndLocalVideoCatalog()
{
    PackageService packageService;
    const ApplicationEntry onlineEntry{
        QStringLiteral("gdocs"), QStringLiteral("Documentos"), QStringLiteral("Online"),
        QStringLiteral("webapp.gdocs"), QStringLiteral("qrc:/assets/webapp.gdocs.png"),
        QStringLiteral("https://docs.google.com/")};
    ApplicationModel applications(&packageService, {onlineEntry});

    QCOMPARE(applications.rowCount(), 1);
    const QModelIndex applicationIndex = applications.index(0, 0);
    QCOMPARE(applications.data(applicationIndex, ApplicationModel::NameRole).toString(),
             QStringLiteral("Documentos"));
    QVERIFY(applications.data(applicationIndex, ApplicationModel::AvailableRole).toBool());
    QCOMPARE(applications.data(applicationIndex, ApplicationModel::InstallStateRole).toString(),
             QStringLiteral("Online"));
    QCOMPARE(applications.data(applicationIndex, ApplicationModel::WebUrlRole).toString(),
             QStringLiteral("https://docs.google.com/"));
    QVERIFY(applications.data(applicationIndex, ApplicationModel::InstallProfileRole).toString().isEmpty());
    QCOMPARE(applications.roleNames().value(ApplicationModel::WebUrlRole), QByteArray("webUrl"));

    VideoModel videos;
    QCOMPARE(videos.rowCount(), 5);
    const QModelIndex videoIndex = videos.index(0, 0);
    QCOMPARE(videos.data(videoIndex, VideoModel::TitleRole).toString(),
             QStringLiteral("Conhecendo o sistema"));
    QCOMPARE(videos.data(videoIndex, VideoModel::IconSourceRole).toString(),
             QStringLiteral("qrc:/assets/icone-videoaulas_g204.svg"));
    QVERIFY(videos.data(videoIndex, VideoModel::FileSourceRole).toString().startsWith(
        QStringLiteral("file:///var/lib/curso-linux/videos/")));
}

void MainuanTest::parsesHelperProtocol()
{
    using InstallProgress::HelperEvent;
    using InstallProgress::parseHelperLine;

    HelperEvent event = parseHelperLine(QStringLiteral("dlstatus:3:41.2500:Retrieving file 3 of 29"));
    QCOMPARE(event.type, HelperEvent::Type::Download);
    QCOMPARE(event.percent, 41.25);
    int current = 0;
    int total = 0;
    QVERIFY(InstallProgress::parseAptFileCounter(event.text, &current, &total));
    QCOMPARE(current, 3);
    QCOMPARE(total, 29);

    event = parseHelperLine(QStringLiteral("pmstatus:clamav:12.6552:Unpacking clamav (amd64)"));
    QCOMPARE(event.type, HelperEvent::Type::Package);
    QCOMPARE(event.name, QStringLiteral("clamav"));
    QVERIFY(!event.configuring);

    event = parseHelperLine(QStringLiteral("pmstatus:ufw:60.1:Preparing to configure ufw (amd64)"));
    QVERIFY(event.configuring);

    event = parseHelperLine(QStringLiteral("pmerror:clamav-daemon:70:subprocess returned error: exit status 1"));
    QCOMPARE(event.type, HelperEvent::Type::PackageError);
    QCOMPARE(event.text, QStringLiteral("subprocess returned error: exit status 1"));

    QCOMPARE(parseHelperLine(QStringLiteral("@@phase signatures")).name, QStringLiteral("signatures"));
    QCOMPARE(parseHelperLine(QStringLiteral("@@error locked")).type, HelperEvent::Type::Error);
    QCOMPARE(parseHelperLine(QStringLiteral("@@error")).name, QStringLiteral("failed"));
    QCOMPARE(parseHelperLine(QStringLiteral("@@done")).type, HelperEvent::Type::Done);
    QCOMPARE(parseHelperLine(QStringLiteral("pmstatus:broken")).type, HelperEvent::Type::Unknown);
    QCOMPARE(parseHelperLine(QStringLiteral("dlstatus:1:abc:x")).type, HelperEvent::Type::Unknown);
    QCOMPARE(parseHelperLine(QStringLiteral("dlstatus:1:250:x")).percent, 100.0);
}

void MainuanTest::parsesFlatpakProgress()
{
    using InstallProgress::FlatpakProgress;
    using InstallProgress::parseFlatpakLine;

    FlatpakProgress state;
    state = parseFlatpakLine(QStringLiteral("Required runtime for app found in remote flathub"), state);
    QVERIFY(!state.changed);

    state = parseFlatpakLine(QStringLiteral("Installing 1/3…"), state);
    QVERIFY(state.changed);
    QCOMPARE(state.operation, 1);
    QCOMPARE(state.operations, 3);
    QCOMPARE(state.percent, -1);

    state = parseFlatpakLine(QStringLiteral("Installing 1/3… ████████▌            45%  1.2 MB/s  00:12"), state);
    QCOMPARE(state.percent, 45);

    state = parseFlatpakLine(QStringLiteral("Instalando 2/3…"), state);
    QCOMPARE(state.operation, 2);
    QCOMPARE(state.percent, -1);

    state = parseFlatpakLine(QStringLiteral("Instalando 2/3… ███ 100%  3,4 MB/s  00:00"), state);
    QCOMPARE(state.percent, 100);

    FlatpakProgress single;
    single = parseFlatpakLine(QStringLiteral("Installing…"), single);
    QCOMPARE(single.operations, 1);
    single = parseFlatpakLine(QStringLiteral("Installing… ██ 12%  800 kB/s"), single);
    QCOMPARE(single.percent, 12);
}

void MainuanTest::realHelperRejectsUntrustedInput()
{
    const auto run = [](const QStringList &arguments, QString *output) {
        QProcess helper;
        helper.start(QStringLiteral(MAINUAN_TEST_HELPER), arguments);
        helper.waitForFinished(10000);
        *output = QString::fromUtf8(helper.readAllStandardOutput()).trimmed();
        return helper.exitCode();
    };

    QString output;
    QCOMPARE(run({}, &output), 2);
    QCOMPARE(output, QStringLiteral("@@error invalid"));
    QCOMPARE(run({QStringLiteral("install"), QStringLiteral("firewall; id")}, &output), 2);
    QCOMPARE(output, QStringLiteral("@@error invalid"));
    QCOMPARE(run({QStringLiteral("install"), QStringLiteral("--help")}, &output), 2);
    QCOMPARE(run({QStringLiteral("remove"), QStringLiteral("ufw")}, &output), 2);
    QCOMPARE(run({QStringLiteral("install"), QStringLiteral("firewall"), QStringLiteral("extra")}, &output), 2);
    if (geteuid() != 0) {
        QCOMPARE(run({QStringLiteral("install"), QStringLiteral("firewall")}, &output), 1);
        QCOMPARE(output, QStringLiteral("@@error permission"));
    }
}

void MainuanTest::installsMissingComponentsWithRealProgress()
{
    MockSystem system({}, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));
    QCOMPARE(stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")).toString(),
             QStringLiteral("missing"));
    QVERIFY(stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("canInstall")).toBool());

    struct Sample { QString phase; int percent; int step; };
    QList<Sample> progress;
    connect(&installer, &InstallService::stateChanged, &installer, [&]() {
        progress.append({installer.phase(), installer.percent(), installer.step()});
    });

    QVERIFY(runToEnd(installer, QStringLiteral("antivirus")));
    QCOMPARE(installer.phase(), QStringLiteral("success"));
    QCOMPARE(installer.percent(), 100);
    QCOMPARE(installer.stepCount(), 2);
    QVERIFY(installer.resultMessage().contains(QStringLiteral("ClamAV e ClamUI")));

    // Phases are reported in order and stages without data stay indeterminate.
    QStringList phases;
    for (const Sample &sample : std::as_const(progress)) {
        if (phases.isEmpty() || phases.constLast() != sample.phase) {
            phases << sample.phase;
        }
        if (sample.phase == QStringLiteral("authenticating") || sample.phase == QStringLiteral("updating")
            || sample.phase == QStringLiteral("preparing")) {
            QCOMPARE(sample.percent, -1);
        }
    }
    for (const QString &expected : {QStringLiteral("authenticating"), QStringLiteral("updating"),
                                    QStringLiteral("downloading"), QStringLiteral("installing"),
                                    QStringLiteral("configuring"), QStringLiteral("success")}) {
        QVERIFY2(phases.contains(expected), qPrintable(expected + QStringLiteral(" missing from ") + phases.join(u',')));
    }

    // Every determinate download value comes from the backend: APT dlstatus
    // lines in step 1, Flatpak progress lines in step 2.
    const QList<int> aptReported = {0, 18, 41, 66, 88, 100};
    const QList<int> flatpakReported = {0, 25, 50, 75, 100};
    int aptSamples = 0;
    int flatpakSamples = 0;
    for (const Sample &sample : std::as_const(progress)) {
        if (sample.phase != QStringLiteral("downloading") || sample.percent < 0) {
            continue;
        }
        const QList<int> &expected = sample.step == 1 ? aptReported : flatpakReported;
        QVERIFY2(expected.contains(sample.percent),
                 qPrintable(QStringLiteral("step %1: %2").arg(sample.step).arg(sample.percent)));
        ++(sample.step == 1 ? aptSamples : flatpakSamples);
    }
    QVERIFY(aptSamples > 0);
    QVERIFY(flatpakSamples > 0);

    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")) == QStringLiteral("installed");
    }, 5000));
    QVERIFY(stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("canLaunch")).toBool());
    for (const QString &package : kClamAv) {
        QVERIFY(system.lines(QStringLiteral("deb")).contains(package));
    }
    QVERIFY(system.lines(QStringLiteral("flatpak")).constFirst().startsWith(kClamUi));
}

void MainuanTest::reportsAlreadyInstalledProfiles()
{
    MockSystem system(kClamAv, {kClamUi});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")) == QStringLiteral("installed");
    }, 5000));
    QVERIFY(!stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("canInstall")).toBool());

    QVERIFY(runToEnd(installer, QStringLiteral("antivirus")));
    QCOMPARE(installer.phase(), QStringLiteral("success"));
    QCOMPARE(installer.stepCount(), 0);
}

void MainuanTest::treatsDismissedAuthenticationAsCancelled()
{
    MockSystem system({}, {});
    qputenv("MOCK_SCENARIO", "cancel");
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));

    QVERIFY(runToEnd(installer, QStringLiteral("firewall")));
    QCOMPARE(installer.phase(), QStringLiteral("cancelled"));
    QVERIFY(!installer.busy());
    QVERIFY(installer.resultMessage().contains(QStringLiteral("Nenhuma alteração")));
    QVERIFY(system.lines(QStringLiteral("deb")).isEmpty());

    qputenv("MOCK_SCENARIO", "auth");
    QVERIFY(runToEnd(installer, QStringLiteral("firewall")));
    QCOMPARE(installer.phase(), QStringLiteral("error"));
    QVERIFY(installer.resultMessage().contains(QStringLiteral("autenticação")));
}

void MainuanTest::reportsPackageManagerErrors()
{
    MockSystem system({}, {});
    qputenv("MOCK_SCENARIO", "apt-error");
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));

    QVERIFY(runToEnd(installer, QStringLiteral("codecs")));
    QCOMPARE(installer.phase(), QStringLiteral("error"));
    QVERIFY(!installer.resultMessage().contains(QStringLiteral("dpkg")));
    QVERIFY(installer.details().contains(QStringLiteral("post-installation script")));
    QVERIFY(installer.details().contains(QStringLiteral("Sub-process /usr/bin/dpkg returned an error code")));
    QVERIFY(stateOf(installer, QStringLiteral("codecs")).value(QStringLiteral("state")) != QStringLiteral("installed"));
}

void MainuanTest::explainsNetworkAndLockFailures()
{
    MockSystem system({}, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));

    qputenv("MOCK_SCENARIO", "network");
    QVERIFY(runToEnd(installer, QStringLiteral("firewall")));
    QCOMPARE(installer.phase(), QStringLiteral("error"));
    QVERIFY(installer.resultMessage().contains(QStringLiteral("conexão com a internet")));
    QVERIFY(installer.details().contains(QStringLiteral("Temporary failure resolving")));

    qputenv("MOCK_SCENARIO", "locked");
    QVERIFY(runToEnd(installer, QStringLiteral("firewall")));
    QCOMPARE(installer.phase(), QStringLiteral("error"));
    QVERIFY(installer.resultMessage().contains(QStringLiteral("Outro programa")));

    qputenv("MOCK_SCENARIO", "flatpak-network");
    MockSystem withClamAv(kClamAv, {});
    Services second = makeServices();
    QVERIFY(waitForDetection(*second.installer));
    QVERIFY(runToEnd(*second.installer, QStringLiteral("antivirus")));
    QCOMPARE(second.installer->phase(), QStringLiteral("error"));
    QVERIFY(second.installer->resultMessage().contains(QStringLiteral("conexão com a internet")));
}

void MainuanTest::rejectsConcurrentStarts()
{
    MockSystem system({}, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));

    QSignalSpy finished(&installer, &InstallService::finished);
    QVERIFY(installer.start(QStringLiteral("firewall")));
    QVERIFY(installer.busy());
    QVERIFY(!installer.start(QStringLiteral("firewall")));
    QVERIFY(!installer.start(QStringLiteral("codecs")));
    installer.reset(); // ignored while running
    QVERIFY(installer.busy());
    QVERIFY(finished.wait(15000));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(system.lines(QStringLiteral("deb")).count(QStringLiteral("ufw")), 1);
}

void MainuanTest::detectsStateAgainOnRestart()
{
    MockSystem system({}, {});
    {
        Services services = makeServices();
        QVERIFY(waitForDetection(*services.installer));
        QVERIFY(runToEnd(*services.installer, QStringLiteral("firewall")));
        QCOMPARE(services.installer->phase(), QStringLiteral("success"));
    }
    Services restarted = makeServices();
    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(*restarted.installer, QStringLiteral("firewall")).value(QStringLiteral("state"))
            == QStringLiteral("installed");
    }, 5000));
    QCOMPARE(restarted.installer->phase(), QStringLiteral("idle"));
}

void MainuanTest::installsOnlyClamUiWhenClamAvExists()
{
    MockSystem system(kClamAv, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")) == QStringLiteral("partial");
    }, 5000));
    QCOMPARE(stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("summary")).toString(),
             QStringLiteral("Falta: ClamUI"));

    QVERIFY(runToEnd(installer, QStringLiteral("antivirus")));
    QCOMPARE(installer.phase(), QStringLiteral("success"));
    QCOMPARE(installer.stepCount(), 1);
    QVERIFY(!installer.details().contains(QStringLiteral("Pacotes a instalar")));
    QCOMPARE(system.lines(QStringLiteral("deb")), kClamAv);
}

void MainuanTest::installsOnlyClamAvWhenClamUiExists()
{
    MockSystem system({}, {kClamUi});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(installer, QStringLiteral("antivirus")).value(QStringLiteral("state")) == QStringLiteral("partial");
    }, 5000));

    QVERIFY(runToEnd(installer, QStringLiteral("antivirus")));
    QCOMPARE(installer.phase(), QStringLiteral("success"));
    QCOMPARE(installer.stepCount(), 1);
    QCOMPARE(system.lines(QStringLiteral("deb")), kClamAv);
    QCOMPARE(system.lines(QStringLiteral("flatpak")).size(), 1);
}

void MainuanTest::completesFirewallWithPlasmaIntegration()
{
    MockSystem system({QStringLiteral("ufw")}, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(QTest::qWaitFor([&]() {
        return stateOf(installer, QStringLiteral("firewall")).value(QStringLiteral("state")) == QStringLiteral("partial");
    }, 5000));

    QVERIFY(runToEnd(installer, QStringLiteral("firewall")));
    QCOMPARE(installer.phase(), QStringLiteral("success"));
    QVERIFY(installer.details().contains(QStringLiteral("Pacotes a instalar: plasma-firewall")));
    QCOMPARE(system.lines(QStringLiteral("deb")), (QStringList{QStringLiteral("ufw"), QStringLiteral("plasma-firewall")}));
}

void MainuanTest::treatsDeniedFlatpakAuthorizationAsCancelled()
{
    MockSystem system(kClamAv, {});
    qputenv("MOCK_SCENARIO", "flatpak-denied");
    Services services = makeServices();
    QVERIFY(waitForDetection(*services.installer));
    QVERIFY(runToEnd(*services.installer, QStringLiteral("antivirus")));
    QCOMPARE(services.installer->phase(), QStringLiteral("cancelled"));
}

void MainuanTest::disablesAptProfilesOnNonDebianSystems()
{
    MockSystem system({}, {});
    InstallService::Backend backend = mockBackend();
    backend.dpkgQuery = QStringLiteral("/nonexistent/dpkg-query");
    Services services = makeServices(backend);
    InstallService &installer = *services.installer;
    QVERIFY(waitForDetection(installer));
    QVERIFY(!installer.aptSupported());

    // Without dpkg the state comes from files on this host. Codecs are APT only:
    // unless the host already has them, they must be reported as unavailable.
    const QVariantMap codecs = stateOf(installer, QStringLiteral("codecs"));
    if (codecs.value(QStringLiteral("state")) != QStringLiteral("installed")) {
        QVERIFY(!codecs.value(QStringLiteral("canInstall")).toBool());
        QVERIFY(!codecs.value(QStringLiteral("unavailableReason")).toString().isEmpty());
        QVERIFY(runToEnd(installer, QStringLiteral("codecs")));
        QCOMPARE(installer.phase(), QStringLiteral("error"));
        QVERIFY(installer.resultMessage().contains(QStringLiteral("Debian ou Ubuntu")));
    }
    // Any profile that cannot be installed explains why; Flatpak-only gaps stay installable.
    for (const QString &profile : {QStringLiteral("antivirus"), QStringLiteral("firewall"), QStringLiteral("codecs")}) {
        const QVariantMap state = stateOf(installer, profile);
        if (state.value(QStringLiteral("state")) != QStringLiteral("installed")
            && !state.value(QStringLiteral("canInstall")).toBool()) {
            QVERIFY(!state.value(QStringLiteral("unavailableReason")).toString().isEmpty());
        }
    }
}

void MainuanTest::rejectsUnknownProfiles()
{
    MockSystem system({}, {});
    Services services = makeServices();
    InstallService &installer = *services.installer;
    QVERIFY(!installer.start(QStringLiteral("rm -rf /")));
    QVERIFY(!installer.start(QStringLiteral("app:org.example.Unknown")));
    QVERIFY(!installer.registerApplication(QStringLiteral("org.example.Unknown"), QStringLiteral("X"), {}, {}));
    QVERIFY(installer.registerApplication(QStringLiteral("com.brave.Browser"), QStringLiteral("Brave"), {}, {}));
    QVERIFY(!installer.registerApplication(QStringLiteral("com.brave.Browser"), QStringLiteral("Brave"), {}, {}));
    QVERIFY(!installer.busy());
    QVERIFY(!installer.launch(QStringLiteral("unknown")));
}

void MainuanTest::startupPreferenceDefaultsToShown()
{
    QTemporaryDir home;
    StartupPreference preference(nullptr, home.path());
    QVERIFY(preference.showAtStartup());
    QCOMPARE(QFileInfo(preference.overridePath()).fileName(), QStringLiteral("org.mainuan.Welcome.desktop"));
    QVERIFY(!QFileInfo::exists(preference.overridePath()));
}

void MainuanTest::startupPreferenceIsStoredPerUser()
{
    QTemporaryDir firstUser;
    QTemporaryDir secondUser;
    StartupPreference preference(nullptr, firstUser.path());
    QSignalSpy changed(&preference, &StartupPreference::showAtStartupChanged);

    preference.setShowAtStartup(false);
    QVERIFY(!preference.showAtStartup());
    QCOMPARE(changed.count(), 1);
    QFile file(preference.overridePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray contents = file.readAll();
    QVERIFY(contents.contains("[Desktop Entry]"));
    QVERIFY(contents.contains("Hidden=true"));

    // The choice survives a restart and does not affect another user.
    QVERIFY(!StartupPreference(nullptr, firstUser.path()).showAtStartup());
    QVERIFY(StartupPreference(nullptr, secondUser.path()).showAtStartup());

    preference.setShowAtStartup(true);
    QVERIFY(preference.showAtStartup());
    QVERIFY(!QFileInfo::exists(preference.overridePath()));
    QVERIFY(StartupPreference(nullptr, firstUser.path()).showAtStartup());
    QCOMPARE(changed.count(), 2);

    preference.setShowAtStartup(true); // no-op
    QCOMPARE(changed.count(), 2);
}

void MainuanTest::startupPreferenceReadsPlasmaOverrides()
{
    QTemporaryDir home;
    StartupPreference preference(nullptr, home.path());
    QDir().mkpath(QFileInfo(preference.overridePath()).absolutePath());
    const auto write = [&preference](const QByteArray &contents) {
        QFile file(preference.overridePath());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(contents);
    };

    write("[Desktop Entry]\nExec=mainuan-welcome\nHidden = TRUE\n");
    preference.reload();
    QVERIFY(!preference.showAtStartup());

    write("[Desktop Entry]\nExec=mainuan-welcome\nHidden=false\n[Desktop Action x]\nHidden=true\n");
    preference.reload();
    QVERIFY(preference.showAtStartup());

    write("[Desktop Entry]\nHiddenSomething=true\n");
    preference.reload();
    QVERIFY(preference.showAtStartup());
}

QTEST_GUILESS_MAIN(MainuanTest)
#include "tst_mainuan.moc"
