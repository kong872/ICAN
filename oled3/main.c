#include "oled.h"
#include "oled_data.h"
//系统头文件
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <sys/stat.h>
#include <errno.h>


// 存储从管道读取的两组车位数据
int left_num2 = 0;
int right_num2 = 0;
// FIFO管道路径
#define FIFO_OLED3_PATH "/temp/oled3_fifo"

// 从管道读取left_num2、right_num2
int read_oled3_fifo(void)
{
    int fd;
    int ret;
    int buf[2]; // buf[0]=left_num2, buf[1]=right_num2

    // 阻塞打开管道，等待写端usb.py写入数据
    fd = open(FIFO_OLED3_PATH, O_RDONLY);
    if (fd < 0)
    {
        perror("open oled3 fifo failed");
        return -1;
    }

    // 读取两个int，共8字节
    ret = read(fd, buf, sizeof(buf));
    close(fd);

    if (ret != sizeof(buf))
    {
        printf("fifo read data incomplete, ret=%d\n", ret);
        return -2;
    }

    // 赋值给全局变量
    left_num2 = buf[0];
    right_num2 = buf[1];
    return 0;
}

int main(void)
{
    int read_res;
    OLED_HW_I2C_Init();
    OLED_Init();
    OLED_Clear();
 
    // 固定绘制左右箭头线条（只画一次）
    OLED_DrawLine(10,25,30,25);
    OLED_DrawLine(10,25,20,20);
    OLED_DrawLine(10,25,20,30);
    OLED_DrawLine(98,25,118,25);
    OLED_DrawLine(118,25,108,20);
    OLED_DrawLine(118,25,108,30);
    OLED_Update();

    while(1)
    {
        // 读取管道数据
        read_res = read_oled3_fifo();
        if (read_res == 0)
        {
            // 读取成功，清屏重绘数字
            OLED_Clear();
            // 重新绘制箭头
            OLED_DrawLine(10,25,30,25);
            OLED_DrawLine(10,25,20,20);
            OLED_DrawLine(10,25,20,30);
            OLED_DrawLine(98,25,118,25);
            OLED_DrawLine(118,25,108,20);
            OLED_DrawLine(118,25,108,30);
            // 打印左右2区域空位数量
            OLED_Printf(20,40,8,"%d",left_num2);
            OLED_Printf(108,40,8,"%d",right_num2);
            OLED_Update();
        }
        // 短暂延时降低CPU占用
        usleep(50000);
    }
}