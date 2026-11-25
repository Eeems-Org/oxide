import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/codes.eeems.oxide"

OxideWindow {
    id: window
    visible: true
    title: fileInfo.fileName
    headerBackgroundColor: "white"
    backgroundColor: "white"
    color: "black"
    reserveSystemSpace: true

    FileInfo {
        id: fileInfo
        filePath: filePath
    }

    leftMenu: [
        Label {
            text: "\u2190"
            color: window.color
            padding: 10
            MouseArea { anchors.fill: parent; onClicked: Qt.quit() }
        }
    ]
    centerMenu: [
        Label { color: window.color; text: window.title; elide: Text.ElideRight; Layout.fillWidth: true }
    ]

    initialItem: ColumnLayout { anchors.fill: parent
        OxideButton {
            text: "\u25b2"
            backgroundColor: window.backgroundColor
            color: window.color
            visible: listView.currentIndex > 0
            Layout.fillWidth: true
            onClicked: {
                listView.currentIndex -= listView.pageSize();
                if (listView.currentIndex < 0) { listView.currentIndex = 0; listView.positionViewAtBeginning(); }
                else { listView.positionViewAtIndex(listView.currentIndex, ListView.Beginning); }
            }
        }
        ListView {
            id: listView
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            snapMode: ListView.SnapOneItem
            interactive: false
            boundsBehavior: Flickable.StopAtBounds
            model: lineModel
            delegate: Text {
                width: listView.width
                height: 32
                text: model.line
                font.family: "monospace"
                font: window.page.font
                verticalAlignment: Text.AlignVCenter
                padding: 4
            }
            ScrollIndicator.vertical: ScrollIndicator {
                contentItem: Rectangle { color: window.color; implicitWidth: 6; radius: width / 2 }
                background: Rectangle { color: window.backgroundColor; border.color: window.color; border.width: 1; implicitWidth: 6; radius: width / 2 }
            }
            function pageSize() { return Math.floor(height / 32); }

            onContentYChanged: {
                var firstVisible = Math.floor(contentY / 32);
                var lastVisible = firstVisible + pageSize();
                lineModel.ensureLoaded(firstVisible, lastVisible);
            }
        }
        OxideButton {
            text: "\u25bc"
            backgroundColor: window.backgroundColor
            color: window.color
            visible: listView.currentIndex < listView.count - 1
            Layout.fillWidth: true
            onClicked: {
                listView.currentIndex += listView.pageSize();
                if (listView.currentIndex >= listView.count) { listView.currentIndex = listView.count - 1; listView.positionViewAtEnd(); }
                else { listView.positionViewAtIndex(listView.currentIndex, ListView.Beginning); }
            }
        }
    }
}
