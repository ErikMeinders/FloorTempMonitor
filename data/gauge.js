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
        lineWidth: 0,
        minorTickInterval: null,
        tickAmount: 10,
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

const APIGW='http://192.168.2.84/api/';

var Gauges = [];

const app = document.getElementById('gauges_canvas');

var request = new XMLHttpRequest();

request.open('GET', APIGW+'list_sensors', true);

request.onload = function () {

  // Begin accessing JSON data here

  var data = JSON.parse(this.response);

  if (request.status >= 200 && request.status < 400) {
    data["sensors"].forEach(sensor => {
      var chart;

      // when no chart for sensor, create one
      if( ( document.getElementById('container-'+sensor.name)) == null )
      {
        const card  = document.createElement('div');
        card.setAttribute('class', 'card');
        card.setAttribute('id', 'card_'+sensor.name);

        app.appendChild(card);

        var h1 = document.createElement('h1');
        h1.setAttribute('id', 'h1_'+sensor.name);
        h1.textContent = sensor.name;
        
        const newChart = document.createElement('div');
        newChart.setAttribute("class","chart-container");
        newChart.setAttribute("id", 'container-'+sensor.name);

        var p = document.createElement('p');
        p.setAttribute('id', 'p_'+sensor.name);
        p.textContent  =`SensorId: ${sensor.sensorID}`

        card.appendChild(h1);
        card.appendChild(newChart);
        card.appendChild(p);

        var info_id = 'info_'+sensor.name;

        Gauges[sensor.counter] = Highcharts.chart('container-'+sensor.name, Highcharts.merge(gaugeOptions, {
            yAxis: {
                min: 15,
                max: 60,
                title: {
                    text: `${sensor.name}`
                }
            },
        
            credits: {
                enabled: false
            },
            
            series: [{
                name: 'C',
                data: [Math.floor(sensor.temperature)],
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

        chart = Gauges[sensor.counter];
        var point;

        point = chart.series[0].points[0];
        point.update(Math.floor(sensor.temperature*10.0)/10.0);
        
        // change if statement to respond to valve settings once API changed

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

        document.getElementById ("info_"+sensor.name).innerHTML="&Delta;t "+Math.floor(10*(data["sensors"][0].temperature - sensor.temperature))/10.0+"|"+sensor.deltatemp;
      }
      
    });
  } else {
    const errorMessage = document.createElement('marquee');
    errorMessage.textContent = `Gah, it's not working!`;
    app.appendChild(errorMessage);
  }
}

function refreshData() {
  request.open('GET', APIGW+'list_sensors', true);
  request.send();
};

request.send();

var timer = setInterval(refreshData, 15 * 1000);

