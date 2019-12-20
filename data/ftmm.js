const APIGW='http://'+window.location.host+'/api/';

const app = document.getElementById('rooms_canvas');

var requestRoom  = new XMLHttpRequest();

requestRoom.onload = function () {

  // Begin accessing JSON data here

  var data = JSON.parse(this.response);

  if (requestRoom.status >= 200 && requestRoom.status < 400) {
    
    data["rooms"].forEach(room => {

        if( ( document.getElementById('card_'+room.name)) == null )
        {
            const card  = document.createElement('div');
            card.setAttribute('class', 'card');
            card.setAttribute('id', 'card_'+room.name);

            // add the card to the canvas
            app.appendChild(card);  
// HEADER
            var header = document.createElement('p');
            header.setAttribute('class', 'roomname');
            header.textContent = room.name;
// ACTUAL
            var actual = document.createElement('p');
            actual.setAttribute('class', 'actual');
            actual.setAttribute('id', 'actual_'+room.name);
// TARGET
            var target = document.createElement('p');
            target.setAttribute('class','target');
            target.setAttribute('id', 'target_'+room.name);

            // add the fields to the card
            card.appendChild(header);
            card.appendChild(actual);
            card.appendChild(target);

        } 

        // update  with actual temperature
        
        var actual = Math.floor(room.actual*10.0)/10.0;
        var target = Math.floor(room.target*10.0)/10.0;
      
        // update display temperatures

        document.getElementById("actual_"+room.name).innerHTML= String(actual)+"&#176;";
        document.getElementById("target_"+room.name).innerHTML= String(target)+"&#176;";
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

};

refreshRoomData(); // initial

var timer = setInterval(refreshRoomData, 15 * 1000); // repeat every 15s
