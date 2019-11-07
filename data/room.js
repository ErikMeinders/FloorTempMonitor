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

const app = document.getElementById('rooms_canvas');

var requestRoom = new XMLHttpRequest();
var requestServo = new XMLHttpRequest();

// requestRoom.open('GET', APIGW+'room/list', true);

function verbalState(i)
{
    sn="["+i+"]";

    switch(servoState[i]) {
        case 0:
            return sn+" Open";
        case 1:
            return sn+" Closed";
        default:
            return sn+" Special"
    }
}

requestServo.onload = function() {

var data = JSON.parse(this.response);

if (requestRoom.status >= 200 && requestRoom.status < 400) {
    servoState = data["servos"];
    }
}
requestRoom.onload = function () {

  // Begin accessing JSON data here

  var data = JSON.parse(this.response);

  if (requestRoom.status >= 200 && requestRoom.status < 400) {
    data["rooms"].forEach(room => {
      var chart;

      // when no chart for room, create one
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

        var p = document.createElement('p');
        p.setAttribute('id', 'p_'+room.name);
        p.textContent  =`Temperature: ${room.actual}`

        card.appendChild(h1);
        card.appendChild(newChart);
        card.appendChild(p);

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
      } else {

        chart = Rooms[room.id];
        var point;

        point = chart.series[0].points[0];
        point.update(Math.floor(room.actual*10.0)/10.0);
        
        // change if statement to respond to valve settings once API changed
/*
        if (sensor.servostate == 0) //servo is open
            newTitle = '<div style="color: red;">';
        else
            newTitle = '<div style="color: green;">';
        newTitle += sensor.name+'</div>';
        
        chart.update({
                yAxis: {
                        title: {
                            text: `${newTitle}`
                        }
                    },
                });
*/
        document.getElementById ("info_"+room.name).innerHTML=Math.floor(10*(room.actual))/10.0+"|"+room.target;
        iHTML="Loop A : "+verbalState(room.servos[0]);
        if(room.servocount>1)
        {
            iHTML+=" | Loop B : "+verbalState(room.servos[1]);
        }
        document.getElementById ("p_"+room.name).innerHTML = iHTML;
      }
      
    });
  } else {
    const errorMessage = document.createElement('marquee');
    errorMessage.textContent = `Gah, it's not working!`;
    app.appendChild(errorMessage);
  }
}

function refreshRoomData() {

    requestServo.open('GET', APIGW+'servo/statusarray', true);
    requestServo.send();

    requestRoom.open('GET', APIGW+'room/list', true);
    requestRoom.send();
};

// requestRoom.send();

refreshRoomData();

var timer = setInterval(refreshRoomData, 15 * 1000);

