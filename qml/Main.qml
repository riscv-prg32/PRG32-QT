import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import PRG32Qt

ApplicationWindow {
    id: root
    visible: true
    width: 1100; height: 760
    minimumWidth: 320; minimumHeight: 260
    title: "PRG32"
    property int page: 0 // 0 setup, 1 store, 2 player
    property string searchText: ""
    property string selectedTag: "All"
    property bool splashVisible: true
    color: "#101216"

    function keyboardMask(key) {
        if (key === Qt.Key_Left || key === Qt.Key_A) return 1
        if (key === Qt.Key_Right || key === Qt.Key_D) return 2
        if (key === Qt.Key_Up || key === Qt.Key_W) return 4
        if (key === Qt.Key_Down || key === Qt.Key_S) return 8
        if (key === Qt.Key_Z || key === Qt.Key_J) return 16
        if (key === Qt.Key_X || key === Qt.Key_K) return 32
        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Space) return 64
        return 0
    }
    function gameTitle(g) { return g.title || g.name || g.id || "Cartridge" }
    function tags() {
        let out=["All"], seen={}
        const a=storeClient.cartridges
        for(let i=0;i<a.length;i++) { const t=a[i].tags || []; for(let j=0;j<t.length;j++) if(!seen[t[j]]) {seen[t[j]]=true;out.push(t[j])} }
        return out.sort((a,b)=>a==="All"?-1:(b==="All"?1:a.localeCompare(b)))
    }
    function filteredGames() {
        const q=searchText.toLowerCase(), a=storeClient.cartridges, out=[]
        for(let i=0;i<a.length;i++) { const g=a[i], t=g.tags||[]; if(selectedTag!=="All" && t.indexOf(selectedTag)<0) continue
            const hay=(gameTitle(g)+" "+(g.summary||"")+" "+t.join(" ")).toLowerCase(); if(q.length===0||hay.indexOf(q)>=0) out.push(g) }
        out.sort((a,b)=>gameTitle(a).localeCompare(gameTitle(b))); return out
    }
    function showPlayer() { appController.resume(); page=2; requestActivate(); keyboardHandler.forceActiveFocus() }

    Item {
        id: keyboardHandler
        anchors.fill: parent
        focus: true
        Keys.onPressed: event => { const m=root.keyboardMask(event.key); if(m){appController.setKeyboardButton(m,true);event.accepted=true} }
        Keys.onReleased: event => { const m=root.keyboardMask(event.key); if(m){appController.setKeyboardButton(m,false);event.accepted=true} }
    }
    onActiveChanged: if (!active) appController.clearKeyboard()
    Component.onCompleted: { storeClient.refresh(); appController.playStartupTone(); splashTimer.start(); requestActivate(); keyboardHandler.forceActiveFocus() }

    Timer { id:splashTimer; interval:900; repeat:false; onTriggered:root.splashVisible=false }
    FileDialog { id:importDialog; title:"Import PRG32 cartridge"; nameFilters:["PRG32 cartridges (*.prg32)","All files (*)"]; onAccepted: if(appController.loadFile(selectedFile)) root.showPlayer() }
    Dialog { id:settingsDialog; title:"Store Settings"; modal:true; standardButtons:Dialog.Close; anchors.centerIn:parent; width:Math.min(parent.width-40,600)
        ColumnLayout { anchors.fill:parent; TextField { id:storeField; Layout.fillWidth:true; text:storeClient.baseUrl; placeholderText:"Cartridge Store URL" }
            RowLayout { Button{text:"Test connection";onClicked:{storeClient.baseUrl=storeField.text;storeClient.refresh()}} Button{text:"Restore default";onClicked:{storeClient.resetDefault();storeField.text=storeClient.baseUrl;storeClient.refresh()}} }
            Label { Layout.fillWidth:true; text:storeClient.error.length?storeClient.error:(storeClient.cartridges.length+" cartridges"); wrapMode:Text.Wrap }
        }
    }
    Dialog { id:aboutDialog; title:"About PRG32-QT"; modal:true; standardButtons:Dialog.Close; anchors.centerIn:parent; width:Math.min(parent.width-40,660); height:Math.min(parent.height-60,620)
        ScrollView { anchors.fill:parent; ColumnLayout { width:parent.width; spacing:12
            Image { Layout.alignment:Qt.AlignHCenter; source:"qrc:/prg32qt/assets/prg32_logo.png"; sourceSize.width:420; fillMode:Image.PreserveAspectFit }
            Label { Layout.fillWidth:true; wrapMode:Text.Wrap; text:"PRG32-QT is the portable Qt/C++ runner for the PRG32 educational RISC-V gaming platform." }
            GroupBox { title:"People"; Layout.fillWidth:true; ColumnLayout { Label{text:"Project lead: Raffaele Montella"} Label{text:"Student contributor: Simone Boscaglia"} Label{text:"Student contributor: Ivan Cafiero"} } }
            GroupBox { title:"University and labs"; Layout.fillWidth:true; Label { width:parent.width; wrapMode:Text.Wrap; text:"Università degli Studi di Napoli Parthenope. PRG32 teaching labs cover console output, input, graphics, sound and timing, scores, and multiplayer on ESP32-C6 hardware or QEMU." } }
            GroupBox { title:"License"; Layout.fillWidth:true; Label { width:parent.width; wrapMode:Text.Wrap; text:"MIT License · Copyright © 2026 Raffaele Montella. See docs/LICENSING.md and LICENSE." } }
        } }
    }

    StackLayout { anchors.fill:parent; currentIndex:root.page
        // Setup screen
        Rectangle { color:"black"
            ScrollView { anchors.fill:parent; ColumnLayout { width:Math.min(parent.width-48,560); anchors.horizontalCenter:parent.horizontalCenter; spacing:12
                Item { Layout.preferredHeight:22 }
                Image { Layout.alignment:Qt.AlignHCenter; source:"qrc:/prg32qt/assets/prg32_logo.png"; Layout.preferredHeight:130; Layout.preferredWidth:500; fillMode:Image.PreserveAspectFit }
                Label { text:"PRG32 SETUP"; color:"white"; font.family:"monospace"; font.pixelSize:18 }
                Label { Layout.fillWidth:true; text:"PLATFORM: "+Qt.platform.os.toUpperCase()+"\nRUNTIME: RV32IMAC · 30 FPS\nCARTRIDGES: "+storeClient.cartridges.length+"\nIP: "+(appController.deviceIp||"No local IPv4 address")+"\nWEB API: "+(appController.webApiUrl||"Unavailable"); color:"#43d17b"; font.family:"monospace"; wrapMode:Text.WrapAnywhere }
                Button { Layout.fillWidth:true; text:"›  RUN CARTRIDGE"; enabled:appController.running; onClicked:root.showPlayer() }
                Button { Layout.fillWidth:true; visible:appController.performanceAvailable; text:"›  RUN PERFORMANCE TEST"; onClicked:{appController.runPerformanceTest();root.showPlayer()} }
                Button { Layout.fillWidth:true; text:"›  BROWSE STORE"; onClicked:root.page=1 }
                Button { Layout.fillWidth:true; text:"›  IMPORT CARTRIDGE"; onClicked:importDialog.open() }
                Button { Layout.fillWidth:true; text:"›  STORE SETTINGS"; onClicked:settingsDialog.open() }
                Button { Layout.fillWidth:true; text:"›  ABOUT PRG32-QT"; onClicked:aboutDialog.open() }
                Label { Layout.fillWidth:true; text:(storeClient.error.length?storeClient.error:appController.status).toUpperCase(); color:"#45c9ff"; wrapMode:Text.Wrap; font.family:"monospace" }
            } }
        }
        // Store browser
        Item { ColumnLayout { anchors.fill:parent; anchors.margins:12; spacing:8
            RowLayout { Layout.fillWidth:true; Button{text:"Setup";onClicked:root.page=0} Label{text:"Cartridge Store";font.pixelSize:20;font.bold:true} Item{Layout.fillWidth:true} Label{text:filteredGames().length+" / "+storeClient.cartridges.length} Button{text:"Refresh";onClicked:storeClient.refresh()} }
            ColumnLayout { Layout.fillWidth:true; spacing:4
                RowLayout { Layout.fillWidth:true; TextField { Layout.fillWidth:true; placeholderText:"Search cartridges"; text:root.searchText; onTextChanged:root.searchText=text }
                    ComboBox { id:tagBox; Layout.preferredWidth:Math.min(150,root.width*.35); model:root.tags(); onCurrentTextChanged:root.selectedTag=currentText } }
                RowLayout { Layout.fillWidth:true; Button { text:"Import .prg32"; onClicked:importDialog.open() }
                    Button { text:"Settings"; onClicked:settingsDialog.open() } Item { Layout.fillWidth:true }
                    Label { text:appController.deviceIp||"Offline"; font.pixelSize:11; color:"#43d17b" } }
            }
            ProgressBar { Layout.fillWidth:true; indeterminate:true; visible:storeClient.loading }
            Label { Layout.fillWidth:true; visible:storeClient.error.length>0; text:storeClient.error; color:"tomato"; wrapMode:Text.Wrap }
            ListView { Layout.fillWidth:true; Layout.fillHeight:true; clip:true; spacing:4; model:root.filteredGames()
                delegate:ItemDelegate { width:ListView.view.width; height:78
                    contentItem:RowLayout { spacing:12
                        Rectangle { Layout.preferredWidth:58; Layout.preferredHeight:58; radius:8; color:"#252932"; Image { anchors.fill:parent; anchors.margins:3; source:storeClient.iconUrl(modelData.id||""); fillMode:Image.PreserveAspectFit; asynchronous:true } }
                        ColumnLayout { Layout.fillWidth:true; Label{text:root.gameTitle(modelData);font.bold:true} Label{Layout.fillWidth:true;text:modelData.summary||modelData.id||"";elide:Text.ElideRight;color:"#aeb5c0"} Label{text:(modelData.tags||[]).slice(0,3).join(" · ");color:"#43c8ef";font.pixelSize:11} }
                        Label { text:"▶"; font.pixelSize:22 }
                    }
                    onClicked:storeClient.downloadGame(modelData)
                }
            }
            Label { Layout.fillWidth:true; text:storeClient.loading?"Loading…":(appController.status|| (storeClient.cartridges.length+" cartridges")); elide:Text.ElideRight }
        } }
        // Player
        Item { id:playerPage
            Rectangle { anchors.fill:parent; color:"#0d1015" }
            Loader { anchors.fill:parent; anchors.topMargin:48; sourceComponent:width>height?landscapePlayer:portraitPlayer }
            RowLayout { anchors.left:parent.left; anchors.right:parent.right; anchors.top:parent.top; anchors.margins:6; height:42; z:3
                Button { text:"‹ Setup"; onClicked:{appController.pause();root.page=0} }
                Label { Layout.fillWidth:true; text:appController.cartridgeName; elide:Text.ElideRight; font.bold:true; horizontalAlignment:Text.AlignHCenter }
                Button { visible:appController.performanceAvailable; text:"Performance"; onClicked:appController.runPerformanceTest() }
            }
        }
    }

    Component { id:screenComponent
        Rectangle { color:"black"; radius:10; border.color:Qt.rgba(appController.ledR/255,appController.ledG/255,appController.ledB/255,Math.min(.5,appController.ledIntensity)); border.width:2
            PRG32Frame { anchors.fill:parent; anchors.margins:10; Component.onCompleted:appController.attachFrame(this) }
        }
    }
    Component { id:dpadComponent
        Item { property real threshold:.28
            Rectangle { anchors.centerIn:parent; width:parent.width/3; height:parent.height; radius:10; color:"#3a3e46" }
            Rectangle { anchors.centerIn:parent; width:parent.width; height:parent.height/3; radius:10; color:"#3a3e46" }
            Label {text:"▲";anchors.horizontalCenter:parent.horizontalCenter;anchors.top:parent.top} Label{text:"▼";anchors.horizontalCenter:parent.horizontalCenter;anchors.bottom:parent.bottom} Label{text:"◀";anchors.verticalCenter:parent.verticalCenter;anchors.left:parent.left} Label{text:"▶";anchors.verticalCenter:parent.verticalCenter;anchors.right:parent.right}
            MouseArea { anchors.fill:parent; preventStealing:true; function update(x,y){let dx=(x-width/2)/(width/2),dy=(y-height/2)/(height/2),m=0;if(dx<-threshold)m|=1;if(dx>threshold)m|=2;if(dy<-threshold)m|=4;if(dy>threshold)m|=8;appController.setDirectional(m)} onPressed:p=>update(p.x,p.y); onPositionChanged:p=>update(p.x,p.y); onReleased:appController.setDirectional(0); onCanceled:appController.setDirectional(0) }
        }
    }
    Component { id:actionsComponent
        Item { RoundButton { width:68;height:68;text:"B";anchors.left:parent.left;anchors.bottom:parent.bottom;onPressed:appController.setButton(32,true);onReleased:appController.setButton(32,false) }
            RoundButton { width:72;height:72;text:"A";anchors.right:parent.right;anchors.top:parent.top;onPressed:appController.setButton(16,true);onReleased:appController.setButton(16,false) } }
    }
    Component { id:portraitPlayer
        ColumnLayout { anchors.fill:parent; anchors.margins:18; spacing:12
            RowLayout { Layout.fillWidth:true; Image{source:"qrc:/prg32qt/assets/prg32_logo.png";Layout.preferredWidth:150;Layout.preferredHeight:42;fillMode:Image.PreserveAspectFit} Item{Layout.fillWidth:true} Rectangle{width:9;height:9;radius:5;color:Qt.rgba(appController.ledR/255,appController.ledG/255,appController.ledB/255,Math.max(.15,appController.ledIntensity))} }
            Loader { Layout.fillWidth:true; Layout.preferredHeight:Math.min(330,width*200/320+20); Layout.fillHeight:true; sourceComponent:screenComponent }
            RowLayout { Layout.fillWidth:true; Layout.preferredHeight:Math.min(150,Math.max(110,parent.height*.2)); Item{Layout.fillWidth:true;Layout.fillHeight:true;Loader{anchors.centerIn:parent;width:Math.min(130,parent.width);height:Math.min(130,parent.height);sourceComponent:dpadComponent}} Item{Layout.fillWidth:true;Layout.fillHeight:true;Loader{anchors.centerIn:parent;width:Math.min(150,parent.width);height:Math.min(120,parent.height);sourceComponent:actionsComponent}} }
            Button { Layout.alignment:Qt.AlignHCenter; text:"SELECT"; onPressed:appController.setButton(64,true);onReleased:appController.setButton(64,false) }
            RowLayout { Layout.fillWidth:true; Label{text:"RV32IMAC · 30 FPS"} Item{Layout.fillWidth:true} Label{text:appController.deviceIp||"Offline"} }
            Label { Layout.alignment:Qt.AlignHCenter; visible:appController.controllerConnected; text:"Controller: "+appController.controllerName; opacity:.7 }
        }
    }
    Component { id:landscapePlayer
        RowLayout { anchors.fill:parent; anchors.margins:14; spacing:12
            ColumnLayout { Layout.preferredWidth:Math.min(190,Math.max(126,parent.width*.16)); Layout.fillHeight:true; Item{Layout.fillHeight:true} Image{Layout.fillWidth:true;Layout.preferredHeight:60;source:"qrc:/prg32qt/assets/prg32_logo.png";fillMode:Image.PreserveAspectFit} Loader{Layout.alignment:Qt.AlignHCenter;Layout.preferredWidth:130;Layout.preferredHeight:130;sourceComponent:dpadComponent} Button{Layout.alignment:Qt.AlignHCenter;text:"SELECT";onPressed:appController.setButton(64,true);onReleased:appController.setButton(64,false)} Item{Layout.fillHeight:true} }
            ColumnLayout { Layout.fillWidth:true; Layout.fillHeight:true; Loader{Layout.fillWidth:true;Layout.fillHeight:true;sourceComponent:screenComponent} Label{Layout.alignment:Qt.AlignHCenter;Layout.fillWidth:true;horizontalAlignment:Text.AlignHCenter;elide:Text.ElideRight;text:(appController.performanceAvailable?"Performance: "+appController.performanceState+" · ":"")+(appController.deviceIp||"Offline")+" · "+(appController.controllerConnected?appController.controllerName:"30 FPS");opacity:.7} }
            ColumnLayout { Layout.preferredWidth:Math.min(190,Math.max(126,parent.width*.16)); Layout.fillHeight:true; Item{Layout.fillHeight:true} Loader{Layout.alignment:Qt.AlignHCenter;Layout.preferredWidth:150;Layout.preferredHeight:130;sourceComponent:actionsComponent} Label{Layout.alignment:Qt.AlignHCenter;text:"PRG32";font.bold:true} Item{Layout.fillHeight:true} }
        }
    }

    Connections { target:storeClient; function onCartridgeDownloaded(id,data){ if(appController.loadBytes(data,id)) root.showPlayer() } }

    Rectangle { visible:root.splashVisible; anchors.fill:parent; color:"black"; z:100
        ColumnLayout { anchors.centerIn:parent; width:Math.min(parent.width-50,560); Image{Layout.fillWidth:true;Layout.preferredHeight:220;source:"qrc:/prg32qt/assets/prg32_logo.png";fillMode:Image.PreserveAspectFit} Label{Layout.alignment:Qt.AlignHCenter;text:"RISC-V PLAYGROUND";color:"#45c9ff";font.family:"monospace";font.letterSpacing:3} }
    }
}
