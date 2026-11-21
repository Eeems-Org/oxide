import QtQuick 2.9
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
    property int basesize: keyboard.width / 15
    width: parent.width
    height: basesize * 6
    implicitHeight: 92 * 6
    Rectangle {
        color: "transparent"
        anchors.fill: keyboard
        Column {
            id: qwerty
            anchors.centerIn: parent
            spacing: 0
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey { text: "Esc"; key: Qt.Key_Escape; keyboard: keyboard }
                Item { width: keyboard.basesize; height: 1 }
                KeyboardKey { text: "F1"; key: Qt.Key_F1; keyboard: keyboard }
                KeyboardKey { text: "F2"; key: Qt.Key_F2; keyboard: keyboard }
                KeyboardKey { text: "F3"; key: Qt.Key_F3; keyboard: keyboard }
                KeyboardKey { text: "F4"; key: Qt.Key_F4; keyboard: keyboard }
                Item { width: keyboard.basesize*0.5; height: 1 }
                KeyboardKey { text: "F5"; key: Qt.Key_F5; keyboard: keyboard }
                KeyboardKey { text: "F6"; key: Qt.Key_F6; keyboard: keyboard }
                KeyboardKey { text: "F7"; key: Qt.Key_F7; keyboard: keyboard }
                KeyboardKey { text: "F8"; key: Qt.Key_F8; keyboard: keyboard }
                Item { width: keyboard.basesize*0.5; height: 1 }
                KeyboardKey { text: "F9"; key: Qt.Key_F9; keyboard: keyboard }
                KeyboardKey { text: "F10"; key: Qt.Key_F10; keyboard: keyboard }
                KeyboardKey { text: "F11"; key: Qt.Key_F11; keyboard: keyboard }
                KeyboardKey { text: "F12"; key: Qt.Key_F12; keyboard: keyboard }
            }
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey { text: "`"; shifttext: "~"; keyboard: keyboard }
                KeyboardKey { text: "1"; shifttext: "!"; keyboard: keyboard }
                KeyboardKey { text: "2"; shifttext: "@"; keyboard: keyboard }
                KeyboardKey { text: "3"; shifttext: "#"; keyboard: keyboard }
                KeyboardKey { text: "4"; shifttext: "$"; keyboard: keyboard }
                KeyboardKey { text: "5"; shifttext: "%"; keyboard: keyboard }
                KeyboardKey { text: "6"; shifttext: "^"; keyboard: keyboard }
                KeyboardKey { text: "7"; shifttext: "&"; keyboard: keyboard }
                KeyboardKey { text: "8"; shifttext: "*"; keyboard: keyboard }
                KeyboardKey { text: "9"; shifttext: "("; keyboard: keyboard }
                KeyboardKey { text: "0"; shifttext: ")"; keyboard: keyboard }
                KeyboardKey { text: "-"; shifttext: "_"; keyboard: keyboard }
                KeyboardKey { text: "="; shifttext: "+"; keyboard: keyboard }
                KeyboardKey { text: "Backspace"; size: 2; key: Qt.Key_Backspace; fontsize: 6; keyboard: keyboard }
            }
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey { text: "⭾"; value: "\t"; size: 2; key: Qt.Key_Tab; keyboard: keyboard }
                KeyboardKey { text: "q"; shifttext: "Q"; keyboard: keyboard }
                KeyboardKey { text: "w"; shifttext: "W"; keyboard: keyboard }
                KeyboardKey { text: "e"; shifttext: "E"; keyboard: keyboard }
                KeyboardKey { text: "r"; shifttext: "R"; keyboard: keyboard }
                KeyboardKey { text: "t"; shifttext: "T"; keyboard: keyboard }
                KeyboardKey { text: "y"; shifttext: "Y"; keyboard: keyboard }
                KeyboardKey { text: "u"; shifttext: "U"; keyboard: keyboard }
                KeyboardKey { text: "i"; shifttext: "I"; keyboard: keyboard }
                KeyboardKey { text: "o"; shifttext: "O"; keyboard: keyboard }
                KeyboardKey { text: "p"; shifttext: "P"; keyboard: keyboard }
                KeyboardKey { text: "["; shifttext: "{"; keyboard: keyboard }
                KeyboardKey { text: "]"; shifttext: "}"; keyboard: keyboard }
                KeyboardKey { text: "\\"; shifttext: "|"; keyboard: keyboard }
            }
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey {
                    text: "Caps Lock"; key: Qt.Key_CapsLock; size: 2; checkable: true; keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasCaps
                    onClick: {
                        keyboard.hasCaps = !keyboard.hasCaps
                        return true;
                    }
                }
                KeyboardKey { text: "a"; shifttext: "A"; keyboard: keyboard }
                KeyboardKey { text: "s"; shifttext: "S"; keyboard: keyboard }
                KeyboardKey { text: "d"; shifttext: "D"; keyboard: keyboard }
                KeyboardKey { text: "f"; shifttext: "F"; keyboard: keyboard }
                KeyboardKey { text: "g"; shifttext: "G"; keyboard: keyboard }
                KeyboardKey { text: "h"; shifttext: "H"; keyboard: keyboard }
                KeyboardKey { text: "j"; shifttext: "J"; keyboard: keyboard }
                KeyboardKey { text: "k"; shifttext: "K"; keyboard: keyboard }
                KeyboardKey { text: "l"; shifttext: "L"; keyboard: keyboard }
                KeyboardKey { text: "↵"; value: "\n"; size: 3; key: Qt.Key_Enter; keyboard: keyboard }
            }
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey {
                    text: "⇧"; key: Qt.Key_Shift; size: 2; checkable: true; keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasShift
                    onClick: {
                        keyboard.hasShift = !keyboard.hasShift
                        return true;
                    }
                }
                KeyboardKey { text: "z"; shifttext: "Z"; keyboard: keyboard }
                KeyboardKey { text: "x"; shifttext: "X"; keyboard: keyboard }
                KeyboardKey { text: "c"; shifttext: "C"; keyboard: keyboard }
                KeyboardKey { text: "v"; shifttext: "V"; keyboard: keyboard }
                KeyboardKey { text: "b"; shifttext: "B"; keyboard: keyboard }
                KeyboardKey { text: "n"; shifttext: "N"; keyboard: keyboard }
                KeyboardKey { text: "m"; shifttext: "M"; keyboard: keyboard }
                KeyboardKey { text: ","; shifttext: "<"; keyboard: keyboard }
                KeyboardKey { text: "."; shifttext: ">"; keyboard: keyboard }
                KeyboardKey { text: "/"; shifttext: "?"; keyboard: keyboard }
                Item { width: keyboard.basesize; height: 1 }
                KeyboardKey { text: "↑"; key: Qt.Key_Up; keyboard: keyboard }
                Item { width: keyboard.basesize; height: 1 }
            }
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey {
                    text: "Ctrl"; key: Qt.Key_Control; checkable: true; keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasCtrl
                    onClick: {
                        keyboard.hasCtrl = !keyboard.hasCtrl
                        return true;
                    }
                }
                KeyboardKey {
                    text: "❖"; key: Qt.Key_Meta; checkable: true; keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasMeta
                    onClick: {
                        keyboard.hasMeta = !keyboard.hasMeta
                        return true;
                    }
                }
                KeyboardKey {
                    text: "Alt"; key: Qt.Key_Alt; checkable: true; keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasAlt
                    onClick: {
                        keyboard.hasAlt = !keyboard.hasAlt
                        return true;
                    }
                }
                Item { width: keyboard.basesize; height: 1 }
                KeyboardKey { text: " "; key: Qt.Key_Space; value: " "; size: 7; keyboard: keyboard }
                Item { width: keyboard.basesize; height: 1 }
                KeyboardKey { text: "←"; key: Qt.Key_Left; keyboard: keyboard }
                KeyboardKey { text: "↓"; key: Qt.Key_Down; keyboard: keyboard }
                KeyboardKey { text: "→"; key: Qt.Key_Right; keyboard: keyboard }
            }
        }
    }
    KeyboardHandler { id: handler; objectName: "keyboard" }
    Component.onCompleted: {
        handler.root = keyboard.parent;
    }
}
