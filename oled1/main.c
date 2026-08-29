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
#include <errno.h>

// 修正管道路径：/tmp 不是 /temp，和python统一命名
#define FIFO_PATH "/temp/oled1_fifo"

int main(void)
{
    int fifo_fd;
    int all_num = 0;
    int ret;
    ret = OLED_HW_I2C_Init();
    if(ret != 0)
    {
        return -1;
    }
    OLED_Init();
    OLED_Clear();
    while(1)
    {
        fifo_fd = open(FIFO_PATH, O_RDONLY);
        if(fifo_fd < 0)
        {
            printf("【警告】管道打开失败，errno=%d，200ms后重试\n", errno);
            usleep(200000);
            continue;
        }
        ssize_t read_len = read(fifo_fd, &all_num, sizeof(int));
        close(fifo_fd);

        if(read_len <= 0)
        {
            printf("【警告】管道无有效数据，跳过刷新\n");
            continue;
        }
        OLED_Clear();
        OLED_Printf(0, 0, OLED_8X16, "剩余车位:%d", all_num);
        if(all_num == 0)
        {
            OLED_ShowString( 30, 30, "车位已满", OLED_8X16);
           
        }
        OLED_Update();
    }
    // 不会执行到这里
    OLED_HW_I2C_Deinit();
    return 0;
}