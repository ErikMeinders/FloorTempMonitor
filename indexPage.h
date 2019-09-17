/*
***************************************************************************  
**  Program  : indexPage.h, part of FloorTempMonitor
**  Version  : v0.4.0
**
**  Mostly stolen from https://www.arduinoforum.de/User-Fips
**  See also https://www.arduinoforum.de/arduino-Thread-SPIFFS-DOWNLOAD-UPLOAD-DELETE-Esp8266-NodeMCU
**
***************************************************************************      
*/


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
    .progressBar { width: 100%; height: 20px; background: lightgray; display: block; }
    .tempBar { width: 1%; height: 16px; margin-top: 2px; background: green;}
    #logWindow { font-size: 14px; margin-left: 20px; width: 90vw; height: 20vh; }
    .footer { position: fixed; left: 0; bottom: 0; width: 100%; color: gray; font-size: small; text-align: right; }
  </style>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.2/Chart.min.js"></script>
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
    <thead>
    <tr>
      <th align='left' width='250px'>Naam</th>
      <th align='right' width='200px'>Temperatuur</th>
      <th align='right'>Bar</th>
    </tr>
    </thead>
  </table>
  <hr>
  <!-- REST -->
  <p><input type="checkbox" id="rawTemp" value="doRawTemp" onChange="setRawMode()"> Raw temp. readings</p>
  <p><input type="checkbox" id="debug" value="doDebug" onChange="setDebugMode()"> Debug</p>
  <div id="Redirect"></div>
  <canvas id="sensorsChart" width="900" height="400"></canvas>
  <textarea id="logWindow"></textarea>
  <!--============================================================================-->
  <script>
  
  'use strict';

  let webSocketConn;
  let needReload = true;
  let singlePair;
  let singleFld;
  let DOMloaded = false;
  let addHeader = true;
  let readRaw   = false;
  let _MAX_DATAPOINTS = 50 -1;  // last one is invalid
  let noSensors    = 1;
  //----- chartJS --------
  var colors          = ['red', 'green', 'blue', 'orange', 'gray', 'purple'];
  var Labels          = [];
  var sensorL         = [];
  var sensorData      = {};     //declare an object
  sensorData.labels   = [];     // add 'labels' element to object (X axis)
  sensorData.datasets = [];     // add 'datasets' array element to object
  
/***
  for(let p=0; p<_MAX_DATAPOINTS; p++) {
    console.log("sensorData.labels["+p+"]: ["+sensorData.labels[p]+"]");
    for(let s=0; s<noSensors; s++) {
       console.log(" ---> sensorData.datasets["+s+"].data["+p+"]: ["+sensorData.datasets[s].data[p]+"]");
    }
  }
  console.log("===================================");
  console.log(sensorData);
  console.log("===================================");
***/
  
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

  var config = {
    type: 'line',
    data: {sensorData}
  }

  var ctx = document.getElementById("sensorsChart").getContext("2d");
  var myChart = new Chart(ctx, {
            type: 'line',
            labels: Labels,
            data: sensorData,
            options : {
              responsive: false,
              maintainAspectRatio: true,
              scales: {
                yAxes: [{
                  scaleLabel: {
                    display: true,
                    labelString: '*C',
                  },
                  ticks: {
                    //max: 35,
                    //min: 15,
                    stepSize: 2,
                  },
                }]
              }
            }
      });

  function parsePayload(payload) {
    console.log("parsePayload(): [" + payload + "]\r\n");
    if ( payload.indexOf('updateDOM:') !== -1 ) { 
      addLogLine("parsePayload(): received 'updateDOM:'");
      let payloadData = payload.replace("updateDOM:", "");
      addLogLine("parsePayload(): Data[" + payloadData + "]");
      singlePair = payloadData.split(":");
      var MyTable = document.getElementById("Sensors"); 
      if (addHeader) {
        addTableHeader(MyTable); 
        addHeader = false;       
      }
      // insert new row. 
      var TR = MyTable.insertRow(-1); 
      if (readRaw) {
        for (let i=0; i<singlePair.length; i++) {
          singleFld = singlePair[i].split("=");
          addLogLine("TD id="+singleFld[0]+" Val="+singleFld[1]);
          var TD = TR.insertCell(i); 
          TD.setAttribute("id", singleFld[0]);
          if (singleFld[0].indexOf('name') !== -1) {
            TD.innerHTML = singleFld[1]; 
            TD.setAttribute("style", "font-size: 20pt; padding-left:10px;");
          } else if (singleFld[0].indexOf('tempC') !== -1) {
            TD.innerHTML = singleFld[1]; 
            TD.setAttribute("align", "right");
            TD.setAttribute("style", "font-size: 20pt; padding-left:10px;");
          } else if (singleFld[0].indexOf('tempBar') !== -1) {
            // skip
          } else if (singleFld[0].indexOf('index') !== -1) {
            TD.innerHTML = singleFld[1]; 
            TD.setAttribute("align", "center");
            TD.setAttribute("style", "padding-left:10px;");
          } else {
            TD.innerHTML = singleFld[1]; 
            TD.setAttribute("align", "left");
            TD.setAttribute("style", "padding-left:10px;");
          }
          webSocketConn.send("DOMloaded");
        }
      } else {
        // compensated mode -- skip first two fields --
        for (let i=2; i<singlePair.length; i++) {
          singleFld = singlePair[i].split("=");
          addLogLine("TD id="+singleFld[0]+" Val="+singleFld[1]);
          var TD = TR.insertCell(i-2); 
          TD.setAttribute("id", singleFld[0]);
          if (singleFld[0].indexOf('name') !== -1) {
            TD.innerHTML = singleFld[1]; 
            TD.setAttribute("style", "font-size: 20pt; padding-left:10px;");
          } else if (singleFld[0].indexOf('tempC') !== -1) {
            TD.innerHTML = singleFld[1]; 
            TD.setAttribute("align", "right");
            TD.setAttribute("style", "font-size: 20pt; padding-left:10px;");
          } else if (singleFld[0].indexOf('tempBar') !== -1) {
            TD.setAttribute("align", "left");
            TD.setAttribute("style", "padding-left:30px; width:300px; height: 20px;");
            var SPAN = document.createElement("span");
            SPAN.setAttribute("class", "progressBar");
            var DIV = document.createElement("div");
            DIV.setAttribute("id", singleFld[0]+"B");
            DIV.setAttribute("class", "tempBar");
            SPAN.appendChild(DIV);
            TD.appendChild(SPAN);
          }

        }
        webSocketConn.send("DOMloaded");
      }
    
    } else {
      singleFld = payload.split("=");
      if (singleFld[0].indexOf("noSensors") > -1) {
        noSensors = singleFld[1] * 1;
        addLogLine("noSensors =>"+noSensors);
        buildDataSet(noSensors);
        myChart.update();
        
      } else if (singleFld[0].indexOf("tempC") > -1) {
        addLogLine(singleFld[0]+"=>"+singleFld[1]);
        document.getElementById( singleFld[0]).innerHTML = singleFld[1];
        
      } else if (singleFld[0].indexOf("tempBar") > -1) {
        if (readRaw) {
          addLogLine("found tempBar: "+singleFld[0]+" =>"+singleFld[1]+" SKIP!");
        } else {
          addLogLine("found tempBar: "+singleFld[0]+" =>"+singleFld[1]);
          moveBar(singleFld[0], singleFld[1]);
        }
        
      } else if (singleFld[0].indexOf("barRange") > -1) {
        if (readRaw) {
          addLogLine("found barRange: "+singleFld[0]+" =>"+singleFld[1]+" SKIP!");
        } else {
          addLogLine("found barRange: "+singleFld[0]+" =>"+singleFld[1]);
          document.getElementById( singleFld[0]).innerHTML = singleFld[1];
        }
        
      } else if (payload.indexOf("plotPoint") > -1) {
        singlePair = payload.split(":");
        for (let i=0; i<singlePair.length; i++) {
          singleFld = singlePair[i].split("=");
          //let sensorNr = singleFld[1] * 1;
          console.log("plotPoint fldnr["+i+"] fldName["+singleFld[0]+"]fldVal["+singleFld[1]+"]");
          if (i == 0) { // this is the dataPoint for the next itterations
            var dataPoint = singleFld[1] * 1;
            console.log("dataPoint ["+dataPoint+"]");
            
          } else if (i==1) {  // this gives timestamp
            let timeStamp = timestamp2Date(singleFld[1] * 1); // timeStamp
            //Labels[dataPoint] = timeStamp; // timeStamp
            sensorData.labels[dataPoint] = timeStamp;
            console.log("sensorData.labels["+dataPoint+"] is ["+sensorData.labels[dataPoint]+"]");
            
          } else if (i>1) { // this gives the tempC for every sensorID
            var sensorID = singleFld[0].replace("S", "") * 1;
            //sensorL[sensorID] = singleFld[0];
            sensorData.datasets[sensorID].label = singleFld[0];
            //console.log("sensorID["+sensorID+"] tempC["+sensorL[sensorID]+"]");
            //dataSet[dataPoint] = singleFld[1];
            sensorData.datasets[sensorID].data[dataPoint] = singleFld[1];
            console.log("["+sensorData.datasets[sensorID].label+"] sensorData.datasets["+sensorID+"].data["
                                  +dataPoint+"] => tempC["+sensorData.datasets[sensorID].data[dataPoint]+"]");
          }
        } // for i ..
        
        for(let p=0; p<_MAX_DATAPOINTS; p++) {
            console.log("sensorData.labels["+p+"]:"+sensorData.labels[p]);
            for(let s=0; s<noSensors; s++) {
              console.log("sensorL["+s+"]["+sensorL[s]+"] -> sensorData.datasets["+s+"].data["+p+"]:"+sensorData.datasets[s].data[p]);
            }
        }

        myChart.update();
      }
      else {  
        document.getElementById( singleFld[0]).innerHTML = singleFld[1];
      }
    }
    //addLogLine("parsePayload(): Don't know: [" + payload + "]\r\n");
  };

  function addTableHeader(table) {
    addLogLine("==> set tHeader ..");
    table.innerHTML = "";

    // Create an empty <thead> element and add it to the table:
    var header = table.createTHead();

    // Create an empty <tr> element and add it to the first position of <thead>:
    var TR = header.insertRow(0);    

    // Insert a new cell (<th>) at the first position of the "new" <tr> element:
    if (readRaw) {
      addLogLine("==> [readRaw] --> insert 4 cell's");
      var TH = TR.insertCell(0);
      TH.innerHTML = "index";
      TH.setAttribute("align", "left");
      TH.setAttribute("style", "font-size: 12pt; width: 50px; padding-left:10px;");

      TH = TR.insertCell(1);
      TH.innerHTML = "sensorID";
      TH.setAttribute("align", "left");
      TH.setAttribute("style", "font-size: 12pt; width: 170px; padding-left:10px;");

      TH = TR.insertCell(2);
      TH.innerHTML = "Naam";
      TH.setAttribute("align", "left");
      TH.setAttribute("style", "font-size: 16pt; width: 300px; padding-left:10px;");

      TH = TR.insertCell(3);
      TH.innerHTML = "Temperatuur";
      TH.setAttribute("align", "right");
      TH.setAttribute("style", "font-size: 16pt; width: 150px; padding-left:10px;");
      
    } else {
      addLogLine("==> [!readRaw (compensated)] --> insert 3 cell's");
      var TH = TR.insertCell(0);
      TH.innerHTML = "Naam";
      TH.setAttribute("align", "left");
      TH.setAttribute("style", "font-size: 16pt; width: 250px; padding-left:10px;");
      TH = TR.insertCell(1);
      TH.innerHTML = "Temperatuur";
      TH.setAttribute("align", "right");
      TH.setAttribute("style", "font-size: 16pt; width: 150px; padding-left:10px;");
      TH = TR.insertCell(2);
      TH.innerHTML = "temp Bar";
      TH.setAttribute("id", "barRange");
      TH.setAttribute("style", "padding-left:30px;");
      TH.setAttribute("align", "left");
      
    }

  }
   
  function moveBar(barID, perc) {
    addLogLine("moveBar("+barID+", "+perc+")");
    var elem = document.getElementById(barID);
    frame(perc);
    function frame(w) {
        elem.style.width = w + '%';
    }
  } // moveBar

  //----------- fill sensorData object ----------------------
  function buildDataSet(noSensors) {
    for (let s=0; s<noSensors; s++) {
      var y = [];
      sensorData.datasets.push({}); //create a new line dataset
      var dataSet = sensorData.datasets[s];
      dataSet.label = null; //"S"+s; //contains the 'Y; axis label
      dataSet.fill  = 'false';
      dataSet.borderColor = colors[(s%6)];
      dataSet.data = []; //contains the 'Y; axis data

      for (let p = 0; p < _MAX_DATAPOINTS; p++) {
        y.push(null); // push some data aka generate distinct separate lines
        if (s === 0)
            sensorData.labels.push(p); // adds x axis labels (timestamp)
      } // for p ..
      sensorData.datasets[s].data = y; //send new line data to dataset
    } // for s ..
    
  } // buildDataSet();


  function timestamp2Date(epoch) {
    var date = new Date(epoch * 1000);
  
    //var year = date.getFullYear();
    //var month = date.getMonth() + 1;
    var day = date.getDate();
    var hours = date.getHours();
    var minutes = date.getMinutes();
    return day+" "+hours+":"+minutes;
    
  } // timestamp2Date()  

  
  function setRawMode() {
    if (document.getElementById('rawTemp').checked)  {
      addLogLine("rawTemp checked!");
      console.log("rawTemp checked!");
      webSocketConn.send("rawMode");
      readRaw = true;
    } else {
      addLogLine("rawTemp unchecked");
      console.log("rawTemp unchecked");
      webSocketConn.send("calibratedMode");
      readRaw = false;
    }
    addHeader = true;
    addLogLine("send(updateDOM) ..");
    webSocketConn.send("updateDOM");
    
  } // setRawMode()
 
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

  
/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
