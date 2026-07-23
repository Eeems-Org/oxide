import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import codes.eeems.oxide 3.0
import "qrc:/codes.eeems.oxide"

OxideWindow {
    id: window
    reserveSystemSpace: true
    objectName: "window"
    visible: true
    title: qsTr("Sketchpad")

    property string activeColor: "black"
    property int activeWidth: 4
    property int activeCap: Qt.RoundCap
    property int activeBrushStyle: Qt.SolidPattern

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
                            radius: window.activeCap === Qt.RoundCap ? 18 : 4
                            color: "white"
                            border.color: "black"
                            border.width: 3
                            Canvas {
                                id: buttonPreviewCanvas
                                anchors.centerIn: parent
                                width: 28
                                height: 28
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);
                                    var r = window.activeCap === Qt.RoundCap ? 14 : 3;
                                    ctx.beginPath();
                                    ctx.moveTo(r, 0);
                                    ctx.arcTo(width, 0, width, height, r);
                                    ctx.arcTo(width, height, 0, height, r);
                                    ctx.arcTo(0, height, 0, 0, r);
                                    ctx.arcTo(0, 0, width, 0, r);
                                    ctx.closePath();
                                    ctx.fillStyle = ctx.createPattern(window.activeColor, window.activeBrushStyle);
                                    ctx.fill();
                                    if (window.activeColor === "white") {
                                        ctx.strokeStyle = "black";
                                        ctx.lineWidth = 1;
                                        ctx.stroke();
                                    }
                                }
                                Connections {
                                    target: window
                                    function onActiveBrushStyleChanged() {
                                        buttonPreviewCanvas.requestPaint()
                                    }
                                    function onActiveCapChanged() {
                                        buttonPreviewCanvas.requestPaint()
                                    }
                                    function onActiveColorChanged() {
                                        buttonPreviewCanvas.requestPaint()
                                    }
                                }
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

        OxideCanvas {
            id: canvas
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: toolbar.bottom
            anchors.bottom: parent.bottom
            onDrawStart: penSettings.close()
            Component.onCompleted: {
                canvas.setPen(
                    Oxide.createPen(
                        Oxide.brushFromColor(window.activeColor, window.activeBrushStyle),
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
            width: 755
            height: colorGrid.implicitHeight + 34
            closePolicy: Popup.CloseOnEscape

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
                        spacing: 4

                        Repeater {
                            model: [
                                "black", "white", "gray", "silver",
                                "maroon", "red", "brown", "orange",
                                "olive", "yellow", "lime", "green",
                                "aqua", "teal", "cyan", "blue",
                                "navy", "purple", "fuchsia", "magenta",
                                "pink", "coral", "tomato", "gold",
                                "plum", "violet", "indigo", "slategray",
                                "dodgerblue", "crimson", "salmon", "turquoise"
                            ]
                            delegate: Button {
                                width: 140
                                height: 120

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
                                            Oxide.brushFromColor(modelData, window.activeBrushStyle),
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

                    ColumnLayout {
                        spacing: 4

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120 * 2 + parent.spacing

                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 120 + 2
                                Canvas {
                                    id: previewCanvas
                                    anchors.centerIn: parent
                                    width: parent.width
                                    height: window.activeWidth
                                    onPaint: {
                                         var ctx = getContext("2d");
                                        ctx.clearRect(0, 0, width, height);
                                        ctx.strokeStyle = ctx.createPattern(window.activeColor, window.activeBrushStyle);
                                        ctx.lineWidth = height;
                                        ctx.lineCap = window.activeCap === Qt.RoundCap ? "round" : "square";
                                        ctx.beginPath();
                                        ctx.moveTo(height / 2, height / 2);
                                        ctx.lineTo(width - height / 2, height / 2);
                                        ctx.stroke();
                                        if(window.activeColor == "white"){
                                            ctx.strokeStyle = "black";
                                            ctx.lineWidth = 1;
                                            ctx.beginPath();
                                            ctx.moveTo(height / 2, height / 2);
                                            ctx.lineTo(width - height / 2, height / 2);
                                            ctx.stroke();
                                        }
                                    }
                                    Connections {
                                        target: window
                                        function onActiveBrushStyleChanged() {
                                            previewCanvas.requestPaint()
                                        }
                                        function onActiveCapChanged() {
                                            previewCanvas.requestPaint()
                                        }
                                        function onActiveColorChanged() {
                                            previewCanvas.requestPaint()
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 56
                                Slider {
                                    id: widthSlider
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
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
                                    onMoved: {
                                        if (window.activeWidth !== value) {
                                            canvas.setPen(
                                                Oxide.createPen(
                                                    Oxide.brushFromColor(window.activeColor, window.activeBrushStyle),
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
                                Text {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: window.activeWidth + "px"
                                    font.pixelSize: 24
                                    color: "black"
                                }
                            }
                        }

                        Row {
                            spacing: 4

                            Repeater {
                                model: [
                                    { label: "Round", value: Qt.RoundCap },
                                    { label: "Square", value: Qt.SquareCap }
                                ]
                                delegate: Button {
                                    width: 120
                                    height: 120
                                    onClicked: {
                                        window.activeCap = modelData.value;
                                        canvas.setPen(
                                            Oxide.createPen(
                                                Oxide.brushFromColor(window.activeColor, window.activeBrushStyle),
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

                        Item {
                            height: 120
                            width: parent.width
                        }

                        Grid {
                            columns: 2
                            columnSpacing: 4
                            rowSpacing: 4

                            Repeater {
                                model: [
                                    Qt.SolidPattern ,
                                    Qt.Dense1Pattern ,
                                    Qt.Dense2Pattern ,
                                    Qt.Dense3Pattern ,
                                    Qt.Dense4Pattern ,
                                    Qt.Dense5Pattern ,
                                    Qt.Dense6Pattern ,
                                    Qt.Dense7Pattern ,
                                    Qt.HorPattern ,
                                    Qt.VerPattern ,
                                    Qt.CrossPattern ,
                                    Qt.BDiagPattern ,
                                    Qt.FDiagPattern ,
                                    Qt.DiagCrossPattern,
                                ]
                                delegate: Button {
                                    width: 120
                                    height: 120
                                    onClicked: {
                                        window.activeBrushStyle = modelData;
                                        canvas.setPen(
                                            Oxide.createPen(
                                                Oxide.brushFromColor(window.activeColor, window.activeBrushStyle),
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
                                        border.width: window.activeBrushStyle === modelData ? 5 : 1
                                    }
                                    contentItem: Item {
                                        anchors.fill: parent
                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 4
                                            Canvas {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                width: 100
                                                height: 100
                                                onPaint: {
                                                    var ctx = getContext("2d");
                                                    ctx.fillStyle = "white";
                                                    ctx.fillRect(0, 0, width, height);
                                                    ctx.strokeStyle = "black";
                                                    ctx.lineWidth = 1;
                                                    ctx.strokeRect(0.5, 0.5, width-1, height-1);
                                                    ctx.fillStyle = ctx.createPattern("black", modelData);
                                                    ctx.fillRect(1, 1, width-2, height-2);
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
