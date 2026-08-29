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

// 修正路径：Linux临时目录是/tmp，不是/temp
#define FIFO_PATH "/temp/oled2_fifo"

int main(void)
{
    
int data_buf[3];
    int fifo_fd;
    int left_num1;
    int straight_num1;
    int right_num1;
    int ret;
   OLED_HW_I2C_Init();
    OLED_Init();
 
    OLED_Clear();

    // 绘制顶部线条，全部带&oled2
    OLED_DrawLine(10,25,30,25);
    OLED_DrawLine( 10,25,20,20);
    OLED_DrawLine( 10,25,20,30);
    OLED_DrawLine( 68,30,68,10);
    OLED_DrawLine(68,10,63,20);
    OLED_DrawLine( 68,10,73,20);
    OLED_DrawLine( 98,25,118,25);
    OLED_DrawLine( 118,25,108,20);
    OLED_DrawLine( 118,25,108,30);
    OLED_Update();
  
    
    while(1)
    {
         fifo_fd = open(FIFO_PATH, O_RDONLY);
        if(fifo_fd < 0)
        {
            printf("【警告】管道打开失败，errno=%d，200ms重试\n", errno);
            usleep(200000);
            continue;
        }

         ssize_t read_len = read(fifo_fd, data_buf, sizeof(data_buf));
        close(fifo_fd);


        if(read_len <= 0)
        {
            printf("【警告】未读到有效数据，跳过刷新\n");
            continue;
        }
        // 拆分数据
        left_num1 = data_buf[0];
        straight_num1 = data_buf[1];
        right_num1 = data_buf[2];
        OLED_Printf(20,40, 8,"%d",left_num1);
        OLED_Printf( 64,40, 8,"%d",straight_num1);
        OLED_Printf( 108,40, 8,"%d",right_num1);
        OLED_Update();
       
    }
    
    OLED_HW_I2C_Deinit();
    return 0;
}