#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <MadgwickAHRS.h>

Adafruit_MPU6050 mpu;
Madgwick filter;

const char* ssid = "SwingMonitor";
const char* password = "12345678";

WebServer server(80);

/* swing data */
float swingSpeed = 0;
float peakSpeed = 0;

float pitch, roll, yaw;

bool swingActive = false;

unsigned long swingStart = 0;
unsigned long impactTime = 0;
unsigned long swingEnd = 0;

float tempo = 0;

String webpage = R"rawliteral(

<!DOCTYPE html>
<html>
<head>

<title>Golf Swing Monitor</title>

<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

<style>

body{
font-family:Arial;
background:#0f5132;
color:white;
text-align:center;
}

#speed{
font-size:60px;
}

#peak{
font-size:30px;
}

</style>

</head>

<body>

<h1>Golf Launch Monitor</h1>

<div>Club Speed</div>
<div id="speed">0</div>

<div>Peak Speed</div>
<div id="peak">0</div>

<div>Tempo</div>
<div id="tempo">0</div>

<canvas id="chart"></canvas>

<div>
Pitch:<span id="pitch"></span>
Roll:<span id="roll"></span>
Yaw:<span id="yaw"></span>
</div>

<script>

let ctx=document.getElementById("chart").getContext("2d");

let chart=new Chart(ctx,{
type:"line",
data:{labels:[],datasets:[{label:"Speed",data:[],borderWidth:2}]},
options:{animation:false}
});

function update(){

fetch("/data")
.then(r=>r.json())
.then(data=>{

document.getElementById("speed").innerHTML=data.speed+" mph";
document.getElementById("peak").innerHTML=data.peak+" mph";
document.getElementById("tempo").innerHTML=data.tempo;

document.getElementById("pitch").innerHTML=data.pitch;
document.getElementById("roll").innerHTML=data.roll;
document.getElementById("yaw").innerHTML=data.yaw;

chart.data.labels.push("");
chart.data.datasets[0].data.push(data.speed);

if(chart.data.labels.length>40){
chart.data.labels.shift();
chart.data.datasets[0].data.shift();
}

chart.update();

});

}

setInterval(update,200);

</script>

</body>
</html>

)rawliteral";


void handleRoot(){
server.send(200,"text/html",webpage);
}

void handleData(){

String json="{";

json+="\"speed\":"+String(swingSpeed)+",";
json+="\"peak\":"+String(peakSpeed)+",";
json+="\"tempo\":"+String(tempo,2)+",";
json+="\"pitch\":"+String(pitch)+",";
json+="\"roll\":"+String(roll)+",";
json+="\"yaw\":"+String(yaw);

json+="}";

server.send(200,"application/json",json);
}

void setup(){

Serial.begin(115200);
Wire.begin();

if(!mpu.begin()){
Serial.println("MPU6050 not found");
while(1);
}

mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
mpu.setGyroRange(MPU6050_RANGE_500_DEG);

filter.begin(100);

WiFi.softAP(ssid,password);

server.on("/",handleRoot);
server.on("/data",handleData);
server.begin();

Serial.println(WiFi.softAPIP());

}

void loop(){

server.handleClient();

sensors_event_t accel,gyro,temp;
mpu.getEvent(&accel,&gyro,&temp);

float ax=accel.acceleration.x;
float ay=accel.acceleration.y;
float az=accel.acceleration.z;

float gx=gyro.gyro.x;
float gy=gyro.gyro.y;
float gz=gyro.gyro.z;

filter.updateIMU(gx,gy,gz,ax,ay,az);

pitch=filter.getPitch();
roll=filter.getRoll();
yaw=filter.getYaw();

float totalAccel=sqrt(ax*ax+ay*ay+az*az);

/* swing detection */

if(totalAccel>14 && !swingActive){

swingActive=true;
swingStart=millis();
peakSpeed=0;

}

if(swingActive){

float currentSpeed=totalAccel*2.7;
swingSpeed=currentSpeed;

if(currentSpeed>peakSpeed){
peakSpeed=currentSpeed;
impactTime=millis();
}

}

/* swing end */

if(totalAccel<11 && swingActive){

swingEnd=millis();

float backswing=(impactTime-swingStart)/1000.0;
float downswing=(swingEnd-impactTime)/1000.0;

if(downswing>0){
tempo=backswing/downswing;
}

swingActive=false;

}

delay(20);

}