import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import codes.eeems.oxide 3.0
import "qrc:/codes.eeems.oxide"

OxideWindow{
    id: window
    objectName: "window"
    visible: true
    focus: true
    Shortcut{
        sequences: [StandardKey.Quit, StandardKey.Cancel, "Backspace", "Ctrl+Q", "Ctrl+W"]
        context: Qt.ApplicationShortcut
        autoRepeat: false
        onActivated: Qt.quit()
    }
    leftMenu: [
        Button{
            text: "Click to Quit"
            onClicked: Qt.quit()
        }
    ]
    centerMenu: [
        Label{
            color: "white"
            text: "Hello World!"
        }

    ]
    rightMenu: [
        Label{
            color: "white"
            text: "Tap to Quit"
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            TapHandler {
                onTapped: Qt.quit()
            }
        }
    ]
    initialItem: Item{
        anchors.fill: parent
        focus: true
        Rectangle{
            color: "white"
            anchors.fill: parent
        }
        component MyButton: Button{
            id: control
            contentItem: Text {
                text: control.text
                font: control.font
                color: control.enabled ? "black" : "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            background: Rectangle{
                color: control.enabled ? "white" : "black"
                border.color: "black"
                border.width: 1
                radius: 2
            }
        }
        RowLayout{
            id: buttons
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            MyButton{
                text: "Black Pen"
                enabled: canvas.brush !== Oxide.brushFromColor("black")
                onClicked: canvas.brush = Oxide.brushFromColor("black")
            }
            MyButton{
                text: "White Pen"
                enabled: canvas.brush !== Oxide.brushFromColor("white")
                onClicked: canvas.brush = Oxide.brushFromColor("white")
            }
            Item{
                Layout.fillWidth: true
            }
            MyButton{
                text: "3px"
                enabled: canvas.penWidth !== 3
                onClicked: canvas.penWidth = 3
            }
            MyButton{
                text: "6px"
                enabled: canvas.penWidth !== 6
                onClicked: canvas.penWidth = 6
            }
            MyButton{
                text: "12px"
                enabled: canvas.penWidth !== 12
                onClicked: canvas.penWidth = 12
            }
            MyButton{
                text: "24px"
                enabled: canvas.penWidth !== 24
                onClicked: canvas.penWidth = 24
            }
        }
        Label{
            id: message
            text: "Ctrl-Q, Ctrl-W, Backspace, or Escape to quit"
            color: "black"
            anchors.centerIn: parent
        }
        Canvas{
            id: canvas
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: buttons.bottom
            anchors.bottom: parent.bottom
            onDrawStart: message.visible = false
            onDrawDone: message.visible = true
        }
    }
}
