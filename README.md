# Tank Sensor
Sensor that is intended to measure the tank air gap in a fluid filled 
tank. From this and knowledge of the tank geometry the consumer can 
compute the volume of liquid in the tank.
## Hardware
  + Sparkfun Thing : Basically an ESP8266 packaged by sparkfun.
  + HCSR04 : Ultrasonic Sensor
## Interface
  + sensor.json : Provides an interface to get the raw sensor samples.
  + data.json : Provides an inteface to get the reading in engineering units. Inches is the current implementation.
