/******************************************************************
   本程序只供学习使用，未经作者许可，不得用于其它任何用途
			
        欢迎访问我的USB专区：http://group.ednchina.com/93/
        欢迎访问我的blog：   http://www.ednchina.com/blog/computer00
                             http://computer00.21ic.org

UsbCore.c file

作者：电脑圈圈
建立日期: 2008.06.29
修改日期: 2009.03.14
版本：V1.1
版权所有，盗版必究。
Copyright(C) 电脑圈圈 2008-2018
All rights reserved            
*******************************************************************/

#include <AT91SAM7S64.H>
#include "config.h"
#include "AT91SAMxUSB.h"
#include "uart.h"
#include "usbcore.h"
#include "led.h"
#include "lcd.h"

uint8 Buffer[16];  //读端点0用的缓冲区

int16 AudioOutBuf[650];  //缓冲区
uint32 AudioOutByteCount=0; //输出字节计数器

//USB设备请求的各字段
uint8  bmRequestType;
uint8  bRequest;
uint16 wValue;
uint16 wIndex;
uint16 wLength;
//当前发送数据的位置
uint8 * pSendData;
//需要发送数据的长度
uint16 SendLength;
//是否需要发送0数据包的标志。在USB控制传输的数据过程中，
//当返回的数据包字节数少于最大包长时，会认为数据过程结束。
//当请求的字节数比实际需要返回的字节数长，而实际返回的字节
//数又刚好是端点0大小的整数倍时，就需要返回一个0长度的数据包
//来结束数据过程。因此这里增加一个标志，供程序决定是否需要返回
//一个0长度的数据包。
uint8 NeedZeroPacket;

//当前的配置值。只有在设置非0配置后
uint8 ConfigValue;

//当前的接口值
uint8 InterfaceValue;

//当前帧号
uint16 CurrentFrameNumber;

//端点1缓冲是否忙的标志。当缓冲区中有数据时，该标志为真。
//当缓冲区中空闲时，该标志为假。
uint8 Ep1InIsBusy;

//端点3缓冲是否忙的标志。当缓冲区中有数据时，该标志为真。
//当缓冲区中空闲时，该标志为假。
uint8 Ep3InIsBusy;


//USB设备描述符的定义
const uint8 DeviceDescriptor[0x12]=  //设备描述符为18字节
{
//bLength字段。设备描述符的长度为18(0x12)字节
 0x12,
 
//bDescriptorType字段。设备描述符的编号为0x01
 0x01,
 
//bcdUSB字段。这里设置版本为USB1.1，即0x0110。
//由于是小端结构，所以低字节在先，即0x10，0x01。
 0x10,
 0x01,
 
//bDeviceClass字段。我们不在设备描述符中定义设备类，
//而在接口描述符中定义设备类，所以该字段的值为0。
 0x00,
 
//bDeviceSubClass字段。bDeviceClass字段为0时，该字段也为0。
 0x00,
 
//bDeviceProtocol字段。bDeviceClass字段为0时，该字段也为0。
 0x00,
 
//bMaxPacketSize0字段。AT91SAM7S64的端点0大小的16字节。
 0x08,
 
//idVender字段。厂商ID号，我们这里取0x8888，仅供实验用。
//实际产品不能随便使用厂商ID号，必须跟USB协会申请厂商ID号。
//注意小端模式，低字节在先。
 0x88,
 0x88,
 
//idProduct字段。产品ID号，由于是第七个实验，我们这里取0x210B。
//注意小端模式，低字节应该在前。
 0x0B,
 0x21,
 
//bcdDevice字段。设备版本号，取1.0版，即0x0100。
//小端模式，低字节在先。
 0x00,
 0x01,
 
//iManufacturer字段。厂商字符串的索引值，为了方便记忆和管理，
//字符串索引就从1开始吧。
 0x01,

//iProduct字段。产品字符串的索引值。刚刚用了1，这里就取2吧。
//注意字符串索引值不要使用相同的值。
 0x02,
 
//iSerialNumber字段。设备的序列号字符串索引值。
//这里取3就可以了。
 0x03,

//bNumConfigurations字段。该设备所具有的配置数。
//我们只需要一种配置就行了，因此该值设置为1。
 0x01
};
//////////////////////////设备描述符完毕//////////////////////////////

//USB配置描述符集合的定义
//配置描述符总长度为99字节
const uint8 ConfigurationDescriptor[99]=
{
 /***************配置描述符***********************/
 //bLength字段。配置描述符的长度为9字节。
 0x09,
 
 //bDescriptorType字段。配置描述符编号为0x02。
 0x02,
 
 //wTotalLength字段。配置描述符集合的总长度，
 //包括配置描述符本身、接口描述符、类描述符、端点描述符等。
 sizeof(ConfigurationDescriptor)&0xFF, //低字节
 (sizeof(ConfigurationDescriptor)>>8)&0xFF, //高字节
 
 //bNumInterfaces字段。该配置包含的接口数，有2个接口。
 0x02,
 
 //bConfiguration字段。该配置的值为1。
 0x01,
 
 //iConfigurationz字段，该配置的字符串索引。这里没有，为0。
 0x00,
 
 //bmAttributes字段，该设备的属性。由于我们的板子是总线供电的，
 //并且我们不想实现远程唤醒的功能，所以该字段的值为0x80。
 0x80,
 
 //bMaxPower字段，该设备需要的最大电流量。由于我们的板子
 //需要的电流不到100mA，因此我们这里设置为100mA。由于每单位
 //电流为2mA，所以这里设置为50(0x32)。
 0x32,
 
 /*****************音频控制接口描述符*******************/
 //bLength字段。接口描述符的长度为9字节。
 0x09,
 
 //bDescriptorType字段。接口描述符的编号为0x04。
 0x04,
 
 //bInterfaceNumber字段。该接口的编号，第一个接口，编号为0。
 0x00,
 
 //bAlternateSetting字段。该接口的备用编号，为0。
 0x00,
 
 //bNumEndpoints字段。非0端点的数目。该接口没有端点
 0x00,
 
 //bInterfaceClass字段。该接口所使用的类。音频接口类的代码为0x01。
 0x01,
 
 //bInterfaceSubClass字段。该接口所使用的子类。音频控制接口的子类代码为0x01。
 0x01,
 
 //bInterfaceProtocol字段。没有使用协议。
 0x00,
 
 //iConfiguration字段。该接口的字符串索引值。这里没有，为0。
 0x00,
 
 
 /*************类特殊音频控制接口**********/
 //bLength字段，该描述符的长度。为9字节。
 0x09,
 
 //bDescriptorType字段，描述符的类型。编号为0x24，CS_INTERFACE_DESCRIPTOR。
 0x24,
 
 //bDescriptorSubtype字段，描述符子类。编号为0x01，HEADER。
 0x01,
 
 //bcdADC字段，协议版本。这里为1.0版。
 0x00,
 0x01,
 
 //wTotalLength字段，类特殊描述符的总长度。这里为30字节。
 0x1E,
 0x00,

 //bInCollection字段，流接口的数量。这里仅有一个。
 0x01,
 
 //baInterfaceNr字段，属于此接口的流接口编号。音频流接口1属于此音频控制接口
 0x01,

 /********************Input Terminal 描述符*************/
 //bLength字段，该描述符的长度，12字节
 0x0C, 
 
 //bDescriptorType字段，CS_INTERFACE descriptor type.
 0x24,
 
 //bDescriptorSubtype字段，INPUT_TERMINAL descriptor subtype.
 0x02,
 
 //bTerminal ID字段，该终端的ID号，为1
 0x01,
 
 //wTerminalType字段，该终端的类型，为TERMINAL_STREAMING,
 0x01,
 0x01,
 
 //bAssocTerminal字段，与该输入终端结合在一起的输出终端，这里没有，为0
 0x00,
 
 //bNrChannels字段，该音频簇的通道数目，这里仅有一个通道，为Center Front (C)
 0x01,
 
 //wChannelConfig，通道配置，设置为D2为1，即Center Front (C)
 0x04,
 0x00,
 
 //iChannelNames字段，描述第一个逻辑通道的字符串描述符的索引，这里没有
 0x00,
 
 //iTerminal字段，该输入终端的字符串描述符的索引，这里没有
 0x00, 
 
 /*****************OutputTerminal描述符****************/
 //bLength字段，该描述符的长度，9字节
 0x09, 
 
 //bDescriptorType字段，CS_INTERFACE descriptor type.
 0x24,
 
 //bDescriptorSubtype字段，OUTPUT_TERMINAL descriptor subtype..
 0x03,
 
 //bTerminal ID字段，该终端的ID号，为2
 0x02,
 
 //wTerminalType字段，该终端的类型，为TERMINAL_OUT_SPEAKER
 0x01,
 0x03,
 
 //bAssocTerminal字段，与该输出终端结合在一起的输入终端，这里没有，为0
 0x00,
 
 //bSourceID字段，连接在该输出终端的输入终端ID号，为0x01，即前面的输入终端
 0x01,
 
 //iTerminal字段，该输入终端的字符串描述符的索引，这里没有
 0x00, 
 
 /*****************备用音频流接口0描述符*******************/
 //bLength字段。接口描述符的长度为9字节。
 0x09,
 
 //bDescriptorType字段。接口描述符的编号为0x04。
 0x04,
 
 //bInterfaceNumber字段。该接口的编号，第二个接口，编号为1。
 0x01,
 
 //bAlternateSetting字段。该接口的备用编号，为0。
 0x00,
 
 //bNumEndpoints字段。非0端点的数目，备用0接口无端点。
 0x00,
 
 //bInterfaceClass字段。该接口所使用的类。音频接口类的代码为0x01。
 0x01,
 
 //bInterfaceSubClass字段。该接口所使用的子类。音频流接口的子类代码为0x02。
 0x02,
 
 //bInterfaceProtocol字段。没有使用协议。
 0x00,
 
 //iConfiguration字段。该接口的字符串索引值。这里没有，为0。
 0x00,
 
 /*****************备用音频流接口1描述符*******************/
 //bLength字段。接口描述符的长度为9字节。
 0x09,
 
 //bDescriptorType字段。接口描述符的编号为0x04。
 0x04,
 
 //bInterfaceNumber字段。该接口的编号，第二个接口，编号为1。
 0x01,
 
 //bAlternateSetting字段。该接口的备用编号，为1。
 0x01,
 
 //bNumEndpoints字段。非0端点的数目。该音频流接口使用一个ISO输出端点。
 0x01,
 
 //bInterfaceClass字段。该接口所使用的类。音频接口类的代码为0x01。
 0x01,
 
 //bInterfaceSubClass字段。该接口所使用的子类。音频流接口的子类代码为0x02。
 0x02,
 
 //bInterfaceProtocol字段。没有使用协议。
 0x00,
 
 //iConfiguration字段。该接口的字符串索引值。这里没有，为0。
 0x00,
 
 /**********类特殊音频流接口描述符*****/
 //bLength字段。该描述符的长度，7字节。
 0x07,
 
 //bDescriptorType字段。该描述符的类型，为CS_INTERFACE。
 0x24,
 
 //bDescriptorSubtype字段。描述符子类，为AS_GENERAL descriptor subtype.
 0x01,
 
 //bTerminalLink字段。该接口连接到的终端ID号
 0x01,
 
 //bDelay字段。数据延时量（多少个frame）
 0x01,
 
 //wFormatTag字段。该接口的音频数据流所使用的格式
 0x01,
 0x00,
 
 /******AudioFormat描述符*************/
 //bLength字段。该描述符的长度，11字节。
 0x0B,
 
 //bDescriptorType字段。该描述符的类型，为CS_INTERFACE。
 0x24,
 
 //bDescriptorSubtype字段。描述符子类，为FORMAT_TYPE descriptor subtype.
 0x02, 
 
 //bFormatType字段，为FORMAT_TYPE_I
 0x01,
 
 //bNrChannels字段。该音频流的物理通道数，只有一个通道
 0x01,
 
 //bSubframeSize字段，每样点分配的字节数
 0x02,
 
 //bBitResolution字段，每样点的分辨率（bit数），为16bit
 0x10,
 
 //bSamFreqType字段，该音频流接口的ISO端点所支持的采样频率的个数，这里为1个
 0x01,
 
 //sampling frequency字段，为32KHz
 0x00,
 0x7D,
 0x00,
 
 /*************标准ISO数据输出端点描述符****************/
 //bLength字段。端点描述符长度为9字节。
 0x09,
 
 //bDescriptorType字段。端点描述符编号为0x05。
 0x05,
 
 //bEndpointAddress字段。端点的地址。我们使用输出端点2。
 //D7位表示数据方向，输出端点D7为0。所以输出端点2的地址为0x02。
 0x02,
 
 //bmAttributes字段。D1~D0为端点传输类型选择。
 //该端点为ISO端点。ISO端点的编号为1。
 //D3~D2为同步类型，01为异步，10为自适应，11为同步。这里选择为同步
 0x0D,
 
 //wMaxPacketSize字段。该端点的最大包长。端点2的最大包长为64字节。
 //注意低字节在先。
 0x40,
 0x00,
 
 //bInterval字段。端点查询的时间，1ms。
 0x01,
 
 //bRefresh字段
 0x00,
 
 //bSynchAddress字段
 0x00,
 
 /**********类特殊音频流ISO数据端点描述符********/
 //bLength字段，该描述符的长度。为7字节。
 0x07,
 
 //bDescriptorType字段，该描述符的类型，为类特殊端点描述符（CS_ENDPOINT）
 0x25,
 
 //bDescriptorSubType字段，该描述符的子类型，为EP_GENERAL descriptor subtype.
 0x01,
 
 //bmAttributes字段，D6~D0表示该端点所支持的控制，D7表示是否必须为最大包长度
 0x00,
 
 //bLockDelayUnits字段。时钟锁定延时的单位
 0x00,
 
 //wLockDelay字段。时钟锁定延时的值
 0x00
};
////////////////////////配置描述符集合完毕//////////////////////////

/************************语言ID的定义********************/
const uint8 LanguageId[4]=
{
 0x04, //本描述符的长度
 0x03, //字符串描述符
 //0x0409为美式英语的ID
 0x09,
 0x04
};
////////////////////////语言ID完毕//////////////////////////////////

/**************************************************/
/*********        本转换结果来自         **********/
/********* Http://computer00.21ic.org    **********/
/*********        作者: 电脑圈圈         **********/
/*********         欢迎大家使用          **********/
/*********    版权所有，盗版请写明出处   **********/
/**************************************************/

//http://computer00.21ic.org/user1/2198/archives/2007/42769.html
//字符串“电脑圈圈的USB专区 Http://group.ednchina.com/93/”的Unicode编码
//8位小端格式
const uint8 ManufacturerStringDescriptor[82]={
 82,         //该描述符的长度为82字节
 0x03,       //字符串描述符的类型编码为0x03
 0x35, 0x75, //电
 0x11, 0x81, //脑
 0x08, 0x57, //圈
 0x08, 0x57, //圈
 0x84, 0x76, //的
 0x55, 0x00, //U
 0x53, 0x00, //S
 0x42, 0x00, //B
 0x13, 0x4e, //专
 0x3a, 0x53, //区
 0x20, 0x00, // 
 0x48, 0x00, //H
 0x74, 0x00, //t
 0x74, 0x00, //t
 0x70, 0x00, //p
 0x3a, 0x00, //:
 0x2f, 0x00, ///
 0x2f, 0x00, ///
 0x67, 0x00, //g
 0x72, 0x00, //r
 0x6f, 0x00, //o
 0x75, 0x00, //u
 0x70, 0x00, //p
 0x2e, 0x00, //.
 0x65, 0x00, //e
 0x64, 0x00, //d
 0x6e, 0x00, //n
 0x63, 0x00, //c
 0x68, 0x00, //h
 0x69, 0x00, //i
 0x6e, 0x00, //n
 0x61, 0x00, //a
 0x2e, 0x00, //.
 0x63, 0x00, //c
 0x6f, 0x00, //o
 0x6d, 0x00, //m
 0x2f, 0x00, ///
 0x39, 0x00, //9
 0x33, 0x00, //3
 0x2f, 0x00  ///
};
/////////////////////////厂商字符串结束/////////////////////////////

//字符串“电脑圈圈做的USB声卡”的Unicode编码”的Unicode编码
//8位小端格式
const uint8 ProductStringDescriptor[54]={
 24,         //该描述符的长度为24字节
 0x03,       //字符串描述符的类型编码为0x03
 0x35, 0x75, //电
 0x11, 0x81, //脑
 0x08, 0x57, //圈
 0x08, 0x57, //圈
 0x5a, 0x50, //做
 0x84, 0x76, //的
 0x55, 0x00, //U
 0x53, 0x00, //S
 0x42, 0x00, //B
 0xf0, 0x58, //声
 0x61, 0x53  //卡
};
////////////////////////产品字符串结束////////////////////////////

//字符串“2009-03-12”的Unicode编码
//8位小端格式
const uint8 SerialNumberStringDescriptor[22]={
22,         //该描述符的长度为22字节
0x03,       //字符串描述符的类型编码为0x03
0x32, 0x00, //2
0x30, 0x00, //0
0x30, 0x00, //0
0x39, 0x00, //9
0x2d, 0x00, //-
0x30, 0x00, //0
0x35, 0x00, //5
0x2d, 0x00, //-
0x31, 0x00, //1
0x33, 0x00  //3
};
//////////////////////产品序列号字符串结束/////////////////////////

/********************************************************************
函数功能：总线挂起中断处理函数。
入口参数：无。
返    回：无。
备    注：无。
********************************************************************/
void UsbBusSuspend(void)
{
#ifdef DEBUG0
 Prints("USB总线挂起。\r\n");
#endif
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：总线复位中断处理函数。
入口参数：无。
返    回：无。
备    注：无。
********************************************************************/
void UsbBusReset(void)
{
#ifdef DEBUG0
 Prints("USB总线复位。\r\n");
#endif
 UsbChipResetEndpoint(); //复位端点
 ConfigValue=0; //配置值初始化为0
 InterfaceValue=0; //当前接口值配置为0
 
 UsbChipSetConfig(0);  //设置芯片的配置值为0
 
 Ep1InIsBusy=0;  //复位后端点1输入缓冲区空闲。
 Ep3InIsBusy=0;  //复位后端点3输入缓冲区空闲。
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：SOF中断处理函数。
入口参数：无。
返    回：无。
备    注：无。
********************************************************************/
void UsbBusSof(void)
{
 if((*AT91C_UDP_NUM)&(1<<17))  //如果当前帧号无错误，则保存11位的帧号
 {
  CurrentFrameNumber=(*AT91C_UDP_NUM)&0x3FF;
 }
 else
 {
  CurrentFrameNumber++; //如果帧号错误，则加1
 }
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：根据pData和SendLength将数据发送到端点0的函数。
入口参数：无。
返    回：无。
备    注：无。
********************************************************************/
void UsbEp0SendData(void)
{
 //将数据写到端点中去准备发送
 //写之前要先判断一下需要发送的数据是否比端点0
 //最大长度大，如果超过端点大小，则一次只能发送
 //最大包长的数据。端点0的最大包长在DeviceDescriptor[7]
 if(SendLength>DeviceDescriptor[7])
 {
  //按最大包长度发送
  UsbChipWriteEndpointBuffer(0,DeviceDescriptor[7],pSendData);
  //发送后剩余字节数减少最大包长
  SendLength-=DeviceDescriptor[7];
  //发送一次后指针位置要调整
  pSendData+= DeviceDescriptor[7];
 }
 else
 {
  if(SendLength!=0)
  {
   //不够最大包长，可以直接发送
   UsbChipWriteEndpointBuffer(0,SendLength,pSendData);
   //发送完毕后，SendLength长度变为0
   SendLength=0;
  }
  else //如果要发送的数据包长度为0
  {
   if(NeedZeroPacket==1) //如果需要发送0长度数据
   {
    UsbChipWriteEndpointBuffer(0,0,pSendData); //发送0长度数据包
    NeedZeroPacket=0; //清需要发送0长度数据包标志
   }
  }
 }
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：USB端点0数据过程数据处理函数。
入口参数：无。
返    回：无。
备    注：该函数用来处理0端点控制传输的数据或状态过程。
********************************************************************/
void UsbEp0DataOut(void)
{
 UsbChipReadEndpointBuffer(0,16,Buffer);
 UsbChipClearBuffer(0);
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：端点0输出中断处理函数。
入口参数：无。
返    回：无。
备    注：无。
********************************************************************/
void UsbEp0Out(void)
{
 int32 L;
#ifdef DEBUG0
 Prints("USB端点0输出中断。\r\n");
#endif
 //判断是否是建立包
 if(UsbChipIsSetup(0))
 {
  L=UsbChipReadEndpointBuffer(0,16,Buffer); //读建立过程数据
  UsbChipAcknowledgeSetup(0); //应答建立包
  UsbChipClearBuffer(0);      //清缓冲区
  if(L!=8) //不是8字节的标准请求，直接返回
   return;
  
  //将缓冲数据填到设备请求的各字段中
  bmRequestType=Buffer[0];
  bRequest=Buffer[1];
  wValue=Buffer[2]+(((uint16)Buffer[3])<<8);
  wIndex=Buffer[4]+(((uint16)Buffer[5])<<8);
  wLength=Buffer[6]+(((uint16)Buffer[7])<<8);
  //下面的代码判断具体的请求，并根据不同的请求进行相关操作
  //如果D7位为1，则说明是输入请求
  if((bmRequestType&0x80)==0x80)
  {
   //根据bmRequestType的D6~5位散转，D6~5位表示请求的类型
   //0为标准请求，1为类请求，2为厂商请求。
   switch((bmRequestType>>5)&0x03)
   {
    case 0:  //标准请求
     #ifdef DEBUG0
      Prints("USB标准输入请求：");
     #endif
     //USB协议定义了几个标准输入请求，我们实现这些标准请求即可
     //请求的代码在bRequest中，对不同的请求代码进行散转
     //事实上，我们还需要对接收者进行散转，因为不同的请求接收者
     //是不一样的。接收者在bmRequestType的D4~D0位中定义。
     //我们这里为了简化操作，有些就省略了对接收者的判断。
     //例如获取描述符的请求，只根据描述符的类型来区别。
     switch(bRequest)
     {
      case GET_CONFIGURATION: //获取配置
       #ifdef DEBUG0
        Prints("获取配置。\r\n");
       #endif
      break;
      
      case GET_DESCRIPTOR:  //获取描述符
       #ifdef DEBUG0
        Prints("获取描述符——");
       #endif
       //对描述符类型进行散转，对于全速设备，
       //标准请求只支持发送到设备的设备、配置、字符串三种描述符
       switch((wValue>>8)&0xFF)
        {
         case DEVICE_DESCRIPTOR: //设备描述符
          #ifdef DEBUG0
           Prints("设备描述符。\r\n");
          #endif
          pSendData=(uint8 *)DeviceDescriptor;  //需要发送的数据
          //判断请求的字节数是否比实际需要发送的字节数多
          //这里请求的是设备描述符，因此数据长度就是
          //DeviceDescriptor[0]。如果请求的比实际的长，
          //那么只返回实际长度的数据
          if(wLength>DeviceDescriptor[0])
          {
           SendLength=DeviceDescriptor[0];
           if(SendLength%DeviceDescriptor[7]==0) //并且刚好是整数个数据包时
           {
            NeedZeroPacket=1; //需要返回0长度的数据包
           }
          }
          else
          {
           SendLength=wLength;
          }
          //将数据通过EP0返回
          UsbEp0SendData();
         break;
         
         case CONFIGURATION_DESCRIPTOR:  //配置描述符
          #ifdef DEBUG0
           Prints("配置描述符。\r\n");
          #endif
          pSendData=(uint8 *)ConfigurationDescriptor; //需要发送的数据为配置描述符
          //判断请求的字节数是否比实际需要发送的字节数多
          //这里请求的是配置描述符集合，因此数据长度就是
          //ConfigurationDescriptor[3]*256+ConfigurationDescriptor[2]。
          //如果请求的比实际的长，那么只返回实际长度的数据
          SendLength=ConfigurationDescriptor[3];
          SendLength=SendLength*256+ConfigurationDescriptor[2];
          if(wLength>SendLength)
          {
           if(SendLength%DeviceDescriptor[7]==0) //并且刚好是整数个数据包时
           {
            NeedZeroPacket=1; //需要返回0长度的数据包
           }
          }
          else
          {
           SendLength=wLength;
          }
          //将数据通过EP0返回
          UsbEp0SendData();
         break;
         
         case STRING_DESCRIPTOR:  //字符串描述符
          #ifdef DEBUG0
           Prints("字符串描述符");
          #endif
          switch(wValue&0xFF)  //根据wValue的低字节（索引值）散转
          {
           case 0:  //获取语言ID
            #ifdef DEBUG0
             Prints("(语言ID)。\r\n");
            #endif
            pSendData=(uint8 *)LanguageId;
            SendLength=LanguageId[0];
           break;
           
           case 1:  //厂商字符串的索引值为1，所以这里为厂商字符串
           #ifdef DEBUG0
             Prints("(厂商描述)。\r\n");
            #endif
            pSendData=(uint8 *)ManufacturerStringDescriptor;
            SendLength=ManufacturerStringDescriptor[0];
           break;
           
           case 2:  //产品字符串的索引值为2，所以这里为产品字符串
           #ifdef DEBUG0
             Prints("(产品描述)。\r\n");
            #endif
            pSendData=(uint8 *)ProductStringDescriptor;
            SendLength=ProductStringDescriptor[0];
           break;
           
           case 3:  //产品序列号的索引值为3，所以这里为序列号
           #ifdef DEBUG0
             Prints("(产品序列号)。\r\n");
            #endif
            pSendData=(uint8 *)SerialNumberStringDescriptor;
            SendLength=SerialNumberStringDescriptor[0];
           break;
           
           default :
            #ifdef DEBUG0
             Prints("(未知的索引值)。\r\n");
            #endif
            //对于未知索引值的请求，返回一个0长度的包
            SendLength=0;
            NeedZeroPacket=1;
           break;
          }
          //判断请求的字节数是否比实际需要发送的字节数多
          //如果请求的比实际的长，那么只返回实际长度的数据
          if(wLength>SendLength)
          {
           if(SendLength%DeviceDescriptor[7]==0) //并且刚好是整数个数据包时
           {
            NeedZeroPacket=1; //需要返回0长度的数据包
           }
          }
          else
          {
           SendLength=wLength;
          }
          //将数据通过EP0返回
          UsbEp0SendData();         
         break;

         case REPORT_DESCRIPTOR:  //报告描述符
          #ifdef DEBUG0
           Prints("报告描述符。\r\n");
          #endif
          pSendData=0; //需要发送的数据为报告描述符
          SendLength=0; //需要返回的数据长度        
          //判断请求的字节数是否比实际需要发送的字节数多
          //如果请求的比实际的长，那么只返回实际长度的数据
          if(wLength>SendLength)
          {
           if(SendLength%DeviceDescriptor[7]==0) //并且刚好是整数个数据包时
           {
            NeedZeroPacket=1; //需要返回0长度的数据包
           }
          }
          else
          {
           SendLength=wLength;
          }
          //将数据通过EP0返回
          UsbEp0SendData();
         break;
                 
         default:  //其它描述符
          #ifdef DEBUG0
           Prints("其他描述符，描述符代码：");
           PrintHex((wValue>>8)&0xFF);
           Prints("\r\n");
          #endif
         break;
        }
       break;
      
      case GET_INTERFACE: //获取接口
       #ifdef DEBUG0
        Prints("获取接口。\r\n");
       #endif
      break;
      
      case GET_STATUS: //获取状态
       #ifdef DEBUG0
        Prints("获取状态。\r\n");
       #endif
      break;
      
      case SYNCH_FRAME: //同步帧
       #ifdef DEBUG0
        Prints("同步帧。\r\n");
       #endif
      break;
      
      default:  //未定义的标准请求
       #ifdef DEBUG0
        Prints("错误：未定义的标准输入请求。\r\n");
       #endif       
      break;
     }
    break;
    
    case 1:  //类请求
     #ifdef DEBUG0
      Prints("USB类输入请求：\r\n");
     #endif
     switch(bRequest)
     {     
      default:
       #ifdef DEBUG0
        Prints("未知类请求。\r\n");
       #endif
       SendLength=0;
       NeedZeroPacket=1;
      break;
     }
    //判断请求的字节数是否比实际需要发送的字节数多
    //如果请求的比实际的长，那么只返回实际长度的数据
    if(wLength>SendLength)
    {
     if(SendLength%DeviceDescriptor[7]==0) //并且刚好是整数个数据包时
     {
      NeedZeroPacket=1; //需要返回0长度的数据包
     }
    }
    else
    {
     SendLength=wLength;
    }
    //将数据通过EP0返回
    UsbEp0SendData();
    break;
    
    case 2:  //厂商请求
     #ifdef DEBUG0
      Prints("USB厂商输入请求：\r\n");
     #endif
    break;
    
    default: //未定义的请求。这里只显示一个报错信息。
     #ifdef DEBUG0
      Prints("错误：未定义的输入请求。\r\n");
     #endif
    break;
   }
  }
  //否则说明是输出请求
  else //if(bmRequestType&0x80==0x80)之else
  {
   //根据bmRequestType的D6~5位散转，D6~5位表示请求的类型
   //0为标准请求，1为类请求，2为厂商请求。
   switch((bmRequestType>>5)&0x03)
   {
    case 0:  //标准请求
     #ifdef DEBUG0
      Prints("USB标准输出请求：");
     #endif
     //USB协议定义了几个标准输出请求，我们实现这些标准请求即可
     //请求的代码在bRequest中，对不同的请求代码进行散转
     switch(bRequest)
     {
      case CLEAR_FEATURE: //清除特性
       #ifdef DEBUG0
        Prints("清除特性。\r\n");
       #endif
      break;
      
      case SET_ADDRESS:  //设置地址
       #ifdef DEBUG0
        Prints("设置地址。地址为：");
        PrintHex(wValue&0xFF); //显示所设置的地址
        Prints("\r\n");
       #endif
       //设置地址没有数据过程，直接进入到状态过程，返回一个0长度的数据包
       SendLength=0;
       NeedZeroPacket=1;
       //将数据通过EP0返回
       UsbEp0SendData();
       UsbChipWriteAddress(wValue&0xFF); //wValue中的低字节是设置的地址值
      break;
      
      case SET_CONFIGURATION: //设置配置
       #ifdef DEBUG0
        Prints("设置配置。\r\n");
       #endif
       //wValue的低字节为配置的值，如果该值为非0，才能使能非0端点。        
       //保存当前配置值
       ConfigValue=wValue&0xFF;
       
       //根据配置值设置LED状态，当配置值为非0时，LED2亮
       if(ConfigValue!=0) OnLed2();
       else OffLed2();
       
       //设置芯片的config值     
       UsbChipSetConfig(ConfigValue); 
         
       //使能非0端点。非0端点只有在设置为非0的配置后才能使能。
       UsbChipSetEndpointEnable(ConfigValue);
      
       //返回一个0长度的状态数据包
       SendLength=0;
       NeedZeroPacket=1;
       //将数据通过EP0返回
       UsbEp0SendData();
      break;
      
      case SET_DESCRIPTOR: //设置描述符
       #ifdef DEBUG0
        Prints("设置描述符。\r\n");
       #endif
      break;
      
      case SET_FEATURE: //设置特性
       #ifdef DEBUG0
        Prints("设置特性。\r\n");
       #endif
      break;

      case SET_INTERFACE: //设置接口
       #ifdef DEBUG0
        Prints("设置接口。\r\n");
       #endif
       //保存当前接口值
       InterfaceValue=wValue&0xFF;
       
       //根据当前接口值，设置LED状态，当接口值为非0时，LED3亮，此时不能拔下USB，否则可能会死机
       if(InterfaceValue!=0) OnLed3();
       else OffLed3();
       
       //返回一个0长度的状态数据包
       SendLength=0;
       NeedZeroPacket=1;
       //将数据通过EP0返回
       UsbEp0SendData();       
      break;      
      
      default:  //未定义的标准请求
       #ifdef DEBUG0
        Prints("错误：未定义的标准输出请求。\r\n");
        Prints("未知请求。\r\n");        
       #endif
        //只需要返回一个0长度的数据包即可
        SendLength=0;
        NeedZeroPacket=1;
        //将数据通过EP0返回
        UsbEp0SendData();
      break;
     }
    break;
    
    case 1:  //类请求
     #ifdef DEBUG0
      Prints("USB类输出请求：");
     #endif
     switch(bRequest)
     {
      case SET_IDLE:
       #ifdef DEBUG0
        Prints("设置空闲。\r\n");
       #endif
       //只需要返回一个0长度的数据包即可
       SendLength=0;
       NeedZeroPacket=1;
       //将数据通过EP0返回
       UsbEp0SendData();
      break;
      
      default:
       #ifdef DEBUG0
        Prints("未知请求。\r\n");
       #endif        
        //只需要返回一个0长度的数据包即可
        SendLength=0;
        NeedZeroPacket=1;
        //将数据通过EP0返回
        UsbEp0SendData();
      break;
     }
    break;
    
    case 2:  //厂商请求
     #ifdef DEBUG0
      Prints("USB厂商输出请求：\r\n");
     #endif
    break;
    
    default: //未定义的请求。这里只显示一个报错信息。
     #ifdef DEBUG0
      Prints("错误：未定义的输出请求。\r\n");
     #endif
    break;
   }
  }
 }
 //普通数据输出
 else //判断setup包之else
 {
  ClrCsr(0,1<<7);  //设置DIR为0
  UsbEp0DataOut(); //处理标准请求的数据部分
 }
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：端点0输入中断处理函数。
入口参数：无。
返    回：无。
备    注：无。
********************************************************************/
void UsbEp0In(void)
{
#ifdef DEBUG0
 Prints("USB端点0输入中断。\r\n");
#endif
 //清除端点0的输入完成中断标志位
 ClrCsr(0,1<<0);
 
 //发送剩余的字节数
 UsbEp0SendData();
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：端点1输入中断处理函数。
入口参数：无。
返    回：无。
备    注：无。
********************************************************************/
void UsbEp1In(void)
{
#ifdef DEBUG0
 Prints("USB端点1输入中断。\r\n");
#endif
 //清除端点1的输入完成中断标志位
 ClrCsr(1,1<<0);
 //端点1输入处于空闲状态
 Ep1InIsBusy=0;
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：端点2输出中断处理函数。
入口参数：无。
返    回：无。
备    注：无。
********************************************************************/
void UsbEp2Out(void)
{
 uint8 Len;
 volatile unsigned int reg;
 
#ifdef DEBUG0
 Prints("USB端点2输出中断。\r\n");
#endif
 
 //清除端点2输出的中断标志位
 //无操作
  
 //读取端点2的数据。返回值为实际读到的数据字节数
 Len=UsbChipReadEndpointBuffer(2,64,((uint8 *)AudioOutBuf)+AudioOutByteCount);
 //清除端点缓冲区
 //不调用UsbChipClearBuffer(2)函数，因为可能会导致死循环 
 reg = AT91C_UDP_CSR[2];
 reg|= REG_NO_EFFECT_1_ALL;
 if(CurrentBank[2]==0) //清除BANK0
 {
  reg&= ~(1<<1);
 }
 else   //清除BANK1
 {
  reg&=~(1<<6);
 }
 AT91C_UDP_CSR[2] = reg;
 
 if(Len!=64)  //因为处理时间较长，会导致缓冲区覆盖，从而读取数据错误，
 //这时干脆将AudioOutByteCount清为0，重新采集数据
 {
  AudioOutByteCount=0;
 }
 else
 {
  AudioOutByteCount+=Len;  //字节累加
 }
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：端点3输入中断处理函数。
入口参数：无。
返    回：无。
备    注：无。
********************************************************************/
void UsbEp3In(void)
{
#ifdef DEBUG0
 Prints("USB端点3输入中断。\r\n");
#endif
 //清除端点3的输入完成中断标志位
 ClrCsr(3,1<<0);
 //端点3输入处于空闲状态
 Ep3InIsBusy=0;
}
////////////////////////End of function//////////////////////////////