import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import "qrc:/codes.eeems.oxide"

Window{
    property alias text: text.text
    property alias image: image.source
    property alias notificationVisible: notification.visible
    property var actions: ({})
    property bool landscape: Oxide.landscape
    signal clicked(string action)
    onActionsChanged: actionRow.recalculate()

    id: window
    objectName: "window"
    flags: Qt.FramelessWindowHint
    visible: true
    x: landscape ? 0 : Screen.width - width - background.border.width
    y: Screen.height - height - background.border.width
    width: 500
    height: 500
    function orientationWidth(){ return landscape ? height : width; }
    function orientationHeight(){ return landscape ? width : height; }
    contentOrientation: landscape ? Qt.LandscapeOrientation : Qt.PortraitOrientation
    color: "transparent"

    Item{
        id: notification
        // Must centerIn and specify width/height to force rotation to actually work
        width: window.orientationWidth()
        height: window.orientationHeight()
        anchors.centerIn: parent
        rotation: landscape ? 90 : 0
        clip: true

        Rectangle{
            id: background
            color: "white"
            border.color: "black"
            border.width: 2
            radius: 10
            anchors.fill: mainLayout
        }
        ColumnLayout{
            id: mainLayout
            width: parent.width
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: 0
            MouseArea{
                anchors.fill: parent
                onClicked: window.clicked("")
            }
            RowLayout{
                id: rowLayout
                Layout.fillWidth: true
                Image{
                    id: image
                    visible: status === Image.Ready
                    Layout.preferredHeight: 50
                    Layout.preferredWidth: 50
                    Layout.alignment: Qt.AlignCenter
                    Layout.leftMargin: 10
                    Layout.topMargin: 10
                    Layout.bottomMargin: 10
                }
                Label{
                    id: text
                    visible: this.text.length
                    color: "black"
                    font.pixelSize: 32
                    wrapMode: Text.WordWrap
                    Layout.margins: 10
                    Layout.alignment: Qt.AlignCenter
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
            TextMetrics{
                id: textMetrics
                font.pixelSize: 24
            }
            Flow{
                id: actionRow
                Layout.fillWidth: true
                Layout.margins: 5
                spacing: 5
                visible: Object.keys(actions).length > 0
                property var buttonWidths: ({})
                onWidthChanged: recalculate()
                Component.onCompleted: recalculate()
                function recalculate(){
                    var keys = Object.keys(actions)
                    var available = actionRow.width
                    var ideal = []
                    for(var i = 0; i < keys.length; i++){
                        textMetrics.text = actions[keys[i]]
                        ideal.push(textMetrics.width + 20)
                    }
                    var rows = []
                    var row = []
                    var rowWidth = 0
                    for(var j = 0; j < ideal.length; j++){
                        var need = row.length === 0 ? ideal[j] : rowWidth + actionRow.spacing + ideal[j]
                        if(need <= available){
                            row.push(j)
                            rowWidth = need
                            continue;
                        }
                        if(row.length > 0){
                            rows.push(row)
                        }
                        row = [j]
                        rowWidth = ideal[j]
                    }
                    if(row.length > 0){
                        rows.push(row)
                    }
                    var widths = {}
                    for(var r = 0; r < rows.length; r++){
                        var items = rows[r]
                        var totalIdeal = 0
                        for(var k = 0; k < items.length; k++){
                            totalIdeal += ideal[items[k]]
                        }
                        var totalSpacing = (items.length - 1) * actionRow.spacing
                        var excess = available - totalIdeal - totalSpacing
                        var perItem = excess / items.length
                        for(var k2 = 0; k2 < items.length; k2++){
                            widths[items[k2]] = ideal[items[k2]] + perItem
                        }
                    }
                    buttonWidths = widths
                }
                Repeater{
                    model: Object.keys(actions)
                    delegate: OxideButton{
                        width: actionRow.buttonWidths[index] || actionRow.width
                        height: 40
                        font.pixelSize: 24
                        text: actions[modelData]
                        onClicked: window.clicked(modelData)
                    }
                }
            }
        }
    }
}
