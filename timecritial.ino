// just a function you call whenever you want to make sure timecritial actions happen

// NOTE: Make sure no to make recurisve calls!

void timeCritical()
{
    yield();
    I2cExpander.connectedToMux();
    timeThis( httpServer.handleClient() );
}