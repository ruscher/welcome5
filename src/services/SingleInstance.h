#pragma once

#include <QLocalServer>
#include <QObject>

class SingleInstance final : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);
    bool tryAcquire();

signals:
    void activationRequested();

private:
    QString serverName() const;
    QLocalServer m_server;
    QString m_name;
};
