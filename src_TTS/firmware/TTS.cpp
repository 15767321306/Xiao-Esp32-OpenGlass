#include "TTS.h"

TTS::TTS(HardwareSerial& serialTTS) :
  _serialTTS(serialTTS) 
{ 
}

void TTS::begin(bool voice,int volume,int speed,int level) {
  this->setVoice(voice);
  this->setVolume(volume);
  this->setSpeed(speed);
  this->setIntonation(level);
}

void TTS::setIntonation(int intonationLevel) {
  if (intonationLevel < 0 || intonationLevel > 9) {
    Serial.println("语调等级错误，请输入 0 到 9 之间的数字。");
    return;
  }
  uint8_t intonationData[] = {0xFD, 0x00, 0x06, 0x01, 0x01, 0x5B, 0x74, 0x30, 0x5D};
  intonationData[7] = 0x30 + intonationLevel;

  _serialTTS.write(intonationData, sizeof(intonationData));
  Serial.println("语调已设置为 [t" + String(intonationLevel) + "]");
}

void TTS::setSpeed(int speedLevel) {
  if (speedLevel < 0 || speedLevel > 9) {
    Serial.println("语速等级错误，请输入 0 到 9 之间的数字。");
    return;
  }
  uint8_t speedData[] = {0xFD, 0x00, 0x06, 0x01, 0x01, 0x5B, 0x73, 0x30, 0x5D};
  speedData[7] = 0x30 + speedLevel;

  _serialTTS.write(speedData, sizeof(speedData));
  Serial.println("语速已设置为 [s" + String(speedLevel) + "]");
}

void TTS::setVoice(bool isMale) {
  uint8_t voiceData[] = {0xFD, 0x00, 0x06, 0x01, 0x01, 0x5B, 0x6D, 0x30, 0x5D};
  if (isMale) {
    voiceData[7] = 0x31;
  }

  _serialTTS.write(voiceData, sizeof(voiceData));
  Serial.println("发音人已设置为 " + String(isMale ? "男声" : "女声"));
}

void TTS::setVolume(int volumeLevel) {
  if (volumeLevel < 0 || volumeLevel > 9) {
    Serial.println("音量等级错误，请输入 0 到 9 之间的数字。");
    return;
  }
  uint8_t volumeData[] = {0xFD, 0x00, 0x06, 0x01, 0x01, 0x5B, 0x76, 0x30, 0x5D};
  volumeData[7] = 0x30 + volumeLevel;

  _serialTTS.write(volumeData, sizeof(volumeData));
  Serial.println("音量已设置为 [v" + String(volumeLevel) + "]");
}

void TTS::sendTTSMessage(const String &message) {
  String utf8_str = message;
  String gb2312_str = GB.get(utf8_str);

  unsigned int len = gb2312_str.length();
  unsigned char buffer[len + 6];  // 包括头字节和长度字节

  // 填充缓冲区
  buffer[0] = 0xFD;
  buffer[1] = (len + 2) >> 8;
  buffer[2] = (len + 2) & 0xFF;
  buffer[3] = 0x01;  // cmd byte
  buffer[4] = 0x01;  // para byte

  // 复制GB2312编码数据
  for (unsigned int i = 0; i < len; i++) {
    buffer[i + 5] = gb2312_str[i];
  }

  // 发送数据
  _serialTTS.write(buffer, len + 5);

  _serialTTS.println("TTS message sent");
}
