#define CAMERA_MODEL_XIAO_ESP32S3  // Define your camera model here
#include "esp_camera.h"
#include "camera_pins.h"
#include <I2S.h>
//新增库
#include <WiFi.h>         //Wifi
#include <HTTPClient.h>   //http传输
#include <ArduinoJson.h>  //json
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
const unsigned long recordingDuration = 2000;  // 3 seconds recording duration

// 静态分配缓冲区以避免内存碎片化
// 假设录音时长为3秒，采样率为16000Hz，位深度为16位（2字节）
// 所需缓冲区大小为 16000 * 3 * 2 = 96000 字节
#define MAX_RECORDING_SIZE (SAMPLE_RATE * 2 * SAMPLE_BITS / 8)
uint8_t s_recording_buffer[MAX_RECORDING_SIZE];
size_t totalBytesRead = 0; // 全局变量，用于累积读取的字节数
size_t recording_buffer_size = FRAME_SIZE * 2;

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
}

void stop_recording(size_t totalBytesRead) {
  isRecording = false;
  recordingStartTime = 0;  // 重置录音起始时间
  Serial.println("录音结束");
  
  // 将录音数据转换为WAV格式并上传
  size_t dataSize = totalBytesRead; // 数据块大小
  uint8_t wavHeader[44];
  createWAVHeader(wavHeader, dataSize); // 创建WAV头部

  // 分配缓冲区，包含WAV头部和录音数据
  size_t totalSize = sizeof(wavHeader) + dataSize;
  uint8_t *wavData = (uint8_t *)malloc(totalSize);
  if (!wavData) {
    Serial.println("内存分配失败");
    return;
  }
  memcpy(wavData, wavHeader, sizeof(wavHeader));
  memcpy(wavData + sizeof(wavHeader), s_recording_buffer, dataSize);

  uploadRecordingToBaidu(wavData, totalSize); // 上传WAV数据到百度云

  free(wavData); // 释放缓冲区
  enable_camera();  // 启用摄像头
}

// 设置WAV文件头
// 设置WAV文件头
void createWAVHeader(uint8_t *header, size_t dataSize) {
  // WAV 文件头参数
  const uint32_t sampleRate = SAMPLE_RATE;     // 采样率
  const uint16_t numChannels = 1;              // 单声道
  const uint16_t bitsPerSample = SAMPLE_BITS;  // 位深度
  const uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
  const uint16_t blockAlign = numChannels * bitsPerSample / 8;
  const uint32_t riffChunkSize = 36 + dataSize;

  // RIFF Header
  memcpy(header, "RIFF", 4);
  memcpy(header + 4, &riffChunkSize, 4);
  memcpy(header + 8, "WAVE", 4);

  // fmt Chunk
  memcpy(header + 12, "fmt ", 4);
  uint32_t fmtChunkSize = 16;
  memcpy(header + 16, &fmtChunkSize, 4);
  uint16_t audioFormat = 1;  // PCM
  memcpy(header + 20, &audioFormat, 2);
  memcpy(header + 22, &numChannels, 2);
  memcpy(header + 24, &sampleRate, 4);
  memcpy(header + 28, &byteRate, 4);
  memcpy(header + 32, &blockAlign, 2);
  memcpy(header + 34, &bitsPerSample, 2);

  // data Chunk
  memcpy(header + 36, "data", 4);
  memcpy(header + 40, &dataSize, 4);
}
// 上传录音文件到百度云进行语音识别
void uploadRecordingToBaidu(uint8_t *buffer, size_t size) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://vop.baidu.com/server_api?dev_pid=1536&cuid=90511851&token=" + accessToken;
    http.begin(url);
    http.addHeader("Content-Type", "audio/wav; rate=16000");

    int httpResponseCode = http.POST(buffer, size);

    if (httpResponseCode > 0) {
      String response = http.getString();

      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);

      if (error) {
        Serial.printf("解析 JSON 失败: %s\n", error.c_str());
      } else {
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
  //configure_sd();
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
      if (totalBytesRead + bytesRead <= MAX_RECORDING_SIZE) {
        memcpy(s_recording_buffer + totalBytesRead, s_recording_buffer, bytesRead); // 修正这里的拷贝源
        totalBytesRead += bytesRead;
      } else {
        stop_recording(totalBytesRead);
        totalBytesRead = 0; // 重置计数器
      }
    }
    if (recordingStartTime == 0) {
      recordingStartTime = millis();
    } else if (millis() - recordingStartTime >= recordingDuration) {
      stop_recording(totalBytesRead);
      totalBytesRead = 0; // 重置计数器
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
