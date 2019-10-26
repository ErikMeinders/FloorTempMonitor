const app = document.getElementById('root');

const APIGW='http://192.168.2.84/api/'

// const logo = document.createElement('img');
// logo.src = 'logo.png';

const container = document.createElement('div');
container.setAttribute('class', 'container');
container.setAttribute('id', 'cards');

// app.appendChild(logo);
app.appendChild(container);

var request = new XMLHttpRequest();
request.open('GET', APIGW+'list_sensors', true);
request.onload = function () {

  // Begin accessing JSON data here
  var data = JSON.parse(this.response);
  if (request.status >= 200 && request.status < 400) {
    data["sensors"].forEach(sensor => {

      var h1, p, t, bd, b;

      // when no card for sensor, create card
      if( document.getElementById('card_'+sensor.name) == null )
      {
        const card = document.createElement('div');
        card.setAttribute('class', 'card');
        card.setAttribute('id', 'card_'+sensor.name);

        h1 = document.createElement('h1');
        h1.setAttribute('id', 'h1_'+sensor.name);

        p = document.createElement('p');
        p.setAttribute('id', 'p_'+sensor.name);

        t = document.createElement('t');
        t.setAttribute('id', 't_'+sensor.name);


        bd = document.createElement('div');
        bd.setAttribute('class', 'grad1');
        bd.setAttribute('id', 'bd_'+sensor.name);

        b = document.createElement('div');
        b.setAttribute('class', 'bar');
        b.setAttribute('id', 'b_'+sensor.name);

        container.appendChild(card);
        card.appendChild(h1);
        card.appendChild(p);
        card.appendChild(t);
        card.appendChild(bd);

        bd.appendChild(b);

      } else {

        h1 = document.getElementById( 'h1_'+sensor.name);
        p  = document.getElementById( 'p_'+sensor.name);
        t  = document.getElementById( 't_'+sensor.name);
        b  = document.getElementById( 'b_'+sensor.name);

      }
      h1.textContent = sensor.name;
      p.textContent  = `Sensor: ${sensor.sensorID}`
      t.textContent  = `Temperature: ${sensor.temperature}`;
      var bar_fill = Math.floor( sensor.temperature) * 2;
      b.style.height = `${bar_fill}px`;
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

var h = 0;
   
   var x ;
   function myFunction() {
     if(x==null){
      x = setInterval(myFunction1, 160);}
     
     function myFunction1(){
     
     h++;

     document.getElementById("status").innerHTML = Math.floor(h / 2) + "%"
     if (document.getElementById("bar").style.height == 200 + "px")

     {
       clearInterval(x);

     }

   }}
