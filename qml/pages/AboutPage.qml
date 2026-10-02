import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    property var system
    property bool dark: false
    property color accent: "#3daee9"
    property color pageColor: "#f3f5f9"
    property color surfaceColor: "#ffffff"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"

    title: "Sobre"
    padding: 0
    background: Rectangle { color: page.pageColor }

    Controls.ScrollView {
        anchors.fill: parent
        clip: true
        ColumnLayout {
            width: Math.min(page.width - 48, 820)
            x: Math.max(24, (page.width - width) / 2)
            y: 24
            spacing: 16
            Kirigami.Heading { text: "Sobre o Mainuan"; level: 1 }
            Controls.Label { Layout.fillWidth: true; text: "O Welcome apresenta ferramentas essenciais para os primeiros minutos no Mainuan."; color: page.mutedColor; wrapMode: Text.WordWrap }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 150
                radius: 14
                color: page.surfaceColor
                border.color: page.borderColor
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 8
                    Controls.Label { text: "Mainuan Welcome"; color: page.textColor; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Controls.Label { text: "Versão nativa Qt 6 · interface QML · backend C++"; color: page.accent }
                    Controls.Label { Layout.fillWidth: true; text: "Este projeto não incorpora nomes de equipe fictícios ou avatares remotos do protótipo. Créditos verificáveis e histórico estão disponíveis no repositório."; color: page.mutedColor; wrapMode: Text.WordWrap }
                    Controls.Button { text: "Abrir repositório"; onClicked: page.system.openUrl("https://github.com/ruscher/welcome5") }
                }
            }
            Controls.Label { text: "Créditos e dependências"; color: page.mutedColor; font.weight: Font.DemiBold; font.pixelSize: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 78; radius: 12; color: page.surfaceColor; border.color: page.borderColor
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    Controls.Label { text: "KDE Plasma"; color: page.textColor; font.weight: Font.DemiBold }
                    Controls.Label { Layout.fillWidth: true; text: "Ambiente de desktop e componentes de integração."; color: page.mutedColor; wrapMode: Text.WordWrap }
                    Controls.Button { text: "kde.org"; onClicked: page.system.openUrl("https://kde.org") }
                }
            }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 78; radius: 12; color: page.surfaceColor; border.color: page.borderColor
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    Controls.Label { text: "Ubuntu / Kubuntu"; color: page.textColor; font.weight: Font.DemiBold }
                    Controls.Label { Layout.fillWidth: true; text: "Base de distribuição considerada pelo legado Mainuan."; color: page.mutedColor; wrapMode: Text.WordWrap }
                    Controls.Button { text: "ubuntu.com"; onClicked: page.system.openUrl("https://ubuntu.com") }
                }
            }
            Controls.Label { Layout.fillWidth: true; Layout.bottomMargin: 20; text: "Software livre. Consulte a licença distribuída pelo pacote e o repositório para informações de contribuição."; color: page.mutedColor; wrapMode: Text.WordWrap }
        }
    }
}
