var gaugeOptions = {

    chart: {
        type: 'solidgauge'
    },

    title: null,

    pane: {
        center: ['50%', '85%'],
        size: '140%',
        startAngle: -90,
        endAngle: 90,
        background: {
            backgroundColor:
                Highcharts.defaultOptions.legend.backgroundColor || '#EEE',
            innerRadius: '60%',
            outerRadius: '100%',
            shape: 'arc'
        }
    },

    tooltip: {
        enabled: false
    },

    // the value axis
    yAxis: {
        stops: [
            [0.1, '#55BF3B'], // green
            [0.5, '#DDDF0D'], // yellow
            [0.9, '#DF5353'] // red
        ],
        lineWidth: 1,
        minorTickInterval: null,
        tickAmount: 8,
        title: {
            y: -70
        },
        labels: {
            y: 16
        }
    },

    plotOptions: {
        solidgauge: {
            dataLabels: {
                y: 5,
                borderWidth: 0,
                useHTML: true
            }
        }
    }
};

var Rooms = [];
var servoState = [0, 0, 0, 0, 0, 0, 0, 0, 0];
//var sensorTemp = [0, 0, 0, 0, 0, 0, 0, 0, 0];
//var sensorText = ["","","","","","","","","","",""];
var sensorInfo = [{},{},{},{},{},{},{},{},{},{},{}];

const app = document.getElementById('rooms_canvas');

var requestRoom  = new XMLHttpRequest();
var requestServo = new XMLHttpRequest();
var requestSensor= new XMLHttpRequest();
var requestPut   = new XMLHttpRequest();

// requestRoom.open('GET', APIGW+'room/list', true);

function verbalState(i)
{
    //sn="valve "+i;
    sn="";
    console.log("display servoState for servo "+i+"from servoState"+servoState);

    // servoState array starts at 0, where 0 is relais 16
    // ToDo: add remaining time for Closed/Loop
    //       add taget dt for Open

    switch(servoState[i]) {
        case 0:
            return sn+" Open";
        case 1:
            return sn+" Closed";
        case 2:
            return sn+" Reflow";
        default:
            return sn+" Special"
    }
}
var influx=30.0;
requestSensor.onload = function()
{
    var data = JSON.parse(this.response);

    if (requestSensor.status >= 200 && requestSensor.status < 400) {
        data["sensors"].forEach(sensor => {
            if (sensor.servonr > 0)
            {     
                temperature = Math.floor(sensor.temperature*10)/10;   
                delta = Math.floor((influx - temperature)*10)/10;
                sensorInfo[sensor.servonr] = {
                    temperature: temperature,
                    targetdelta: sensor.deltatemp,
                    delta: delta,
                    text: "&Delta;t<sub>"+sensor.servonr+"</sub>: "+influx+"-"+temperature+"="+delta+" | " + sensor.deltatemp  
                };
            }
            if (sensor.name == "influx")
                influx = Math.floor(sensor.temperature*10)/10;
        });
    }

}
requestServo.onload = function() {

    var data = JSON.parse(this.response);
    if (requestServo.status >= 200 && requestServo.status < 400) {
        servoState = data["servos"];
    }
}
requestRoom.onload = function () {

  // Begin accessing JSON data here

  var data = JSON.parse(this.response);

  if (requestRoom.status >= 200 && requestRoom.status < 400) {
    data["rooms"].forEach(room => {
        var chart;

        // when no chart for room, create one else just update based on IDs

        if( ( document.getElementById('container-'+room.name)) == null )
        {
            const card  = document.createElement('div');
            card.setAttribute('class', 'card');
            card.setAttribute('id', 'card_'+room.name);

            app.appendChild(card);

            var h1 = document.createElement('h1');
            h1.setAttribute('id', 'h1_'+room.name);
            h1.textContent = room.name;
            
            const newChart = document.createElement('div');
            newChart.setAttribute("class","chart-container");
            newChart.setAttribute("id", 'container-'+room.name);

            var ul = document.createElement('ul');
            ul.setAttribute('class','valve-list');
            
            for (i=0 ; i < room.servocount ; i++)
            {
                var li1 = document.createElement('li');
                li1.setAttribute('class', 'valve');
                    
                var h31 = document.createElement('p');
                h31.setAttribute( 'id', "valve-txt-"+room.servos[i]);
                h31.innerHTML = "30 &#8451; | "+verbalState(room.servos[i]);
        
                var pg1 = document.createElement('meter');
                pg1.setAttribute( 'id', "progress-"+room.servos[i]);
                pg1.setAttribute('class', 'valve-1');
                pg1.setAttribute('max', 35);
                pg1.setAttribute('min', 15);
                pg1.setAttribute('high',32);
                pg1.setAttribute('low', 20);

                pg1.setAttribute('value', 20);
                
                li1.appendChild(h31);
                li1.appendChild(pg1);

                ul.appendChild(li1);
            }    
            // SLIDER
            var d = document.createElement('div');
            d.setAttribute("class","slidercontainer");

            var sl = document.createElement('input');
            sl.setAttribute("type", "range");
            sl.setAttribute("min", "15");
            sl.setAttribute("max", "25");
            sl.setAttribute("value",room.target);
            sl.setAttribute("step", "0.1");
            sl.setAttribute("class", "slider");
            sl.setAttribute("id", "sl_"+room.name);

            //    <input type="range" min="15" max="25" value="50" class="slider" id="myRange"></input>

            var vd = document.createElement('p');
            var sp = document.createElement('span');
            sp.setAttribute("id", "vd_"+room.name);
            vd.appendChild(sp);

            // <p>Value: <span id="demo"></span></p>
            d.appendChild(sl);
            d.appendChild(vd);

            sp.innerHTML = sl.value;

            sl.oninput = function() {
                sp.innerHTML = this.value;
            }

            sl.onmouseup = function() {
                console.log("mouse up");
                requestPut.open('PUT', APIGW+'room?room='+room.name+'&temp='+this.value, true);
                requestPut.send();  
            }

            card.appendChild(h1);
            card.appendChild(newChart);
            card.appendChild(d);
            card.appendChild(ul);

            var info_id = 'info_'+room.name;

            Rooms[room.id] = Highcharts.chart('container-'+room.name, Highcharts.merge(gaugeOptions, {
                yAxis: {
                    min: 15,
                    max: 25,
                    title: {
                        text: `${room.name}`
                    }
                },
            
                credits: {
                    enabled: false
                },
                
                series: [{
                    name: 'C',
                    data: [Math.floor(room.actual)],
                    dataLabels: {
                        format:
                            '<div style="text-align:center">' +
                            '<span style="font-size:25px">{y}&deg;C</span><br/>' +
                            '<span style="font-size:10px;opacity:0.4" id='+info_id+'>degrees</span>' +
                            '</div>'
                    },
                    tooltip: {
                        valueSuffix: ' degrees'
                    }
                }]
            
            }));
        } 

        chart = Rooms[room.id];

        // update gauge with actual temperature
        
        var t = Math.floor(room.actual*10.0)/10.0;
        var point = chart.series[0].points[0];
        var temptext="";

        point.update(t);
        
        // update large display temperature

        if (t > room.target+0.2)
        {
            temptext=t+">"+room.target+" [CLOSED]";
        } else {
            temptext=t+"<"+room.target+" [OPEN]";
        }
        document.getElementById ("info_"+room.name).innerHTML=temptext;
        
        // update UGFH loop info

        for (i=0 ; i < room.servocount ; i++)
        {
            var t=sensorInfo[room.servos[i]].temperature;
            var o=influx-sensorInfo[room.servos[i]].targetdelta;

            console.log("updating progress for "+room.name+" with "+i+" "+servoState[ room.servos[i]] );
            var h31=document.getElementById("valve-txt-"+room.servos[i]);
            h31.innerHTML = sensorInfo[room.servos[i]].text+" | "+verbalState(room.servos[i]);
        
            var pg1=document.getElementById( "progress-"+room.servos[i]);
            pg1.setAttribute('value', t);   
            pg1.setAttribute('optimum', o);
        }   
        
        
    });
  } else {
    const errorMessage = document.createElement('marquee');
    errorMessage.textContent = `Gah, it's not working!`;
    app.appendChild(errorMessage);
  }
}

function refreshRoomData() {
    requestRoom.open('GET', APIGW+'room/list', true);
    requestRoom.send();

    requestServo.open('GET', APIGW+'servo/statusarray', true);
    requestServo.send();

    requestSensor.open('GET', APIGW+'sensor/list', true);
    requestSensor.send();
};

// requestRoom.send();

refreshRoomData(); // initial

var timer = setInterval(refreshRoomData, 15 * 1000); // repeat every 15s

