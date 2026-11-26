import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/codes.eeems.oxide"

OxideWindow {
    id: window
    visible: true
    title: "Open " + filePath.split('/').pop() + " with"
    headerBackgroundColor: "white"
    backgroundColor: "white"
    color: "black"
    reserveSystemSpace: true

    leftMenu: [
        OxideButton {
            text: "⬅️"
            onClicked: Qt.quit()
            color: window.color
            backgroundColor: window.backgroundColor
            borderColor: window.backgroundColor
        }
    ]
    centerMenu: [
        Label { color: window.color; text: window.title; elide: Text.ElideRight; Layout.fillWidth: true }
    ]
    rightMenu: []

    initialItem: ColumnLayout {
        anchors.fill: parent
        spacing: 0

        OxideButton {
            text: "▲"
            visible: listView.currentIndex > 0
            Layout.fillWidth: true
            color: window.color
            backgroundColor: window.backgroundColor
            borderColor: window.backgroundColor
            onClicked: {
                listView.currentIndex = listView.currentIndex - listView.pageSize()
                if (listView.currentIndex < 0) {
                    listView.currentIndex = 0
                    listView.positionViewAtBeginning()
                } else {
                    listView.positionViewAtIndex(listView.currentIndex, ListView.Beginning)
                }
            }
        }
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            property string selectedPath: ""
            currentIndex: 0
            model: candidates
            snapMode: ListView.SnapOneItem
            interactive: false
            boundsBehavior: Flickable.StopAtBounds

            function pageHeight() {
                var item = contentItem.children[0];
                if (!item) {
                    return 1;
                }
                return height / item.height;
            }
            function pageSize() { return Math.floor(pageHeight()); }

            ScrollIndicator.vertical: ScrollIndicator {
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

            delegate: Rectangle {
                width: listView.width
                height: 80
                color: "transparent"
                border.width: listView.selectedPath === modelData.path ? 1 : 0
                border.color: "black"

                Text {
                    text: modelData.displayName
                    font: window.page.font
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    elide: Text.ElideRight
                    anchors.rightMargin: 20
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        listView.selectedPath = modelData.path
                    }
                    onDoubleClicked: {
                        listView.selectedPath = modelData.path
                        chooser.launch(modelData.path, filePath)
                        Qt.quit()
                    }
                }
            }
        }
        OxideButton {
            text: "▼"
            visible: listView.currentIndex < listView.count - listView.pageSize()
            Layout.fillWidth: true
            color: window.color
            backgroundColor: window.backgroundColor
            borderColor: window.backgroundColor
            onClicked: {
                listView.currentIndex = listView.currentIndex + listView.pageSize()
                if (listView.currentIndex >= listView.count) {
                    listView.currentIndex = listView.count - 1
                }
                listView.positionViewAtIndex(listView.currentIndex, ListView.Beginning)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 3
            Layout.rightMargin: 3
            Layout.bottomMargin: 3
            uniformCellSizes: true
            spacing: 0

            OxideButton {
                text: "Always open " + mimeType
                enabled: listView.selectedPath !== ""
                Layout.fillWidth: true
                onClicked: {
                    chooser.setDefault(listView.selectedPath, mimeType)
                    chooser.launch(listView.selectedPath, filePath)
                    Qt.quit()
                }
                color: window.color
                backgroundColor: window.backgroundColor
            }
            OxideButton {
                text: "Open once"
                enabled: listView.selectedPath !== ""
                Layout.fillWidth: true
                onClicked: {
                    chooser.launch(listView.selectedPath, filePath)
                    Qt.quit()
                }
                color: window.color
                backgroundColor: window.backgroundColor
            }
        }
    }
}
