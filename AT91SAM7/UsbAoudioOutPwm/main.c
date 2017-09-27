/******************************************************************
   本程序只供学习使用，未经作者许可，不得用于其它任何用途

        欢迎访问我的USB专区：http://group.ednchina.com/93/
        欢迎访问我的blog：   http://www.ednchina.com/blog/computer00
                             http://computer00.21ic.org

main.c  file

作者：Computer-lov
建立日期: 2009-03-04
修改日期: 2009-03-10
版本：V1.0
版权所有，盗版必究。
Copyright(C) Computer-lov 2009-2019
All rights reserved
*******************************************************************/

#include <AT91SAM7S64.H>
#include "SystemClock.h"
#include "uart.h"
#include "UsbCore.h"
#include "AT91SAMxUSB.h"
#include "config.h"
#include "led.h"
#include "Timer.h"
#include "PWM.H"

const uint8 HeadTable[][74]={
"********************************************************************\r\n",
"******            21IC DIY U盘学习板 之USB 音频输出设备       ******\r\n",
"******                     AT91SAM7S64 CPU                    ******\r\n",
"******                  建立日期：",__DATE__,"                 ******\r\n",
"******                   建立时间：",__TIME__,"                   ******\r\n",
"******                    作者：电脑圈圈                      ******\r\n",
"******                    欢迎访问作者的                      ******\r\n",
"******           USB专区：http://group.ednchina.com/93/       ******\r\n",
"******      BLOG1：http://www.ednchina.com/blog/computer00    ******\r\n",
"******                BLOG2：http://computer00.21ic.org       ******\r\n",
"******         通过USB口输出音频，然后通过PWM产生音频信号     ******\r\n",
"******  请在音频设备中选中该设备然后播放一个音乐文件进行测试  ******\r\n",
"********************************************************************\r\n",
};
extern int16 AudioOutBuf1[32];  //缓冲区1
extern int16 AudioOutBuf2[32];  //缓冲区2
/********************************************************************
函数功能：主函数。
入口参数：无。
返    回：无。
备    注：无。
********************************************************************/
void main(void)
{
 uint8 Ep2NoDataOut=0;  //标志是否有数据输出
 
 uint16 OldFrameNumber=0; //上一次保存的帧号
 
 int i;
 
 int InterruptSource;
 
 SystemClockInit(); //系统时钟初始化
 LedInit();         //LED对应的管脚初始化
 Uart0Init();       //串口0初始化
 
 for(i=0;i<17;i++)   //显示头信息
 {
  Prints((uint8 *)HeadTable[i]);
 }

 PwmInit();  //PWM初始化
 
 Timer0Init(); //定时器0初始化

 UsbChipInit();  //初始化USB部分
 
 while(1)
 {
  InterruptSource=(*AT91C_UDP_ISR)&(0x0F|(1<<8)|(1<<12)|(1<<11)); //取出需要的中断
  if(InterruptSource) //如果监视的中断发生
  {
   if(InterruptSource&(1<<8))
   {
    *AT91C_UDP_ICR=1<<8; //清除中断
    UsbBusSuspend(); //总线挂起中断处理
   }
   if(InterruptSource&(1<<12))
   {
    *AT91C_UDP_ICR=1<<12; //清除中断
    UsbBusReset();   //总线复位中断处理
   }
   if(InterruptSource&(1<<0))
   {
    if(AT91C_UDP_CSR[0]&((1<<1)|(1<<2)|(1<<6)))  //如果是SETUP包、缓冲未空等
    {
     UsbEp0Out();     //端点0输出中断处理
    }
    if(AT91C_UDP_CSR[0]&(1<<0))  //如果是端点0输入完成
    {
     UsbEp0In();     //端点0输入中断处理
    }
   }
   if(InterruptSource&(1<<11))  //SOF中断
   {
    *AT91C_UDP_ICR=1<<11; //清除中断
    UsbBusSof();  //SOF中断处理
   }
   if(ConfigValue!=0)
   {
    if(InterruptSource&(1<<1))
    {
     UsbEp1In();      //端点1输入中断处理
    }
    if(InterruptSource&(1<<2))
    {
     UsbEp2Out();     //端点2输出中断处理
     Ep2NoDataOut=0;  //说明输出了数据
    }
    if(InterruptSource&(1<<3))
    {
     UsbEp3In();      //端点3输入中断处理
    }
    if((uint16)(CurrentFrameNumber-OldFrameNumber)>=8)  //每8个帧处理一次
    {
     OldFrameNumber=CurrentFrameNumber;  //保存当前帧号
     if(Ep2NoDataOut)  //如果没有数据，则
     {
      //将缓冲区内数据慢慢减小
      for(i=0;i<32;i++)
      {
       AudioOutBuf1[i]/=2;
       AudioOutBuf2[i]/=2;
      }
     }
     Ep2NoDataOut=1;  //设置为没有数据输出
    }
   }
  }
 }
}
/////////////////////////End of function/////////////////////////////

