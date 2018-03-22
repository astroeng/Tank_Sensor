#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <limits.h>

ESP8266WebServer server(80);

const char WiFiSSID[] = "sensors";
const char WiFiPSK[] = "sensors123";

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
     <script> \n\
       function drawCharts() { \n\
          var jsonData = $.ajax({url: 'data.json', dataType: 'jsonp'}).done(function (results) { \n\
             var dataHumidity = google.visualization.arrayToDataTable([['Sensor', 'Value'], ['Humidity', parseFloat(results[0].h_rp)]]); \n\
             var dataTemperature = google.visualization.arrayToDataTable([['Sensor', 'Value'], ['Temperature', parseFloat(results[0].t_df)]]); \n\
             var dataPressure = google.visualization.arrayToDataTable([['Sensor', 'Value'], ['Pressure', parseFloat(results[0].p_in)]]); \n\
             var optionsHumidity = {width: 800, height: 250, min: 0, max: 100, greenFrom: 40, greedTo: 60, redColor: '#66ff66', redFrom: 35, redTo: 65, minorTicks: 5}; \n\
             var optionsPressure = {width: 800, height: 250, min: 28, max: 31, greenFrom: 29.25, greedTo: 29.75, minorTicks: 5}; \n\
             var optionsTemperature = {width: 800, height: 250, min: 0, max: 100, redColor: '#33adff', redFrom: 0, redTo: 32, yellowColor: '#33ffff', yellowFrom: 32, yellowTo: 35, minorTicks: 5}; \n\
             var chartHumidity = new google.visualization.Gauge($('#gaugeHumidity').get(0)); \n\
             chartHumidity.draw(dataHumidity, optionsHumidity); \n\
             var chartTemperature = new google.visualization.Gauge($('#gaugeTemperature').get(0)); \n\
             chartTemperature.draw(dataTemperature, optionsTemperature); \n\
             var chartPressure = new google.visualization.Gauge($('#gaugePressure').get(0)); \n\
             chartPressure.draw(dataPressure, optionsPressure); \n\
             document.getElementById('time').innerHTML = parseFloat(results[0].time); \n\
        }); \n\
      } \n\
      google.load('visualization', '1', {packages: ['corechart','gauge']}); \n\
      google.setOnLoadCallback(drawCharts); \n\
     </script></head> \n\
     <body><table width=80% align=center> \n\
       <tr> \n\
         <td><div id='time' style='width: 40%;'></div></td> \n\
         <td><div id='distance' style='width: 40%;'></div></td> \n\
      </tr></table><table width=80% align=center> \n\
      <tr> \n\
         <td><p></td> \n\
         <td>Sample Time</td> \n\
         <td>Valid</td> \n\
         <td>Distance</td> \n\
      </tr><tr> \n\
         <td>HCSR04</td> \n\
         <td><div id='timeS0' style='width: 33%;'></div></td> \n\
         <td><div id='validS0' style='width: 33%;'></div></td> \n\
         <td><div id='distanceS0' style='width: 34%;'></div></td> \n\
      </tr></table><script> \n\
       function loadSensorData() { \n\
         var jsonData = $.ajax({url: 'sensor.json', dataType: 'jsonp'}).done(function (results) { \n\
           document.getElementById('timeS0').innerHTML = parseFloat(results[0].time); \n\
           document.getElementById('validS0').innerHTML = parseFloat(results[0].valid); \n\
           document.getElementById('distanceS0').innerHTML = parseFloat(results[0].d_in); \n\
         }); \n\
       } \n\
       loadSensorData(); \n\
     </script> \n\
     </body></html>\n";


double distance = 0.0;
unsigned long sampleTime = 0;

void handleRoot() 
{
  server.send(200, "text/html", webpage);
}

void dataJSON() 
{
  String value = server.arg("callback");

  String message = "typeof " + value + " === 'function' && " + value + 
                   "([{ time:" + String(sampleTime) + "," + 
                       "d_in:" + String(distance) + " }])\n";

  server.send(200, "text/plain", message);
}

String JSON_SensorValues()
{
  String element = "{ time:" + String(sampleTime) + "," + 
                       "d_in:" + String(distance) + " }";
  return element;
}

void sensorJSON() 
{
  String value = server.arg("callback");

  String message = "typeof " + value + " === 'function' && " + value + 
                   "([" + JSON_SensorValues() + "]);\n";

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


#define SENSOR_INTERVAL 1000

#define POWER 0
#define TRIG 12
#define PULSE 15

double pulseLength = 0;

unsigned long startMeasurementTime = SENSOR_INTERVAL;
unsigned long pulseStart = 0;
unsigned long pulseEnd = 1;

bool pulseHappened = false;

void pulseISR()
{
  // Note: micros will not increment during the ISR.

  // If pulseStart is less than pulseEnd it means that
  // this is the start of a new pulse. Otherwise this
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

  pinMode(TRIG, OUTPUT);
  pinMode(PULSE, INPUT);

  // Trigger should initialize to LOW.
  digitalWrite(TRIG, LOW);

  // Attach the pulseISR to the pulse PIN.
  // NOTE: on the ESP8266 all I/O pins can be interrupt pins.
  attachInterrupt(digitalPinToInterrupt(PULSE), pulseISR, CHANGE);

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
    pulseLength = (pulseLength * 0.8) + ((pulseEnd - pulseStart) * 0.2);
    pulseHappened = false;

    distance = (pulseLength )  /  148.0;
  }

}


