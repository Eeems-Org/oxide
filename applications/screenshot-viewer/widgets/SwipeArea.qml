/* This code was written by Sergejs Kovrovs and has been placed in the public domain. */
// https://gist.github.com/kovrov/1742405#file-swipearea-qml

import QtQuick 2.0


MouseArea {
    property point origin
    property bool ready: false
    property bool swiping: false
    signal move(int x, int y)
    signal swipe(string direction)

    propagateComposedEvents: true
    onPressed: {
        drag.axis = Drag.XAndYAxis;
        origin = Qt.point(mouse.x, mouse.y);
        mouse.accepted = false;
        swiping = true;
    }

    onPositionChanged: {
        switch (drag.axis) {
            case Drag.XAndYAxis:
                if (Math.abs(mouse.x - origin.x) > 16) {
                    drag.axis = Drag.XAxis;
                } else if (Math.abs(mouse.y - origin.y) > 16) {
                    drag.axis = Drag.YAxis;
                }
                mouse.accepted = true;
                swiping = containsPress;
                break;
            case Drag.XAxis:
                move(mouse.x - origin.x, 0);
                mouse.accepted = true;
                swiping = containsPress;
                break;
            case Drag.YAxis:
                move(0, mouse.y - origin.y);
                mouse.accepted = true;
                swiping = containsPress;
                break;
            default:
                mouse.accepted = false;
        }
    }

    onReleased: {
        switch (drag.axis) {
            case Drag.XAndYAxis:
                canceled(mouse);
                mouse.accepted = true;
                break;
            case Drag.XAxis:
                swipe(mouse.x - origin.x < 0 ? "right" : "left");
                mouse.accepted = true;
                break;
            case Drag.YAxis:
                swipe(mouse.y - origin.y < 0 ? "up" : "down");
                mouse.accepted = true;
                break;
            default:
                mouse.accepted = false;
        }
        swiping = false;
    }
}
