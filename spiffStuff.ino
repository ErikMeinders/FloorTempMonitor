/*
***************************************************************************
**  Program  : spiffsStuff, part of FloorTempMonitor
**  Version  : v0.6.1
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

static      FSInfo SPIFFSinfo;

//===========================================================================================
uint32_t freeSpace()
{
  int32_t space;

  SPIFFS.info(SPIFFSinfo);

  space = (int32_t)(SPIFFSinfo.totalBytes - SPIFFSinfo.usedBytes);
  if (space < 1) space = 0;

  return space;

} // freeSpace()

//===========================================================================================
void listSPIFFS()
{
  Dir dir = SPIFFS.openDir("/");

  DebugTln("\r\n");
  while (dir.next()) {
    File f = dir.openFile("r");
    Debugf("%-25s %6d \r\n", dir.fileName().c_str(), f.size());
    yield();
  }

  SPIFFS.info(SPIFFSinfo);

  Debugln("\r");
  if (freeSpace() < (10 * SPIFFSinfo.blockSize))
        Debugf("Available SPIFFS space [%6d]kB (LOW ON SPACE!!!)\r\n", (freeSpace() / 1024));
  else  Debugf("Available SPIFFS space [%6d]kB\r\n", (freeSpace() / 1024));
  Debugf("           SPIFFS Size [%6d]kB\r\n", (SPIFFSinfo.totalBytes / 1024));
  Debugf("     SPIFFS block Size [%6d]bytes\r\n", SPIFFSinfo.blockSize);
  Debugf("      SPIFFS page Size [%6d]bytes\r\n", SPIFFSinfo.pageSize);
  Debugf(" SPIFFS max.Open Files [%6d]\r\n\r\n", SPIFFSinfo.maxOpenFiles);


} // listSPIFFS()



/***************************************************************************
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
***************************************************************************/
