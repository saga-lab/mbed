#include "mbed.h"
#include <algorithm>
#include <stdlib.h>
DigitalOut SN74LV595_DIN(p14);
DigitalOut SN74LV595_RCLK(p13);
DigitalOut SN74LV595_CLR(p12);
DigitalOut SN74LV595_CLK(p11);
DigitalIn  SN74LV595_DOUT(p10); //Shift register output, so normally it is not connected.
AnalogIn ain(p20);
Serial pc(USBTX, USBRX);
const int SERIAL_SPEED = 921600; //fastest

//DAAD
SPI spiDAAD(p5, p6, p7);     // mosi(master output slave input, miso(not connected), clock signal
DigitalOut DA_sync(p8);        //chip select for AD5452
DigitalOut AD_cs(p9);        //chip select for AD7276

//Other I/O
BusOut myleds(LED1, LED2, LED3, LED4);

// stimulation mode
#define DA_TEST 0xFA //sinusoidal wave mode to test DA
#define Zara 0x41
#define Gotsu 0x42
#define Tsubu 0x43
#define Gowa 0x44
#define Tsuru 0x45
#define Jori 0x46
int Mode = DA_TEST;

const int ELECTRODE_NUM = 16;

short stim_pattern[ELECTRODE_NUM] = { 0 };
short impedance[ELECTRODE_NUM] = { 0 };
short twod_stim_pattern[ELECTRODE_NUM] = { 0 };

double freq = 0.0;
double amp = 0.0;
double fc = 100.0;
double fs = 50.0;
double df = 100.0;
Timer timer;
int haha = 1;

bool AccessDeny = false;
bool trian = true;
bool delta_flag = false;
bool delta_oldflag = false;
int delta_cnt = 0;

void SN74LV595FastScan(int usWhichPin)
{
    int ii, pin;
    static int pos;

    SN74LV595_RCLK = 0;
    if (usWhichPin == 0) { //set 01�iHigh)
        SN74LV595_DIN = 0;
        SN74LV595_CLK = 1;
        SN74LV595_CLK = 0;
        SN74LV595_DIN = 1;
        SN74LV595_CLK = 1;
        SN74LV595_CLK = 0;
        pos = 0;
    } else {
        pin = usWhichPin - pos;
        for (ii = 0; ii < pin; ii++) {//set 10 (GND)
            SN74LV595_DIN = 1;
            SN74LV595_CLK = 1;
            SN74LV595_CLK = 0;
            SN74LV595_DIN = 0;
            SN74LV595_CLK = 1;
            SN74LV595_CLK = 0;
        }
        pos = usWhichPin;
    }
    //Load S/R
    SN74LV595_RCLK = 1;
    SN74LV595_RCLK = 0;
}

void SN74LV595AllScan()
{
    int ii, pin;

    SN74LV595_RCLK = 0;
    for (int i = 1; i < ELECTRODE_NUM+1; i++) {
        SN74LV595_DIN = ((twod_stim_pattern[i] ==1)?1:0);
        SN74LV595_CLK = 1;
        SN74LV595_CLK = 0;
        SN74LV595_DIN = ((twod_stim_pattern[i] ==1)?0:1);
        SN74LV595_CLK = 1;
        SN74LV595_CLK = 0;
    }
    //Load S/R
    SN74LV595_RCLK = 1;
    SN74LV595_RCLK = 0;
}

void SN74LV595Clear()
{
    SN74LV595_CLR = 0;
    SN74LV595_RCLK  = 0;
    SN74LV595_CLK = 0;
    SN74LV595_CLK = 1;
    SN74LV595_CLK = 0;
    SN74LV595_CLR = 1;
}

void SN74LV595Init(int TotalPin)
{
    int ii;

    SN74LV595_CLR = 1;
    SN74LV595_CLK = 0;
    SN74LV595_RCLK = 0;
    for (ii = 0; ii < TotalPin; ii++) {
        SN74LV595_DIN = 1;
        SN74LV595_CLK = 1;
        SN74LV595_CLK = 0;
        SN74LV595_DIN = 0;
        SN74LV595_CLK = 1;
        SN74LV595_CLK = 0;
    }
    //Load S/R
    SN74LV595_RCLK = 1;
    SN74LV595_RCLK = 0;
}

short DAAD(short DA)
{
    short AD;

    //enable
    DA_sync = 0;
    AD_cs = 0;
    //simultaneous DA and AD
    AD = spiDAAD.write(DA << 2);
    //disable
    DA_sync = 1;
    AD_cs = 1;

    return AD >> 2;//bottom 2bits are unnecessary
}

void DAADinit()
{
    //Setup SPI, 16bit, falling edge, 48MHz clock
    spiDAAD.format(16, 2);
    spiDAAD.frequency(48000000);
}

void SerialReceiveInterrupt()
{
    int rcv, i;
    unsigned char data[255];
    int datai[255];

    rcv = pc.getc();
    if (rcv == DA_TEST) {
        Mode = DA_TEST;
        myleds = 1;
    } else if (rcv == Zara) {
        //freq = 180.0;
        Mode = Zara;
        myleds = 2;
    } else if (rcv == Gowa) {
        //freq = 50.0;
        Mode = Gowa;
        myleds = 2;
    } else if (rcv == Tsuru) {
        //freq = 440.0;
        Mode = Tsuru;
        myleds = 2;
    } else if (rcv == Gotsu) {
        //freq = 20.0;
        Mode = Gotsu;
        myleds = 2;
    } else if (rcv == Tsubu) {
        //freq = 12.0;
        delta_flag=false;
        delta_oldflag=false;
        Mode = Tsubu;
        myleds = 2;
    } else if (rcv == Jori) {
        //freq = 125.0;
        delta_flag=false;
        delta_oldflag=false;
        Mode = Jori;
        myleds = 2;
    } else if(rcv == 0x2E) {
        amp -= 10.0;
        if(amp <0.0)
            amp = 0.0;
    } else if(rcv == 0x2F) {
        amp += 10.0;
        if(amp > 500.0)
            amp = 500.0;
    } else if(rcv == 0x7A) {
        freq += 5.0;
        if(freq > 500.0)
            freq = 500.0;
    } else if(rcv == 0x7B) {
        freq -= 5.0;
        if(freq < 0.0)
            freq = 0.0;
    }
}

float SerialLeap()
{
    float speed = 0.0;
    if (pc.readable()) {
        char leap[4]; // 受信用バッファー
        pc.read(leap, 4); // 4バイト受信

        // leapをfloat型に変換
        float receivedSpeed;
        memcpy(&receivedSpeed, leap, sizeof(float));
        speed = receivedSpeed;
    }
    
    return speed;
}

int main()
{
    double t = 0.0;
    char rcv;
    int i;
    short AD;
    float cntrspeed = 0.0;

    int prevmode;

    DAADinit();
    SN74LV595Init(ELECTRODE_NUM);
    pc.baud(921600);
    pc.attach(SerialReceiveInterrupt);
    timer.start();

    for (int t = 0; t < 8; t++) {
        myleds = 1 << (t % 4);
        wait_us(50000);
    }
    myleds = 1;

    while (1) {
        cntrspeed = SerialLeap();
    }

    while (1) {
        if (Mode == DA_TEST) {
            t = (double)timer.read_us() * 0.000001;
			AD = DAAD((short)(200.0 * (1.0 + sin(2.0 * 3.1415926 * freq * t))));
        } else if (Mode == Zara) {
            AD = DAAD((short)(amp * (1.0 + sin(2.0 * 3.1415926 * freq * cntrspeed))));
            prevmode = Mode;
        } else if (Mode == Gowa) {
            if(sin(2.0 * 3.1415926 * freq * cntrspeed) > 0.0)
                AD = DAAD((short)amp * 2);
            else
                AD = DAAD((short)0);
            prevmode = Mode;
        } else if (Mode == Tsuru) {
            if(sin(2.0 * 3.1415926 * freq * cntrspeed) > 0.995)
                delta_flag = true;
            else
                delta_flag = false;
            if(delta_cnt == 8) {
                delta_cnt = 0;
                AD = DAAD((short)0);
            }
            if(delta_cnt == 7) {
                delta_cnt = 8;
            }
            if(delta_cnt == 6) {
                delta_cnt = 7;
            }
            if(delta_cnt == 5) {
                delta_cnt = 6;
            }
            if(delta_cnt == 4) {
                delta_cnt = 5;
            }
            if(delta_cnt == 3) {
                delta_cnt = 4;
            }
            if(delta_cnt == 2) {
                delta_cnt = 3;
            }
            if(delta_cnt == 1) {
                delta_cnt = 2;
            }
            if(delta_oldflag == false && delta_flag == true) {
                delta_cnt = 1;
                AD = DAAD((short) amp * 2);
            }
            delta_oldflag = delta_flag;

            prevmode = Mode;
        } else if (Mode == Gowa) {
            if(sin(2.0 * 3.1415926 * freq * cntrspeed) > 0.0)
                AD = DAAD((short)amp * 2);
            else
                AD = DAAD((short)0);
            prevmode = Mode;
        } else if (Mode == Gotsu) {
            int d = (int)(cntrspeed * 100 * freq) % 100;
            double dd = (double)d / 100;
            dd *= 2 * amp;
            if(dd > 600)
                dd = 600;
            AD = DAAD((short)dd);
            prevmode = Mode;
        } else if (Mode == Tsubu) {
            AD = DAAD((short)(amp * (1.0 + sin(2.0 * 3.1415926 * freq * cntrspeed)))); 
            prevmode = Mode;
        } else if (Mode == Jori) {
            if(sin(2.0 * 3.1415926 * freq * cntrspeed) >0.995) 
                delta_flag = true;
            else
                delta_flag = false;
            if(delta_cnt == 8) {
                delta_cnt = 0;
                AD = DAAD((short)0);
            }
            if(delta_cnt == 7) {
                delta_cnt = 8;
            }
            if(delta_cnt == 6) {
                delta_cnt = 7;
            }
            if(delta_cnt == 5) {
                delta_cnt = 6;
            }
            if(delta_cnt == 4) {
                delta_cnt = 5;
            }
            if(delta_cnt == 3) {
                delta_cnt = 4;
            }
            if(delta_cnt == 2) {
                delta_cnt = 3;
            }
            if(delta_cnt == 1) {
                delta_cnt = 2;
            }
            if(delta_oldflag == false && delta_flag == true) {
                delta_cnt = 1;
                AD = DAAD((short) amp * 2);
            }
            delta_oldflag = delta_flag;

            prevmode = Mode;
        }
    }
}
