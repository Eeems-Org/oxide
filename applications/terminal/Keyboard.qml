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
            }
            Row {
                spacing: parent.spacing
                anchors.horizontalCenter: parent.horizontalCenter
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
                KeyboardKey { text: "Space"; size: 4; key: Qt.Key_Space; value: " "; keyboard: keyboard }
            }
        }
    }
    KeyboardHandler { id: handler; objectName: "keyboard" }
    Component.onCompleted: {
        handler.root = keyboard.parent;
    }
}
