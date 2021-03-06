#ifdef ESP32
#include <WiFi.h>
#include <AsyncUDP.h>
#include <AsyncTCP.h> //https://github.com/me-no-dev/AsyncTCP
#include <ESPmDNS.h>
#include <Update.h>
// #include <WiFiManager.h> //https://github.com/tzapu/WiFiManager/tree/development
#elif defined(ESP8266)
#include <Hash.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h> //https://github.com/me-no-dev/ESPAsyncTCP
#include <ESPAsyncUDP.h> //https://github.com/me-no-dev/ESPAsyncUDP
#if defined(ESP8266) and (defined(PIO_PLATFORM) or defined(USE_EADNS))
#include <ESPAsyncDNSServer.h> //https://github.com/devyte/ESPAsyncDNSServer
#else
#include <DNSServer.h>
#endif
#else
#error Platform not supported
#endif
#include <ESPAsyncE131.h>        //https://github.com/forkineye/ESPAsyncE131
#include <ESPAsyncWebServer.h>   //https://github.com/me-no-dev/ESPAsyncWebServer
#include <ESPAsyncWiFiManager.h> //https://github.com/alanswx/ESPAsyncWiFiManager
#include <EEPROM.h>
#include "version.h"

#define USE_NEOPIXELS 2 // Use any pin under 32
// #define USE_DOTSTAR //comment above line to use APA102

#ifdef USE_NEOPIXELS
#include <NeoPixelBus.h> //https://github.com/Makuna/NeoPixelBus
#elif defined(USE_DOTSTAR)
#include <Adafruit_DotStar.h> //https://github.com/debsahu/Adafruit_DotStar
#endif

#define WIFI_HTM_GZ_PROGMEM //comment to serve minimized html instead of gziped version
// #define SERIAL_DEBUG        //uncomment to see if E1.31 data is received
//#define SHOW_FPS_SERIAL    //uncomment to see Serial FPS

#define HOSTNAME "E131PixelPusher"
#define HTTP_PORT 80

uint8_t START_UNIVERSE = 1;   // First DMX Universe to listen for
uint8_t END_UNIVERSE = 7;     // Total number of Universes to listen for, starting at START_UNIVERSE max 7 for multicast and 12 for unicast
uint16_t ledCount = 12 * 170; // 170 LEDs per Universe
bool unicast_flag = false;

#if defined(USE_NEOPIXELS) and defined(USE_DOTSTAR)
#error "Choose only one type of LEDs"
#endif

#ifndef WIFI_HTM_GZ_PROGMEM
//size: 3195
static const char index_htm[] PROGMEM = R"=====(
<!DOCTYPE html><html lang="en"><head><meta http-equiv="Content-Type" content="text/html; charset=utf-8"><meta name="viewport" content="width=device-width"><script>function LoadBody(){var e="/data";fetch(e,{method:"GET"}).then(function(e){if(e.ok)return e.json();throw new Error("Network response was not ok.")}).then(function(e){console.log("Success:",JSON.stringify(e)),document.getElementById("pixelct").innerHTML=170*(e.uct-e.su+1),document.getElementById("mode").value=e.mode,document.getElementById("su").value=e.su,document.getElementById("uct").value=e.uct}).catch(function(e){console.error("Error:",e)})}function validateVal(){var e=document.getElementById("su").value,t=document.getElementById("uct").value,n=1190;return"unicast"==document.frmmr.mode.value&&(n=2040),!(e>t||t-e+1<=0||170*(t-e+1)>n)||(e>t&&alert("Starting universe cant be higher than ending universe!"),t-e+1<=0&&alert("Number of Pixels cant be negative/zero!"),170*(t-e+1)>n&&alert("Cant access that many universes!"),!1)}function calcPixels(){document.getElementById("pixelct").innerHTML=170*(document.getElementById("uct").value-document.getElementById("su").value+1)}function checkUnicast(){"unicast"==document.frmmr.mode.value?(document.getElementById("su").max="12",document.getElementById("uct").max="12"):(document.getElementById("su").max="7",document.getElementById("uct").max="7")}</script><style>.subbt,body{text-align:center;color:#fff}.subbt,.title,a,body{color:#fff}.github a,.subbt,a{text-decoration:none}body{width:100%;height:100%;margin:auto;background-color:#c6b3d4;font-family:sans-serif;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}#wrapper{width:250px;height:270px;border:2px solid #8531C6;border-radius:8px;margin:25px auto auto;background-color:#580797}.subbt{margin:20px;background-color:#008CBA;border:none;padding:10px 30px;display:inline-block;font-size:20px;border-radius:8px}.title{line-height:24px;display:block;padding:2px 0}.form{width:100%;padding:10px;margin:8px 0;box-sizing:border-box}.github{margin-top:10px}.github a{color:#EF5989;font-weight:700;font-size:16px}</style></head><body onload="LoadBody()"><div id="wrapper"><h4 class="title">E1.31 Pixel Pusher</h4><form name="frmmr" class="form" method="POST" action="/updateparams" onsubmit="return validateVal()">Pixels: <span id="pixelct"></span><br>Mode:<select id="mode" name="mode" onblur="checkUnicast()"><option value="unicast">UNICAST</option><option value="multicast" selected>MULTICAST</option></select><br>Start Universe: <input id="su" name="su" type="number" placeholder="Starting universe" max="12" min="1" onkeyup="this.value=this.value.replace(/[^\d]/,&#34;&#34;)" onblur="calcPixels()" onclick="calcPixels()"><br>End Universe : <input id="uct" name="uct" type="number" placeholder="Ending universe" max="12" min="1" onkeyup="this.value=this.value.replace(/[^\d]/,&#34;&#34;)" onblur="calcPixels()" onclick="calcPixels()"><br><input class="subbt" type="submit" value="Update"></form></div><br><a href="/update">Update Firmware?</a><br><div class="github"><a href="https://github.com/debsahu/E131_PixelPusher">E131_PixelPusher by @debsahu</a></div><script></script></body></html>
)=====";
#else
//gziped: 1385 (~57% compression)
#define index_htm_gz_len 1385
static const uint8_t index_htm_gz[] PROGMEM = {
    0x1f, 0x8b, 0x08, 0x00, 0xef, 0xa0, 0x4e, 0x5c, 0x00, 0x03, 0xc5, 0x57,
    0x6d, 0x53, 0xe3, 0x36, 0x10, 0xfe, 0x2b, 0x42, 0x4c, 0x99, 0xa4, 0x17,
    0xdb, 0x31, 0x2f, 0x07, 0x38, 0x71, 0xae, 0x77, 0x34, 0xd7, 0x5e, 0xe7,
    0x8e, 0x63, 0x06, 0xe8, 0x4c, 0xa7, 0x6f, 0x23, 0xdb, 0x9b, 0x58, 0x8d,
    0x2d, 0xb9, 0x92, 0x4c, 0xc8, 0x01, 0xff, 0xbd, 0x2b, 0xcb, 0x0e, 0x81,
    0x3b, 0xae, 0xf4, 0x53, 0xbf, 0x10, 0x4b, 0xda, 0xb7, 0x67, 0x77, 0xf5,
    0xac, 0x18, 0x6f, 0x7d, 0xff, 0xf1, 0xe4, 0xe2, 0x97, 0xb3, 0x29, 0xc9,
    0x4d, 0x59, 0x4c, 0xc6, 0xf6, 0x2f, 0x29, 0x98, 0x98, 0xc7, 0x14, 0x04,
    0xc5, 0x35, 0xb0, 0x6c, 0x32, 0x2e, 0xc1, 0x30, 0x14, 0x30, 0x95, 0x07,
    0x7f, 0xd7, 0xfc, 0x2a, 0xa6, 0x27, 0x52, 0x18, 0x10, 0xc6, 0xbb, 0x58,
    0x55, 0x40, 0x49, 0xea, 0x56, 0x31, 0x35, 0x70, 0x6d, 0x02, 0x6b, 0x62,
    0x44, 0xd2, 0x9c, 0x29, 0x0d, 0x26, 0xae, 0xcd, 0xcc, 0x3b, 0xa2, 0xad,
    0x09, 0xc1, 0x4a, 0x88, 0xe9, 0x15, 0x87, 0x65, 0x25, 0x95, 0xd9, 0x50,
    0x5c, 0xf2, 0xcc, 0xe4, 0x71, 0x06, 0x57, 0x3c, 0x05, 0xaf, 0x59, 0xa0,
    0x8a, 0x4e, 0x15, 0xaf, 0xcc, 0x64, 0x56, 0x8b, 0xd4, 0x70, 0x29, 0xc8,
    0x7b, 0xc9, 0xb2, 0x37, 0x32, 0x5b, 0xf5, 0xfa, 0x37, 0x57, 0x4c, 0x11,
    0xb4, 0x14, 0x64, 0xcc, 0x30, 0x3a, 0x9a, 0x81, 0x49, 0xf3, 0x1e, 0x0c,
    0x6e, 0xd0, 0x49, 0x2e, 0xb3, 0x88, 0xfe, 0x30, 0xbd, 0xa0, 0x77, 0x7d,
    0xdf, 0xe4, 0x20, 0x7a, 0x9d, 0x7a, 0x0f, 0xfa, 0x37, 0x7c, 0xd6, 0x03,
    0x5f, 0x2e, 0xfa, 0x0a, 0x4c, 0xad, 0x04, 0x01, 0xff, 0x2f, 0x8d, 0x07,
    0xfd, 0x91, 0xc9, 0x95, 0x5c, 0x12, 0x01, 0x4b, 0x32, 0x55, 0x4a, 0xaa,
    0x1e, 0x3d, 0x05, 0xb3, 0x94, 0x6a, 0x41, 0x14, 0xe8, 0x4a, 0x0a, 0x0d,
    0x64, 0xc9, 0x34, 0x11, 0xd2, 0x10, 0xb9, 0xf0, 0x69, 0xff, 0x4b, 0xa6,
    0x11, 0x8a, 0x96, 0x05, 0xf8, 0x85, 0x9c, 0xf7, 0xe8, 0x79, 0x9d, 0xa6,
    0xa0, 0x75, 0x44, 0x07, 0x3f, 0x9d, 0x7f, 0x3c, 0xf5, 0xb5, 0x51, 0x5c,
    0xcc, 0xf9, 0x6c, 0x85, 0x82, 0xfd, 0x41, 0x26, 0xd3, 0xba, 0x44, 0xd4,
    0xfe, 0x1c, 0xcc, 0xb4, 0x00, 0xfb, 0xf9, 0x66, 0xf5, 0x2e, 0xeb, 0xd1,
    0x8a, 0x5f, 0x43, 0x91, 0x1a, 0xda, 0xf7, 0xb9, 0x10, 0xa0, 0x7e, 0xbc,
    0xf8, 0xf0, 0x3e, 0x0e, 0x0f, 0x87, 0xdf, 0x62, 0xcc, 0x75, 0x6a, 0x3c,
    0xf0, 0x75, 0xfd, 0x22, 0xfc, 0x8a, 0x7e, 0x29, 0x33, 0x40, 0xe5, 0x2b,
    0x56, 0xd4, 0x10, 0x83, 0x6f, 0x97, 0x4f, 0x0b, 0xeb, 0x7a, 0x43, 0x54,
    0xd7, 0x4f, 0x0b, 0xd6, 0x4d, 0x44, 0x9d, 0x24, 0xae, 0x10, 0x7d, 0xca,
    0x6c, 0xc2, 0xbf, 0x04, 0x1f, 0x5c, 0xfe, 0x9a, 0x34, 0x22, 0x7c, 0xc0,
    0x5c, 0xdd, 0xad, 0x0b, 0x88, 0x46, 0x38, 0x56, 0x0c, 0x7e, 0x66, 0xc5,
    0xba, 0x86, 0xcf, 0x08, 0x70, 0x60, 0x9e, 0x96, 0xda, 0x88, 0x6e, 0x20,
    0xe2, 0x30, 0x3c, 0x1e, 0x8e, 0x5c, 0x71, 0x69, 0x2d, 0x78, 0xca, 0xb4,
    0xa1, 0xf1, 0xbd, 0xf2, 0x4c, 0x95, 0xa5, 0x6a, 0xf2, 0xe2, 0x34, 0x76,
    0x76, 0x7a, 0x22, 0xde, 0x1d, 0xee, 0x0f, 0xfb, 0x83, 0xad, 0x1e, 0x4c,
    0xcc, 0xed, 0x2d, 0x66, 0xf9, 0x45, 0x38, 0x8e, 0x87, 0xb7, 0xb7, 0x4d,
    0xe2, 0x9b, 0x65, 0x7f, 0x22, 0xfa, 0xb7, 0xb7, 0xf6, 0x7c, 0x67, 0x87,
    0x15, 0xa0, 0x0c, 0xd6, 0xd7, 0x30, 0x65, 0xb0, 0xa4, 0x04, 0x9d, 0x5c,
    0x01, 0xf6, 0x39, 0x49, 0x99, 0x30, 0x24, 0x01, 0x92, 0xf3, 0x79, 0x0e,
    0x8a, 0x98, 0x9c, 0x61, 0x7b, 0x89, 0x6c, 0x53, 0x66, 0x8b, 0xf6, 0x07,
    0x9d, 0xfd, 0xb5, 0xa5, 0xd3, 0xba, 0x4c, 0x50, 0x5e, 0xce, 0xc8, 0x99,
    0xad, 0xbe, 0x5e, 0x1b, 0x12, 0x30, 0x67, 0x06, 0x15, 0x83, 0x4f, 0xa0,
    0xa4, 0x55, 0x7d, 0x10, 0xd0, 0x5a, 0xff, 0xc4, 0x8a, 0xb3, 0xa6, 0xdb,
    0xac, 0x53, 0x43, 0x4a, 0x26, 0x56, 0x6b, 0x97, 0xda, 0x2a, 0x6e, 0x85,
    0x1b, 0x25, 0x48, 0x59, 0x91, 0x3a, 0x4f, 0x58, 0x81, 0xff, 0xde, 0x89,
    0xcf, 0xa9, 0x83, 0xf7, 0x8c, 0x92, 0xbe, 0x78, 0x10, 0x53, 0x0e, 0xe9,
    0xe2, 0xd2, 0x95, 0x0b, 0xa3, 0x7a, 0x4e, 0xe5, 0x5e, 0x3d, 0x1d, 0x49,
    0xe3, 0xa4, 0x64, 0xd7, 0x31, 0x0d, 0x77, 0xe9, 0xbf, 0xb5, 0x75, 0x27,
    0xd7, 0x8f, 0x9e, 0x63, 0xf0, 0xf0, 0x79, 0xf6, 0x0e, 0x91, 0x21, 0xc6,
    0x41, 0xcb, 0x5e, 0x63, 0x6d, 0x56, 0x05, 0x4c, 0xf0, 0x8a, 0x25, 0x89,
    0x19, 0x24, 0x48, 0x5f, 0x37, 0x96, 0x25, 0x3d, 0xbc, 0x0a, 0x73, 0x11,
    0xa5, 0xa8, 0x0f, 0x6a, 0x94, 0xca, 0x02, 0xaf, 0xcb, 0xf6, 0x6c, 0x36,
    0xbb, 0x6b, 0x05, 0x7d, 0xc3, 0x4d, 0x01, 0x03, 0xe6, 0x34, 0x36, 0xcf,
    0xe7, 0xdc, 0xe4, 0x75, 0x42, 0xd8, 0xa0, 0x95, 0x64, 0xce, 0x5e, 0x06,
    0xa9, 0x54, 0xcc, 0xe6, 0x33, 0x12, 0x52, 0xc0, 0x5d, 0xa3, 0xd7, 0x30,
    0x69, 0x14, 0x0e, 0x87, 0xdf, 0x8c, 0x72, 0xc0, 0xce, 0x34, 0xee, 0xbb,
    0x64, 0x6a, 0xce, 0x45, 0xc4, 0x6a, 0x23, 0x47, 0x09, 0x4b, 0x17, 0x73,
    0x25, 0x6b, 0x91, 0x79, 0xad, 0x97, 0xf4, 0x65, 0xb2, 0x97, 0xed, 0x8f,
    0x66, 0xc8, 0xcd, 0xde, 0x8c, 0x95, 0xbc, 0x58, 0x45, 0x9a, 0x09, 0xed,
    0x69, 0x50, 0x7c, 0x36, 0xf2, 0x96, 0x90, 0x2c, 0xb8, 0xf1, 0x12, 0x79,
    0xed, 0x69, 0xfe, 0x09, 0x1b, 0x3c, 0x4a, 0xa4, 0xca, 0x40, 0xd9, 0x9d,
    0x91, 0x57, 0xca, 0x4f, 0x4f, 0x1c, 0x7d, 0x71, 0xf7, 0x6e, 0x7b, 0xa9,
    0x58, 0x55, 0x81, 0x6a, 0x43, 0xdd, 0x3d, 0x18, 0x56, 0xd7, 0x5d, 0xac,
    0xbb, 0x87, 0x76, 0xe1, 0x84, 0xa3, 0xdd, 0xea, 0x9a, 0x20, 0xc3, 0xf0,
    0x8c, 0x6c, 0x1f, 0x1d, 0xec, 0x85, 0x27, 0x2f, 0xdb, 0x03, 0x4f, 0xb1,
    0x8c, 0xd7, 0x3a, 0x3a, 0x42, 0xd1, 0x16, 0xd7, 0xee, 0x01, 0xca, 0x5a,
    0x70, 0xe4, 0x09, 0x84, 0x07, 0x47, 0xc3, 0xc3, 0xe3, 0xc3, 0x36, 0xd5,
    0x37, 0x9d, 0x56, 0xe3, 0xec, 0x33, 0xd9, 0xe1, 0xf0, 0xe8, 0xe4, 0xcd,
    0xeb, 0x2e, 0x0a, 0x9b, 0xda, 0x51, 0xc5, 0x32, 0x7b, 0xb1, 0x31, 0x97,
    0xe8, 0x67, 0xcf, 0xaa, 0x65, 0x5c, 0x57, 0x05, 0x5b, 0x45, 0x5c, 0x14,
    0x5c, 0x80, 0x97, 0x14, 0x32, 0x5d, 0xb8, 0x04, 0x22, 0x62, 0x68, 0x4d,
    0x3f, 0x0e, 0xf7, 0xce, 0xd5, 0xf8, 0xa6, 0x51, 0xe9, 0x20, 0xef, 0x6f,
    0x58, 0x73, 0x66, 0x3a, 0x6f, 0x36, 0x01, 0xc3, 0x3b, 0x7f, 0x26, 0x55,
    0xb9, 0x59, 0xd7, 0xcd, 0x60, 0xba, 0x04, 0x1c, 0x59, 0xd1, 0x27, 0x12,
    0xde, 0xf6, 0x4f, 0x8b, 0xda, 0x33, 0xb2, 0x6a, 0x54, 0xef, 0xfb, 0xaa,
    0x6b, 0xb6, 0xe9, 0xdb, 0x83, 0xe3, 0xa3, 0x63, 0x87, 0x62, 0xe9, 0xa2,
    0x3b, 0x1c, 0x0e, 0x37, 0x50, 0x85, 0x2f, 0x51, 0x0d, 0xfb, 0xbc, 0xe9,
    0xef, 0x71, 0xe0, 0x5e, 0x0a, 0xb6, 0xed, 0x88, 0x14, 0x05, 0x8e, 0xea,
    0x98, 0xde, 0x0f, 0x6c, 0x9c, 0xe6, 0x19, 0xbf, 0x22, 0x1c, 0x37, 0xdb,
    0x82, 0xdb, 0xb7, 0xc5, 0x3e, 0x49, 0x0b, 0xa6, 0x35, 0xbe, 0x1b, 0x6c,
    0x22, 0xe8, 0x64, 0x1a, 0xfa, 0x7b, 0xa1, 0x23, 0x42, 0x72, 0x56, 0x6b,
    0x24, 0x52, 0x34, 0xbb, 0x3f, 0x19, 0x5b, 0xcc, 0xed, 0xdb, 0xa1, 0x21,
    0x02, 0xda, 0xe9, 0xd9, 0x03, 0x4a, 0xdc, 0xd4, 0x8f, 0xe9, 0xd9, 0xc7,
    0xf3, 0x0b, 0x8a, 0x64, 0x68, 0x2f, 0x01, 0xbe, 0x0d, 0xea, 0xca, 0xce,
    0x9a, 0x8a, 0x29, 0x56, 0x6a, 0x8a, 0x31, 0x61, 0xb5, 0x4b, 0x8e, 0x6f,
    0x8d, 0x76, 0xf8, 0x3f, 0x18, 0x46, 0x74, 0xe2, 0x48, 0x31, 0x22, 0x63,
    0x5d, 0x21, 0x75, 0xdb, 0x40, 0x3b, 0x12, 0x44, 0x6c, 0x76, 0x0f, 0xb1,
    0xa9, 0xc9, 0x07, 0x64, 0xa0, 0x68, 0xac, 0xa1, 0x80, 0xd4, 0x34, 0x42,
    0xcd, 0xcc, 0x6d, 0x63, 0x73, 0xdf, 0x52, 0x24, 0x45, 0xad, 0x62, 0xfa,
    0x90, 0xd5, 0xd0, 0x8a, 0xac, 0xba, 0x21, 0x88, 0x93, 0x74, 0x4d, 0x72,
    0x93, 0xcb, 0xd3, 0x77, 0x27, 0xaf, 0xcf, 0x2f, 0xc6, 0x81, 0x3b, 0x7f,
    0x2c, 0x57, 0xd6, 0x85, 0x71, 0x92, 0xc4, 0xb9, 0x85, 0x6c, 0xf2, 0xe1,
    0xf2, 0xfd, 0xc5, 0x23, 0xa5, 0xc0, 0x1d, 0x36, 0x41, 0x36, 0xd3, 0x89,
    0x5c, 0xb6, 0x33, 0x00, 0x21, 0x71, 0x51, 0xd5, 0x2e, 0x5c, 0xe4, 0xb2,
    0x36, 0x58, 0xfb, 0x65, 0xf0, 0xf5, 0x16, 0x53, 0xd1, 0x8c, 0x20, 0x4a,
    0xb0, 0xe7, 0x52, 0xc8, 0x65, 0x81, 0x9d, 0x12, 0x7f, 0x3e, 0xe1, 0x30,
    0xcd, 0x2d, 0x57, 0x92, 0x92, 0x63, 0x7a, 0x43, 0x8b, 0x74, 0x01, 0xab,
    0xba, 0xc2, 0xf2, 0xe5, 0x5c, 0xb7, 0x2f, 0x84, 0xfb, 0x4f, 0x5f, 0x41,
    0x63, 0xb1, 0x17, 0xfc, 0xfa, 0xc7, 0x6f, 0xd9, 0xef, 0xc1, 0x60, 0x67,
    0x7b, 0x6f, 0x7f, 0xd4, 0xfc, 0xe9, 0x6f, 0x64, 0x69, 0x63, 0x1e, 0xd9,
    0xdd, 0xb4, 0xe0, 0xe9, 0xe2, 0xd1, 0x76, 0x03, 0x6a, 0x2a, 0xb2, 0x35,
    0x24, 0xf2, 0x00, 0x93, 0x65, 0xde, 0x16, 0x54, 0xf3, 0xf9, 0x15, 0x54,
    0xd3, 0x87, 0x13, 0xf9, 0xff, 0xc6, 0xd4, 0x82, 0x68, 0x9b, 0xb9, 0x61,
    0xa3, 0x2e, 0x7c, 0xd7, 0xac, 0xb4, 0xeb, 0x82, 0xcb, 0xa6, 0x97, 0x6d,
    0x2b, 0xda, 0x96, 0xc7, 0x1f, 0xbc, 0x4d, 0xce, 0x04, 0xbe, 0xcb, 0x15,
    0xcc, 0xd6, 0xed, 0x8e, 0xfd, 0xd4, 0xfc, 0x92, 0xb7, 0x5c, 0x95, 0x4b,
    0xa6, 0xe0, 0xd5, 0x38, 0x60, 0x4e, 0xd2, 0x5e, 0xc0, 0xd6, 0x95, 0xbb,
    0xeb, 0xf4, 0x5e, 0xdb, 0xbe, 0xed, 0x75, 0x14, 0x04, 0xee, 0xc0, 0x4f,
    0x65, 0x19, 0x64, 0x90, 0x68, 0x96, 0xd7, 0xc1, 0x34, 0xdc, 0x0b, 0xff,
    0x6c, 0xe2, 0x76, 0x57, 0xd2, 0x5e, 0xd3, 0x87, 0x3b, 0x24, 0x59, 0x91,
    0xef, 0x5a, 0xf1, 0xc6, 0x9b, 0x8b, 0xae, 0x1b, 0x80, 0xeb, 0x49, 0x18,
    0x58, 0x72, 0xb0, 0x4c, 0x61, 0xff, 0xd3, 0xf8, 0x07, 0xce, 0x1f, 0x2f,
    0x41, 0x79, 0x0c, 0x00, 0x00};
#endif

//ESPAsyncE131 pointer
ESPAsyncE131 *e131;
AsyncWebServer server(HTTP_PORT);

#ifdef ESP32
#if defined(USE_NEOPIXELS) and defined(ESP32)
#define PIN USE_NEOPIXELS //Use any pin under 32
NeoEsp32I2s1800KbpsMethod *dma;
// NeoEsp32RmtWs2813Method *dma;
uint8_t *pixel = (uint8_t *)malloc(ledCount);
;
#endif

#if defined(USE_DOTSTAR) and defined(ESP32)
#define PIN_CLK SCK
#define PIN_DATA MOSI
Adafruit_DotStar *dma;
#endif

#elif defined(ESP8266)
#if defined(USE_NEOPIXELS) and defined(ESP8266)
NeoEsp8266Dma800KbpsMethod *dma; //uses RX/GPIO3 pin
uint8_t *pixel;
#endif

#if defined(USE_DOTSTAR) and defined(ESP8266)
#define PIN_CLK SCK
#define PIN_DATA MOSI
Adafruit_DotStar *dma;
#endif
#endif

#if defined(ESP8266) and (defined(PIO_PLATFORM) or defined(USE_EADNS))
AsyncDNSServer dns;
#else
DNSServer dns;
#endif

struct
{
    bool unicast = false;
    uint16_t startUniverse = START_UNIVERSE;
    uint16_t endUniverse = END_UNIVERSE;
} config;

int eeprom_addr = 0;

#ifdef SHOW_FPS_SERIAL
uint64_t frameCt = 0;
uint64_t PM = 0;
float interval = 10 * 1000.0; // 10s
#endif

bool shouldReboot = false;
bool updatedParams = false;
const char update_html[] PROGMEM = "<!DOCTYPE html><html lang=\"en\"><head><title>Firmware Update</title><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"favicon.ico\"></head><body><h3>Update Firmware</h3><br><form method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"update\"> <input type=\"submit\" value=\"Update\"></form></body></html>";

void initDMA(void)
{
    if (dma)
    {
        delete dma;
#if defined(USE_NEOPIXELS) and defined(ESP32)
        delete pixel;
    }
    dma = new NeoEsp32I2s1800KbpsMethod(PIN, ledCount, 3);
    //dma = new NeoEsp32RmtWs2813Method(PIN, ledCount, 3);
    dma->Initialize();
    pixel = (uint8_t *)malloc(ledCount);
    memset(pixel, 0, sizeof(pixel));
#endif
#if defined(USE_NEOPIXELS) and defined(ESP8266)
    delete pixel;
}
dma = new NeoEsp8266Dma800KbpsMethod(ledCount, 3);
dma->Initialize();
pixel = (uint8_t *)malloc(ledCount);
memset(pixel, 0, sizeof(pixel));
#endif
#ifdef USE_DOTSTAR
}
dma = new Adafruit_DotStar(ledCount, DOTSTAR_BRG);
dma->begin();
#endif
}

void initE131(void)
{
    uint8_t TOTAL_UNIVERSES = (END_UNIVERSE - START_UNIVERSE + 1);
    ledCount = TOTAL_UNIVERSES * 170;
    if (e131)
        delete e131;
    initDMA();
    e131 = new ESPAsyncE131(TOTAL_UNIVERSES);
    if (e131->begin((unicast_flag) ? E131_UNICAST : E131_MULTICAST, START_UNIVERSE, TOTAL_UNIVERSES)) // Listen via Unicast/Multicast
        Serial.println(F(">>> Listening for E1.31 data..."));
    else
        Serial.println(F(">>> e131.begin failed :("));
}

void readEEPROM(void)
{
    char eeprominit = char(EEPROM.read(eeprom_addr));
    if (eeprominit != 'w')
    {
        EEPROM.write(eeprom_addr, 'w');
        writeEEPROM();
    }
    else
    {
        EEPROM.get(eeprom_addr + 1, config);
        unicast_flag = config.unicast;
        START_UNIVERSE = config.startUniverse;
        END_UNIVERSE = config.endUniverse;
#ifdef SERIAL_DEBUG
        Serial.printf("READ>> Mode: %s Start Universe: %d End Universe: %d\n", (unicast_flag) ? "unicast" : "multicast", START_UNIVERSE, END_UNIVERSE);
#endif
    }
}

void writeEEPROM(void)
{
    config.unicast = unicast_flag;
    config.startUniverse = START_UNIVERSE;
    config.endUniverse = END_UNIVERSE;
#ifdef SERIAL_DEBUG
    Serial.printf("WRITE>> Mode: %s Start Universe: %d End Universe: %d\n", (unicast_flag) ? "unicast" : "multicast", START_UNIVERSE, END_UNIVERSE);
#endif
    EEPROM.put(eeprom_addr + 1, config);
    EEPROM.commit();
}

void setup()
{
    EEPROM.begin(512);
    Serial.begin(115200);
    delay(10);

    Serial.println();
    readEEPROM();

    char NameChipId[64] = {0}, chipId[9] = {0};

#ifdef USE_DOTSTAR
    Serial.print("Hardware SPI //DATA PIN: ");
    Serial.println(MOSI); //GPIO23:ESP32 GPIO13:ESP8266
    Serial.print("Hardware SPI //CLOCK PIN: ");
    Serial.println(SCK); //GPIO18:ESP32 GPIO14:ESP8266
#endif
#ifdef ESP32
    snprintf(chipId, sizeof(chipId), "%08x", (uint32_t)ESP.getEfuseMac());
    snprintf(NameChipId, sizeof(NameChipId), "%s_%08x", HOSTNAME, (uint32_t)ESP.getEfuseMac());

    WiFi.mode(WIFI_STA); // Make sure you're in station mode
    WiFi.setHostname(const_cast<char *>(NameChipId));
    // WiFiManager wifiManager;
    AsyncWiFiManager wifiManager(&server, &dns);
#else
        snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
        snprintf(NameChipId, sizeof(NameChipId), "%s_%06x", HOSTNAME, ESP.getChipId());

        WiFi.mode(WIFI_STA); // Make sure you're in station mode
        WiFi.hostname(const_cast<char *>(NameChipId));
        AsyncWiFiManager wifiManager(&server, &dns); //Local intialization. Once its business is done, there is no need to keep it around
#endif
    wifiManager.setConfigPortalTimeout(180); //sets timeout until configuration portal gets turned off, useful to make it all retry or go to sleep in seconds
    if (!wifiManager.autoConnect(NameChipId))
    {
        Serial.println("Failed to connect and hit timeout");
        ESP.restart();
    }
    Serial.println("");
    Serial.print(F(">>> Connected with IP: "));
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
#ifdef WIFI_HTM_GZ_PROGMEM
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_htm_gz, index_htm_gz_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
#else
            request->send_P(200, "text/html", index_htm);
#endif
    });
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"ct\":" + String(ledCount) + ",\"mode\":\"" + String((unicast_flag) ? "unicast" : "multicast") + "\",\"su\":" + String(START_UNIVERSE) + ",\"uct\":" + String(END_UNIVERSE - START_UNIVERSE + 1) + "}");
    });
    server.on("/updateparams", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("mode", true))
        {
            unicast_flag = (request->getParam("mode", true)->value() == "unicast") ? true : false;
            updatedParams = true;
        }
        if (request->hasParam("su", true))
        {
            START_UNIVERSE = constrain(request->getParam("su", true)->value().toInt(), 1, (unicast_flag) ? 12 : 7);
            updatedParams = true;
        }
        if (request->hasParam("uct", true))
        {
            END_UNIVERSE = constrain(request->getParam("uct", true)->value().toInt(), 1, (unicast_flag) ? 12 : 7);
            updatedParams = true;
        }
        request->send(200, "text/html", "<META http-equiv='refresh' content='3;URL=/'><body align=center>LED Count: " + String((END_UNIVERSE - START_UNIVERSE + 1) * 170) + ", Mode: " + String((unicast_flag) ? "unicast" : "multicast") + ", Starting Universe: " + String(START_UNIVERSE) + ", Universe Count: " + String(END_UNIVERSE - START_UNIVERSE + 1) + "</body>");
    });
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", SKETCH_VERSION);
    });
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", update_html);
        request->send(response);
    });
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
        shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", shouldReboot ? "<META http-equiv='refresh' content='15;URL=/'>Update Success, rebooting..." : "FAIL");
        response->addHeader("Connection", "close");
        request->send(response); }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
          if (!filename.endsWith(".bin")) {
                return;
            }
            if(!index){
                if(Serial) Serial.printf("Update Start: %s\n", filename.c_str());
#ifdef ESP32
                uint32_t maxSketchSpace = len; // for ESP32 you just supply the length of file
#elif defined(ESP8266)
                Update.runAsync(true); // There is no async for ESP32
                uint32_t maxSketchSpace = ((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
#endif
                if(!Update.begin(maxSketchSpace)) {
                if(Serial) Update.printError(Serial);
                }
            }
            if(!Update.hasError()){
                if(Update.write(data, len) != len) {
                if(Serial) Update.printError(Serial);
                }
            }
            if(final){
                if(Update.end(true)) 
                if(Serial) Serial.printf("Update Success: %uB\n", index+len);
                else {
                if(Serial) Update.printError(Serial);
            }
        } });

    if (MDNS.begin(NameChipId))
    {
        MDNS.addService("http", "tcp", HTTP_PORT);
        MDNS.addService("e131", "udp", E131_DEFAULT_PORT);
#ifndef ARDUINO_ESP8266_RELEASE_2_4_2
        MDNS.addServiceTxt("e131", "udp", "CID", String(chipId));
        MDNS.addServiceTxt("e131", "udp", "Model", "E131_PixelPusher");
        MDNS.addServiceTxt("e131", "udp", "Manuf", "debsahu");
#endif
        //MDNS.setInstanceName("E1.31 PixelPusher"); //Adding this line decreases FPS by 4 times
        Serial.printf(">>> MDNS Started: http://%s.local/\n", NameChipId);
    }
    else
    {
        Serial.println(F(">>> Error setting up mDNS responder <<<"));
    }

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    server.begin();
#if !defined(SHOW_FPS_SERIAL) and !defined(SERIAL_DEBUG)
    Serial.end();
#endif
    initE131();
    delay(1000);
}

void loop()
{
#ifdef ESP8266
    MDNS.update();
#endif

    if (updatedParams)
    {
        initE131();
        writeEEPROM();
        updatedParams = false;
    }

    if (!e131->isEmpty())
    {
        e131_packet_t packet;
        e131->pull(&packet); // Pull packet from ring buffer

        uint16_t CURRENT_UNIVERSE = htons(packet.universe);
        uint8_t *data = packet.property_values + 1;

        if (CURRENT_UNIVERSE < START_UNIVERSE || CURRENT_UNIVERSE > END_UNIVERSE)
            return; //async will take care about filling the buffer

#ifdef SERIAL_DEBUG
        Serial.printf("Universe %u / %u Channels | Packet#: %u / Errors: %u / CH1: %u\n",
                      htons(packet.universe),                 // The Universe for this packet
                      htons(packet.property_value_count) - 1, // Start code is ignored, we're interested in dimmer data
                      e131->stats.num_packets,                // Packet counter
                      e131->stats.packet_errors,              // Packet error counter
                      packet.property_values[1]);             // Dimmer data for Channel 1
#endif

        uint16_t multipacketOffset = (CURRENT_UNIVERSE - START_UNIVERSE) * 170; //if more than 170 LEDs (510 channels), client will send in next higher universe
        if (ledCount <= multipacketOffset)
            return;
        uint16_t len = (170 + multipacketOffset > ledCount) ? (ledCount - multipacketOffset) * 3 : 510;
        //memcpy(pixel + multipacketOffset * 3, data, len); // Burden on source to send in correct color order
        //memcpy(dma->getPixels() + multipacketOffset * 3, data, len);
#ifdef USE_NEOPIXELS
        memmove(&pixel[multipacketOffset * 3], data, len);
#elif defined(USE_DOTSTAR)
        memmove(&dma->getPixels()[multipacketOffset * 3], data, len);
#endif
    }

#ifdef USE_NEOPIXELS
    if (dma->IsReadyToUpdate())
#endif
    {
#ifdef USE_NEOPIXELS
        memcpy(dma->getPixels(), pixel, dma->getPixelsSize());
        dma->Update(true);
#elif defined(USE_DOTSTAR)
            dma->show();
#endif
#ifdef SHOW_FPS_SERIAL
        frameCt++;
#endif
    }

#ifdef SHOW_FPS_SERIAL
    if (millis() - PM >= interval)
    {
        PM = millis();
        Serial.printf("FPS: %.2f\n", interval / frameCt);
        frameCt = 0;
    }
#endif

    if (shouldReboot)
    {
#ifdef SHOW_FPS_SERIAL
        Serial.println("Rebooting...");
#endif
        delay(100);
        ESP.restart();
    }
}
