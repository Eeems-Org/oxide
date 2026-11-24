import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import codes.eeems.oxide 3.0
import "qrc:/codes.eeems.oxide"

OxideWindow {
    id: window
    visible: true
    title: "Files"
    backgroundColor: "white"
    color: "black"

    Connections {
        target: controller
        function onCurrentPathChanged() { listView.selectedPath = "" }
        function onEntriesChanged() { listView.selectedPath = "" }
    }

    initialItem: Item {
        anchors.fill: parent

        ColumnLayout {
            id: rootLayout
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: keyboard.visible ? keyboard.top : parent.bottom
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 4
                spacing: 4

                OxideButton { text: "Up"; onClicked: controller.goUp() }
                OxideButton { text: "Home"; onClicked: controller.goHome() }
                OxideButton { text: "Refresh"; onClicked: controller.refresh() }
                OxideButton {
                    text: "New Folder"
                    function submit() { controller.createFolder(dialogField.text) }
                    onClicked: {
                        dialogField.text = ""
                        dialogField.visible = true
                        dialogText.text = "New folder"
                        dialog.submit = submit
                        dialog.visible = true
                    }
                }

                Item{ Layout.fillWidth: true}

                OxideButton {
                    text: "Copy"
                    enabled: listView.selectedPath !== ""
                    function submit() { controller.copyPaths([listView.selectedPath], dialogField.text) }
                    onClicked: {
                        dialogField.text = controller.currentPath
                        dialogField.visible = true
                        dialogText.text = "Copy to"
                        dialog.submit = submit
                        dialog.visible = true
                    }
                }
                OxideButton {
                    text: "Move"
                    enabled: listView.selectedPath !== ""
                    function submit() {
                        controller.movePaths([listView.selectedPath], dialogField.text)
                        listView.selectedPath = ""
                    }
                    onClicked: {
                        dialogField.text = controller.currentPath
                        dialogField.visible = true
                        dialogText.text = "Move to"
                        dialog.submit = submit
                        dialog.visible = true
                    }
                }
                OxideButton {
                    text: "Open"
                    enabled: listView.selectedPath !== ""
                    onClicked: controller.openFile(listView.selectedPath)
                }
                OxideButton {
                    text: "Rename"
                    enabled: listView.selectedPath !== ""
                    function submit() { controller.renamePath(listView.selectedPath, dialogField.text) }
                    onClicked: {
                        var info = listView.selectedPath.split("/")
                        dialogField.text = info[info.length - 1]
                        dialogField.visible = true
                        dialogText.text = "Rename"
                        dialog.submit = submit
                        dialog.visible = true
                    }
                }
                OxideButton {
                    text: "Delete"
                    enabled: listView.selectedPath !== ""
                    function submit() {
                        controller.removePaths([listView.selectedPath])
                        listView.selectedPath = ""
                    }
                    onClicked: {
                        dialogField.visible = false
                        dialogText.text = "Delete selected item?"
                        dialog.submit = submit
                        dialog.visible = true
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.margins: 4
                font: window.page.font
                text: controller.currentPath
                elide: Text.ElideMiddle
                textFormat: Text.PlainText
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                OxideButton {
                    text: "▲"
                    visible: listView.currentIndex > 0
                    Layout.fillWidth: true
                    color: window.color
                    backgroundColor: window.backgroundColor
                    borderColor: window.backgroundColor
                    onClicked: {
                        listView.currentIndex = listView.currentIndex - listView.pageSize();
                        if (listView.currentIndex < 0) {
                            listView.currentIndex = 0;
                            listView.positionViewAtBeginning();
                        } else {
                            listView.positionViewAtIndex(listView.currentIndex, ListView.Beginning);
                        }
                    }
                }
                ListView {
                    id: listView
                    objectName: "fileListView"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    currentIndex: 0
                    property string selectedPath: ""
                    model: controller.entries
                    snapMode: ListView.SnapOneItem
                    interactive: false
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollIndicator.vertical: ScrollIndicator {
                        id: scrollbar
                        width: 10
                        contentItem: Rectangle {
                            color: "black"
                            implicitWidth: 6
                            implicitHeight: 100
                        }
                        background: Rectangle {
                            color: "white"
                            border.color: "black"
                            border.width: 1
                            implicitWidth: 6
                            implicitHeight: 100
                        }
                    }
                    function pageHeight() {
                        var item = contentItem.children[0];
                        if (!item) {
                            return 1;
                        }
                        return height / item.height;
                    }
                    function pageSize() { return Math.floor(pageHeight()); }
                    delegate: Rectangle {
                        width: listView.width - scrollbar.width
                        height: 80
                        color: "transparent"
                        border.width: listView.selectedPath === modelData.path ? 1 : 0
                        border.color: "black"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 4
                            spacing: 8

                            Text {
                                text: modelData.isDir ? "[D]" : "[F]"
                                font.bold: true
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text {
                                    text: modelData.name
                                    font: window.page.font
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                    textFormat: Text.PlainText
                                }
                                Text {
                                    text: modelData.isDir ? "Directory" : (modelData.size + " bytes")
                                    font.pixelSize: 16
                                    color: "black"
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                listView.selectedPath = modelData.path
                            }
                            onDoubleClicked: {
                                if (modelData.isDir) {
                                    controller.openPath(modelData.path)
                                } else {
                                    controller.openFile(modelData.path)
                                }
                            }
                        }
                    }
                }
                OxideButton {
                    text: "▼"
                    visible: listView.currentIndex < listView.count - 1
                    Layout.fillWidth: true
                    color: window.color
                    backgroundColor: window.backgroundColor
                    borderColor: window.backgroundColor
                    onClicked: {
                        listView.currentIndex = listView.currentIndex + listView.pageSize();
                        if (listView.currentIndex >= listView.count) {
                            listView.currentIndex = listView.count - 1;
                            listView.positionViewAtEnd();
                        } else {
                            listView.positionViewAtIndex(listView.currentIndex, ListView.Beginning);
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.margins: 4
                visible: !!controller.status.length
                font: window.page.font
                text: controller.status
                color: "#333333"
                wrapMode: Text.Wrap
            }
        }

        Keyboard {
            id: keyboard
            anchors.bottom: parent.bottom
            width: parent.width
            z: 20
            onVisibleChanged: if (visible && listView.currentIndex >= 0) Qt.callLater(listView.positionViewAtIndex, listView.currentIndex, ListView.Contain)
        }
        Rectangle {
            id: dialog
            anchors.fill: parent
            color: "transparent"
            visible: false
            z: 10
            property var submit: null

            onVisibleChanged: if (visible) dialogField.forceActiveFocus()

            MouseArea {
                anchors.fill: parent
                onClicked: { }
            }

            Rectangle {
                width: Math.min(parent.width - 40, 420)
                height: dialogField.visible ? 220 : 140
                anchors.centerIn: parent
                color: "white"
                border.color: "black"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text { id: dialogText; font.bold: true }

                    TextField {
                        id: dialogField
                        Layout.fillWidth: true
                        visible: true
                        text: ""
                    }

                    Text {
                        visible: !dialogField.visible
                        text: listView.selectedPath
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        OxideButton {
                            text: "OK"
                            Layout.fillWidth: true
                            onClicked: {
                                if (dialog.submit) {
                                    dialog.submit()
                                }
                                dialog.visible = false
                            }
                        }
                        OxideButton {
                            text: "Cancel"
                            Layout.fillWidth: true
                            onClicked: dialog.visible = false
                        }
                    }
                }
            }
        }
    }
}
