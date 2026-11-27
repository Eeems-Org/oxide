import QtQuick 2.9
import QtQuick.Window 2.15
import QtQuick.Layouts 1.0
import codes.eeems.oxide 3.0
import "."

Item {
    id: keyboard
    clip: true
    property var activeFocusItem: Window.window ? Window.window.activeFocusItem : null
function isTextInput(item) {
        if (!item) {
            return false
        }
        if (item.hasOwnProperty("inputMethodVisible") && item.inputMethodVisible) {
            return true
        }
        if (!item.hasOwnProperty("text") || !item.hasOwnProperty("readOnly")) {
            if (!item.hasOwnProperty("inputMethodHints") &&
                !item.hasOwnProperty("inputMethodComposing") &&
                !item.hasOwnProperty("contentType") &&
                !item.hasOwnProperty("acceptRichText") &&
                !item.hasOwnProperty("textDocument") &&
                !item.hasOwnProperty("cursorPosition") &&
                !item.hasOwnProperty("selectionStart") &&
                !item.hasOwnProperty("selectionEnd") &&
                !item.hasOwnProperty("validator") &&
                !item.hasOwnProperty("inputMask")) {
                return false
            }
        }
        if (item.hasOwnProperty("readOnly") && item.readOnly) {
            return false
        }
        if (item.hasOwnProperty("enabled") && !item.enabled) {
            return false
        }
        if (item.hasOwnProperty("visible") && !item.visible) {
            return false
        }
        if (item.hasOwnProperty("activeFocus") && !item.activeFocus) {
            return false
        }
        return true
    }
    visible: !handler.keyboardAttached && isTextInput(activeFocusItem)
    property bool hasCaps: false
    property bool hasShift: false
    property bool hasAlt: false
    property bool hasCtrl: false
    property bool hasMeta: false
    property bool hasFn: false
    property int columns: 15
    property int rows: 6
    property int basesize: qwerty.width / columns
    width: parent.width
    height: basesize * rows
    implicitHeight: basesize * rows
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
                KeyboardKey {
                    size: 1
                    text: "Esc"
                    key: Qt.Key_Escape
                    keyboard: keyboard
                }
                Spacer {
                    Layout.fillHeight: true
                    keyboard: keyboard
                    size: 1
                }
                KeyboardKey {
                    size: 1
                    text: "F1"
                    key: Qt.Key_F1
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "F2"
                    key: Qt.Key_F2
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "F3"
                    key: Qt.Key_F3
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "F4"
                    key: Qt.Key_F4
                    keyboard: keyboard
                }
                Spacer {
                    Layout.fillHeight: true
                    keyboard: keyboard
                    size: 0.5
                }
                KeyboardKey {
                    size: 1
                    text: "F5"
                    key: Qt.Key_F5
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "F6"
                    key: Qt.Key_F6
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "F7"
                    key: Qt.Key_F7
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "F8"
                    key: Qt.Key_F8
                    keyboard: keyboard
                }
                Spacer {
                    Layout.fillHeight: true
                    keyboard: keyboard
                    size: 0.5
                }
                KeyboardKey {
                    size: 1
                    text: "F9"
                    key: Qt.Key_F9
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "F10"
                    key: Qt.Key_F10
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: keyboard.hasFn ? "Menu" : "F11"
                    key: keyboard.hasFn ? Qt.Key_Menu : Qt.Key_F11
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: keyboard.hasFn ? "Help" : "F12"
                    key: keyboard.hasFn ? Qt.Key_Help : Qt.Key_F12
                    keyboard: keyboard
                }
            }
            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                KeyboardKey {
                    size: 1
                    text: "`"
                    shifttext: "~"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "1"
                    shifttext: "!"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "2"
                    shifttext: "@"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "3"
                    shifttext: "#"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "4"
                    shifttext: "$"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "5"
                    shifttext: "%"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "6"
                    shifttext: "^"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "7"
                    shifttext: "&"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "8"
                    shifttext: "*"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "9"
                    shifttext: "("
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "0"
                    shifttext: ")"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "-"
                    shifttext: "_"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "="
                    shifttext: "+"
                    keyboard: keyboard
                }
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1
                    Layout.topMargin: -1
                    text: keyboard.hasFn ? "Delete" : "Backspace"
                    size: 2
                    key: keyboard.hasFn ? Qt.Key_Delete : Qt.Key_Backspace
                    keyboard: keyboard
                }
            }
            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                KeyboardKey {
                    Layout.fillHeight: true
                    Layout.preferredWidth: keyboard.basesize * size
                    Layout.leftMargin: -1
                    Layout.topMargin: -1
                    text: "Tab"
                    value: "\t"
                    size: 1.5
                    key: Qt.Key_Tab
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "q"
                    shifttext: "Q"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "w"
                    shifttext: "W"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "e"
                    shifttext: "E"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "r"
                    shifttext: "R"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "t"
                    shifttext: "T"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "y"
                    shifttext: "Y"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "u"
                    shifttext: "U"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "i"
                    shifttext: "I"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "o"
                    shifttext: "O"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "p"
                    shifttext: "P"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "["
                    shifttext: "{"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "]"
                    shifttext: "}"
                    keyboard: keyboard
                }
                KeyboardKey {
                    text: "\\"
                    shifttext: "|"
                    size: 1.5
                    keyboard: keyboard
                }
            }
            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                KeyboardKey {
                    text: "Caps Lock"
                    key: Qt.Key_CapsLock
                    size: 1.75
                    checkable: true
                    keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasCaps
                    onClicked: {
                        keyboard.hasCaps = !keyboard.hasCaps
                        doClick()
                    }
                }
                KeyboardKey {
                    size: 1
                    text: "a"
                    shifttext: "A"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "s"
                    shifttext: "S"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "d"
                    shifttext: "D"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "f"
                    shifttext: "F"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "g"
                    shifttext: "G"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "h"
                    shifttext: "H"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "j"
                    shifttext: "J"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "k"
                    shifttext: "K"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "l"
                    shifttext: "L"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: ";"
                    shifttext: ":"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "'"
                    shifttext: "\""
                    keyboard: keyboard
                }
                KeyboardKey {
                    text: "Enter"
                    value: "\n"
                    size: 2.25
                    key: Qt.Key_Enter
                    keyboard: keyboard
                    Layout.columnSpan: size * 2
                }
                Spacer {
                    Layout.fillHeight: true
                    keyboard: keyboard
                    size: 1
                }
            }
            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                KeyboardKey {
                    font.pixelSize: 32
                    text: "⇧"
                    key: Qt.Key_Shift
                    size: 2
                    checkable: true
                    keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasShift
                    onClicked: {
                        keyboard.hasShift = !keyboard.hasShift
                        doClick()
                    }
                }
                KeyboardKey {
                    size: 1
                    text: "z"
                    shifttext: "Z"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "x"
                    shifttext: "X"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "c"
                    shifttext: "C"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "v"
                    shifttext: "V"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "b"
                    shifttext: "B"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "n"
                    shifttext: "N"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "m"
                    shifttext: "M"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: ","
                    shifttext: "<"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "."
                    shifttext: ">"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    text: "/"
                    shifttext: "?"
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    font.pixelSize: 20
                    text: keyboard.hasFn ? "Home" : "PgUp"
                    key: keyboard.hasFn ? Qt.Key_Home : Qt.Key_PageUp
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    font.pixelSize: 32
                    text: "↑"
                    key: Qt.Key_Up
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    font.pixelSize: 20
                    text: keyboard.hasFn ? "End" : "PgDown"
                    key: keyboard.hasFn ? Qt.Key_End : Qt.Key_PageDown
                    keyboard: keyboard
                }
            }
            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                KeyboardKey {
                    size: 1.25
                    text: "Ctrl"
                    key: Qt.Key_Control
                    checkable: true
                    keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasCtrl
                    onClicked: {
                        keyboard.hasCtrl = !keyboard.hasCtrl
                        doClick()
                    }
                }
                KeyboardKey {
                    size: 1.25
                    text: "❖"
                    key: Qt.Key_Meta
                    checkable: true
                    keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasMeta
                    onClicked: {
                        keyboard.hasMeta = !keyboard.hasMeta
                        doClick()
                    }
                }
                KeyboardKey {
                    clip: false
                    size: 1.25
                    text: "Alt"
                    key: Qt.Key_Alt
                    checkable: true
                    keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasAlt
                    onClicked: {
                        keyboard.hasAlt = !keyboard.hasAlt
                        doClick()
                    }
                }
                Spacer {
                    Layout.fillHeight: true
                    keyboard: keyboard
                    size: 0.25
                }
                KeyboardKey {
                    text: ""
                    key: Qt.Key_Space
                    value: " "
                    size: 6
                    keyboard: keyboard
                }
                Spacer {
                    Layout.fillHeight: true
                    keyboard: keyboard
                    size: 0.25
                }
                KeyboardKey {
                    clip: false
                    size: 1.25
                    text: "Fn"
                    key: Qt.Key_Alt
                    checkable: true
                    keyboard: keyboard
                    borderWidth: this.checked ? 3 : 1
                    checked: keyboard.hasFn
                    onClicked: {
                        keyboard.hasFn = !keyboard.hasFn
                        doClick()
                    }
                }
                Spacer {
                    Layout.fillHeight: true
                    keyboard: keyboard
                    size: 0.5
                }
                KeyboardKey {
                    size: 1
                    font.pixelSize: 32
                    text: "←"
                    key: Qt.Key_Left
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    font.pixelSize: 32
                    text: "↓"
                    key: Qt.Key_Down
                    keyboard: keyboard
                }
                KeyboardKey {
                    size: 1
                    font.pixelSize: 32
                    text: "→"
                    key: Qt.Key_Right
                    keyboard: keyboard
                }
            }
        }
    }
    KeyboardHandler {
        id: handler
        objectName: "keyboard"
    }
    Component.onCompleted: {
        handler.root = keyboard.parent
    }
}
