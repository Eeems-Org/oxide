import QtQuick 2.9
import QtQuick.Layouts 1.0
import KeyboardHandler 1.0
import "."

Item {
    id: keyboard
    clip: true
    visible: !handler.keyboardAttached
    property bool hasCaps: false
    property bool hasShift: false
    property bool hasAlt: false
    property bool hasCtrl: false
    property bool hasMeta: false
    property int basesize: qwerty.width / 15
    width: parent.width
    height: basesize * 6
    implicitHeight: basesize * 6
    Rectangle {
        color: "transparent"
        anchors.fill: keyboard
        ColumnLayout {
            id: qwerty
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.bottomMargin: 3
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: 0
            uniformCellSizes: true

            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "Esc"; key: Qt.Key_Escape; keyboard: keyboard
                }
                Item {
                    Layout.fillHeight: true;
                    implicitWidth: keyboard.basesize
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F1"; key: Qt.Key_F1; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F2"; key: Qt.Key_F2; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F3"; key: Qt.Key_F3; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F4"; key: Qt.Key_F4; keyboard: keyboard
                }
                Item {
                    Layout.fillHeight: true;
                    implicitWidth: keyboard.basesize / 2
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F5"; key: Qt.Key_F5; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F6"; key: Qt.Key_F6; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F7"; key: Qt.Key_F7; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F8"; key: Qt.Key_F8; keyboard: keyboard
                }
                Item {
                    Layout.fillHeight: true;
                    implicitWidth: keyboard.basesize / 2
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F9"; key: Qt.Key_F9; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F10"; key: Qt.Key_F10; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F11"; key: Qt.Key_F11; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "F12"; key: Qt.Key_F12; keyboard: keyboard
                }
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
            }
            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "`"; shifttext: "~"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "1"; shifttext: "!"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "2"; shifttext: "@"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "3"; shifttext: "#"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "4"; shifttext: "$"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "5"; shifttext: "%"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "6"; shifttext: "^"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "7"; shifttext: "&"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "8"; shifttext: "*"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "9"; shifttext: "("; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "0"; shifttext: ")"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "-"; shifttext: "_"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "="; shifttext: "+"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "Backspace"; size: 2; key: Qt.Key_Backspace;
                    keyboard: keyboard
                }
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
            }
            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "Tab"; value: "\t"; size: 2; key: Qt.Key_Tab
                    keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "q"; shifttext: "Q"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "w"; shifttext: "W"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "e"; shifttext: "E"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "r"; shifttext: "R"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "t"; shifttext: "T"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "y"; shifttext: "Y"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "u"; shifttext: "U"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "i"; shifttext: "I"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "o"; shifttext: "O"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "p"; shifttext: "P"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "["; shifttext: "{"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "]"; shifttext: "}"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "\\"; shifttext: "|"; keyboard: keyboard
                }
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
            }
            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "Caps Lock"; key: Qt.Key_CapsLock; size: 2; checkable: true
                    keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasCaps
                    onClicked: {
                        keyboard.hasCaps = !keyboard.hasCaps
                        doClick()
                    }
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "a"; shifttext: "A"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "s"; shifttext: "S"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "d"; shifttext: "D"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "f"; shifttext: "F"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "g"; shifttext: "G"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "h"; shifttext: "H"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "j"; shifttext: "J"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "k"; shifttext: "K"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "l"; shifttext: "L"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "Enter"; value: "\n"; size: 3; key: Qt.Key_Enter; keyboard: keyboard
                    Layout.columnSpan: size * 2
                }
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
            }
            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    font.pixelSize: 32
                    text: "⇧"; key: Qt.Key_Shift; size: 2; checkable: true
                    keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasShift
                    onClicked: {
                        keyboard.hasShift = !keyboard.hasShift
                        doClick()
                    }
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "z"; shifttext: "Z"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "x"; shifttext: "X"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "c"; shifttext: "C"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "v"; shifttext: "V"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "b"; shifttext: "B"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "n"; shifttext: "N"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "m"; shifttext: "M"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: ","; shifttext: "<"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "."; shifttext: ">"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "/"; shifttext: "?"; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    font.pixelSize: 20
                    text: "PgUp"; key: Qt.Key_PageUp; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    font.pixelSize: 32
                    text: "↑"; key: Qt.Key_Up; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    font.pixelSize: 20
                    text: "PgDown"; key: Qt.Key_PageDown; keyboard: keyboard
                }
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
            }
            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "Ctrl"; key: Qt.Key_Control; checkable: true
                    keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasCtrl
                    onClicked: {
                        keyboard.hasCtrl = !keyboard.hasCtrl
                        doClick()
                    }
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "❖"; key: Qt.Key_Meta; checkable: true
                    keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasMeta
                    onClicked: {
                        keyboard.hasMeta = !keyboard.hasMeta
                        doClick()
                    }
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: "Alt"; key: Qt.Key_Alt; checkable: true
                    keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasAlt
                    onClicked: {
                        keyboard.hasAlt = !keyboard.hasAlt
                        doClick()
                    }
                }
                Item {
                    Layout.fillHeight: true;
                    implicitWidth: keyboard.basesize
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    text: " "; key: Qt.Key_Space; value: " "; size: 7
                    keyboard: keyboard
                }
                Item {
                    Layout.fillHeight: true;
                    implicitWidth: keyboard.basesize
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    font.pixelSize: 32
                    text: "←"; key: Qt.Key_Left; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    font.pixelSize: 32
                    text: "↓"; key: Qt.Key_Down; keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1; Layout.topMargin: -1
                    font.pixelSize: 32
                    text: "→"; key: Qt.Key_Right; keyboard: keyboard
                }
                Item { Layout.fillWidth: true; Layout.fillHeight: true; width: 1; height: 1 }
            }
        }
    }
    KeyboardHandler { id: handler; objectName: "keyboard" }
    Component.onCompleted: {
        handler.root = keyboard.parent;
    }
}
