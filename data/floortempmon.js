/*
***************************************************************************  
**  Program  : floortempmon.js, part of FloorTempMonitor
**  Version  : v0.6.9
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
let sRelays		= [];
let DOMloaded = false;
let addHeader = true;
let readRaw   = false;
let _MAX_DATAPOINTS = 100 -1;  // must be the same as in FloorTempMonitor in main .ino -1 (last one is zero)
let noSensors    = 0;
//----- chartJS --------
// Red for input sensor0 (=hot), blue for retour sensor1 (=cold)
var colors          = [   'Red', 'Blue', 'Green', 'Yellow', 'FireBrick', 'CornflowerBlue', 'Orange'
                        , 'DeepSkyBlue', 'Gray', 'Purple', 'Brown', 'MediumVioletRed', 'LightSalmon'
                        , 'BurlyWood', 'Gold'
                       ];
//var sensorL         = [];
var sensorData      = {};     //declare an object
sensorData.labels   = [];     // add 'labels' element to object (X axis)
sensorData.datasets = [];     // add 'datasets' array element to object

//----------------Sensor Temp Chart----------------------------------------------------------
var ctxSensor = document.getElementById("sensorsChart").getContext("2d");
var mySensorChart = new Chart(ctxSensor, {
          type: 'line',
          data: sensorData,
          options : {
            responsive: true,
            maintainAspectRatio: true,
            tooltips: {
//            	mode: 'index',
            	mode: 'label',
//            	displayColors: true,
            	callbacks: {
            		label: function(tooltipItem, data) { 
            			let temp = parseFloat(tooltipItem.yLabel);
  								return data.datasets[tooltipItem.datasetIndex].label+": "+parseFloat(temp).toFixed(1)+"*C";
  							}
						  }
 					  },        
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

//----------------Servo State Chart----------------------------------------------------------
var servoData       = {};     //declare an object
servoData.labels    = [];     // add 'labels' element to object (X axis)
servoData.datasets  = [];     // add 'datasets' array element to object

var ctxServo = document.getElementById("servosChart").getContext("2d");
var myServoChart = new Chart(ctxServo, {
          type: 'line',
          data: servoData,
          options : {
            responsive: true,
            maintainAspectRatio: true,
            lineTension: 0,
            tooltips: {
            	mode: 'label',
            	callbacks: {
            		// Use the label callback to display servo state in the tooltip
            		label: function(tooltipItem, data) { 
            				let state = "?";
  									if (tooltipItem.yLabel > 20) state = "Open";
  									else if (tooltipItem.yLabel > 10) state = "Loop";
  									else if (tooltipItem.yLabel < 0)  state = "Closed";
  									else state = "unKnown";
  									//data.datasets[tooltipItem.datasetIndex].pointHoverBackgroundColor;
  									return data.datasets[tooltipItem.datasetIndex].label+": "+state;
  								},
  	            textLabelColor: function(tooltipItem, data) {
  	            		return data.datasets[tooltipItem.datasetIndex].pointHoverBackgroundColor;
  	              }
  	        	 }
  					},        
            scales: {
              yAxes: [{
                scaleLabel: {
                  display: true,
                  labelString: 'Open/Closed',
                },
                ticks: {
                  display: false,
                  max: +35,
                  min: -20,
                  stepSize: 10,
                },
              }]
            }
          }
    });

window.onload=bootsTrap;

window.onerror = function myErrorHandler(errorMsg, lineNumber) {
    console.log("Error occured in line["+lineNumber+"]: " + errorMsg);
    return false;
}

window.onfocus = function() {
  if (needReload) {
    window.location.reload(true);
  }
};


function bootsTrap() {
  needReload = true;
  
  //document.getElementById('sensorsChart').style.display = 'block';
  //document.getElementById('servosChart').style.display  = 'none';
  document.getElementById('FSexplorer').addEventListener('click',function() 
                                     { location.href = "/FSexplorer";
                                     });
 
} // bootsTrap()

webSocketConn = new WebSocket('ws://'+location.host+':81/', ['arduino']);

webSocketConn.onopen    = function () { 
  webSocketConn.send('Connect ' + new Date()); 
  needReload  = false;
  DOMloaded   = false;
  webSocketConn.send("updateDOM");
  setChartType("N");

}; 
webSocketConn.onclose     = function () { 
  needReload  = true;
  let redirectButton = "<p></p><hr><p></p><p></p>"; 
  redirectButton    += "<style='font-size: 50px;'>Disconneted from GUI"; 
  redirectButton    += "<input type='submit' value='re-Connect' "; 
  redirectButton    += " onclick='window.location=\"/\";' />  ";     
  redirectButton    += "<p></p><p></p><hr><p></p>"; 
  document.getElementById("redirect").innerHTML = redirectButton;
  DOMloaded   = false;

}; 
webSocketConn.onerror   = function (error)  { 
  console.log('WebSocket Error ', error);
};
webSocketConn.onmessage = function (e) {
  parsePayload(e.data); 
};

//===========================================================================================
function parsePayload(payload) {
  if ((payload.indexOf('plotSensorTemp') == -1) &&
      (payload.indexOf('plotServoState') == -1) &&
      (payload.indexOf('barRange') == -1) &&
      (payload.indexOf('servoState') == -1) &&
      (payload.indexOf('dataPoint') == -1) &&
      (payload.indexOf('tempBar') == -1) &&
      (payload.indexOf('state') == -1) &&
      (payload.indexOf('tempC') == -1) &&
      (payload.indexOf('clock') == -1)) {
    console.log("parsePayload(): [" + payload + "]\r\n");
  }
  // updateDOM:index<n>=<n>:sensorID<n>=<sensodID>:name<n>=<name>:tempC<n>=<->:tempBar<n>=<0>
  if ( payload.indexOf('updateDOM:') !== -1 ) {          
    let payloadData = payload.replace("updateDOM:", "");
    // index<n>=<n>:sensorID<n>=<sensodID>:name<n>=<name>:tempC<n>=<->:tempBar<n>=<0>
    singlePair = payloadData.split(":");
    // [0] index<n>=<n>
    // [1] sensorID<n>=<sensodID>
    // [2] name<n>=<name>
    // [3] relay<n>=<servoNr>
    // [4] tempC<n>=<->
    // [5] tempBar<n>=<0>
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
          
        } else if (singleFld[0].indexOf('relay') !== -1) {
        	console.log("got relay ["+singleFld[0]+"]["+singleFld[1]+"]");
        	if (singleFld[1] > 0)
        				TD.innerHTML = "R"+singleFld[1]; 
        	else	TD.innerHTML = " "; 
          TD.setAttribute("align", "center");
          TD.setAttribute("style", "font-size: 12pt; padding-left:10px;");
          
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
        if (!DOMloaded) {
        	webSocketConn.send("DOMloaded");
        }
        DOMloaded = true;
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
          
        } else if (singleFld[0].indexOf('relay') !== -1) {
        	if (singleFld[1] > 0) {
        				TD.innerHTML = "R"+singleFld[1]; 
        				sRelays[sensorNr] = singleFld[1];
        	}
        	else	TD.innerHTML = " "; 
          TD.setAttribute("align", "center");
          TD.setAttribute("style", "font-size: 12pt; padding-left:10px;");
          
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
      if (!DOMloaded) {
      	webSocketConn.send("DOMloaded");
      }
      DOMloaded = true;
    }
  
  } else {
    singleFld = payload.split("=");
    // [0] noSensors
    // [1] <noSensors>
    if (singleFld[0].indexOf("noSensors") > -1) {
      noSensors = singleFld[1] * 1;
      for(let r=0; r<noSensors; r++) {
      	sRelays.push(-1);
      }
      buildDataSets(noSensors);
      mySensorChart.update();
      myServoChart.update();
      
    } else if (singleFld[0].indexOf("tempC") > -1) {
      // [0] tempC<n>
      // [1] <nnn.nnn℃>
      document.getElementById( singleFld[0]).innerHTML = singleFld[1];

    } else if (singleFld[0].indexOf("tempBar") > -1) {
      // [0] tempBar<n>B
      // [1] <perc>
      if (!readRaw) {
        moveBar(singleFld[0], singleFld[1]);
      }
      
    } else if (singleFld[0].indexOf("barRange") > -1) {
      // [0] barRange
      // [1] <low x℃ - high y℃"
      if (!readRaw) {
        document.getElementById( singleFld[0]).innerHTML = singleFld[1];
      }
      
    } else if (singleFld[0].indexOf("servoState") > -1) {
      // [0] servoState
      // [1] <text>
      if (!readRaw) {
        document.getElementById( singleFld[0]).innerHTML = singleFld[1];
        //console.log("singleFld[1] => ["+singleFld[1]+"]");
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

//=============================================================================================
    } else if (payload.indexOf("plotSensorTemp") > -1) {
//=============================================================================================
      singlePair = payload.split(":");
      // plotSensorTemp=<n>:TS=<(day) HH-MM>:S<n>=<tempC>:..:S<nn>=<tempC
      var SKIP;
      var newData       = {};     //declare an object
      		newData.labels    = '?';    // add 'labels' element (timestamp) to object (X axis)
      		newData.datasets  = [];     // add 'datasets' array element to object
      for (let s=0; s<noSensors; s++) {
      	newData.datasets.push({});
      	newData.datasets.label = [];
      	newData.datasets.data  = [];
      }
      	  
      for (let i=0; i<singlePair.length; i++) {
        singleFld = singlePair[i].split("=");
        //console.log("plotSensorTemp fldnr["+i+"] fldName["+singleFld[0]+"]fldVal["+singleFld[1]+"]");
        if (i == 0) { // this is the dataPoint for the next itterations
          // [0] plotSensorTemp
          // [1] <n>
          var dataPoint = singleFld[1] * 1;
          //console.log("Sensor: dataPoint ["+dataPoint+"]");
          
        } else if (i==1) {  // this gives timestamp
          // [0] TS
          // [1] <(day) HH-MM>
          let timeStamp = singleFld[1].replace(/-/g,":");     // timeStamp
          // timeStamp = "<(day) HH:MM>"
          if (timeStamp === sensorData.labels[_MAX_DATAPOINTS]) {
          	console.log("SKIP ["+newData.labels+"] === ["+sensorData.labels[_MAX_DATAPOINTS]+"]");
          	SKIP = true;
          } else {
          	SKIP = false;
            newData.labels = timeStamp;
          }
          
        } else if (i>1) { // this gives the tempC for every sensorID
          // [0] S<n>
          // [1] <tempC>
          var sensorID = singleFld[0].replace("S", "") * 1;
          // sensorID = "<n>"
          newData.datasets.label[sensorID] = singleFld[0];
          newData.datasets.data[sensorID]  = parseFloat(singleFld[1]).toFixed(2);
        }
      } // for i ..
      if (!SKIP) {
      	shiftDataSets(noSensors, sensorData, newData);
      	mySensorChart.update();
      }
    
//=============================================================================================
    } else if (payload.indexOf("plotServoState") > -1) {
//=============================================================================================
      singlePair = payload.split(":");
      // plotServoState=<n>:TS=<(day) HH-MM>:S<n>=<tempC>:..:S<nn>=<state>
      var SKIP;
      var newData       = {};     //declare an object
      		newData.labels    = '?';    // add 'labels' element (timestamp) to object (X axis)
      		newData.datasets  = [];     // add 'datasets' array element to object
      for (let s=0; s<noSensors; s++) {
      	newData.datasets.push({});
      	newData.datasets.label = [];
      	newData.datasets.data  = [];
      }
      	  
      for (let i=0; i<singlePair.length; i++) {
        singleFld = singlePair[i].split("=");
        //console.log("plotServoState fldnr["+i+"] fldName["+singleFld[0]+"]fldVal["+singleFld[1]+"]");
        if (i == 0) { // this is the dataPoint for the next itterations
          // [0] plotSensorTemp
          // [1] <n>
          var dataPoint = singleFld[1] * 1;
          //console.log("Servo: dataPoint ["+dataPoint+"]");
          
        } else if (i==1) {  // this gives timestamp
          // [0] TS
          // [1] <(day) HH-MM>
          let timeStamp = singleFld[1].replace(/-/g,":");     // timeStamp
          // timeStamp = "<(day) HH:MM>"
          if (timeStamp === servoData.labels[_MAX_DATAPOINTS]) {
          	console.log("SKIP ["+newData.labels+"] === ["+servoData.labels[_MAX_DATAPOINTS]+"]");
          	SKIP = true;
          } else {
          	SKIP = false;
            newData.labels = timeStamp;
          }
          
        } else if (i>1) { // this gives the tempC for every sensorID
          // [0] S<n>
          // [1] <tempC>
          var sensorID = singleFld[0].replace("S", "") * 1;
          // sensorID = "<n>"
        //newData.datasets.label[sensorID] = singleFld[0];
          if (sRelays[sensorID] < 1) 
          			newData.datasets.label[sensorID] = "  ";
          else	newData.datasets.label[sensorID] = "R"+sRelays[sensorID];
          if ((singleFld[1] * 1) == 0)
          		 	newData.datasets.data[sensorID]  = null;
          else 	newData.datasets.data[sensorID]  = singleFld[1] * 1;
          if (sRelays[sensorID] < 1) {
          	newData.datasets.data[sensorID]  = null;
          }
        }
      } // for i ..
      if (!SKIP) {
      	shiftDataSets(noSensors, servoData, newData);
      	myServoChart.update();
      }
    
    }
    else {  
      document.getElementById( singleFld[0]).innerHTML = singleFld[1];
    	//console.log("Field["+singleFld[0]+"] value ["+singleFld[1]+"]");
      if (singleFld[0].indexOf('state')  > -1) {
      	if (singleFld[1].indexOf('I2CMUX')  > -1) {
      		//console.log("Field["+singleFld[0]+"] set color!!!!");
      		document.getElementById('state').setAttribute("style", "font-size: 16px; color: red; font-weight: bold;");
      	} else {
      		document.getElementById('state').setAttribute("style", "font-size: 16px; color: blue; font-weight: bold;");
      	}
      }
    }
  }
};

//-------------------------------------------------------------------------------------------
function addTableHeader(table) {
  table.innerHTML = "";

  // Create an empty <thead> element and add it to the table:
  var header = table.createTHead();

  // Create an empty <tr> element and add it to the first position of <thead>:
  var TR = header.insertRow(0);    

  // Insert a new cell (<th>) at the first position of the "new" <tr> element:
  if (readRaw) {
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

    TH = TR.insertCell(4);
    TH.innerHTML = "Relay";
    TH.setAttribute("align", "left");
    TH.setAttribute("style", "font-size: 12pt; width: 30px; padding-left:10px;");
    
  } else {
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
    TH.innerHTML = "Relay";
    TH.setAttribute("align", "left");
    TH.setAttribute("style", "font-size: 12pt; width: 30px; padding-left:10px;");
    
    TH = TR.insertCell(4);
    TH.innerHTML = "Servo/Valve";
    TH.setAttribute("id", "servoState");
    TH.setAttribute("style", "padding-left:90px;");
    TH.setAttribute("align", "left");
    
  }

}
 
//-------------------------------------------------------------------------------------------
function moveBar(barID, perc) {
	var elem = document.getElementById(barID);
	frame(perc);
	function frame(w) {
 	  elem.style.width = w + '%';
 	}

} // moveBar

//----------- fill sensorData object --------------------------------------------------------
function buildDataSets(noSensors) {
  for (let s=0; s<noSensors; s++) {
    var y = [];
    
    sensorData.datasets.push({}); //create a new line dataset
    var dataSetSensor 				= sensorData.datasets[s];
    dataSetSensor.label 			= null; //"S"+s; //contains the 'Y; axis label
    dataSetSensor.fill  			= 'false';
    dataSetSensor.borderColor = colors[(s%colors.length)];
    dataSetSensor.data 				= []; //contains the 'Y; axis data

    servoData.datasets.push({});  //create a new line dataset
    var dataSetServo  				= servoData.datasets[s];
    dataSetServo.label 				= null; //"S"+s; //contains the 'Y; axis label
    dataSetServo.fill  				= 'false';	// seems to have no effect
    dataSetServo.borderColor 	= colors[(s%colors.length)];
    dataSetServo.backgroundColor 	= colors[(s%colors.length)];
  //dataSetServo.fillColor    = colors[(s%colors.length)];
    dataSetServo.pointColor   = colors[(s%colors.length)];
    dataSetServo.pointBackgroundColor   = colors[(s%colors.length)];
    dataSetServo.pointHoverBackgroundColor = colors[(s%colors.length)];
    dataSetServo.pointLabelFontColor  = colors[(s%colors.length)];
    dataSetServo.data 				= []; //contains the 'Y; axis data

    for (let p = 0; p < _MAX_DATAPOINTS; p++) {
      y.push(null); // push some data aka generate distinct separate lines
      if (s === 0) {
          sensorData.labels.push(null); // adds x axis labels (timestamp)
          servoData.labels.push(null);  // adds x axis labels (timestamp)
      }
    } // for p ..
    sensorData.datasets[s].data = y; //send new line data to dataset
    servoData.datasets[s].data  = y; //send new line data to dataset
  } // for s ..
  
} // buildDataSets();

//----------- shift dataSet object -------------------------------------------------------
function shiftDataSets(noSensors, dataSet, newData) {
	if (noSensors == 0) return;
  for (let s=0; s<noSensors; s++) {
    
    for (let p = 0; p < _MAX_DATAPOINTS; p++) {
    	if (s===0) {
    		dataSet.labels[p] = dataSet.labels[p+1];	// timestamp!
    	}
      dataSet.datasets[s].data[p] = dataSet.datasets[s].data[p+1];
      //}
    } // for p ..
    dataSet.datasets[s].label = newData.datasets.label[s]; 	// add new data at end
   	dataSet.labels[_MAX_DATAPOINTS] = newData.labels;				// timestamp
    dataSet.datasets[s].data[_MAX_DATAPOINTS]  = newData.datasets.data[s]; // add new data at end

  } // for s ..
  
} // shiftDataSets();

//----------- empty dataSet object -------------------------------------------------------
function emptyDataSets(noSensors, dataSet) {
  for (let s=0; s<noSensors; s++) {
    
    for (let p = 0; p < _MAX_DATAPOINTS; p++) {
    	if (s===0) {
    		dataSet.labels[p] = null;	// timestamp!
    	}
      dataSet.datasets[s].data[p] = null;
      //}
    } // for p ..
    dataSet.datasets[s].label = null; 	// add new data at end
   	dataSet.labels[_MAX_DATAPOINTS] = null;				// timestamp
    dataSet.datasets[s].data[_MAX_DATAPOINTS]  = null; // add new data at end

  } // for s ..
  
} // emptyDataSets();

//-------------------------------------------------------------------------------------------
function setRawMode() {
  if (document.getElementById('rawTemp').checked)  {
    console.log("rawTemp checked!");
    webSocketConn.send("rawMode");
    readRaw = true;
  } else {
    console.log("rawTemp unchecked");
    webSocketConn.send("calibratedMode");
    readRaw = false;
  }
  addHeader = true;
  webSocketConn.send("updateDOM");
  
} // setRawMode()

//-------------------------------------------------------------------------------------------
function setChartType(type) {
	console.log("in setChartType("+type+") ..");
  if (type == "T")  {
  	console.log("chartType set to 'T'");
    document.getElementById('sensorsChart').style.display = "block";
    document.getElementById('servosChart').style.display  = "none";
    emptyDataSets(noSensors, servoData);
    webSocketConn.send("chartType=T");

  } else if (type == "S") {
  	console.log("chartType set to 'S'");
    document.getElementById('sensorsChart').style.display = "none";
    emptyDataSets(noSensors, sensorData);
    document.getElementById('servosChart').style.display  = "block";
    webSocketConn.send("chartType=S");

  } else {
  	console.log("chartType set to 'N'");
    document.getElementById('sensorsChart').style.display = "none";
    emptyDataSets(noSensors, sensorData);
    document.getElementById('servosChart').style.display  = "none";
    emptyDataSets(noSensors, servoData);
    webSocketConn.send("chartType=N");
  }
} // setChartType()

 
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
