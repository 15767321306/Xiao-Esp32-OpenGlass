#ifndef TTS_SETTINGS_H
#define TTS_SETTINGS_H

#include <Arduino.h>
#include <HardwareSerial.h> 
#include "UTF8ToGB2312.h"
class TTS {
public:
  TTS(HardwareSerial& serialTTS);

  void begin(bool voice = false, int volume = 1, int speed = 4, int level = 8);
  
  // 设置语音合成的语调 (0-9)
  void setIntonation(int intonationLevel);

  // 设置语音合成的语速 (0-9)
  void setSpeed(int speedLevel);

  // 设置语音合成的发音人 (true: 男声, false: 女声)
  void setVoice(bool isMale);

  // 设置语音合成的音量 (0-9)
  void setVolume(int volumeLevel); 

  // 发送文本到TTS模块进行语音合成
  void sendTTSMessage(const String &message);

private:
  HardwareSerial& _serialTTS; 
};

#endif