#include <esp-knx-ip.h>

#define KNXROOMS noRooms

DECLARE_TIMERs(knxUpdate,60);

int8_t occasionally=0;

void myKNX_init() {
  
  callback_id_t tempCallBackId, targetCallBackId,stateCallBackId;

  knx.physical_address_set(knx.PA_to_address(1, 1, 200));

  tempCallBackId = knx.callback_register("Temperature", temp_cb, nullptr, ok_fun);
  targetCallBackId = knx.callback_register("Target", target_cb, nullptr, ok_fun);
  stateCallBackId = knx.callback_register("State", state_cb, nullptr, ok_fun);

  for( int8_t Room=0 ; Room < KNXROOMS ; Room++) // HJM HJM fix in juiste aantal!
  {
    
    DebugTf("teCB %d taCB %d & stBC %d\n", tempCallBackId,targetCallBackId,stateCallBackId );

    address_t GA;
    
    Rooms[Room].knxActual = 15.0f;
    // GA = knx.GA_to_address(10, Room, 1); // actual = 1
    knx.callback_assign (tempCallBackId, Rooms[Room].GA_actual);

    Rooms[Room].knxTarget = 25.0f;
    // GA = knx.GA_to_address(10, Room, 2); // target = 2
    knx.callback_assign (targetCallBackId, Rooms[Room].GA_target);
    
    Rooms[Room].knxState = false;
    // GA = knx.GA_to_address(10, Room, 3); // state = 3
    knx.callback_assign (stateCallBackId, Rooms[Room].GA_target);

  }
  // Start knx
  knx.start(nullptr);

  myKNX_update();
}

bool ok_fun()
{
    return true;
}

void myKNX_loop() 
{
  knx.loop();
  
  if ( DUE(knxUpdate) )
    myKNX_update();
}

void myKNX_update()
{
  address_t GA;

  // knx.write_1bit(knx.GA_to_address(1,0,60), 1);

  for( int8_t Room=0 ; Room < KNXROOMS ; Room++)
  {
    if (!occasionally || (Rooms[Room].knxActual != Rooms[Room].actualTemp))
    {
      // update actual temp on KNX bus
      Rooms[Room].knxActual = Rooms[Room].actualTemp;
      DebugTf("writing to %d/%d/%d actual temp %4.1f\n", 
              Rooms[Room].GA_actual.ga.area,
              Rooms[Room].GA_actual.ga.line,
              Rooms[Room].GA_actual.ga.member,
              Rooms[Room].knxActual);
      knx.write_2byte_float(Rooms[Room].GA_actual, Rooms[Room].knxActual);
      delay(50);
    }
    if (!occasionally || (Rooms[Room].knxTarget != Rooms[Room].targetTemp))
    {
      // update target on KNX bus
      Rooms[Room].knxTarget = Rooms[Room].targetTemp;
      DebugTf("writing to %d/%d/%d target temp %4.1f\n", 
              Rooms[Room].GA_target.ga.area,
              Rooms[Room].GA_target.ga.line,
              Rooms[Room].GA_target.ga.member,
              Rooms[Room].knxTarget);
      knx.write_2byte_float(Rooms[Room].GA_target, Rooms[Room].knxTarget);
        delay(50);
    }

    bool isHeating = false;

    for (int8_t k=0 ; k < 2 ; k++)
      if( Rooms[Room].Servos[k] >= 0)
      { 
        DebugTf("Servo[%d] state Room[%d] is %d\n", Rooms[Room].Servos[k], 
                                                    Room,
                                                    servoArray[Rooms[Room].Servos[k]].servoState );
        if( servoArray[Rooms[Room].Servos[k]].servoState == SERVO_IS_OPEN || servoArray[Rooms[Room].Servos[k]].servoState == SERVO_IS_OPENING )
          isHeating = true;
      }
      
    if( !occasionally || (Rooms[Room].knxState != isHeating ))
    {
      Rooms[Room].knxState = isHeating;

      DebugTf("writing to %d/%d/%d state heating is %s\n", 
              Rooms[Room].GA_state.ga.area,
              Rooms[Room].GA_state.ga.line,
              Rooms[Room].GA_state.ga.member,
              isHeating ? "on" : "off");

      knx.write_1bit(Rooms[Room].GA_state, isHeating);

      // DebugTf("writing to 10/%d/3 heating is %s\n", Room, isHeating ? "on" : "off");
      // knx.write_1bit(knx.GA_to_address(10, Room, 3), isHeating);
      delay(50);
    }
  }
  if(++occasionally == 3)
    occasionally = 0;

}
  

void temp_cb(message_t const &msg, void *arg)
{
  address_t _from = msg.received_on;

  int roomNr = _from.ga.line;

  DebugTf("temp CB callback triggered for room %d\n", roomNr);

  switch (msg.ct)
  {
    case KNX_CT_READ:
    {
      DebugTf("About to reply %f\n", Rooms[roomNr].actualTemp);
      knx.answer_2byte_float(msg.received_on, Rooms[roomNr].actualTemp);
      break;
    }
  }
}

void target_cb(message_t const &msg, void *arg)
{
  address_t _from = msg.received_on;

  int roomNr = _from.ga.line;
  DebugTf("target CB callback triggered with %d for room %d\n", msg.ct, roomNr);

  switch (msg.ct)
  {
    case KNX_CT_READ:
    {
      DebugTf("About to reply target for the room is %f\n", Rooms[roomNr].targetTemp);
      knx.answer_2byte_float(msg.received_on, Rooms[roomNr].targetTemp);
      
      break;
    }
    case KNX_CT_WRITE:
    {
      Rooms[roomNr].targetTemp = knx.data_to_2byte_float(msg.data);
      DebugTf("Set target for the room to %f\n", Rooms[roomNr].targetTemp);

      break;
    }
  }
}

void state_cb(message_t const &msg, void *arg)
{
  address_t _from = msg.received_on;

  int roomNr = _from.ga.line;

  DebugTf("state CB callback triggered for room %d\n", roomNr);

  switch (msg.ct)
  {
    case KNX_CT_READ:
    {
      DebugTf("About to reply %s\n", Rooms[roomNr].knxState? "on" : "off");
      knx.answer_1bit(msg.received_on, Rooms[roomNr].knxState);
      break;
    }
  }
}
