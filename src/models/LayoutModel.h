#pragma once

#include <QAbstractListModel>
#include <QList>

class LayoutModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        LayoutIdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        StatusRole,
        AvailableRole
    };

    explicit LayoutModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Entry {
        QString id;
        QString name;
        QString description;
        QString status;
        bool available = false;
    };
    QList<Entry> m_entries;
};
