import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import codes.eeems.oxide 3.0
import "qrc:/codes.eeems.oxide"

OxideWindow {
    id: window
    objectName: "window"
    visible: true
    title: qsTr("Sketchpad")

    property string activeColor: "black"
    property int activeWidth: 4

    Shortcut {
        sequences: [StandardKey.Quit, StandardKey.Cancel, "Backspace", "Ctrl+Q", "Ctrl+W"]
        context: Qt.ApplicationShortcut
        autoRepeat: false
        onActivated: Qt.quit()
    }

    initialItem: Item {
        anchors.fill: parent
        Rectangle{
            color: "white"
            anchors.fill: parent
        }

        Rectangle {
            id: toolbar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 56
            color: "white"
            border.color: "black"
            border.width: 3

            RowLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 6

                ColorCircle { penColor: "black"; }
                ColorCircle { penColor: "white"; }
                ColorCircle { penColor: "gray"; }
                ColorCircle { penColor: "red"; }
                ColorCircle { penColor: "blue"; }
                ColorCircle { penColor: "green"; }
                ColorCircle { penColor: "yellow"; }
                ColorCircle { penColor: "orange"; }
                ColorCircle { penColor: "brown"; }

                Item { Layout.fillWidth: true; }

                WidthCircle { penWidth: 2; }
                WidthCircle { penWidth: 4; }
                WidthCircle { penWidth: 8; }
                WidthCircle { penWidth: 16; }
                WidthCircle { penWidth: 24; }
            }
        }

        Canvas {
            id: canvas
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: toolbar.bottom
            anchors.bottom: parent.bottom
            brush: Oxide.brushFromColor("black")
            penWidth: 4
        }
    }

    component ColorCircle: Button {
        id: widget
        property string penColor: "black"
        implicitWidth: 40
        implicitHeight: 40

        onClicked: {
            canvas.brush = Oxide.brushFromColor(penColor);
            window.activeColor = penColor;
            console.log("Pen colour: " + penColor);
        }

        background: Rectangle {
            anchors.centerIn: parent
            width: 36
            height: 36
            radius: 18
            color: "white"
            border.color: "black"
            border.width: window.activeColor === penColor ? 5 : 1

            Rectangle {
                anchors.centerIn: parent
                width: 24
                height: 24
                radius: 24 / 2
                color: widget.penColor
                border.color: "black"
                border.width: widget.penColor == "white" ? 1 : 0
            }
        }
    }

    component WidthCircle: Button {
        id: widget
        property int penWidth: 3
        implicitWidth: 40
        implicitHeight: 40

        onClicked: {
            canvas.penWidth = penWidth;
            window.activeWidth = penWidth;
            console.log("Pen width: " + penWidth);
        }

        background: Rectangle {
            anchors.centerIn: parent
            width: 36
            height: 36
            radius: 18
            color: "white"
            border.color: "black"
            border.width: window.activeWidth === penWidth ? 5 : 1

            Rectangle {
                anchors.centerIn: parent
                width: widget.penWidth
                height: widget.penWidth
                radius: widget.penWidth / 2
                color: "black"
            }
        }
    }
}
