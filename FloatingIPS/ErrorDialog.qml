import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.2

import './components/bootstrap_button' as BootstrapButton

Dialog {
    id: root
    modality: Qt.WindowModal
    title: "Error Message"
    property alias text: textArea.text

    contentItem: Rectangle {
        color: "#4e4e4e"
        implicitWidth: 400
        implicitHeight: 150

        Column {
            anchors.fill: parent

            Flickable {
                width: parent.width; height: parent.height - 75
                contentHeight: textArea.height
                clip: true

                flickableDirection: Flickable.VerticalFlick
                ScrollBar.vertical: ScrollBar {
                    width: 15
                    active: true
                    onActiveChanged: {
                        if (!active)
                            active = true;//Keep scrollbar always visible
                    }
                }

                TextArea {
                    id: textArea
                    clip: true
                    readOnly: true
                    width: parent.width
                    anchors.margins: 2
                    anchors.centerIn: parent
                    wrapMode: TextEdit.Wrap
                    color: "white"
                    font.family: "Helvetica"
                    font.bold: true
                    font.pointSize: 18
                    textFormat: Text.RichText
                    onLinkActivated: Qt.openUrlExternally(link)
                    text: ''
                }

            }

            Item {
                width: parent.width; height: 75
                BootstrapButton.Component {
                    anchors.centerIn: parent
                    type: 'danger'
                    text: 'Close Error'
                    onClicked: root.close()
                }
            }
        }

    }
}
