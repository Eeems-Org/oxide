import QtQuick 2.10
import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.0
import "./widgets"
import "page.js" as JS

ApplicationWindow {
    id: window
    objectName: "window"
    visible: stateController.state !== "loading"
    width: screenGeometry.width
    height: screenGeometry.height
    title: page.url
    property string home: "https://eink.link"
    FontLoader { id: iconFont; source: "/font/icomoon.ttf" }
    Component.onCompleted: {
        controller.startup();
        page.load(window.home);
    }
    header: Rectangle {
        color: "black"
        height: menu.height
        RowLayout {
            id: menu
            anchors.left: parent.left
            anchors.right: parent.right
            Label {
                text: "⬅️"
                color: enabled ? "white" : "black"
                topPadding: 5
                bottomPadding: 5
                leftPadding: 10
                rightPadding: 10
                enabled: !page.loading && page.history.length
                MouseArea {
                    enabled: parent.enabled
                    anchors.fill: parent
                    onClicked: page.back()
                }
            }
            Label {
                text: "⮕"
                color: enabled ? "white" : "black"
                topPadding: 5
                bottomPadding: 5
                leftPadding: 10
                rightPadding: 10
                enabled: !page.loading && page._history.length
                MouseArea {
                    enabled: parent.enabled
                    anchors.fill: parent
                    onClicked: page.next()
                }
            }
            TextInput {
                id: input
                color: enabled ? "white" : "grey"
                text: "about:blank"
                clip: true
                Layout.fillWidth: true
                onFocusChanged: keyboard.visible = focus
                readOnly: page.loading
                onAccepted: {
                    page.open(text);
                    focus = false;
                }
            }
            CustomMenu {
                visible: stateController.state === "loaded"
                BetterMenu {
                    title: qsTr("");
                    font: iconFont.name
                    width: 310
                    MenuItem {
                        text: "Go Home"
                        onClicked: page.open(window.home);
                    }
                    MenuItem {
                        text: "Exit"
                        onClicked: Qt.quit();
                    }
                }
            }
        }
    }
    contentData: [
        Rectangle {
            anchors.fill: parent
            color: "white"
            enabled: stateController.state == "loaded"
            visible: enabled
            Text {
                id: page
                objectName: "page"
                text: ""
                onLinkActivated: open(link)
                focus: true
                Keys.enabled: true
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                property bool loading: false
                property string url: "about:blank"
                property var history: []
                property var _history: []
                function open(url){
                    history.push(this.url);
                    _history = [];
                    load(url);
                }
                function back(){
                    if(!history.length){
                        return;
                    }
                    var url = history.pop()
                    _history.push(url);
                    load(url);
                }
                function next(){
                    if(!_history.length){
                        return;
                    }
                    var url = _history.pop();
                    history.push(url);
                    load(url);
                }
                function load(url){
                    loading = true;
                    input.text = "Loading...";
                    var req = new XMLHttpRequest();
                    req.onreadystatechange = function(){
                        if(req.readyState === XMLHttpRequest.DONE){
                            input.text = page.url = url;
                            page.y = 100;
                            page.text = "<link type='stylesheet' href='page.css'/>" + JS.replace_all_rel_by_abs(url, req.responseText);
                            console.log("Loaded: " + url);
                            loading = false;
                        }
                    };
                    req.onerror = function(e){
                        console.log(e);
                        // TODO handle displaying error
                    };
                    req.open("GET", url);
                    req.send();
                }
                anchors { left: parent.left; right: parent.right; }
                anchors.margins: 50;
            }
        },
        Keyboard { id: keyboard; visible: false }
    ]
    StateGroup {
        id: stateController
        objectName: "stateController"
        state: "loading"
        states: [
            State { name: "loaded" },
            State { name: "loading" }
        ]
        transitions: [
            Transition {
                from: "*"; to: "loaded"
                SequentialAnimation {
                    ScriptAction { script: {
                        console.log("Display loaded");
                    } }
                }
            },
            Transition {
                from: "*"; to: "loading"
                SequentialAnimation {
                    ScriptAction { script: {
                        console.log("Loading display");
                        controller.startup();
                    } }
                }
            }
        ]
    }
}
