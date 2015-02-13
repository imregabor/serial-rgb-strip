serial-rgb-strip
================

RGB addressable LED strip driving through serial port. This is a simple Arduino project.


Getting started
---------------

  - Install Arduino IDE from <http://arduino.cc/en/Main/Software>
  - Install Adafruit NeoPixel library from <https://github.com/adafruit/Adafruit_NeoPixel>
  - Install CH340 driver when neceassary (Do not forget to check binary on <http://virustotal.com>)
  - Set board, CPU and port in Arduino IDE
  
SHA512 sum of the win driver for my iformation  

````
451a15213c3057a37f83a4250e9d8a18aa0c3df6ee09b05759b767860ef12d19ed263f6a23c7fc6249cb64b8b140766fb3258f24c01faaeca87069f21a52b4b4 *ch341ser.exe
````

Hardware info
-------------

  - Arduino Nano
  - WS2812B based LED strip on digital port 6
  
  
Protocol
--------

  - `?;`: Return info like `{ numPixels : 8, bitRate : 115200 }`
  - `@RRGGBB.....RRGGBB;` RGB brightness values for LEDs, up to the predefined stip length. Partial can be sent. Returns
    `ok.` upon completion
  - `' '` and `'\n'` and `;` characters ignored when not in data package
  - Upon parse error `?` is returned; send `;` or `'\n'` to recover.  

  