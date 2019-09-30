/*
***************************************************************************  
**  Program  : sensorEdit.js, part of FloorTempMonitor
**  Version  : v0.6.3
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
  
"use strict";

	let webSocketConn;
	let needReload 	= true;
	let sensorNr = 0;
  let singlePair;
  let singleFld;
  
  window.onload=bootsTrap;
  window.onfocus = function() {
  	if (needReload) {
  		window.location.reload(true);
  	}
  };
  	
  
  function bootsTrap() {
		addLogLine("bootsTrap()");
		needReload = true;
		
		document.getElementById('debug').checked = false;
    document.getElementById('logWindow').style.display = "none";
    document.getElementById('bTerug').addEventListener('click',function()
    																						{location.href = "/FSexplorer";});
    document.getElementById('bVorige').addEventListener('click',function() 
    																						{getPrevious();});
    document.getElementById('bVolgende').addEventListener('click',function() 
    																						{getNext();});
    document.getElementById('bOpslaan').addEventListener('click',function() 
    																						{saveSensorValues();});

    document.getElementById('sName').addEventListener('change',function() 
    																						{editValue('sName', 'T', 12);});
    document.getElementById('sPosition').addEventListener('change',function() 
    																						{editValue('sPosition', 'N', 0);});
    document.getElementById('sTempOffset').addEventListener('change',function() 
    																						{editValue('sTempOffset', 'N', 5);});
    document.getElementById('sTempFactor').addEventListener('change',function() 
    																						{editValue('sTempFactor', 'N', 6);});
    document.getElementById('sServo').addEventListener('change',function() 
    																						{editValue('sServo', 'N', 0);});
    document.getElementById('sDeltaTemp').addEventListener('change',function() 
    																						{editValue('sDeltaTemp', 'N', 1);});

  } // bootsTrap()
  
  webSocketConn	= new WebSocket('ws://'+location.host+':81/', ['arduino']);
  addLogLine(" ");
  addLogLine("WebSocket('ws://"+location.host+":81/', ['arduino'])");
  console.log("WebSocket('ws://"+location.host+":81/', ['arduino'])");
  
  webSocketConn.onopen 		= function () { 
  	addLogLine("Connected!");
   	webSocketConn.send('Connect ' + new Date()); 
   	addLogLine("getDevInfo");
   	webSocketConn.send("getDevInfo");
   	needReload	= false;
   	addLogLine("getFirstSensor");
   	webSocketConn.send("getFirstSensor");

  }; 
  
  webSocketConn.onclose 		= function () { 
   	addLogLine(" ");
   	addLogLine("Disconnected!");
   	addLogLine(" ");
   	needReload	= true;
   	let redirectButton = "<p></p><hr><p></p><p></p>"; 
   	redirectButton    += "<style='font-size: 50px;'>Disconneted from FloorTempMonitor"; 
   	redirectButton    += "<input type='submit' value='re-Connect' "; 
    redirectButton    += " onclick='window.location=\"/\";' />  ";     
   	redirectButton    += "<p></p><p></p><hr><p></p>"; 

  }; 
  webSocketConn.onerror 	= function (error) 	{ 
   	addLogLine("Error: " + error);
   	console.log('WebSocket Error ', error);
  };
  webSocketConn.onmessage	= function (e) {
  	needReload = false;
    parsePayload(e.data); 
  };
  

  
  function parsePayload(payload) {
		console.log("parsePayload("+payload+")");

 		singlePair   = payload.split(",");
 		var msgType = singlePair[0].split("=");

 		if (msgType[1] == "devInfo") {
 			console.log("devInfo: "+devName+","+devVersion);
 			for ( var i = 1; i < singlePair.length; i++ ) {
 				singleFld = singlePair[i].split("=");
 				document.getElementById(singleFld[0].trim()).innerHTML = singleFld[1].trim();
 			}
 		} else if (msgType[1] == "editSensor") {
 			for ( var i = 1; i < singlePair.length; i++ ) {
 				singleFld = singlePair[i].split("=");
				console.log("setting ["+singleFld[0]+"] value ["+singleFld[1]+"]");
 				if (document.getElementById('sPosition').value == 0) {	// Flux IN
 					document.getElementById('tDeltaTemp').innerHTML = "Closed Time (min.)";
 				} else {
 					document.getElementById('tDeltaTemp').innerHTML = "Delta Temperatuur";
 				} 					
 				if (singleFld[0].indexOf("sensorNr") > -1) {
 					sensorNr = singleFld[1] * 1;
 					document.getElementById(singleFld[0].trim()).innerHTML = "("+singleFld[1]+") SensorID ";
 				} else {
 					document.getElementById(singleFld[0]).value = singleFld[1];
 				}
 			}
 		}
 		else {  
			singleFld = singlePair[0].split("=");
			console.log("["+singleFld[0]+"] -> ["+singleFld[1]+"]");
			if (   (singleFld[0].indexOf("tempC") !== -1) ) {
				var sNr = "S"+singleFld[0].replace("tempC", "")+": ";
   			document.getElementById( singleFld[0]).innerHTML = sNr+singleFld[1]+" ";
   			document.getElementById( singleFld[0]).removeAttribute("style", "display: none");
   			document.getElementById( singleFld[0]).setAttribute("style", "border: 1px solid darkGray; color: black; width:14%;");
   		} else if ( (singleFld[0].indexOf("clock") !== -1) ) {
				document.getElementById( singleFld[0]).innerHTML = singleFld[1];
   		} else if ( (singleFld[0].indexOf("state") !== -1) ) {
				document.getElementById( singleFld[0]).innerHTML = singleFld[1];
				if (singleFld[1].indexOf('SX1509')  > -1) {
					document.getElementById('state').setAttribute("style", "font-size: 14px; color: red; font-weight: bold;");
				}
   		}

   	}
  };	// parsePayload()


 	function editValue(Field, Type, Len) {
		var Fld = document.getElementById(Field).value.trim();
 		console.log("editValue(): Field["+Field+"]=["+Fld+"], Type["+Type+"], Len["+Len+"]");
 		if (Type == 'T') {
 			Fld.substring(0, Len);
 		} else if (Type == 'N') {
 			Fld = round(Fld, Len)
 		}
 		document.getElementById(Field).value = Fld;

    //checkValidity();

 	}	// editValue()
 	
 	
 	function saveSensorValues() {
		console.log("saveSettings() ...");
		var updRec = "updSensorNr="+sensorNr
		             +",sensorID="+document.getElementById('sID').value
		             +",name="+document.getElementById('sName').value+" "
		             +",position="+document.getElementById('sPosition').value * 1
		             +",tempOffset="+document.getElementById('sTempOffset').value * 1.0
		             +",tempFactor="+document.getElementById('sTempFactor').value * 1.0
		             +",servoNr="+document.getElementById('sServo').value * 1
		             +",deltaTemp="+document.getElementById('sDeltaTemp').value * 1.0;
		webSocketConn.send(updRec);
		var warning = document.getElementById('rebootWarning');
		warning.setAttribute("style", "color: red; text-align: center");

 	}	// saveSensorValues()
 	
  function getPrevious() {
		console.log("editSensorNr="+(sensorNr - 1));
		webSocketConn.send("editSensorNr="+(sensorNr - 1));	// Bottum UP!

 	}	// getPrevious()
 	
  function getNext() {
		console.log("editSensorNr="+(sensorNr + 1));
		webSocketConn.send("editSensorNr="+(sensorNr + 1));	// Bottum UP!

 	}	// getNext()
 	
 	
  function setDebugMode() {
  	if (document.getElementById('debug').checked)  {
  		addLogLine("DebugMode checked!");
  		document.getElementById('logWindow').style.display = "block";
  	} else {
  		addLogLine("DebugMode unchecked");
  		document.getElementById('logWindow').style.display = "none";
  	}
  }	// setDebugMode()
  
  
  function addLogLine(text) {
  	if (document.getElementById('debug').checked) {
  		let logWindow = document.getElementById("logWindow");
  		let myTime = new Date().toTimeString().replace(/.*(\d{2}:\d{2}:\d{2}).*/, "$1");
  		let addText = document.createTextNode("["+myTime+"] "+text+"\r\n");
  		logWindow.appendChild(addText);
  		document.getElementById("logWindow").scrollTop = document.getElementById("logWindow").scrollHeight 
  	} 
  }	// addLogLine()
   

  function round(value, precision) {
    var multiplier = Math.pow(10, precision || 0);
    return Math.round(value * multiplier) / multiplier;
  }
  

  function existingId(elementId) {
    if(document.getElementById(elementId)){
      return true;
    } 
    return false;
    
  } // existingId()
  
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
