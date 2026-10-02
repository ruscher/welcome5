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

    title: "Contribuir"
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
            Kirigami.Heading { text: "Contribuir"; level: 1 }
            Controls.Label { Layout.fillWidth: true; text: "Ajude a manter o Mainuan útil, transparente e desenvolvido em comunidade."; color: page.mutedColor; wrapMode: Text.WordWrap }

            Rectangle {
                Layout.fillWidth: true; implicitHeight: 190; radius: 14; color: page.surfaceColor; border.color: page.borderColor
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 10
                    Controls.Label { text: "Doação via Pix"; color: page.textColor; font.weight: Font.DemiBold; font.pixelSize: 16 }
                    Controls.Label { Layout.fillWidth: true; text: "A chave é fornecida pela configuração do sistema quando o pacote oficial a disponibilizar. Nenhum valor fictício é exibido."; color: page.mutedColor; wrapMode: Text.WordWrap }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 42; radius: 8; color: pageColor; border.color: page.borderColor; Controls.Label { anchors.fill: parent; anchors.margins: 12; text: page.system.pixKey.length > 0 ? page.system.pixKey : "Chave Pix não configurada"; color: page.system.pixKey.length > 0 ? page.textColor : page.mutedColor; elide: Text.ElideMiddle; verticalAlignment: Text.AlignVCenter } }
                    Controls.Button { text: "Copiar chave Pix"; enabled: page.system.pixKey.length > 0; onClicked: page.system.copyText(page.system.pixKey) }
                }
            }

            Controls.Label { text: "Outras formas de ajudar"; color: page.mutedColor; font.weight: Font.DemiBold; font.pixelSize: 12 }
            Controls.Button { Layout.fillWidth: true; text: "Contribuir com código · GitHub"; onClicked: page.system.openUrl("https://github.com/mainuanos/mainuan") }
            Controls.Button { Layout.fillWidth: true; text: "Reportar um problema"; onClicked: page.system.openUrl("https://github.com/mainuanos/mainuan/issues/new") }
            Controls.Button { Layout.fillWidth: true; text: "Comunidade no Telegram"; onClicked: page.system.openUrl("https://t.me/mainuanos") }
            Controls.Label { Layout.fillWidth: true; Layout.bottomMargin: 20; text: "Os links só são abertos quando você os solicita; a interface não depende de rede para renderizar."; color: page.mutedColor; wrapMode: Text.WordWrap }
        }
    }
}
