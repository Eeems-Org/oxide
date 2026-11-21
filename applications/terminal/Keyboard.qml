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
    property int columns: 15
    property int basesize: keyboard.width / keyboard.columns
    width: parent.width
    height: basesize * 6
    implicitHeight: 92 * 6
    Rectangle {
        color: "transparent"
        anchors.fill: keyboard
        GridLayout {
            anchors.fille: parent
            rows: 6
            columns: keyboard.columns * 2
            spacing: 0
            KeyboardKey { text: "Esc"; key: Qt.Key_Escape; keyboard: keyboard; layout.columnSpan: size * 2 }
            Item { width: keyboard.basesize; height: 1; layout.columnSpan: 2 }
            KeyboardKey { text: "F1"; key: Qt.Key_F1; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "F2"; key: Qt.Key_F2; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "F3"; key: Qt.Key_F3; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "F4"; key: Qt.Key_F4; keyboard: keyboard; layout.columnSpan: size * 2 }
            Item { width: keyboard.basesize*0.5; height: 1; layout.columnSpan: 1 }
            KeyboardKey { text: "F5"; key: Qt.Key_F5; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "F6"; key: Qt.Key_F6; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "F7"; key: Qt.Key_F7; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "F8"; key: Qt.Key_F8; keyboard: keyboard; layout.columnSpan: size * 2 }
            Item { width: keyboard.basesize*0.5; height: 1; layout.columnSpan: 1 }
            KeyboardKey { text: "F9"; key: Qt.Key_F9; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "F10"; key: Qt.Key_F10; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "F11"; key: Qt.Key_F11; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "F12"; key: Qt.Key_F12; keyboard: keyboard; layout.columnSpan: size * 2 }

            KeyboardKey { text: "`"; shifttext: "~"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "1"; shifttext: "!"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "2"; shifttext: "@"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "3"; shifttext: "#"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "4"; shifttext: "$"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "5"; shifttext: "%"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "6"; shifttext: "^"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "7"; shifttext: "&"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "8"; shifttext: "*"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "9"; shifttext: "("; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "0"; shifttext: ")"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "-"; shifttext: "_"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "="; shifttext: "+"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey {
                text: "Backspace"; size: 2; key: Qt.Key_Backspace; fontsize: 6
                keyboard: keyboard; layout.columnSpan: size * 2
            }

            KeyboardKey {
                text: "⭾"; value: "\t"; size: 2; key: Qt.Key_Tab
                keyboard: keyboard; layout.columnSpan: size * 2
            }
            KeyboardKey { text: "q"; shifttext: "Q"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "w"; shifttext: "W"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "e"; shifttext: "E"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "r"; shifttext: "R"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "t"; shifttext: "T"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "y"; shifttext: "Y"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "u"; shifttext: "U"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "i"; shifttext: "I"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "o"; shifttext: "O"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "p"; shifttext: "P"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "["; shifttext: "{"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "]"; shifttext: "}"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "\\"; shifttext: "|"; keyboard: keyboard; layout.columnSpan: size * 2 }

            KeyboardKey {
                text: "Caps Lock"; key: Qt.Key_CapsLock; size: 2; checkable: true
                keyboard: keyboard; layout.columnSpan: size * 2
                borderWidth: this.checked ? 3 : 1
                checked: keyboard.hasCaps
                onClick: {
                    keyboard.hasCaps = !keyboard.hasCaps
                    return true;
                }
            }
            KeyboardKey { text: "a"; shifttext: "A"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "s"; shifttext: "S"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "d"; shifttext: "D"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "f"; shifttext: "F"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "g"; shifttext: "G"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "h"; shifttext: "H"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "j"; shifttext: "J"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "k"; shifttext: "K"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "l"; shifttext: "L"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey {
                text: "↵"; value: "\n"; size: 3; key: Qt.Key_Enter; keyboard: keyboard
                layout.columnSpan: size * 2
            }

            KeyboardKey {
                text: "⇧"; key: Qt.Key_Shift; size: 2; checkable: true
                keyboard: keyboard; layout.columnSpan: size * 2
                borderWidth: this.checked ? 3 : 1
                checked: keyboard.hasShift
                onClick: {
                    keyboard.hasShift = !keyboard.hasShift
                    return true;
                }
            }
            KeyboardKey { text: "z"; shifttext: "Z"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "x"; shifttext: "X"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "c"; shifttext: "C"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "v"; shifttext: "V"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "b"; shifttext: "B"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "n"; shifttext: "N"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "m"; shifttext: "M"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: ","; shifttext: "<"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "."; shifttext: ">"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "/"; shifttext: "?"; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "PgUp"; key: Qt.Key_PageUp; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "↑"; key: Qt.Key_Up; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "PgDown"; key: Qt.Key_PageDown; keyboard: keyboard; layout.columnSpan: size * 2 }

            KeyboardKey {
                text: "Ctrl"; key: Qt.Key_Control; checkable: true
                keyboard: keyboard; layout.columnSpan: size * 2
                borderWidth: this.checked ? 3 : 1
                checked: keyboard.hasCtrl
                onClick: {
                    keyboard.hasCtrl = !keyboard.hasCtrl
                    return true;
                }
            }
            KeyboardKey {
                text: "❖"; key: Qt.Key_Meta; checkable: true
                keyboard: keyboard; layout.columnSpan: size * 2
                borderWidth: this.checked ? 3 : 1
                checked: keyboard.hasMeta
                onClick: {
                    keyboard.hasMeta = !keyboard.hasMeta
                    return true;
                }
            }
            KeyboardKey {
                text: "Alt"; key: Qt.Key_Alt; checkable: true
                keyboard: keyboard; layout.columnSpan: size * 2
                borderWidth: this.checked ? 3 : 1
                checked: keyboard.hasAlt
                onClick: {
                    keyboard.hasAlt = !keyboard.hasAlt
                    return true;
                }
            }
            Item { width: keyboard.basesize; height: 1; layout.columnSpan: 2 }
            KeyboardKey {
                text: " "; key: Qt.Key_Space; value: " "; size: 7
                keyboard: keyboard; layout.columnSpan: size * 2
            }
            Item { width: keyboard.basesize; height: 1; layout.columnSpan: 2 }
            KeyboardKey { text: "←"; key: Qt.Key_Left; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "↓"; key: Qt.Key_Down; keyboard: keyboard; layout.columnSpan: size * 2 }
            KeyboardKey { text: "→"; key: Qt.Key_Right; keyboard: keyboard; layout.columnSpan: size * 2 }
        }
    }
    KeyboardHandler { id: handler; objectName: "keyboard" }
    Component.onCompleted: {
        handler.root = keyboard.parent;
    }
}
