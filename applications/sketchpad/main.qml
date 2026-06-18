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
    property int activeCap: Qt.RoundCap
    property list<string> colors: [
        "black", "white", "gray", "silver",
        "maroon", "red", "brown", "orange",
        "olive", "yellow", "lime", "green",
        "aqua", "teal", "cyan", "blue",
        "navy", "purple", "fuchsia", "magenta",
        "pink", "coral", "tomato", "gold",
        "plum", "violet", "indigo", "slategray",
        "dodgerblue", "crimson", "salmon", "turquoise"
    ]

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

                Button {
                    implicitHeight: 40
                    onClicked: penSettings.opened ? penSettings.close() : penSettings.open()
                    contentItem: Row {
                        spacing: 8
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
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
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: window.activeWidth + "px"
                            font.pixelSize: 24
                            color: "black"
                        }
                    }
                    background: null
                }
                Item { Layout.fillWidth: true; }
            }
        }

        Canvas {
            id: canvas
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: toolbar.bottom
            anchors.bottom: parent.bottom
            Component.onCompleted: {
                canvas.setPen(
                    Oxide.createPen(
                        Oxide.brushFromColor(window.activeColor),
                        window.activeWidth,
                        Qt.SolidLine,
                        window.activeCap,
                        Qt.BevelJoin
                    )
                );
            }
        }

        Popup {
            id: penSettings
            y: toolbar.y + toolbar.height - toolbar.border.width
            x: 0
            width: 690
            height: Math.max(colorGrid.implicitHeight, sizeSettings.height) + 34
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

            Column {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.topMargin: 10
                anchors.rightMargin: 24
                anchors.bottomMargin: 24
                spacing: 8

                Row {
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
                                    canvas.setPen(
                                        Oxide.createPen(
                                            Oxide.brushFromColor(modelData),
                                            window.activeWidth,
                                            Qt.SolidLine,
                                            window.activeCap,
                                            Qt.BevelJoin
                                        )
                                    );
                                    window.activeColor = modelData;
                                    console.log("Pen colour: " + modelData);
                                }
                            }
                        }
                    }

                    Item {
                        id: sizeSettings
                        width: 200
                        height: colorGrid.implicitHeight

                        ColumnLayout {
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: 10
                            spacing: 4

                            Item {
                                Layout.alignment: Qt.AlignHCenter
                                width: 100
                                height: 64

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: parent.width
                                    height: window.activeWidth
                                    radius: Math.min(parent.width / 2, window.activeWidth / 2)
                                    color: window.activeColor
                                    border.color: "black"
                                    border.width: window.activeColor === "white" ? 1 : 0
                                }
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: window.activeWidth + "px"
                                font.pixelSize: 24
                                color: "black"
                            }

                            Slider {
                                id: widthSlider
                                Layout.fillWidth: true
                                Layout.preferredHeight: 56
                                from: 1
                                to: 64
                                stepSize: 1
                                value: window.activeWidth

                                background: Rectangle {
                                    x: widthSlider.leftPadding
                                    y: widthSlider.topPadding + widthSlider.availableHeight / 2 - height / 2
                                    width: widthSlider.availableWidth
                                    height: 8
                                    color: "black"
                                }

                                handle: Rectangle {
                                    x: widthSlider.leftPadding + widthSlider.visualPosition * (widthSlider.availableWidth - width)
                                    y: widthSlider.topPadding + widthSlider.availableHeight / 2 - height / 2
                                    width: 30
                                    height: 30
                                    radius: 15
                                    color: "white"
                                    border.color: "black"
                                    border.width: 2
                                }

                                onValueChanged: {
                                    if (window.activeWidth !== value) {
                                        canvas.setPen(
                                            Oxide.createPen(
                                                Oxide.brushFromColor(window.activeColor),
                                                value,
                                                Qt.SolidLine,
                                                window.activeCap,
                                                Qt.BevelJoin
                                            )
                                        );
                                        window.activeWidth = value;
                                    }
                                }
                            }

                            Column {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 4

                                Repeater {
                                    model: [
                                        { label: "Round", value: Qt.RoundCap },
                                        { label: "Square", value: Qt.SquareCap },
                                        { label: "Flat", value: Qt.FlatCap }
                                    ]
                                    delegate: Button {
                                        width: 130
                                        height: 114
                                        onClicked: {
                                            window.activeCap = modelData.value;
                                            canvas.setPen(
                                                Oxide.createPen(
                                                    Oxide.brushFromColor(window.activeColor),
                                                    window.activeWidth,
                                                    Qt.SolidLine,
                                                    window.activeCap,
                                                    Qt.BevelJoin
                                                )
                                            );
                                        }
                                        background: Rectangle {
                                            anchors.fill: parent
                                            anchors.margins: 2
                                            radius: 8
                                            color: "white"
                                            border.color: "black"
                                            border.width: window.activeCap === modelData.value ? 5 : 1
                                        }
                                        contentItem: Item {
                                            anchors.fill: parent
                                            anchors.topMargin: 12
                                            anchors.bottomMargin: 8
                                            anchors.leftMargin: 10
                                            anchors.rightMargin: 10
                                            Column {
                                                anchors.centerIn: parent
                                                spacing: 4
                                                Rectangle {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    width: modelData.value === Qt.FlatCap ? 6 : 32
                                                    height: 32
                                                    radius: modelData.value === Qt.RoundCap ? 16 : modelData.value === Qt.FlatCap ? 0 : 2
                                                    color: "black"
                                                }
                                                Text {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: modelData.label
                                                    font.pixelSize: 24
                                                    color: "black"
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
