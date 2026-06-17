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
    property var colors: [
        "black", "white", "gray", "silver",
        "maroon", "red", "brown", "orange",
        "olive", "yellow", "lime", "green",
        "aqua", "teal", "cyan", "blue",
        "navy", "purple", "fuchsia", "magenta",
        "pink", "coral", "tomato", "gold",
        "plum", "violet", "indigo", "slategray",
        "dodgerblue", "crimson", "salmon", "turquoise"
    ]
    property var widths: [1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 24, 32, 48, 64]

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
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                anchors.leftMargin: 20
                anchors.rightMargin: 4
                spacing: 12

                ColorSelector { }
                WidthSelector { }
                Item { Layout.fillWidth: true; }
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

        Popup {
            id: mergedPopup
            y: toolbar.y + toolbar.height - toolbar.border.width
            x: 0
            width: 690
            height: Math.max(colorGrid.implicitHeight, sizeGridContainer.height) + 34
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

            background: Item {
                Rectangle {
                    anchors.fill: parent
                    color: "white"
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    color: "black"
                }
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    color: "black"
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 3
                    color: "black"
                }
            }

            Row {
                id: mergedRow
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.topMargin: 10
                anchors.rightMargin: 24
                anchors.bottomMargin: 24
                spacing: 20

                Grid {
                    id: colorGrid
                    columns: 3
                    columnSpacing: 4
                    rowSpacing: 4

                    Repeater {
                        model: window.colors
                        delegate: Button {
                            width: 140
                            height: 114

                            background: Rectangle {
                                anchors.fill: parent
                                anchors.margins: 2
                                radius: 8
                                color: "white"
                                border.color: "black"
                                border.width: window.activeColor === modelData ? 5 : 1
                            }

                            contentItem: Item {
                                anchors.fill: parent
                                anchors.topMargin: 12
                                anchors.bottomMargin: 8
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10

                                Text {
                                    id: colorLabel
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.bottom: parent.bottom
                                    text: modelData
                                    font.pixelSize: 24
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                    width: 110
                                }

                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    y: (colorLabel.y - height) / 2
                                    width: 40
                                    height: 40
                                    radius: 20
                                    color: modelData
                                    border.color: "black"
                                    border.width: modelData === "white" ? 1 : 0
                                }
                            }

                            onClicked: {
                                canvas.brush = Oxide.brushFromColor(modelData);
                                window.activeColor = modelData;
                                console.log("Pen colour: " + modelData);
                            }
                        }
                    }
                }

                Item {
                    id: sizeGridContainer
                    width: sizeGrid.implicitWidth
                    height: colorGrid.implicitHeight

                    Grid {
                        id: sizeGrid
                        anchors.top: parent.top
                        columns: 2
                        columnSpacing: 4
                        rowSpacing: 4

                        Repeater {
                            model: window.widths
                            delegate: Button {
                                width: 100
                                height: 114

                                background: Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    radius: 8
                                    color: "white"
                                    border.color: "black"
                                    border.width: window.activeWidth === modelData ? 5 : 1
                                }

                                contentItem: Item {
                                    anchors.fill: parent
                                    anchors.topMargin: 12
                                    anchors.bottomMargin: 8
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8

                                    Text {
                                        id: sizeLabel
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.bottom: parent.bottom
                                        text: modelData
                                        font.pixelSize: 24
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    Rectangle {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        y: (sizeLabel.y - height) / 2
                                        width: 60
                                        height: modelData
                                        radius: Math.min(30, modelData / 2)
                                        color: "black"
                                    }
                                }

                                onClicked: {
                                    canvas.penWidth = modelData;
                                    window.activeWidth = modelData;
                                    console.log("Pen width: " + modelData);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component ColorSelector: Button {
        id: colorSelector
        implicitWidth: 40
        implicitHeight: 40

        onClicked: mergedPopup.opened ? mergedPopup.close() : mergedPopup.open()

        background: Rectangle {
            anchors.centerIn: parent
            width: 36
            height: 36
            radius: 18
            color: "white"
            border.color: "black"
            border.width: 3

            Rectangle {
                anchors.centerIn: parent
                width: 28
                height: 28
                radius: 14
                color: window.activeColor
                border.color: "black"
                border.width: window.activeColor == "white" ? 1 : 0
            }
        }
    }

    component WidthSelector: Button {
        id: widthSelector
        implicitWidth: 40
        implicitHeight: 40

        onClicked: mergedPopup.opened ? mergedPopup.close() : mergedPopup.open()

        contentItem: Text {
            text: window.activeWidth + "px"
            font.pixelSize: 24
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: "black"
        }

        background: null
    }
}
