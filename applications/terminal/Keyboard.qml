import QtQuick 2.9
import KeyboardHandler 1.0
import "."

Item {
    id: keyboard
    clip: true
    visible: !handler.keyboardAttached
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    property int page: 0
    property bool hasCaps: false
    property bool hasShift: false
    property bool hasAlt: false
    property bool hasCtrl: false
    property bool hasMeta: false
    property int basesize: 90
    function hide() {
        keyboard.visible = false;
    }
    width: parent.width
    height: 630
    Rectangle {
        color: "transparent"
        anchors.fill: keyboard
        Column {
            id: qwerty
            anchors.centerIn: parent
            spacing: 4
            visible: keyboard.page == 0
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey { text: "Esc"; key: Qt.Key_Escape; fontsize: 6; keyboard: keyboard }
                Item { width: 90; height: 1 }
                KeyboardKey { text: "F1"; key: Qt.Key_F1; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "F2"; key: Qt.Key_F2; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "F3"; key: Qt.Key_F3; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "F4"; key: Qt.Key_F4; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "F5"; key: Qt.Key_F5; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "F6"; key: Qt.Key_F6; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "F7"; key: Qt.Key_F7; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "F8"; key: Qt.Key_F8; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "F9"; key: Qt.Key_F9; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "F10"; key: Qt.Key_F10; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "F11"; key: Qt.Key_F11; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "F12"; key: Qt.Key_F12; fontsize: 6; keyboard: keyboard }
            }
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
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
                KeyboardKey { text: "Enter"; size: 2; key: Qt.Key_Enter; keyboard: keyboard }
                Item { width: 90*2; height: 1 }
            }
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey {
                    text: "Shift"; key: Qt.Key_Shift; size: 2; checkable: true; keyboard: keyboard
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
                Item { width: 90; height: 1 }
                KeyboardKey { text: "↑"; key: Qt.Key_Up; fontsize: 6; keyboard: keyboard }
                Item { width: 90; height: 1 }
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
                    text: "Meta"; key: Qt.Key_Meta; checkable: true; keyboard: keyboard
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
                Item { width: 90*3; height: 1 }
                KeyboardKey { text: "Space"; key: Qt.Key_Space; value: " "; size: 4; keyboard: keyboard }
                Item { width: 90*2; height: 1 }
                KeyboardKey { text: "←"; key: Qt.Key_Left; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "↓"; key: Qt.Key_Down; fontsize: 6; keyboard: keyboard }
                KeyboardKey { text: "→"; key: Qt.Key_Right; fontsize: 6; keyboard: keyboard }
            }
        }
    }
    KeyboardHandler { id: handler; objectName: "keyboard" }
    Component.onCompleted: {
        handler.root = keyboard.parent;
    }
}
