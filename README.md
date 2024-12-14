## esp32c3 ddsu666 data transfer with mqtt
~~~
// 换到modbus
// 算CS https://www.23bei.com/tool/205.html
FE FE FE FE 68 48 89 09 19 08 21 68 14 0E 33 33 35 3D 35 33 33 33 33 33 33 33 33 33 E6 16
参考教程: https://www.xingkongbeta.com/?p=272
~~~
>
>  ddsu666电表, modbus交互数据格式参考: https://www.modbus.cn/10181.html
>
## 特征

wifi:
+ 支持esptouch v1配网.
+ 连接失败自动进入配网模式, 配网失败重启系统.
+ 支持手动进入配网模式.

系统:
+ 支持读取esp的系统信息, 包括:
  * 开机时间
  * 剩余堆内存
  * wifi名称, ip地址, 信号强度, mac地址
  * 核心温度
  * ...
+ 手动重启, 重启成功并成功联网, 蓝色led闪烁1次.
+ 硬件看门狗

mqtt:
+ 异步监听系统命令.
+ 断开检测, 重连失败累计10次重启
+ 55000ms刷新连接.

ddsu666电表通讯:
+ 更改电表数据读取频率.
+ 异步读取电表数据
+ 支持电能清零

ota升级：
+ 一键重启进入升级模式
> 如果升级成功蓝色led闪烁5次


## 其他
uint8转float的代码参考: https://github.com/gjtimms/Modbus-RTU-Listen

## 效果
![example](https://cf.mb6.top/lib/images/github/20241211/b00b9f9237d13798349b2a507c80cbb4.jpg)
![example](https://cf.mb6.top/lib/images/github/20241211/C_N4NO1R8U@SJICKCO_NTEB.png)
![example](https://cf.mb6.top/lib/images/github/20241211/97899446cfe9345c70c12590cad1b4c7.jpg)


