#define CAMERA_MODEL_XIAO_ESP32S3  // Define your camera model here
#include "esp_camera.h"
#include "camera_pins.h"
#include <I2S.h>
//新增库
#include <WiFi.h>         //Wifi
#include <HTTPClient.h>   //http传输
#include <ArduinoJson.h>  //json
#include <SD.h>           //sd卡存储
#include "FS.h"
#include "SPI.h"
#include <base64.h>            //base64编码
#include <WiFiClientSecure.h>  //安全证书，暂时没用上
#include "secrets.h"           //信息存储
String accessToken = "";
String text = "";  // 定义全局变量 text 用于存储识别结果
// 录音设置
#define CODEC_PCM          //格式pcm
#define SAMPLE_RATE 16000  //采样率
#define SAMPLE_BITS 16     //比特位
#define FRAME_SIZE 160     //大小
#define VOLUME_GAIN 2
unsigned long recordingStartTime = 0;
bool isRecording = false;
const unsigned long recordingDuration = 3000;  // 3 seconds recording duration

File file; //全局文件变量

// 静态分配缓冲区以避免内存碎片化
uint8_t s_recording_buffer[FRAME_SIZE * 2];      // 使用静态分配的缓冲区
uint8_t s_compressed_frame[FRAME_SIZE * 2 + 3];  // 使用静态分配的缓冲区
size_t recording_buffer_size = FRAME_SIZE * 2;
size_t compressed_buffer_size = recording_buffer_size + 3;

// Take photo function
camera_fb_t *fb = NULL;
bool take_photo() {
  if (fb) {
    Serial.println("释放fb内存");
    esp_camera_fb_return(fb);
    fb = NULL;  // Ensure fb is reinitialized
  }
  Serial.println("拍照中...");
  //Serial.printf("Free heap before getting camera frame buffer: %d bytes\n", ESP.getFreeHeap());
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.printf("Failed to get camera frame buffer, free heap: %d bytes\n", ESP.getFreeHeap());
    return false;
  }
  Serial.printf("拍照成功, size: %d bytes\n", fb->len);
  //Serial.printf("Free heap after getting camera frame buffer, photo taken, free heap: %d bytes\n", ESP.getFreeHeap());
  return true;
}
//将fb的照片传给gpt-4o进行图像解析
String sendToOpenAI(camera_fb_t *fb) {
  Serial.println("OpenAI Vision");
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setTimeout(20000);  // 设置超时时间为20秒
    http.begin(chatApiUrl);
    http.addHeader("Authorization", "Bearer " + String(OpenAIKey));
    http.addHeader("Content-Type", "application/json");

    String base64Image = base64::encode(fb->buf, fb->len);

    // 创建 JSON 数据
    DynamicJsonDocument jsonDoc(4096);  // 使用更大的缓冲区来处理 JSON 数据
    jsonDoc["model"] = "gpt-4o-mini";

    JsonObject message = jsonDoc.createNestedArray("messages").createNestedObject();
    message["role"] = "user";
    JsonArray content = message.createNestedArray("content");

    JsonObject textObject = content.createNestedObject();
    textObject["type"] = "text";
    textObject["text"] = "你的任务是详细描述图片包含的内容和细节,进而解释图片的含义。";

    JsonObject imageObject = content.createNestedObject();
    imageObject["type"] = "image_url";
    JsonObject imageUrlObject = imageObject.createNestedObject("image_url");
    imageUrlObject["url"] = "data:image/jpeg;base64," + base64Image;

    jsonDoc["max_tokens"] = 500;

    // 序列化 JSON 数据
    String jsonString;
    serializeJson(jsonDoc, jsonString);

    int httpCode = http.POST(jsonString);

    if (httpCode > 0) {
      String response = http.getString();
      //Serial.println(response);
      DynamicJsonDocument responseDoc(4096);  // 使用更大的缓冲区来处理响应
      deserializeJson(responseDoc, response);
      String result = responseDoc["choices"][0]["message"]["content"].as<String>();
      Serial.println("Result: " + result);
      return result;
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
  return "";
}
// 麦克风初始化
void configure_microphone() {
  I2S.setAllPins(-1, 42, 41, -1, -1);
  if (!I2S.begin(PDM_MONO_MODE, SAMPLE_RATE, SAMPLE_BITS)) {
    Serial.println("Failed to initialize I2S!");
    while (1)
      ;
  }
}
// 从麦克风获取数据
size_t read_microphone() {
  size_t bytes_recorded = 0;
  esp_i2s::i2s_read(esp_i2s::I2S_NUM_0, s_recording_buffer, recording_buffer_size, &bytes_recorded, portMAX_DELAY);
  return bytes_recorded;
}
// 摄像头初始化
void configure_camera() {
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
  config.pixel_format = PIXFORMAT_JPEG;  //jpeg格式
  config.fb_count = 1;                   //缓冲区数量1个
  config.jpeg_quality = 5;               //图像质量，数字越大质量越小
  config.frame_size = FRAMESIZE_SVGA;    //分辨率 最大为UXGA，最小为QVGA
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera initialization failed 0x%x\n", err);
    return;
  } else {
    Serial.println("Camera initialized successfully");
  }
}
// Get Access Token
String getAccessToken() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://aip.baidubce.com/oauth/2.0/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "grant_type=client_credentials&client_id=" + String(apiKey) + "&client_secret=" + String(secretKey);
    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(response);

      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      const char *token = doc["access_token"];
      return String(token);
    } else {
      Serial.printf("Error getting access token, error: %s\n", http.errorToString(httpResponseCode).c_str());
      return "";
    }
    http.end();
  }
  return "";
}
void disable_camera() {
  esp_camera_deinit();
}

void enable_camera() {
  configure_camera();
}

void start_recording() {
  disable_camera();  // 禁用摄像头
  if (!psramFound()) {
    Serial.println("PSRAM not found. Restarting...");
    ESP.restart();
  }
  Serial.println("PSRAM可用");
  isRecording = true;
  recordingStartTime = 0;  // 重置录音起始时间
  Serial.println("录音开始");
  file = SD.open("/recording.wav", FILE_WRITE);
  if (!file) {
    Serial.println("无法打开文件进行写入");
    isRecording = false;
    enable_camera();  // Re-enable camera if recording fails
    return;
  }

  createWAVHeader(file, 0);  // 初始化WAV文件头
}

void stop_recording() {
  isRecording = false;
  // 重置录音起始时间
  recordingStartTime = 0;  // 重置录音起始时间
  if (!file) {
    Serial.println("文件未打开");
    enable_camera();  // 启用摄像头
    return;
  }
  size_t dataSize = file.size() - 44;  // 计算数据块大小，减去文件头的44字节
  file.seek(4);                        // 跳转到文件大小字段位置
  file.write((uint8_t *)&dataSize, 4);
  file.seek(40);  // 跳转到数据大小字段位置
  file.write((uint8_t *)&dataSize, 4);
  file.close();  // 关闭文件
  Serial.println("录音结束");
  uploadRecordingToBaidu("/recording.wav");
  enable_camera();  // 启用摄像头
}
// 初始化SD卡
void configure_sd() {
  if (!SD.begin(21)) {
    Serial.println("SD卡初始化失败！");
    while (1)
      ;
  }
  Serial.println("SD卡初始化成功");
}

// 设置WAV文件头
void createWAVHeader(File &file, size_t dataSize) {
  // WAV 文件头参数
  const uint32_t sampleRate = SAMPLE_RATE;     // 采样率
  const uint16_t numChannels = 1;              // 单声道
  const uint16_t bitsPerSample = SAMPLE_BITS;  // 位深度
  const uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
  const uint16_t blockAlign = numChannels * bitsPerSample / 8;
  const uint32_t riffChunkSize = 36 + dataSize;

  // 写入 RIFF Header
  file.write((const uint8_t *)"RIFF", 4);
  file.write((const uint8_t *)&riffChunkSize, 4);
  file.write((const uint8_t *)"WAVE", 4);

  // 写入 fmt Chunk
  file.write((const uint8_t *)"fmt ", 4);
  uint32_t fmtChunkSize = 16;
  file.write((const uint8_t *)&fmtChunkSize, 4);
  uint16_t audioFormat = 1;  // PCM
  file.write((const uint8_t *)&audioFormat, 2);
  file.write((const uint8_t *)&numChannels, 2);
  file.write((const uint8_t *)&sampleRate, 4);
  file.write((const uint8_t *)&byteRate, 4);
  file.write((const uint8_t *)&blockAlign, 2);
  file.write((const uint8_t *)&bitsPerSample, 2);

  // 写入 data Chunk
  file.write((const uint8_t *)"data", 4);
  file.write((const uint8_t *)&dataSize, 4);
}

// 上传录音文件到百度云进行语音识别
void uploadRecordingToBaidu(const char *filePath) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://vop.baidu.com/server_api?dev_pid=1536&cuid=90511851&token=" + accessToken;
    http.begin(url);
    http.addHeader("Content-Type", "audio/wav; rate=16000");

    File file = SD.open(filePath, FILE_READ);
    if (!file) {
      Serial.println("无法打开录音文件");
      return;
    }

    size_t fileSize = file.size();
    uint8_t *fileBuffer = (uint8_t *)malloc(fileSize);
    if (!fileBuffer) {
      Serial.println("内存分配失败");
      file.close();
      return;
    }
    file.read(fileBuffer, fileSize);
    file.close();

    int httpResponseCode = http.POST(fileBuffer, fileSize);

    if (httpResponseCode > 0) {
      String response = http.getString();
      //Serial.println("服务器响应: " + response);

      // 使用 ArduinoJson 解析 JSON 响应
      DynamicJsonDocument doc(1024);  // 适当的大小，取决于响应的大小
      DeserializationError error = deserializeJson(doc, response);

      if (error) {
        Serial.printf("解析 JSON 失败: %s\n", error.c_str());
      } else {
        // 提取 result 字段
        if (doc.containsKey("result") && doc["result"].is<JsonArray>()) {
          JsonArray resultArray = doc["result"].as<JsonArray>();
          if (resultArray.size() > 0) {
            text = resultArray[0].as<String>();  // 提取第一个结果
            Serial.println("识别结果: " + text);
          } else {
            Serial.println("识别结果为空");
          }
        } else {
          Serial.println("结果字段不存在或格式不正确");
        }
      }
    } else {
      Serial.printf("上传失败, 错误码: %d\n", httpResponseCode);
    }

    free(fileBuffer);
    http.end();
  } else {
    Serial.println("WiFi 未连接");
  }
}

// 发送聊天请求到 OpenAI API
void sendChatRequest(const String &text) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(chatApiUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(OpenAIKey));

    // 创建 JSON 对象
    DynamicJsonDocument doc(2048);
    JsonArray messages = doc.createNestedArray("messages");
    JsonObject message = messages.createNestedObject();
    message["role"] = "user";
    message["content"] = text;

    doc["model"] = "gpt-3.5-turbo";

    // 将 JSON 对象转换为字符串
    String jsonString;
    serializeJson(doc, jsonString);

    // 发送 POST 请求
    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0) {
      String response = http.getString();
      //Serial.println("聊天 API 响应: " + response);

      // 解析聊天 API 的响应
      DynamicJsonDocument responseDoc(1024);
      DeserializationError error = deserializeJson(responseDoc, response);

      if (error) {
        Serial.printf("解析聊天 API 响应失败: %s\n", error.c_str());
      } else {
        // 提取对话 API 响应中的内容
        if (responseDoc.containsKey("choices")) {
          JsonArray choices = responseDoc["choices"].as<JsonArray>();
          if (choices.size() > 0) {
            String reply = choices[0]["message"]["content"].as<String>();
            Serial.println("对话回复: " + reply);
          } else {
            Serial.println("对话 API 响应中没有 'choices' 字段");
          }
        } else {
          Serial.println("对话 API 响应中没有 'choices' 字段");
        }
      }
    } else {
      Serial.printf("对话 API 请求失败, 错误码: %d\n", httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi 未连接");
  }
}

// Initialize system
void setup() {
  Serial.begin(921600);
  Serial.println("Initialization start");
  // if (psramFound()) {
  //   Serial.println("PSRAM enabled");
  // } else {
  //   Serial.println("PSRAM not available");
  // }
  configure_sd();
  configure_camera();
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  accessToken = getAccessToken();
  if (accessToken.length() > 0) {
    Serial.println("百度云语音token获取成功!");
  }
  configure_microphone();
}

// Main loop
void loop() {
  if (Serial.available() > 0) {
    String inputString = Serial.readStringUntil('\n');
    inputString.trim();
    if (inputString == "1" && !isRecording) {
      start_recording();
    }
  }
  if (isRecording) {
    size_t bytesRead = read_microphone();
    if (bytesRead > 0) {
      file.write(s_recording_buffer, bytesRead);
    }
    if (recordingStartTime == 0) {
      recordingStartTime = millis();
    } else if (millis() - recordingStartTime >= recordingDuration) {
      stop_recording();
    }
  }
  if (text.length() > 0) {
    if (text == "拍照" || text == "拍照照" || text == "拍照照照") {
      if (take_photo()) {
        Serial.println("拍照成功");
        sendToOpenAI(fb);
      }
      text = "";  // 清空文本，避免重复发送
    } else {
      sendChatRequest(text);
      text = "";  // 清空文本，避免重复发送
    }
  }
}
