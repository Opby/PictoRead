//#include "camera_ui.h"
#include "camera.h"
#include "sd_card.h"
//#include "main_ui.h"
#include "lv_img.h"
#include <ArduinoJson.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include "FS.h"
#include "SD_MMC.h"
#include "SPI.h"
#include "Audio.h"
#include "lvgl.h"
#include "lv_example_btn.h"

#define SD_MMC_CMD 38 
#define SD_MMC_CLK 39 
#define SD_MMC_D0  40
#define I2S_BCLK 42 
#define I2S_DOUT 41 
#define I2S_LRC 14

Audio audio;
int music_button_state = 0;   //UI Button status
int music_task_flag = 0;       //music thread running flag
TaskHandle_t musicTaskHandle;  //music thread task handle

camera_fb_t *fb = NULL;           //data structure of camera frame buffer
camera_fb_t *fb_buf = NULL;

// Server URL for uploading images and receiving text
const char* serverUrl = "https://nathang2022--readbuddy-backend-endpoint.modal.run/process_image";
// const char* serverUrl = "http://192.168.86.36:8000/process_image";
unsigned long lastCaptureTime = 0; // Last shooting time
int imageCount = 1;                // File Counter

// SD card write file
void writeFile(fs::FS &fs, const char * path, uint8_t * data, size_t len){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.write(data, len) == len){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

// Save pictures to SD card
void photo_save(const char * fileName) {
  // Take a photo
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to get camera frame buffer");
    return;
  }
  // Save photo to file
  writeFile(SD_MMC, fileName, fb->buf, fb->len);
  
  // Release image buffer
  //esp_camera_fb_return(fb);

  Serial.println("Photo saved to file");
}



//Initialize the audio interface
int music_iis_init(void) {
  return audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
}
//Set the volume: 0-21
void music_set_volume(int volume) {
  audio.setVolume(volume);
}
//Query volume
int music_read_volume(void) {
  return audio.getVolume();
}
//load the mp3
void music_load_mp3(char *name) {
  audio.connecttoFS(SD_MMC, name);
}
//Pause/play the music
void music_pause_resume(void) {
  audio.pauseResume();
}
//Stop the music
void music_stop(void) {
  audio.stopSong();
}
//Whether the music is running
bool music_is_running(void) {
  return audio.isRunning();
}
//Gets how long the music player has been playing
long music_get_total_playing_time(void) {
  return (long)audio.getTotalPlayingTime() / 1000;
}
//Obtain the playing time of the music file
long music_get_file_duration(void) {
  return (long)audio.getAudioFileDuration();
}
//Set play position
bool music_set_play_position(int second) {
  return audio.setAudioPlayPosition((uint16_t)second);
}
//Gets the current playing time of the music
long music_read_play_position(void) {
  return audio.getAudioCurrentTime();
}
//Non-blocking music execution function
void music_loop(void) {
  audio.loop();
}

//music player thread
void loopTask_music(void *pvParameters) {
  Serial.println("loopTask_music start...");
  int temp = 0;
  while (music_task_flag == 1) {
    music_loop();
    int t1 = music_get_total_playing_time();//Gets how long the music player has been playing
    int t2 = music_get_file_duration();     //Gets the playing time of the music file
    int t3 = music_read_play_position();    //Gets the current playing time of the music
    if(temp==1){
      int t4 = map(t3, 0, t2, 0, 100);
      if(t4<=100)
        //lv_bar_set_value(guider_music_ui.music_bar_time, t4, LV_ANIM_OFF);
      Serial.printf("t1: %d\t t2: %d\t t3: %d\t t4: %d\r\n", t1, t2, t3, t4);
    }    
    if ((t1 < t2) && (t2 > 0) && (temp == 0)) { //The music starts to play
      //lv_bar_set_value(guider_music_ui.music_bar_time, 0, LV_ANIM_OFF);
      temp = 1;
    } else if ((t2 == 0) && (temp == 1)) {      //The music stop to play
      temp = 0;
      music_task_flag = 0;
      //lv_img_set_src(guider_music_ui.music_imgbtn_play, &img_pause);
      music_button_state = 0;
      break;
    }
  }
  music_stop();
  Serial.println("loopTask_music stop...");
  vTaskDelete(musicTaskHandle);
}

//Create music task thread
void start_music_task(void) {
  if (music_task_flag == 0) {
    music_task_flag = 1;
    xTaskCreate(loopTask_music, "loopTask_music", 8192, NULL, 1, &musicTaskHandle);
  } else {
    Serial.println("loopTask_music is running...");
  }
}

//Close the music thread
void stop_music_task(void) {
  if (music_task_flag == 1) {
    music_task_flag = 0;
    while (1) {
      if (eTaskGetState(musicTaskHandle) == eDeleted) {
        break;
      }
      vTaskDelay(10);
    }
    Serial.println("loopTask_music deleted!");
  }
}

//Check whether the thread is running
int music_task_is_running(void) {
  return music_task_flag;
}

// optional
void audio_info(const char *info) {
  Serial.print("info        ");
  Serial.println(info);
}
void audio_eof_mp3(const char *info) {  
  Serial.print("eof_mp3     ");
  Serial.println(info);
}


String extractUrl(String data, String key) {
  int keyIndex = data.indexOf(key);
  if (keyIndex == -1) {
    return ""; // Key not found
  }

  int urlStart = data.indexOf("\"", keyIndex + key.length() + 2); // Find the opening quote of the URL
  if (urlStart == -1) {
    return ""; // Opening quote not found
  }

  int urlEnd = data.indexOf("\"", urlStart + 1); // Find the closing quote of the URL
  if (urlEnd == -1) {
    return ""; // Closing quote not found
  }

  return data.substring(urlStart + 1, urlEnd);
}

// Function to download and save a file from a URL
bool downloadFile(String url, const char* filePath) {
  //WiFiClient client;
  HTTPClient http;

  Serial.print("Downloading file from: ");
  Serial.println(url);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("HTTP get done. Code: %d\n", httpCode);

    // File found at server
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      File file = SD_MMC.open(filePath, FILE_WRITE);

      if (!file) {
        Serial.println("Failed to open file for writing");
        http.end();
        return false;
      }

      // Stream the content from the server to the SD card file
      int bytesRead = http.writeToStream(&file);
      Serial.printf("%d bytes downloaded\n", bytesRead);

      file.close();
      http.end();
      return true;
    } else {
      Serial.printf("File not found or error, code: %d\n", httpCode);
      http.end();
      return false;
    }
  } else {
    Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
}



String captureAndUploadImage() {
  camera_fb_t *fb = esp_camera_fb_get();  // Capture the image
  if (!fb) {
    Serial.println("Failed to capture image");
    return "";
  }
  unsigned long now = millis();
  char filename[32];
  sprintf(filename, "/image%d.jpg", imageCount);
  photo_save(filename);
  Serial.printf("Saved picture: %s\r\n", filename);
  Serial.println("Photos will begin in one minute, please be ready.");
  Serial.printf("Captured image of size: %u bytes\n", fb->len);

  HTTPClient http;
  http.begin(serverUrl);  // Initialize the HTTP request
  http.setTimeout(1500000);  // Set timeout for the request

  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String contentType = "multipart/form-data; boundary=" + boundary;
  http.addHeader("Content-Type", contentType);

  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"file\"; filename=\"image1.jpg\"\r\n";
  bodyStart += "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";

  size_t bodySize = bodyStart.length();
  size_t bodyEndSize = bodyEnd.length();
  size_t totalSize = bodySize + fb->len + bodyEndSize;

  uint8_t* bodyBuffer = (uint8_t*)malloc(totalSize);
  if (bodyBuffer == NULL) {
    Serial.println("Failed to allocate memory for body buffer");
    esp_camera_fb_return(fb);
    return "";
  }

  // Copy data into the buffer
  memcpy(bodyBuffer, bodyStart.c_str(), bodySize);
  memcpy(bodyBuffer + bodySize, fb->buf, fb->len);  // Add the image data
  memcpy(bodyBuffer + bodySize + fb->len, bodyEnd.c_str(), bodyEndSize);

  int httpResponseCode = http.sendRequest("POST", bodyBuffer, totalSize);  // Send the POST request
  String response = "";
  if (httpResponseCode > 0) {
    response = http.getString();  // Get the server response
    Serial.println("Image uploaded successfully");
  } else {
    Serial.printf("Error on HTTP request: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  free(bodyBuffer);  // Free allocated memory
  http.end();  // Close the connection
  esp_camera_fb_return(fb);  // Release the frame buffer

  return response;  // Return the response from the server
  Serial.println(response);
}

static void event_handler(lv_event_t * e){
  lv_event_code_t code = lv_event_get_code(e);
  switch (code) {
    case LV_EVENT_VALUE_CHANGED:
      {
        LV_LOG_USER("Toggled");
      }
      break;
    case LV_EVENT_CLICKED:
      {
        String detectedText = captureAndUploadImage();
        String imageUrl = extractUrl(detectedText, "image_url");
        String mp3Url = extractUrl(detectedText, "mp3_url");
        Serial.print("Image URL: ");
        Serial.println(imageUrl);
        Serial.print("MP3 URL: ");
        Serial.println(mp3Url);
        Serial.println("Detected text: " + detectedText);
        char* filename = "/test.mp3"; // File path on the SD card
        if (downloadFile(mp3Url, filename)) {
          Serial.println("File downloaded successfully!");
            } else {
            Serial.println("File download failed!");
          }
        audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
        audio.setVolume(12); // 0...21
        music_button_state = 0;
        music_load_mp3(filename);
        start_music_task();
        music_button_state = 1;
      }
      break;
    default:
      break;
  }
}

//Two types of button trigger events
void lv_example_btn_1(void){
  lv_obj_t * label;
  lv_obj_t * btn1 = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
  lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);
  label = lv_label_create(btn1);
  lv_label_set_text(label, "Button");
  lv_obj_center(label);

  lv_obj_t * btn2 = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btn2, event_handler, LV_EVENT_ALL, NULL);
  lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
  lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_height(btn2, LV_SIZE_CONTENT);
  label = lv_label_create(btn2);
  lv_label_set_text(label, "Toggle");
  lv_obj_center(label);
}
