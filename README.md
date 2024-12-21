# esp32c3 ddsu666 data transfer with mqtt

## 特征

wifi:
+ 支持esptouch v1配网.
+ 连接失败自动进入配网模式, 配网失败再次启动配网，二次失败重启系统.
+ 支持手动进入配网模式.

系统:
+ 支持读取esp的系统信息, 包括:
  * 开机时间.
  * 剩余堆内存.
  * wifi名称, ip地址, 信号强度, mac地址
  * 核心温度等cpu信息.
  * 系统版本.
  * 硬件信息.
  * ...
+ 支持手动重启, 重启成功并成功联网, 蓝色led会闪烁1次.
+ 任务看门狗, 系统卡死会自动重启.

mqtt:
+ 异步监听订阅，订阅收到消息蓝色led闪烁1次.
+ 断开检测, 蓝色led闪烁3次, 重连失败累计10次重启.
+ 55000ms刷新连接, 防止连接被交换网断开.

ddsu666电表通讯:
+ 异步读取电表数据.
+ 支持电能清零.
+ 更改电表数据读取频率, 支持手动进入数据监听模式, 无监听续期, 会超时后进入休眠模式.

ota升级：
+ 一键重启进入升级模式. 如果升级成功蓝色led闪烁5次.

## 使用

### 准备

> 配置好mqtt服务器, 可以使用公有云或者自己搭建.

### 配置固件
+ 安装visualgdb
+ idf menu 内找到Example Configuration菜单, 编辑填写自己的mqtt地址和ota地址, 然后remake项目
+ build

### 安装客户端
推荐使用IoT MQTT Panel 客户端, 有安卓和ios客户端, 应用内支持定义自己的面板.

> 安卓下载: https://play.google.com/store/apps/details?id=snr.lab.iotmqttpanel.prod
> 
> ios下载: https://apps.apple.com/cn/app/iot-mqtt-panel/id6466780124

这里分享我的面板配置文件,文件名为IoTMQTTPanel-241215_130105.json, 可以导入软件使用. 效果在尾图.

### 其他接入
先订阅esp32_response这个topic, 然后向sysop-get的topic发送info-power, qos为0, 即可获得数据. 后续流程需要自己定制.

## 已知的问题
+ mqtt订阅sysop-get的topic, 收到info-sys, 执行系统信息查询, 然后发布系统数据到esp32_response的topic. 这个过程有概率触发系统崩溃重启, 无法稳定复现问题. 如果首次启动系统后立刻操作有概率触发.

## 笔记

~~~
// 出厂的2007协议换到modbus协议
// 算CS https://www.23bei.com/tool/205.html
FE FE FE FE 68 48 89 09 19 08 21 68 14 0E 33 33 35 3D 35 33 33 33 33 33 33 33 33 33 E6 16
// 参考教程: https://www.xingkongbeta.com/?p=272
~~~
>
> ddsu666电表, modbus交互数据格式参考: https://www.modbus.cn/10181.html
> uint8转float的代码参考: https://github.com/gjtimms/Modbus-RTU-Listen
>

## 效果
![example](https://cf.mb6.top/lib/images/github/20241211/b00b9f9237d13798349b2a507c80cbb5.webp)
![example](https://cf.mb6.top/lib/images/github/20241211/C_N4NO1R8U@SJICKCO_NTEB.png)
![example](https://cf.mb6.top/lib/images/github/20241211/97899446cfe9345c70c12590cad1b4c7.jpg)
![example](https://cf.mb6.top/lib/images/github/20241211/8ec66436be77d89e5c580668dc014438.webp)


