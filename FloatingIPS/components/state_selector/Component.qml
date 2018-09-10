import QtQuick 2.7
import QtGraphicalEffects 1.0
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

Item {
    id: selector

    property alias list: view.model
    property alias selectedIndex: view.currentIndex
    property alias label: labelText.text
    property bool expanded

    width: parent.width; height: parent.height

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; }

        height: parent.height

        radius: 2
        border.width: 1
        border.color: "#4e4e4e"
        color: "#4e4e4e"

        MouseArea {
            anchors.fill: parent

            onClicked: selector.expanded = !selector.expanded

            Text {
                id: labelText
                font.pointSize: 24
                font.bold: true
                color: "white"
                anchors { left: parent.left; top: parent.top; margins: 2 }
            }

            Rectangle {
                anchors {
                    left: parent.left; top: labelText.bottom;
                    right: parent.right; bottom: parent.bottom;
                    margins: 2
                    leftMargin: 10
                }

                radius: 2
                color: "grey"

                ListView {
                    id: view

                    anchors.fill: parent

                    clip: true

                    delegate: Text {
                        anchors { left: parent.left; right: parent.right }
                        height: 35

                        verticalAlignment: Text.AlignVCenter

                        font.pointSize: 30
                        font.bold: true
                        color: "white"
                        text: modelData

                        MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                view.currentIndex = index
                                selector.expanded = !selector.expanded
                            }
                        }
                    }

                    highlight: Rectangle {
                        anchors { left: parent.left; right: parent.right }
                        height: 35
                        radius: 2

                        color: "#2f2f2f"
                    }
                }
            }
        }
    }
}
