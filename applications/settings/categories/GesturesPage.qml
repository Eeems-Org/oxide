import QtQuick.Window 2.2
import QtQuick 2.10
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "qrc:/codes.eeems.oxide"
import "../widgets"

Page {
    id: root
    objectName: "gestures"
    title: "Gestures"
    background: Rectangle { color: "white" }
    StackView.onActivated: gesturesController.activate()
    StackView.onDeactivating: gesturesController.deactivate()
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Right Swipe"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Length"
                font.pixelSize: 28
                Layout.preferredWidth: 100
            }
            Slider {
                id: rightSlider
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                from: 10
                to: Screen.width * 0.5
                stepSize: 10
                value: gesturesController.swipeLengthRight
                onMoved: gesturesController.swipeLengthRight = value
                background: Rectangle {
                    x: rightSlider.leftPadding
                    y: rightSlider.topPadding + rightSlider.availableHeight / 2 - height / 2
                    width: rightSlider.availableWidth
                    height: 8
                    color: "black"
                }
                handle: Rectangle {
                    x: rightSlider.leftPadding + rightSlider.visualPosition * (rightSlider.availableWidth - width)
                    y: rightSlider.topPadding + rightSlider.availableHeight / 2 - height / 2
                    width: 50
                    height: 50
                    radius: 25
                    color: "white"
                    border.color: "black"
                    border.width: 2
                }
            }
            Label {
                text: Math.round(rightSlider.value / Screen.width * 100) + "%"
                font.pixelSize: 28
                Layout.preferredWidth: 60
                horizontalAlignment: Text.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Left Swipe"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Length"
                font.pixelSize: 28
                Layout.preferredWidth: 100
            }
            Slider {
                id: leftSlider
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                from: 10
                to: Screen.width * 0.5
                stepSize: 10
                value: gesturesController.swipeLengthLeft
                onMoved: gesturesController.swipeLengthLeft = value
                background: Rectangle {
                    x: leftSlider.leftPadding
                    y: leftSlider.topPadding + leftSlider.availableHeight / 2 - height / 2
                    width: leftSlider.availableWidth
                    height: 8
                    color: "black"
                }
                handle: Rectangle {
                    x: leftSlider.leftPadding + leftSlider.visualPosition * (leftSlider.availableWidth - width)
                    y: leftSlider.topPadding + leftSlider.availableHeight / 2 - height / 2
                    width: 50
                    height: 50
                    radius: 25
                    color: "white"
                    border.color: "black"
                    border.width: 2
                }
            }
            Label {
                text: Math.round(leftSlider.value / Screen.width * 100) + "%"
                font.pixelSize: 28
                Layout.preferredWidth: 60
                horizontalAlignment: Text.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Up Swipe"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Length"
                font.pixelSize: 28
                Layout.preferredWidth: 100
            }
            Slider {
                id: upSlider
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                from: 10
                to: Screen.height * 0.5
                stepSize: 10
                value: gesturesController.swipeLengthUp
                onMoved: gesturesController.swipeLengthUp = value
                background: Rectangle {
                    x: upSlider.leftPadding
                    y: upSlider.topPadding + upSlider.availableHeight / 2 - height / 2
                    width: upSlider.availableWidth
                    height: 8
                    color: "black"
                }
                handle: Rectangle {
                    x: upSlider.leftPadding + upSlider.visualPosition * (upSlider.availableWidth - width)
                    y: upSlider.topPadding + upSlider.availableHeight / 2 - height / 2
                    width: 50
                    height: 50
                    radius: 25
                    color: "white"
                    border.color: "black"
                    border.width: 2
                }
            }
            Label {
                text: Math.round(upSlider.value / Screen.height * 100) + "%"
                font.pixelSize: 28
                Layout.preferredWidth: 60
                horizontalAlignment: Text.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Down Swipe"
                font.pixelSize: 32
                Layout.fillWidth: true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Length"
                font.pixelSize: 28
                Layout.preferredWidth: 100
            }
            Slider {
                id: downSlider
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                from: 10
                to: Screen.height * 0.5
                stepSize: 10
                value: gesturesController.swipeLengthDown
                onMoved: gesturesController.swipeLengthDown = value
                background: Rectangle {
                    x: downSlider.leftPadding
                    y: downSlider.topPadding + downSlider.availableHeight / 2 - height / 2
                    width: downSlider.availableWidth
                    height: 8
                    color: "black"
                }
                handle: Rectangle {
                    x: downSlider.leftPadding + downSlider.visualPosition * (downSlider.availableWidth - width)
                    y: downSlider.topPadding + downSlider.availableHeight / 2 - height / 2
                    width: 50
                    height: 50
                    radius: 25
                    color: "white"
                    border.color: "black"
                    border.width: 2
                }
            }
            Label {
                text: Math.round(downSlider.value / Screen.height * 100) + "%"
                font.pixelSize: 28
                Layout.preferredWidth: 60
                horizontalAlignment: Text.AlignRight
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }
}
