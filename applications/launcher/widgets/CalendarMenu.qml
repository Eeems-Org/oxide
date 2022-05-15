import QtQuick 2.10
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import Qt.labs.calendar 1.0
import "../widgets"

Item {
    id: root
    signal closed
    property date currentDate: new Date()
    x: (parent.width / 2) - (calendar.width / 2)
    y: (parent.height / 2) - (calendar.height / 2)
    width: 800
    Popup {
        id: calendar
        visible: root.visible
        closePolicy: Popup.NoAutoClose
        onClosed: {
            parent.closed()
        }
        width: parent.width
        clip: true
        ColumnLayout {
            width: parent.width
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                Label {
                    text: grid.title
                    Layout.columnSpan: 2
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                }
                DayOfWeekRow {
                    locale: grid.locale
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                }
                WeekNumberColumn {
                    month: grid.month
                    year: grid.year
                    locale: grid.locale
                    Layout.fillHeight: true
                }
                MonthGrid {
                    id: grid
                    month: currentDate.getMonth()
                    year: currentDate.getFullYear()
                    locale: Qt.locale("en_US")
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    delegate: Label {
                        background: Rectangle {
                            color: model.today ? "black" : "transparent"
                        }
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        opacity: model.month === grid.month ? 1 : 0
                        text: model.day
                        color: model.today ? "white" : "black"
                        font: grid.font
                    }
                }
            }
            BetterButton{
                text: "Close"
                Layout.fillWidth: true
                onClicked: {
                    controller.breadcrumb("calendar.close", "click", "ui");
                    calendar.close();
                }
            }
        }
    }
}
