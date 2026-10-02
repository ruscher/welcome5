import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

// A small group of mutually exclusive buttons ("Claro | Escuro"). The current
// value comes from the system, so the buttons never keep a local checked state.
RowLayout {
    id: choice

    // [{ value: "light", label: "Claro", icon: "weather-clear" }, …]
    property var options: []
    property string current: ""
    property string groupName: ""
    signal activated(string value)

    spacing: 6
    Accessible.role: Accessible.Grouping
    Accessible.name: groupName

    Repeater {
        model: choice.options
        delegate: Controls.Button {
            required property var modelData
            readonly property bool selected: choice.current === modelData.value
            text: modelData.label
            icon.name: modelData.icon
            highlighted: selected
            onClicked: if (!selected) choice.activated(modelData.value)
            Accessible.role: Accessible.RadioButton
            Accessible.checkable: true
            Accessible.checked: selected
            Accessible.name: choice.groupName + ": " + modelData.label
        }
    }
}
