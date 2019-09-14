

static const char serverIndex[] PROGMEM =
  R"====(
  <!DOCTYPE html>
  <html charset="UTF-8">
  <style type='text/css'>
     body { background-color: lightblue;}
    .header h1 span { position: relative; top: 1px; left: 10px; }
    .nav-container a:hover { color: black; background: black; /* only for FSexplorer - Rest does not work */}
    .nav-clock {position: fixed; top: 1px; right: 10px; color: blue; font-size: small; text-align: right; padding-right: 10px; }
    .nav-left { flex: 2 1 0; text-align: left; width: 70% }
    .nav-right { flex: 1 1 0; text-align: right; width: 20%; }
    .nav-img { top: 1px; display: inline-block; width: 40px; height: 40px; }
    .nav-item { display: inline-block; font-size: 16px; padding: 10px 0; height: 20px; border: none; color: black; }
    #logWindow { font-size: 14px; margin-left: 20px; width: 90vw; height: 20vh; }
    [tooltip]:before { content: attr(tooltip);  position: absolute; width: 250px; opacity: 0; transition: all 0.15s ease;  padding: 10px; color: black; border-radius: 10px; box-shadow: 2px 2px 1px silver; }
    [tooltip]:hover:before { opacity: 1;  background: lightgray;  margin-top: -50px; margin-left: 20px;}
    [tooltip]:not([tooltip-persistent]):before {  pointer-events: none;}
    .footer { position: fixed; left: 0; bottom: 0; width: 100%; color: gray; font-size: small; text-align: right; }
  </style>
  <body>
  <div class="nav-container">
    <div class='nav-left'>
      <span class='nav-item'>
      <h1>
        <span id="sysName">FloorTempMonitor</span> &nbsp; &nbsp; &nbsp;
        <span id="state" style='font-size: small;'>-</span> &nbsp;
      </h1>
      </span>
      <a id='FSexplorer'  class='nav-img'>
                      <img src='/FSexplorer.png' alt='FSexplorer'>
      </a>
    </div>
    <div class='nav-right'>
       <span id='clock' class='nav-item nav-clock'>00:00</span>
    </div>  
  </div>  <!-- nav-container -->
  <hr>
  <div>
    <span id="redirect"></span>
  </div>  
  <h3>Aangesloten DS18B20 sensoren</h3>
  <table id="Sensors">
    <tr>
      <th align='left'>Index</th><th align='left'>unieke ID</th>
      <th align='left' width='250px'>Naam</th><th align='right' width='200px'>Temperatuur</th>
    </tr>
  </table>
  <hr>
  <!-- REST -->
  <p><input type="checkbox" id="rawTemp" value="doRawTemp" onChange="setRawMode()"> Raw temp. readings</p>
  <p><input type="checkbox" id="debug" value="doDebug" onChange="setDebugMode()"> Debug</p>
  <div id="Redirect"></div>
  <textarea id="logWindow"></textarea>

  <script>
  
  'use strict';

  let webSocketConn;
  let needReload = true;
  let singlePair;
  let singleFld;
  let DOMloaded = false;
  
  window.onload=bootsTrap;
  window.onfocus = function() {
    if (needReload) {
      window.location.reload(true);
    }
  };
  
  
  function bootsTrap() {
    addLogLine('bootsTrap()');
    needReload = true;
    
    document.getElementById('logWindow').style.display = 'none';
    document.getElementById('FSexplorer').addEventListener('click',function() 
                                       { addLogLine("newTab: goFSexplorer");
                                         location.href = "/FSexplorer";
                                       });
   
  } // bootsTrap()
  
  webSocketConn = new WebSocket('ws://'+location.host+':81/', ['arduino']);
  addLogLine(" ");
  addLogLine('WebSocket("ws://'+location.host+':81/", ["arduino"])');
  
  webSocketConn.onopen    = function () { 
    addLogLine("Connected!");
    webSocketConn.send('Connect ' + new Date()); 
    needReload  = false;
    addLogLine("updateDOM");
    webSocketConn.send("updateDOM");

  }; 
  webSocketConn.onclose     = function () { 
    addLogLine(" ");
    addLogLine("Disconnected!");
    addLogLine(" ");
    needReload  = true;
    let redirectButton = "<p></p><hr><p></p><p></p>"; 
    redirectButton    += "<style='font-size: 50px;'>Disconneted from GUI"; 
    redirectButton    += "<input type='submit' value='re-Connect' "; 
    redirectButton    += " onclick='window.location=\"/\";' />  ";     
    redirectButton    += "<p></p><p></p><hr><p></p>"; 
    document.getElementById("redirect").innerHTML = redirectButton;

  }; 
  webSocketConn.onerror   = function (error)  { 
    addLogLine("Error: " + error);
    console.log('WebSocket Error ', error);
  };
  webSocketConn.onmessage = function (e) {
  //addLogLine("onmessage: " + e.data);
    parsePayload(e.data); 
  };
    
  function parsePayload(payload) {
    console.log("parsePayload(): [" + payload + "]\r\n");
    if ( payload.indexOf('updateDOM:') !== -1 ) { 
      addLogLine("parsePayload(): received 'updateDOM:'");
      let payloadData = payload.replace("updateDOM:", "");
      console.log("parsePayload(): Data[" + payloadData + "]");
      singlePair = payloadData.split(":");
      var MyTable = document.getElementById("Sensors"); 
      // insert new row. 
      var TR = MyTable.insertRow(-1); 
      for (let i=0; i<singlePair.length; i++) {
        singleFld = singlePair[i].split("=");
        console.log("TD id="+singleFld[0]+" Val="+singleFld[1]);
        var TD = TR.insertCell(i); 
        TD.innerHTML = singleFld[1]; 
        TD.setAttribute("id", singleFld[0], 0);
        if (singleFld[0].indexOf('name') !== -1) {
          TD.setAttribute("style", "font-size: 20pt;");
        } else if (singleFld[0].indexOf('tempC') !== -1) {
          TD.setAttribute("align", "right");
          TD.setAttribute("style", "font-size: 20pt;");
        }
      }
      webSocketConn.send("DOMloaded");
    
    } else {
      singleFld = payload.split("=");
      if (singleFld[0].indexOf("tempC") > -1) {
        console.log(singleFld[0]+"=>"+singleFld[1]);
        document.getElementById( singleFld[0]).innerHTML = singleFld[1];
      }
      else {  
        document.getElementById( singleFld[0]).innerHTML = singleFld[1];
      }
    }
    //addLogLine("parsePayload(): Don't know: [" + payload + "]\r\n");
  };
 
  function setRawMode() {
    if (document.getElementById('rawTemp').checked)  {
      addLogLine("rawTemp checked!");
      webSocketConn.send("rawMode");
    } else {
      addLogLine("rawTemp unchecked");
      webSocketConn.send("calibratedMode");
    }
  } // setDebugMode()
 
  function setDebugMode() {
    if (document.getElementById('debug').checked)  {
      addLogLine("DebugMode checked!");
      document.getElementById('logWindow').style.display = "block";
    } else {
      addLogLine("DebugMode unchecked");
      document.getElementById('logWindow').style.display = "none";
    }
  } // setDebugMode()
  
  function addLogLine(text) {
    if (document.getElementById('debug').checked) {
      let logWindow = document.getElementById("logWindow");
      let myTime = new Date().toTimeString().replace(/.*(\d{2}:\d{2}:\d{2}).*/, "$1");
      let addText = document.createTextNode("["+myTime+"] "+text+"\r\n");
      logWindow.appendChild(addText);
      document.getElementById("logWindow").scrollTop = document.getElementById("logWindow").scrollHeight 
    } 
  } // addLogLine()

  </script>

  </body>
    <div class="footer">
      <hr>
      2019 &copy; Willem Aandewiel &nbsp; &nbsp;
    </div>
</html>

  )====";
