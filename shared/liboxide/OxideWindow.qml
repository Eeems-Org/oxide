import QtQuick 2.10
import QtQuick.Window 2.15
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0

ApplicationWindow {
    id: window
    property alias page: page
    property alias focus: page.focus
    property alias leftMenu: leftMenu.children
    property alias rightMenu: rightMenu.children
    property alias centerMenu: centerMenu.children
    property alias stack: stack
    property alias initialItem: stack.initialItem
    property alias headerBackgroundColor: header.color
    property bool landscape: Oxide.landscape
    Component.onCompleted: stack.forceActiveFocus()
    function orientationWidth(){ return landscape ? height : width; }
    function orientationHeight(){ return landscape ? width : height; }
    signal keyPressed(var event)
    signal keyReleased(var event)
    width: Screen.width
    height: Screen.height
    contentOrientation: landscape ? Qt.LandscapeOrientation : Qt.PortraitOrientation
    Page {
        id: page
        focus: true
        title: window.title
        visible: window.visible
        rotation: window.landscape ? 90 : 0
        // Must centerIn and specify width/height to force rotation to actually work
        anchors.centerIn: parent
        width: window.orientationWidth()
        height: window.orientationHeight()
        header: Rectangle {
            id: header
            color: "black"
            height: Math.max(leftMenu.height, centerMenu.height, rightMenu.height)
            RowLayout {
                id: leftMenu
                anchors.left: parent.left
            }
            RowLayout {
                id: rightMenu
                anchors.right: parent.right
            }
            // Comes last to make sure it is overtop of any overflow
            RowLayout {
                id: centerMenu
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.bottom: parent.bottom
            }
        }
        background: Rectangle {
            id: background
            color: window.color
        }
        contentData: [
            StackView {
                id: stack
                anchors.fill: parent
                popEnter: Transition{}
                popExit: Transition{}
                pushEnter: Transition{}
                pushExit: Transition{}
                replaceEnter: Transition{}
                replaceExit: Transition{}
            }
        ]
        Keys.onPressed: (event) => window.keyPressed(event)
        Keys.onReleased: (event) => window.keyReleased(event)
    }
}
