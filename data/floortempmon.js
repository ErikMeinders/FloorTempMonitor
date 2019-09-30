/*
***************************************************************************  
**  Program  : floortempmon.js, part of FloorTempMonitor
**  Version  : v0.6.3
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
  

'use strict';

let webSocketConn;
let needReload = true;
let singlePair;
let singleFld;
let sensorNr;
let DOMloaded = false;
let addHeader = true;
let readRaw   = false;
let _MAX_DATAPOINTS = 100 -1;  // must be the same as in FloorTempMonitor in main .ino -1 (last one is zero)
let noSensors    = 1;
//----- chartJS --------
var colors          = [   'Green', 'CornflowerBlue', 'Red', 'Yellow', 'FireBrick', 'Blue', 'Orange'
                        , 'DeepSkyBlue', 'Gray', 'Purple', 'Brown', 'MediumVioletRed', 'LightSalmon'
                        , 'BurlyWood', 'Gold'
                       ];
var Labels          = [];
var sensorL         = [];
var sensorData      = {};     //declare an object
sensorData.labels   = [];     // add 'labels' element to object (X axis)
sensorData.datasets = [];     // add 'datasets' array element to object

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
  setDebugMode();

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

//-------------------------------------------------------------------------------------------
var ctx = document.getElementById("sensorsChart").getContext("2d");
var myChart = new Chart(ctx, {
          type: 'line',
          labels: Labels,
          data: sensorData,
          options : {
            responsive: true,
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

//-------------------------------------------------------------------------------------------
function parsePayload(payload) {
  if (   (payload.indexOf('plotPoint') == -1) 
      && (payload.indexOf('barRange') == -1)
      && (payload.indexOf('dataPoint') == -1)
      && (payload.indexOf('tempBar') == -1)
      && (payload.indexOf('clock') == -1)) {
    console.log("parsePayload(): [" + payload + "]\r\n");
  }
  // updateDOM:index<n>=<n>:sensorID<n>=<sensodID>:name<n>=<name>:tempC<n>=<->:tempBar<n>=<0>
  if ( payload.indexOf('updateDOM:') !== -1 ) {             
    addLogLine("parsePayload(): received 'updateDOM:'");
    let payloadData = payload.replace("updateDOM:", "");
    // index<n>=<n>:sensorID<n>=<sensodID>:name<n>=<name>:tempC<n>=<->:tempBar<n>=<0>
    addLogLine("parsePayload(): Data[" + payloadData + "]");
    singlePair = payloadData.split(":");
    // [0] index<n>=<n>
    // [1] sensorID<n>=<sensodID>
    // [2] name<n>=<name>
    // [3] tempC<n>=<->
    // [4] tempBar<n>=<0>
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
        if (singleFld[0].indexOf("name") !== -1) {
          sensorNr = singleFld[0].replace("name", "") * 1;
        }
        var TD = TR.insertCell(i); 
        TD.setAttribute("id", singleFld[0]);
        if (singleFld[0].indexOf('name') !== -1) {
          TD.innerHTML = singleFld[1]; 
          TD.setAttribute("style", "font-size: 18pt; padding-left:10px;");
        } else if (singleFld[0].indexOf('tempC') !== -1) {
          TD.innerHTML = singleFld[1]; 
          TD.setAttribute("align", "right");
          TD.setAttribute("style", "font-size: 18pt; padding-left:10px;");
        } else if (singleFld[0].indexOf('tempBar') !== -1) {
          // skip
        } else if (singleFld[0].indexOf('index') !== -1) {
          TD.innerHTML = singleFld[1]; 
          TD.setAttribute("align", "center");
          TD.setAttribute("style", "padding-left:10px;");
        } else if (singleFld[0].indexOf('servoState') !== -1) {
        	// skip
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
      	console.log(singlePair[i]);
        singleFld = singlePair[i].split("=");
        if (singleFld[0].indexOf("name") !== -1) {
          sensorNr = singleFld[0].replace("name", "") * 1;
        }
        var TD = TR.insertCell(i-2); 
        TD.setAttribute("id", singleFld[0]);
        if (singleFld[0].indexOf('name') !== -1) {
          TD.innerHTML = singleFld[1]; 
          TD.setAttribute("style", "font-size: 18pt; padding-left:10px;");
        } else if (singleFld[0].indexOf('tempC') !== -1) {
          TD.innerHTML = singleFld[1]; 
          TD.setAttribute("align", "right");
          TD.setAttribute("style", "font-size: 18pt; padding-left:10px;");
        } else if (singleFld[0].indexOf('tempBar') !== -1) {
          TD.setAttribute("align", "left");
          TD.setAttribute("style", "padding-left:30px; width:300px; height: 18px;");
          var SPAN = document.createElement("span");
          SPAN.setAttribute("class", "progressBar");
          var DIV = document.createElement("div");
          DIV.setAttribute("id", singleFld[0]+"B");
          DIV.setAttribute("class", "tempBar");
          DIV.setAttribute("style", "background-color:"+colors[(sensorNr%colors.length)]+";");
          SPAN.appendChild(DIV);
          TD.appendChild(SPAN);
        } else if (singleFld[0].indexOf('servoState') !== -1) {
          //TD.setAttribute("align", "left");
          TD.setAttribute("style", "padding-left:30px; height: 18px; font-size: 13px;");
          var SPAN = document.createElement("span");
          //SPAN.setAttribute("class", "servoState");
          var DIV = document.createElement("div");
          DIV.setAttribute("id", singleFld[0]+"S");	// +S needs to be different from the TD id
          SPAN.appendChild(DIV);
          TD.appendChild(SPAN);
        }

      }
      webSocketConn.send("DOMloaded");
    }
  
  } else {
    singleFld = payload.split("=");
    // [0] noSensors
    // [1] <noSensors>
    if (singleFld[0].indexOf("noSensors") > -1) {
      noSensors = singleFld[1] * 1;
      addLogLine("noSensors =>"+noSensors);
      buildDataSet(noSensors);
      myChart.update();
      
    } else if (singleFld[0].indexOf("tempC") > -1) {
      // [0] tempC<n>
      // [1] <nnn.nnn℃>
      addLogLine(singleFld[0]+"=>"+singleFld[1]);
      document.getElementById( singleFld[0]).innerHTML = singleFld[1];
      
    } else if (singleFld[0].indexOf("tempBar") > -1) {
      // [0] tempBar<n>B
      // [1] <perc>
      if (readRaw) {
        addLogLine("found tempBar: "+singleFld[0]+" =>"+singleFld[1]+" SKIP!");
      } else {
        addLogLine("found tempBar: "+singleFld[0]+" =>"+singleFld[1]);
        moveBar(singleFld[0], singleFld[1]);
      }
      
    } else if (singleFld[0].indexOf("barRange") > -1) {
      // [0] barRange
      // [1] <low x℃ - high y℃"
      if (readRaw) {
        addLogLine("found barRange: "+singleFld[0]+" =>"+singleFld[1]+" SKIP!");
      } else {
        addLogLine("found barRange: "+singleFld[0]+" =>"+singleFld[1]);
        document.getElementById( singleFld[0]).innerHTML = singleFld[1];
      }
      
    } else if (singleFld[0].indexOf("servoState") > -1) {
      // [0] servoState
      // [1] <text"
      if (readRaw) {
        addLogLine("found servoState: "+singleFld[0]+" =>"+singleFld[1]+" SKIP!");
      } else {
        addLogLine("found servoState: "+singleFld[0]+" =>"+singleFld[1]);
        document.getElementById( singleFld[0]).innerHTML = singleFld[1];
        console.log("singleFld[1] => ["+singleFld[1]+"]");
        var DIV = document.getElementById(singleFld[0]);
        if (singleFld[1].indexOf("OPEN") !== -1) {
       		DIV.setAttribute("style", "text-align:center; vertical-align:-2px; height:18px; background-color:#e25822; color:white;");	// "flame"
        } else if (singleFld[1].indexOf("CLOSED") !== -1) {
       		DIV.setAttribute("style", "text-align:center; vertical-align:-2px; height:18px; background-color:blue; color:white;");	
        } else if (singleFld[1].indexOf("LOOP") !== -1) {
       		DIV.setAttribute("style", "text-align:center; vertical-align:-2px; height:18px; background-color:#eb8b66;");	
        } else if (singleFld[1].indexOf("CYCLE") !== -1) {
       		DIV.setAttribute("style", "text-align:center; vertical-align:-2px; height:18px; background-color:SkyLightBlue; color:white;");	
        } else {	// retour Flux!
       		DIV.setAttribute("style", "text-align:center; vertical-align:-2px; height:18px; background-color:lightGray; font-weight: bold;");	
        }
      }
      
    } else if (payload.indexOf("plotPoint") > -1) {
      singlePair = payload.split(":");
      // plotPoint=<n>:TS=<(day) HH-MM>:S<n>=<tempC>:..:S<nn>=<tempC
      for (let i=0; i<singlePair.length; i++) {
        singleFld = singlePair[i].split("=");
        //console.log("plotPoint fldnr["+i+"] fldName["+singleFld[0]+"]fldVal["+singleFld[1]+"]");
        if (i == 0) { // this is the dataPoint for the next itterations
          // [0] plotPoint
          // [1] <n>
          var dataPoint = singleFld[1] * 1;
          //console.log("dataPoint ["+dataPoint+"]");
          
        } else if (i==1) {  // this gives timestamp
          // [0] TS
          // [1] <(day) HH-MM>
          let timeStamp = singleFld[1].replace(/-/g,":");     // timeStamp
          // timeStamp = "<(day) HH:MM>"
          sensorData.labels[dataPoint] = timeStamp;
          //console.log("sensorData.labels["+dataPoint+"] is ["+sensorData.labels[dataPoint]+"]");
          
        } else if (i>1) { // this gives the tempC for every sensorID
          // [0] S<n>
          // [1] <tempC>
          var sensorID = singleFld[0].replace("S", "") * 1;
          // sensorID = "<n>"
          sensorData.datasets[sensorID].label = singleFld[0];
          sensorData.datasets[sensorID].data[dataPoint] = parseFloat(singleFld[1]).toFixed(2);
          //console.log("["+sensorData.datasets[sensorID].label+"] sensorData.datasets["+sensorID+"].data["
          //                      +dataPoint+"] => tempC["+sensorData.datasets[sensorID].data[dataPoint]+"]");
        }
      } // for i ..
      myChart.update();
    }
    else {  
      document.getElementById( singleFld[0]).innerHTML = singleFld[1];
      if (singleFld[1].indexOf('SX1509')  > -1) {
      	document.getElementById('state').setAttribute("style", "font-size: 16px; color: red; font-weight: bold;");
      }
    }
  }
  //addLogLine("parsePayload(): Don't know: [" + payload + "]\r\n");
};

//-------------------------------------------------------------------------------------------
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
    TH.setAttribute("style", "font-size: 14pt; width: 300px; padding-left:10px;");

    TH = TR.insertCell(3);
    TH.innerHTML = "Temperatuur";
    TH.setAttribute("align", "right");
    TH.setAttribute("style", "font-size: 14pt; width: 150px; padding-left:10px;");
    
  } else {
    addLogLine("==> [!readRaw (compensated)] --> insert 4 cell's");
    var TH = TR.insertCell(0);
    TH.innerHTML = "Naam";
    TH.setAttribute("align", "left");
    TH.setAttribute("style", "font-size: 14pt; width: 250px; padding-left:10px;");
    TH = TR.insertCell(1);
    TH.innerHTML = "Temperatuur";
    TH.setAttribute("align", "right");
    TH.setAttribute("style", "font-size: 14pt; width: 150px; padding-left:10px;");
    TH = TR.insertCell(2);
    TH.innerHTML = "temp Bar";
    TH.setAttribute("id", "barRange");
    TH.setAttribute("style", "padding-left:30px;");
    TH.setAttribute("align", "left");
    TH = TR.insertCell(3);
    TH.innerHTML = "Servo/Valve";
    TH.setAttribute("id", "servoState");
    TH.setAttribute("style", "padding-left:90px;");
    TH.setAttribute("align", "left");
    
  }

}
 
//-------------------------------------------------------------------------------------------
function moveBar(barID, perc) {
  addLogLine("moveBar("+barID+", "+perc+")");
  var elem = document.getElementById(barID);
  frame(perc);
  function frame(w) {
      elem.style.width = w + '%';
  }
} // moveBar

//----------- fill sensorData object --------------------------------------------------------
function buildDataSet(noSensors) {
  for (let s=0; s<noSensors; s++) {
    var y = [];
    sensorData.datasets.push({}); //create a new line dataset
    var dataSet = sensorData.datasets[s];
    dataSet.label = null; //"S"+s; //contains the 'Y; axis label
    dataSet.fill  = 'false';
    dataSet.borderColor = colors[(s%colors.length)];
    dataSet.data = []; //contains the 'Y; axis data

    for (let p = 0; p < _MAX_DATAPOINTS; p++) {
      y.push(null); // push some data aka generate distinct separate lines
      if (s === 0)
          sensorData.labels.push(p); // adds x axis labels (timestamp)
    } // for p ..
    sensorData.datasets[s].data = y; //send new line data to dataset
  } // for s ..
  
} // buildDataSet();


//-------------------------------------------------------------------------------------------
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

//-------------------------------------------------------------------------------------------
function setDebugMode() {
  if (document.getElementById('debug').checked)  {
    addLogLine("DebugMode checked!");
    document.getElementById('logWindow').style.display = "block";
  } else {
    addLogLine("DebugMode unchecked");
    document.getElementById('logWindow').style.display = "none";
  }
} // setDebugMode()

//-------------------------------------------------------------------------------------------
function addLogLine(text) {
  if (document.getElementById('debug').checked) {
    let logWindow = document.getElementById("logWindow");
    let myTime = new Date().toTimeString().replace(/.*(\d{2}:\d{2}:\d{2}).*/, "$1");
    let addText = document.createTextNode("["+myTime+"] "+text+"\r\n");
    logWindow.appendChild(addText);
    document.getElementById("logWindow").scrollTop = document.getElementById("logWindow").scrollHeight 
  } 
} // addLogLine()
  
/*
***************************************************************************
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
***************************************************************************
*/
