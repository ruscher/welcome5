#include "SingleInstance.h"

#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection, this, [this]() {
        while (m_server.hasPendingConnections()) {
            QLocalSocket *socket = m_server.nextPendingConnection();
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
                socket->readAll();
                emit activationRequested();
                socket->disconnectFromServer();
            });
            connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        }
    });
}

QString SingleInstance::serverName() const
{
    const QString localData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(localData).filePath(QStringLiteral("mainuan-welcome.sock"));
}

bool SingleInstance::tryAcquire()
{
    m_name = serverName();
    QDir().mkpath(QFileInfo(m_name).absolutePath());

    QLocalSocket client;
    client.connectToServer(m_name);
    if (client.waitForConnected(150)) {
        client.write("activate");
        client.waitForBytesWritten(100);
        client.disconnectFromServer();
        return false;
    }

    QLocalServer::removeServer(m_name);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(m_name)) {
        // Failing to create the local socket must not prevent a user from opening the app.
        return true;
    }
    return true;
}
