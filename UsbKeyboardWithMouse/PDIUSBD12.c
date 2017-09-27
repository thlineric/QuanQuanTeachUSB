/******************************************************************
   本程序只供学习使用，未经作者许可，不得用于其它任何用途

        欢迎访问我的USB专区：http://group.ednchina.com/93/
        欢迎访问我的blog：   http://www.ednchina.com/blog/computer00
                             http://computer00.21ic.org

        感谢PCB赞助商——电子园： http://bbs.cepark.com/

PDIUSBD12.C  file

作者：电脑圈圈
建立日期: 2008.06.27
修改日期: 2008.07.10
版本：V1.1
版权所有，盗版必究。
Copyright(C) 电脑圈圈 2008-2018
All rights reserved            
*******************************************************************/

#include <AT89x52.H>
#include "MyType.h"
#include "PDIUSBD12.H"
#include "config.h"
#include "UART.h"

/********************************************************************
函数功能：D12写命令。
入口参数：Command：一字节命令。
返    回：无。
备    注：无。
********************************************************************/
void D12WriteCommand(uint8 Command)
{
 D12SetCommandAddr();  //设置为命令地址
 D12ClrWr(); //WR置低
 D12SetPortOut(); //将数据口设置为输出状态（注意这里为空宏，移植时可能有用）
 D12SetData(Command);  //输出命令到数据口上
 D12SetWr(); //WR置高
 D12SetPortIn(); //将数据口设置为输入状态，以备后面输入使用
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：读一字节D12数据。
入口参数：无。
返    回：读回的一字节。
备    注：无。
********************************************************************/
uint8 D12ReadByte(void)
{
 uint8 temp;
 D12SetDataAddr(); //设置为数据地址
 D12ClrRd(); //RD置低
 temp=D12GetData(); //读回数据
 D12SetRd(); //RD置高
 return temp; //返回读到数据
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：读D12的ID。
入口参数：无。
返    回：D12的ID。
备    注：无。
********************************************************************/
uint16 D12ReadID(void)
{
 uint16 id;
 D12WriteCommand(Read_ID); //写读ID命令
 id=D12ReadByte(); //读回ID号低字节
 id|=((uint16)D12ReadByte())<<8; //读回ID号高字节
 return id;
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：写一字节D12数据。
入口参数：Value：要写的一字节数据。
返    回：无。
备    注：无。
********************************************************************/
void D12WriteByte(uint8 Value)
{
 D12SetDataAddr(); //设置为数据地址
 D12ClrWr(); //WR置低
 D12SetPortOut(); //将数据口设置为输出状态（注意这里为空宏，移植时可能有用）
 D12SetData(Value); //写出数据
 D12SetWr(); //WR置高
 D12SetPortIn(); //将数据口设置为输入状态，以备后面输入使用
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：读取D12最后传输状态寄存器的函数。
入口参数：Endp：端点号。
返    回：端点的最后传输状态。
备    注：该操作将清除该端点的中断标志位。
********************************************************************/
uint8 D12ReadEndpointLastStatus(uint8 Endp)
{
 D12WriteCommand(0x40+Endp); //读取端点最后状态的命令
 return D12ReadByte();
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：选择端点的函数，选择一个端点后才能对它进行数据操作。
入口参数：Endp：端点号。
返    回：无。
备    注：无。
********************************************************************/
void D12SelectEndpoint(uint8 Endp)
{
 D12WriteCommand(0x00+Endp); //选择端点的命令
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：清除接收端点缓冲区的函数。
入口参数：无。
返    回：无。
备    注：只有使用该函数清除端点缓冲后，该接收端点才能接收新的数据包。
********************************************************************/
void D12ClearBuffer(void)
{
 D12WriteCommand(D12_CLEAR_BUFFER);
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：应答建立包的函数。
入口参数：无。
返    回：无。
备    注：无。
********************************************************************/
void D12AcknowledgeSetup(void)
{
 D12SelectEndpoint(1); //选择端点0输入
 D12WriteCommand(D12_ACKNOWLEDGE_SETUP); //发送应答设置到端点0输入
 D12SelectEndpoint(0); //选择端点0输出
 D12WriteCommand(D12_ACKNOWLEDGE_SETUP); //发送应答设置到端点0输出
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：读取端点缓冲区函数。
入口参数：Endp：端点号；Len：需要读取的长度；Buf：保存数据的缓冲区。
返    回：实际读到的数据长度。
备    注：无。
********************************************************************/
uint8 D12ReadEndpointBuffer(uint8 Endp, uint8 Len, uint8 *Buf)
{
 uint8 i,j;
 D12SelectEndpoint(Endp); //选择要操作的端点缓冲
 D12WriteCommand(D12_READ_BUFFER); //发送读缓冲区的命令
 D12ReadByte();   //该字节数据是保留的，不用。
 j=D12ReadByte(); //这里才是实际的接收到的数据长度
 if(j>Len) //如果要读的字节数比实际接收到的数据长
 {
  j=Len;  //则只读指定的长度数据
 }
#ifdef DEBUG1 //如果定义了DEBUG1，则需要显示调试信息
 Prints("读端点");
 PrintLongInt(Endp/2); //端点号。由于D12特殊的端点组织形式，
                       //这里的0和1分别表示端点0的输出和输入；
                       //而2、3分别表示端点1的输出和输入；
                       //3、4分别表示端点2的输出和输入。
                       //因此要除以2才显示对应的端点。
 Prints("缓冲区");
 PrintLongInt(j);      //实际读取的字节数
 Prints("字节。\r\n");
#endif
 for(i=0;i<j;i++)
 {
  //这里不直接调用读一字节的函数，而直接在这里模拟时序，可以节省时间
  D12ClrRd();  //RD置低
  *(Buf+i)=D12GetData(); //读一字节数据
  D12SetRd();  //RD置高
#ifdef DEBUG1
  PrintHex(*(Buf+i)); //如果需要显示调试信息，则显示读到的数据
  if(((i+1)%16)==0)Prints("\r\n"); //每16字节换行一次
#endif
 }
#ifdef DEBUG1
 if((j%16)!=0)Prints("\r\n"); //换行。
#endif
 return j; //返回实际读取的字节数。
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：使能发送端点缓冲区数据有效的函数。
入口参数：无。
返    回：无。
备    注：只有使用该函数使能发送端点数据有效之后，数据才能发送出去。
********************************************************************/
void D12ValidateBuffer(void)
{
 D12WriteCommand(D12_VALIDATE_BUFFER);
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：将数据写入端点缓冲区函数。
入口参数：Endp：端点号；Len：需要发送的长度；Buf：保存数据的缓冲区。
返    回：Len的值。
备    注：无。
********************************************************************/
uint8 D12WriteEndpointBuffer(uint8 Endp,uint8 Len,uint8 * Buf)
{
 uint8 i;
 D12SelectEndpoint(Endp); //选择端点
 D12WriteCommand(D12_WRITE_BUFFER); //写Write Buffer命令
 D12WriteByte(0); //该字节必须写0
 D12WriteByte(Len);  //写需要发送数据的长度
 
#ifdef DEBUG1 //如果定义了DEBUG1，则需要显示调试信息
 Prints("写端点");
 PrintLongInt(Endp/2); //端点号。由于D12特殊的端点组织形式，
                       //这里的0和1分别表示端点0的输出和输入；
                       //而2、3分别表示端点1的输出和输入；
                       //3、4分别表示端点2的输出和输入。
                       //因此要除以2才显示对应的端点。
 Prints("缓冲区");
 PrintLongInt(Len);    //写入的字节数
 Prints("字节。\r\n");
#endif
 D12SetPortOut(); //将数据口设置为输出状态（注意这里为空宏，移植时可能有用）
 for(i=0;i<Len;i++)
 {
  //这里不直接调用写一字节的函数，而直接在这里模拟时序，可以节省时间
  D12ClrWr(); //WR置低  
  D12SetData(*(Buf+i)); //将数据放到数据线上
  D12SetWr();  //WR置高，完成一字节写
#ifdef DEBUG1
  PrintHex(*(Buf+i));  //如果需要显示调试信息，则显示发送的数据
  if(((i+1)%16)==0)Prints("\r\n"); //每16字节换行一次
#endif
  }
#ifdef DEBUG1
 if((Len%16)!=0)Prints("\r\n"); //换行
#endif
 D12SetPortIn(); //数据口切换到输入状态
 D12ValidateBuffer(); //使端点数据有效
 return Len; //返回Len
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：设置地址函数。
入口参数：Addr：要设置的地址值。
返    回：无。
备    注：无。
********************************************************************/
void D12SetAddress(uint8 Addr)
{
 D12WriteCommand(D12_SET_ADDRESS_ENABLE); //写设置地址命令
 D12WriteByte(0x80 | Addr); //写一字节数据：使能及地址
}
////////////////////////End of function//////////////////////////////

/********************************************************************
函数功能：使能端点函数。
入口参数：Enable: 是否使能。0值为不使能，非0值为使能。
返    回：无。
备    注：无。
********************************************************************/
void D12SetEndpointEnable(uint8 Enable)
{
 D12WriteCommand(D12_SET_ENDPOINT_ENABLE);
 if(Enable!=0)
 {
  D12WriteByte(0x01); //D0为1使能端点
 }
 else
 {
  D12WriteByte(0x00); //不使能端点
 }
}
////////////////////////End of function//////////////////////////////