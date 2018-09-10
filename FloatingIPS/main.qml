import QtQuick 2.7
import QtQuick.Window 2.3
import QtGraphicalEffects 1.0
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import './components/nav_button' as NavButton

ApplicationWindow {
    id: root
    visible: true
    minimumWidth: 450
    minimumHeight: 450
    maximumWidth: 450
    maximumHeight: 450
    color: '#4e4e4e'
    title: qsTr("Floating IPS")

    Rectangle {
            id: nav
            height: parent.height; width: 75
            color: "#333333"

            property int index: 0

            Flickable {
                anchors.fill: parent
                contentHeight: col.height
                clip: true
                flickableDirection: Flickable.VerticalFlick
                Column {
                    id: col
                    spacing: 2
                    NavButton.Component {
                        source: '/icons/patch.png'
                        size: nav.width
                        isSelected: (nav.index == 0)
                        onClicked: nav.index = 0
                    }

                    NavButton.Component {
                        source: '/icons/create.png'
                        size: nav.width
                        isSelected: (nav.index == 1)
                        onClicked: nav.index = 1
                    }

                    NavButton.Component {
                        source: '/icons/about.png'
                        size: nav.width
                        isSelected: (nav.index == 2)
                        onClicked: nav.index = 2
                    }

                }
            }
    }

    StackLayout {
            id: mainSideNav_stackLayout
            width: parent.width - nav.width; height: parent.height
            anchors.left:  nav.right
            currentIndex: nav.index

            Item { ApplyPage { } }

            Item { CreatePage { } }

            Item { InfoPage { } }
    }
}
