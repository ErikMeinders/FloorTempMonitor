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
        tickAmount: 2,
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
        const newChart = document.createElement('div');
        newChart.setAttribute("class","chart-container");
        newChart.setAttribute("id", 'container-'+sensor.name);
        app.appendChild(newChart);

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
                        '<span style="font-size:25px">{y}</span><br/>' +
                        '<span style="font-size:12px;opacity:0.4">degrees</span>' +
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
        point.update(Math.floor(sensor.temperature));
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

