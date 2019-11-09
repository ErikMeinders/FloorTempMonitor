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
            return sn+" Specail"
    }
}

requestServo.onload = function() {

    var data = JSON.parse(this.response);
    console.log("servo.onload "+data["servos"]);
    if (requestServo.status >= 200 && requestServo.status < 400) {
        console.log("assigning to servoState");
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

        //var p = document.createElement('p');
        //p.setAttribute('id', 'p_'+room.name);
        //p.textContent  =`Temperature: ${room.actual}`

        var ul = document.createElement('ul');
        ul.setAttribute('class','valve-list');
        
        for (i=0 ; i < room.servocount ; i++)
        {
            var li1 = document.createElement('li');
            li1.setAttribute('class', 'valve');
                
            var h31 = document.createElement('p');
            h31.setAttribute( 'id', "valve-txt-"+room.servos[i]);
            h31.innerHTML = "30 &#8451; | "+verbalState(room.servos[i]);
       
            var pg1 = document.createElement('progress');
            pg1.setAttribute( 'id', "progress-"+room.servos[i]);
            pg1.setAttribute('class', 'valve-1');
            pg1.setAttribute('max', 45);
            pg1.setAttribute('value', 30);
            
            li1.appendChild(h31);
            li1.appendChild(pg1);

            ul.appendChild(li1);
        }    
         
        card.appendChild(h1);
        card.appendChild(newChart);
        //card.appendChild(p);
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
      } else {

        chart = Rooms[room.id];
        var point;

        // update graph 

        point = chart.series[0].points[0];
        point.update(Math.floor(room.actual*10.0)/10.0);
        
        // update large number temperature

        document.getElementById ("info_"+room.name).innerHTML=Math.floor(10*(room.actual))/10.0+"|"+room.target;
        
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

