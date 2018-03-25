#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <limits.h>

#define SAMPLE_SIZE 16
#define SENSOR_INTERVAL 30000

#define TRIG 12
#define PULSE 13

typedef struct
{
  unsigned long data;
  int age;
} SampleData_Type;

SampleData_Type samples[SAMPLE_SIZE];

ESP8266WebServer server(80);

const char WiFiSSID[] = "sensors";
const char WiFiPSK[] = "sensors123";

unsigned long sampleTime = 0;
double distance = 0.0;
double distanceMedian = 0.0;

// While this (holding the html & javascript text in a string) "works" 
// it would be much better to get an SD card shield and serve this type 
// of thing from an actual file. It just so happens that I do not have
// a SD card shield and soldering directly to the tabs on an SD card
// seems less than wise. So here it is ... please just scroll to past
// the web code.

String webpage =
"<!DOCTYPE html> \n\
 <html> \n\
   <head> \n\
     <meta http-equiv=refresh content=300> \n\
     <script src=https://ajax.googleapis.com/ajax/libs/jquery/1.11.1/jquery.min.js></script> \n\
     <script src=https://www.google.com/jsapi></script> \n\
     <body><table width=80% align=center> \n\
       <tr> \n\
         <td><div id='time' style='width: 40%;'></div></td> \n\
         <td><div id='distance' style='width: 40%;'></div></td> \n\
      </tr></table><table width=80% align=center> \n\
      <tr> \n\
         <td><p></td> \n\
         <td>Sample Time</td> \n\
         <td>Valid</td> \n\
         <td>Distance (avg)</td> \n\
         <td>Distance (median)</td> \n\
      </tr><tr> \n\
         <td>HCSR04</td> \n\
         <td><div id='timeS0' style='width: 25%;'></div></td> \n\
         <td><div id='validS0' style='width: 25%;'></div></td> \n\
         <td><div id='distanceS0' style='width: 25%;'></div></td> \n\
         <td><div id='distanceMS0' style='width: 25%;'></div></td> \n\
      </tr></table><script> \n\
       function loadSensorData() { \n\
         var jsonData = $.ajax({url: 'data.json', dataType: 'jsonp'}).done(function (results) { \n\
           document.getElementById('timeS0').innerHTML = parseFloat(results[0].time); \n\
           document.getElementById('validS0').innerHTML = parseFloat(results[0].valid); \n\
           document.getElementById('distanceS0').innerHTML = parseFloat(results[0].d_in); \n\
           document.getElementById('distanceMS0').innerHTML = parseFloat(results[0].dm_in); \n\
         }); \n\
       } \n\
       loadSensorData(); \n\
     </script> \n\
     </body></html>\n";

void handleRoot() 
{
  server.send(200, "text/html", webpage);
}

void dataJSON() 
{
  String value = server.arg("callback");

  String message = "typeof " + value + " === 'function' && " + value + 
                   "([{ time:" + String(sampleTime) + "," + 
                       "d_in:" + String(distance) + "," +
                       "dm_in:" + String(distanceMedian) + " }])\n";

  server.send(200, "text/plain", message);
}

String JSON_SensorValues(SampleData_Type data)
{
  String element = "{ age:" + String(data.age) + "," + 
                       "d_in:" + String(data.data) + " }\n  ";
  return element;
}

void sensorJSON() 
{
  String value = server.arg("callback");

  String message = "typeof " + value + " === 'function' && " + value + 
                   "([" + JSON_SensorValues(samples[0]) + "," + 
                          JSON_SensorValues(samples[1]) + "," + 
                          JSON_SensorValues(samples[2]) + "," + 
                          JSON_SensorValues(samples[3]) + "," + 
                          JSON_SensorValues(samples[4]) + "," + 
                          JSON_SensorValues(samples[5]) + "," + 
                          JSON_SensorValues(samples[6]) + "," + 
                          JSON_SensorValues(samples[7]) + "," + 
                          JSON_SensorValues(samples[8]) + "," + 
                          JSON_SensorValues(samples[9]) + "," + 
                          JSON_SensorValues(samples[10]) + "," + 
                          JSON_SensorValues(samples[11]) + "," + 
                          JSON_SensorValues(samples[12]) + "," + 
                          JSON_SensorValues(samples[13]) + "," + 
                          JSON_SensorValues(samples[14]) + "," + 
                          JSON_SensorValues(samples[15]) + "]);\n";

  server.send(200, "text/plain", message);
}


void setupServer()
{
  server.on("/", handleRoot);
  server.on("/data.json", dataJSON);
  server.on("/sensor.json", sensorJSON);
  server.onNotFound(handleNotFound);
  server.begin();
}

void handleNotFound()
{

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);

}

void setupWifi()
{
  pinMode(5, OUTPUT);
  int state = 0;
  WiFi.mode(WIFI_STA);

  WiFi.begin(WiFiSSID, WiFiPSK);

  while (WiFi.status() != WL_CONNECTED)
  {
    // During the wait blink the status LED rapidly.
    digitalWrite(5, state);
    state ^= 1;
    delay(100);
  }
  
}


void setup_serial()
{
  Serial.begin(115200);
}


double pulseLength = 0;

unsigned long startMeasurementTime = SENSOR_INTERVAL;
unsigned long pulseStart = 0;
unsigned long pulseEnd = 1;

bool pulseHappened = false;

void pulseISR()
{
  // Note: micros will not increment during the ISR.

  // If pulseStart is less than pulseEnd it means that
  // this is the start of a new pulse. Otherwise, this
  // is the end of a pulse.

  if (pulseStart < pulseEnd)
  {
    pulseStart = micros();
  }
  else
  {
    pulseEnd = micros();
    pulseHappened = true;
  }

  // If pulseEnd is nearer to ULONG_MAX than to pulseStart 
  // a rollover has occured. Reset pulseEnd to allow the 
  // logic above to continue working as expected.

  if ((pulseEnd - pulseStart) > (ULONG_MAX / 2))
  {
    pulseEnd = 0;  
  }
  
}



void setup() 
{
  setup_serial();

  setupWifi();
  setupServer();

  for (int i = 0; i < SAMPLE_SIZE; i++)
  {
    samples[i].data = 0;
    samples[i].age = 0;
  }

  pinMode(TRIG, OUTPUT);
  pinMode(PULSE, INPUT);

  // Trigger should initialize to LOW.
  digitalWrite(TRIG, LOW);

  // Attach the pulseISR to the pulse PIN.
  // NOTE: on the ESP8266 all I/O pins can be interrupt pins.
  attachInterrupt(digitalPinToInterrupt(PULSE), pulseISR, CHANGE);

}

void addElement(unsigned long value)
{
  bool added = false;
   
  for (int i = 0; i < SAMPLE_SIZE; i++)
  {
    if (samples[i].age <= 0 && !added)
    {
      samples[i].data = value;
      samples[i].age  = SAMPLE_SIZE-1;
      added = true;
    }
    else
    {
      samples[i].age--;
    }
  }
}

double getDistance()
{
  unsigned long temp[SAMPLE_SIZE];
   
  for (int i = 0; i < SAMPLE_SIZE; i++)
  {
    temp[i] = samples[i].data;
  }

  for (int i = 0; i < SAMPLE_SIZE-1; i++)
  {
    for (int j = i+1; j < SAMPLE_SIZE; j++)
    {
      if (temp[i] > temp[j])
      {
        unsigned long temp2 = temp[i];
        temp[i] = temp[j];
        temp[j] = temp2;  
      }  
    }  
  }
  
  return (temp[5] + temp[6] + temp[7] + temp[8] + temp[9] + temp[10]) / 6.0;
  
}

void loop() 
{
  // Call into the server handler, this allows the web
  // interface to work.
  server.handleClient();

  // Kickoff a measurement every interval.
  if (millis() > startMeasurementTime)
  {
    digitalWrite(TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG, LOW);

    startMeasurementTime += SENSOR_INTERVAL;
  }

  if (pulseHappened)
  {
    sampleTime = millis();
    addElement(pulseEnd - pulseStart);
    pulseLength = (pulseLength * 0.9) + ((pulseEnd - pulseStart) * 0.1);
    pulseHappened = false;

    distance = (pulseLength)  /  148.0;
    distanceMedian = getDistance()   /  148.0;
  }

}


