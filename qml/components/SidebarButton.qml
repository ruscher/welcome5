import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Controls.Button {
    id: control

    property bool selected: false
    property bool collapsed: false
    property string label: ""
    property string iconName: ""
    property bool dark: false
    property color accent: "#3daee9"
    property color mutedColor: "#697286"
    property color textColor: "#202532"

    implicitHeight: 44
    implicitWidth: collapsed ? 58 : 210
    text: collapsed ? "" : label
    display: collapsed ? Controls.AbstractButton.IconOnly : Controls.AbstractButton.TextBesideIcon
    icon.name: iconName
    icon.width: 20
    icon.height: 20
    leftPadding: collapsed ? 18 : 16
    rightPadding: collapsed ? 18 : 12
    Accessible.name: label
    Controls.ToolTip.visible: collapsed && hovered
    Controls.ToolTip.text: label

    background: Rectangle {
        radius: 10
        color: control.selected ? Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.20 : 0.12)
                                : (control.hovered ? Qt.rgba(1, 1, 1, dark ? 0.07 : 0.50) : "transparent")
        border.color: control.selected ? Qt.rgba(accent.r, accent.g, accent.b, 0.45) : "transparent"
        border.width: control.selected ? 1 : 0
    }

    contentItem: Row {
        spacing: 11
        anchors.fill: parent
        anchors.leftMargin: control.leftPadding
        anchors.rightMargin: control.rightPadding
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        layoutDirection: Qt.LeftToRight

        Kirigami.Icon {
            width: 20
            height: 20
            anchors.verticalCenter: parent.verticalCenter
            source: control.iconName
            color: control.selected ? accent : mutedColor
        }

        Controls.Label {
            visible: !control.collapsed
            width: parent.width - 31
            anchors.verticalCenter: parent.verticalCenter
            text: control.label
            color: control.selected ? accent : textColor
            elide: Text.ElideRight
            font.weight: control.selected ? Font.DemiBold : Font.Normal
        }
    }
}
