/*********
 * 
 * 
 * 
 * 
 * 
 * --------------------
 * Starting point for this code:
 * 
 * Rui Santos
 * Complete instructions at https://RandomNerdTutorials.com/esp32-cam-projects-ebook/
  
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
 
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems
#include "esp_http_server.h"
#include <ESP32Servo.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

// Internal filesystem (SPIFFS)
// used for non-volatile camera settings
#include "storage.h"


#include "device_name.h"
#include "network_credentials.h"
//#include "cam_index.h" // ie. index.html
#include "index.h"
#include "index_other.h"
#include "css.h"
#include "logo.h"

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM


// Hardware Vertical Flip , 0 or 1 (overrides default board setting)
#define REVERSE_MOUNT 1


#include "camera_pins.h"


#define PART_BOUNDARY "123456789000000000000987654321"

#define SERVO_PAN_PIN      12
#define SERVO_TILT_PIN      15

// define the number of bytes you want to access
#define EEPROM_SIZE 2

#define EEPROM_PAN_ADDRESS 0
#define EEPROM_TILT_ADDRESS 1

#define STEP   5

#define OTA_UPDATE true

#if defined(NO_FS)
    bool filesystem = false;
#else
    bool filesystem = true;
#endif

Servo servoN1;
Servo servoN2;
Servo panServo;  // create servo object to control a servo
Servo tiltServo;


int tiltServoPos = 90;
int panServoPos = 0;

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;


static void setTiltPanFromEeprom(){
    // read saved pan & tilt angles, if they exist.
  panServoPos = EEPROM.read(EEPROM_PAN_ADDRESS);
  if (panServoPos==255) panServoPos = 0;
  panServo.write(panServoPos);


  tiltServoPos = EEPROM.read(EEPROM_TILT_ADDRESS);
  if (tiltServoPos==255) tiltServoPos = 0;
  tiltServo.write(tiltServoPos);
}


static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t style_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char *)style_css, style_css_len);
}

static esp_err_t logo_svg_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "image/svg+xml");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char *)logo_svg, logo_svg_len);
}


static esp_err_t info_handler(httpd_req_t *req){
    static char json_response[256];
    char * p = json_response;
    *p++ = '{';
    p+=sprintf(p, "\"cam_name\":\"%s\"", DEVICE_NAME);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t streamviewer_handler(httpd_req_t *req){
    Serial.println("Stream Viewer requested");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char *)streamviewer_html, streamviewer_html_len);
}

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
  }
  return res;
}



static esp_err_t status_handler(httpd_req_t *req){
    static char json_response[1024];
    sensor_t * s = esp_camera_sensor_get();
    char * p = json_response;
    *p++ = '{';
    p+=sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p+=sprintf(p, "\"quality\":%u,", s->status.quality);
    p+=sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p+=sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p+=sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p+=sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
    p+=sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p+=sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p+=sprintf(p, "\"awb\":%u,", s->status.awb);
    p+=sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p+=sprintf(p, "\"aec\":%u,", s->status.aec);
    p+=sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p+=sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p+=sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p+=sprintf(p, "\"agc\":%u,", s->status.agc);
    p+=sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p+=sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p+=sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p+=sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p+=sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p+=sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p+=sprintf(p, "\"vflip\":%u,", s->status.vflip);
    p+=sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p+=sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p+=sprintf(p, "\"colorbar\":%u,", s->status.colorbar);
    p+=sprintf(p, "\"cam_name\":\"%s\"", DEVICE_NAME);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t cmd_handler(httpd_req_t *req){
  static char json_response[1024];

  char*  buf;
  size_t buf_len;
  char variable[32] = {0,};
  char value[32] = {0,};
  char degrees[32] = {0,};

  int res = 0;
  int deg = STEP;
  
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char*)malloc(buf_len);
    if(!buf){
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {// control servo(s)
      if (httpd_query_key_value(buf, "go", variable, sizeof(variable)) == ESP_OK) { 
        if (httpd_query_key_value(buf, "degrees", degrees, sizeof(degrees)) == ESP_OK){
          deg = atoi(degrees);
        } else {
          deg = STEP;
        }

        if(!strcmp(variable, "down")) {
          if(tiltServoPos <= (180- deg)) {
            tiltServoPos += deg;
            tiltServo.write(tiltServoPos);
          }
          Serial.println(tiltServoPos);
          Serial.println("down");
        }
        else if(!strcmp(variable, "up")) {
          if(tiltServoPos >= deg) {
            tiltServoPos -= deg;
            tiltServo.write(tiltServoPos);
          }
          Serial.println(tiltServoPos);
          Serial.println("up");
        }
        else if(!strcmp(variable, "right")) {
          if(panServoPos <= (180 - deg)) {
            panServoPos += deg;
            panServo.write(panServoPos);
          }
          Serial.println("right");
        }
        else if(!strcmp(variable, "left")) {
          if(panServoPos >= deg) {
            panServoPos -= deg;
            panServo.write(panServoPos);
          }
          Serial.println(panServoPos);
          Serial.println("left");
        }
        else {
          res = -1;
        }
      } 
      else if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) { //chnage camera settings
        int val = atoi(value);
        sensor_t * s = esp_camera_sensor_get();
      
        if(!strcmp(variable, "framesize")) {
          if(s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val);
        }
        else if(!strcmp(variable, "quality")) res = s->set_quality(s, val);
        else if(!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
        else if(!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
        else if(!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
        else if(!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
        else if(!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
        else if(!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
        else if(!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
        else if(!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
        else if(!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
        else if(!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
        else if(!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
        else if(!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
        else if(!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
        else if(!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
        else if(!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
        else if(!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
        else if(!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
        else if(!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
        else if(!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
        else if(!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
        else if(!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
        else if(!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);
        else if(!strcmp(variable, "save_prefs")) {
          if (filesystem) savePrefs(SPIFFS);
        }
        else if(!strcmp(variable, "clear_prefs")) {
          if (filesystem) removePrefs(SPIFFS);
        }
      } 
       
      else if (httpd_query_key_value(buf, "reset", variable, sizeof(variable)) == ESP_OK) { // change camera quality
        ESP.restart(); //when something not working
      }      
      else if (httpd_query_key_value(buf, "eeprom", variable, sizeof(variable)) == ESP_OK) { // 0 to revert to last eeprom, 1 to save current tilt & pan to eeprom.
        int val = atoi(variable);
        if (val==1){
          EEPROM.write(EEPROM_TILT_ADDRESS,tiltServoPos);
          EEPROM.write(EEPROM_PAN_ADDRESS,panServoPos);
          EEPROM.commit();
        } else {
          setTiltPanFromEeprom();
        }
      }     
      
      else {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

 

  if(res){
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  char * p = json_response;
  *p++ = '{';
  p+=sprintf(p, "\"tilt\":%u,", tiltServo.read());
  p+=sprintf(p, "\"pan\":%u", panServo.read());
  *p++ = '}';
  *p++ = 0;
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json_response, strlen(json_response));
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri       = "/control",
    .method    = HTTP_GET,
    .handler   = cmd_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t status_uri = {
    .uri       = "/status",
    .method    = HTTP_GET,
    .handler   = status_handler,
    .user_ctx  = NULL
  };


  httpd_uri_t style_uri = {
      .uri       = "/style.css",
      .method    = HTTP_GET,
      .handler   = style_handler,
      .user_ctx  = NULL
  };

  httpd_uri_t logo_svg_uri = {
      .uri       = "/logo.svg",
      .method    = HTTP_GET,
      .handler   = logo_svg_handler,
      .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t info_uri = {
    .uri       = "/info",
    .method    = HTTP_GET,
    .handler   = info_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t streamviewer_uri = {
      .uri       = "/view",
      .method    = HTTP_GET,
      .handler   = streamviewer_handler,
      .user_ctx  = NULL
  };


  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
    httpd_register_uri_handler(camera_httpd, &style_uri);
    httpd_register_uri_handler(camera_httpd, &logo_svg_uri);
    httpd_register_uri_handler(camera_httpd, &status_uri);
  }
  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    httpd_register_uri_handler(stream_httpd, &info_uri);
    httpd_register_uri_handler(stream_httpd, &streamviewer_uri);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  panServo.setPeriodHertz(50);    // standard 50 hz servo
  tiltServo.setPeriodHertz(50);    // standard 50 hz servo
  // think these are 'fake' servos' that aenble PWM on cam pins.
  servoN1.attach(2, 1000, 2000); 
  servoN2.attach(13, 1000, 2000);
  
  panServo.attach(SERVO_PAN_PIN);
  tiltServo.attach(SERVO_TILT_PIN);
  

   // initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);
  setTiltPanFromEeprom();

  // read saved pan & tilt angles, if they exist.
  panServoPos = EEPROM.read(EEPROM_PAN_ADDRESS);
  if (panServoPos==255) panServoPos = 0;
  panServo.write(panServoPos);


  tiltServoPos = EEPROM.read(EEPROM_TILT_ADDRESS);
  if (tiltServoPos==255) tiltServoPos = 0;
  tiltServo.write(tiltServoPos);

  
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 16;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  sensor_t * s = esp_camera_sensor_get();
  // effectively rotate 180 deg to suit install on pan and tilt holder
  #if defined(REVERSE_MOUNT)
    // effectively rotate 180 deg to suit install on pan and tilt holder
      s->set_hmirror(s, REVERSE_MOUNT);
      s->set_vflip(s, REVERSE_MOUNT);
  #endif

  if (filesystem) {
      filesystemStart();
      loadPrefs(SPIFFS);
  } else {
      Serial.println("No Internal Filesystem, cannot save preferences");
  }

  // Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  if (!MDNS.begin(DEVICE_NAME)) {
      Serial.println("Error setting up MDNS responder!");
      while(1){
          delay(1000);
      }
  }

 #if defined(OTA_UPDATE)
    // Start OTA once connected
    // https://community.platformio.org/t/esp32-ota-using-platformio/15057//
    // https://docs.platformio.org/en/latest/platforms/espressif32.html#over-the-air-ota-update
    Serial.println("Setting up OTA");
    // Port defaults to 3232
    // ArduinoOTA.setPort(3232); 
    // Hostname defaults to esp3232-[MAC]
    ArduinoOTA.setHostname(DEVICE_NAME);
    // No authentication by default
    #if defined (OTA_PASSWORD)
        ArduinoOTA.setPassword(OTA_PASSWORD);
        Serial.printf("OTA Password: %s\n\r", OTA_PASSWORD);
    #endif
    ArduinoOTA
        .onStart([]() {
          String type;
          if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
          else // U_SPIFFS
            type = "filesystem";
          // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
          Serial.println("Start updating " + type);
        })
        .onEnd([]() {
          Serial.println("\nEnd");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
          Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
          Serial.printf("Error[%u]: ", error);
          if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
          else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
          else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
          else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
          else if (error == OTA_END_ERROR) Serial.println("End Failed");
        });
    ArduinoOTA.begin();
  #endif

  // Start streaming web server
  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void loop() {
  
}