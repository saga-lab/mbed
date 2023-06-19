#include "mbed.h"

Serial pc(USBTX, USBRX);

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

        // speedをバイト列に変換
        char speedBytes[sizeof(float)];
        memcpy(speedBytes, &speed, sizeof(float));

        // ビッグエンディアンに変換
        char bigEndianBytes[sizeof(float)];
        bigEndianBytes[0] = speedBytes[3];
        bigEndianBytes[1] = speedBytes[2];
        bigEndianBytes[2] = speedBytes[1];
        bigEndianBytes[3] = speedBytes[0];

        // バイト列をPCに送信
        pc.write(bigEndianBytes, sizeof(float));
    }

    return speed;

}

int main() {
    float cntrspeed = 0.0;

    pc.baud(921600);

    while (1) {
        cntrspeed = SerialLeap();
    }
}