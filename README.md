#### 注：本项目无web实现，只在板子上接通了gpt对话和图像解析基础实现，后续可以自己接入扬声器等

# 一、参考项目地址

项目地址：https://github.com/BasedHardware/OpenGlass

# 二、项目准备

#### 01 下载Arduino IDE

Arduino产品地址：https://www.arduino.cc/en/software

#### 02 设置板子URL

###### 1)、菜单栏，选择**“文件”>“首选项”(File > Preferences)**，然后在**“其他 Boards Manager URL”("Additional Boards Manager URLs" )**填写这个链接：https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

![图片](https://mmbiz.qpic.cn/mmbiz_png/kEIAbQcIpytla7XyC5ibzJp6ibJg3OR5FuJDr5ahL3BoPXX5Tsq2liaSGCsb9HYB4gzrIKTV89rtUs5ic5OWiclZw4Q/640?wx_fmt=png&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

###### 2)、导航到**“工具”>“开发板”>“开发板管理器...”（Tools > Board > Boards Manager）**，在搜索框中输入关键字 **esp32**，选择2.0.17版本，其他版本可能保错

###### 3)、**选择您的主板和端口：**

- - 在 Arduino IDE 顶部，选择开发板和端口。
  - 弹窗中搜索 xiao 并选择 XIAO_ESP32S3 。

###### 4)、设置PSRAM，不然摄像头无法获得缓冲区

![图片](https://mmbiz.qpic.cn/mmbiz_png/kEIAbQcIpytla7XyC5ibzJp6ibJg3OR5FufrlzBO4ibHPGksKxeRNqLVKzbTkcLCibKTqPDMowZcDGcsgrMTzibhicicw/640?wx_fmt=png&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

###### 5)、我们还需要点击左边菜单的库，安装摄像头所必须的库**esp_camera**，以及下面的新增库
![屏幕截图 2024-07-26 162338](https://github.com/user-attachments/assets/092e559f-7676-4bb3-a45a-6d890d7b60d0)


# 三、获取百度云短语音的试用

###### 1、百度云开发平台：[百度智能云控制台 (baidu.com)](https://console.bce.baidu.com/)

![屏幕截图 2024-07-26 162700](https://github.com/user-attachments/assets/1eee4936-c424-48e8-a9d7-3bba435156bd)


###### 2、进行操作指引的两个步骤获得密钥

![屏幕截图 2024-07-26 162822](https://github.com/user-attachments/assets/a4a1dfa6-334e-489b-9aa1-c32e8f346eba)



###### 3、填写secrets.h头文件的密钥信息，WiFi信息等（WiFi连接2.4G频段的，如果连不上请检查是否为2.4G频段，热点有些是5G频段的）

# 四、填写你的GPT的URL，以及key，这里可以使用国内中转的URL地址，以及Key

###### 如：清风阁gpt4中转平台：[OpenKEY.Cloud-4.0 (996444.icu)](https://4.0.996444.icu/home)等等，尽量使用官方一点的URL或者Key

#五、其他参考博主
####### 新探：https://mp.weixin.qq.com/s/O_rENZfu5pXn-cF9_z-gzQ

