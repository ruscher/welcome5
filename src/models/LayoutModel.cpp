#include "LayoutModel.h"

LayoutModel::LayoutModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Plasma 6 does not provide a stable public API for importing arbitrary panel
    // layouts. Latte Dock and Bismuth are not Plasma 6.6 baseline dependencies.
    m_entries = {
        {QStringLiteral("plasma-default"), QStringLiteral("Plasma padrão"),
         QStringLiteral("Layout nativo do Plasma, sem dependências externas."), QStringLiteral("Disponível"), true},
        {QStringLiteral("panel-top"), QStringLiteral("Painel superior"),
         QStringLiteral("Requer um perfil específico do Plasma; não disponível com segurança."), QStringLiteral("Incompatível"), false},
        {QStringLiteral("floating"), QStringLiteral("Flutuante"),
         QStringLiteral("Dependia de configurações antigas de dock."), QStringLiteral("Incompatível"), false},
        {QStringLiteral("minimal"), QStringLiteral("Minimalista"),
         QStringLiteral("Não há API pública do Plasma 6.6 para aplicar este perfil."), QStringLiteral("Incompatível"), false},
        {QStringLiteral("latte-unity"), QStringLiteral("Unity-like"),
         QStringLiteral("Latte Dock não é dependência suportada no Plasma 6.6."), QStringLiteral("Legado"), false},
        {QStringLiteral("tiling"), QStringLiteral("Tiling"),
         QStringLiteral("Bismuth não é dependência suportada no Plasma 6.6."), QStringLiteral("Legado"), false}
    };
}

int LayoutModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant LayoutModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case LayoutIdRole: return entry.id;
    case NameRole: return entry.name;
    case DescriptionRole: return entry.description;
    case StatusRole: return entry.status;
    case AvailableRole: return entry.available;
    default: return {};
    }
}

QHash<int, QByteArray> LayoutModel::roleNames() const
{
    return {{LayoutIdRole, "layoutId"}, {NameRole, "name"}, {DescriptionRole, "description"},
            {StatusRole, "status"}, {AvailableRole, "available"}};
}
