import QtQuick 2.9
import KeyboardHandler 1.0
import "."

Item {
    id: keyboard
    clip: true
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    property int page: 0
    property bool hasCaps: false
    property bool hasShift: false
    property bool hasAlt: false
    property bool hasCtrl: false
    property bool hasMeta: false
    function hide() {
        keyboard.visible = false;
    }
    width: parent.width
    height: 260
    Rectangle {
        color: "black"
        anchors.fill: keyboard
        Column {
            id: qwerty
            anchors.centerIn: parent
            spacing: 4
            visible: keyboard.page == 0
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey { text: "1"; shifttext: "!" }
                KeyboardKey { text: "2"; shifttext: "@" }
                KeyboardKey { text: "3"; shifttext: "#" }
                KeyboardKey { text: "4"; shifttext: "$" }
                KeyboardKey { text: "5"; shifttext: "%" }
                KeyboardKey { text: "6"; shifttext: "^" }
                KeyboardKey { text: "7"; shifttext: "&" }
                KeyboardKey { text: "8"; shifttext: "*" }
                KeyboardKey { text: "9"; shifttext: "(" }
                KeyboardKey { text: "0"; shifttext: ")" }
                KeyboardKey { text: "-"; shifttext: "_" }
                KeyboardKey { text: "="; shifttext: "+" }
                KeyboardKey { text: "Backspace"; size: 2; key: Qt.Key_Backspace; fontsize: 6 }
            }
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey { text: "q"; shifttext: "Q" }
                KeyboardKey { text: "w"; shifttext: "W" }
                KeyboardKey { text: "e"; shifttext: "E" }
                KeyboardKey { text: "r"; shifttext: "R" }
                KeyboardKey { text: "t"; shifttext: "T" }
                KeyboardKey { text: "y"; shifttext: "Y" }
                KeyboardKey { text: "u"; shifttext: "U" }
                KeyboardKey { text: "i"; shifttext: "I" }
                KeyboardKey { text: "o"; shifttext: "O" }
                KeyboardKey { text: "p"; shifttext: "P" }
                KeyboardKey { text: "["; shifttext: "{" }
                KeyboardKey { text: "]"; shifttext: "}" }
                KeyboardKey { text: "\\"; shifttext: "|" }
            }
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey { text: "a"; shifttext: "A" }
                KeyboardKey { text: "s"; shifttext: "S" }
                KeyboardKey { text: "d"; shifttext: "D" }
                KeyboardKey { text: "f"; shifttext: "F" }
                KeyboardKey { text: "g"; shifttext: "G" }
                KeyboardKey { text: "h"; shifttext: "H" }
                KeyboardKey { text: "j"; shifttext: "J" }
                KeyboardKey { text: "k"; shifttext: "K" }
                KeyboardKey { text: "l"; shifttext: "L" }
                KeyboardKey { text: "Enter"; size: 2; key: Qt.Key_Enter }
            }
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
                KeyboardKey { text: "z"; shifttext: "Z" }
                KeyboardKey { text: "x"; shifttext: "X" }
                KeyboardKey { text: "c"; shifttext: "C" }
                KeyboardKey { text: "v"; shifttext: "V" }
                KeyboardKey { text: "b"; shifttext: "B" }
                KeyboardKey { text: "n"; shifttext: "N" }
                KeyboardKey { text: "m"; shifttext: "M" }
                KeyboardKey { text: ","; shifttext: "<" }
                KeyboardKey { text: "."; shifttext: ">" }
                KeyboardKey { text: "/"; shifttext: "?" }
                KeyboardKey { text: "Space"; size: 4; key: Qt.Key_Space; value: " " }
            }
        }
    }
    KeyboardHandler { id: handler; objectName: "keyboard" }
    Component.onCompleted: {
        handler.root = keyboard.parent;
    }
}
