#include "display.h"
#include <lvgl.h>
#include "sd_card.h"
#include "camera.h"
#include "camera_ui.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "lv_example_btn.h"
//#include "Audio.h"

Display screen;
//Audio audio;

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid     = "xxxxxxxx";
const char* password = "xxxxxxxx";
//const char* ssid     = "xxxxxxxx";
//const char* password = "xxxxxxx";

void setup(){
    /* prepare for possible serial debug */
    Serial.begin( 115200 );
    
    /*** Init drivers ***/   
    sdcard_init();       //Initialize the SD_MMC module
    camera_init();       //Initialize the camera drive
    screen.init();       //Initialize the Screen
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);
  
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      }
    Serial.println("");
    Serial.println("WiFi connected");
    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial.println( LVGL_Arduino );
    Serial.println( "I am LVGL_Arduino" );
    

    lv_example_btn_1();//Two types of button trigger events

    Serial.println( "Setup done" );
}

void loop(){
    screen.routine(); /* let the UI do its work */
    delay( 5 );
    //audio.loop();
}
