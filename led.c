#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define DS 178              //GPIO5_C2
#define ST 176              //GPIO5_C0
#define SH 174              //GPIO5_B6
#define FIFO_PATH "/temp/led_fifo"
uint8_t led_data=0;
static int gpio_export(int gpio_num)
{
    int fd=open("/sys/class/gpio/export",O_WRONLY);
    if(fd<0)
    {
        return -1;
    }
    char buf[16];
    sprintf(buf,"%d",gpio_num);
    write(fd,buf,strlen(buf));
    close(fd);
    return 0;
}
// 设置GPIO方向 out=1 in=0
static int gpio_set_dir(int gpio_num, int is_out)
{
    char path[64];
    sprintf(path, "/sys/class/gpio/gpio%d/direction", gpio_num);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    write(fd, is_out ? "out" : "in", 3);
    close(fd);
    return 0;
}
// 设置GPIO电平 1高 0低
static void gpio_set_level(int gpio_num, int level)
{
    char path[64];
    sprintf(path, "/sys/class/gpio/gpio%d/value", gpio_num);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return;
    write(fd, level ? "1" : "0", 1);
    close(fd);
}

void SEND(uint16_t data)
{
        for(uint8_t i=0;i<8;i++)
        {
            uint8_t bit =((~data)>>(7-i))&0x01;
            gpio_set_level(DS,bit);
            usleep(1);
            
            gpio_set_level(SH,1);
            usleep(1);
            gpio_set_level(SH,0);
        }
        gpio_set_level(ST,1);
        usleep(1);
        gpio_set_level(ST,0);
}
// 阻塞读取管道8位灯光数据
int read_light_fifo(uint8_t *out_byte)
{
    int fd = open(FIFO_PATH, O_RDONLY);
    if(fd < 0)
    {
        // 管道未创建/无写入端，直接返回失败
        perror("open light fifo failed");
        return -1;
    }
    ssize_t ret = read(fd, out_byte, 1);
    close(fd);
    if(ret != 1)
    {
        printf("fifo read byte error, ret=%zd\n", ret);
        return -2;
    }
    return 0;
}
int main(void)
{
    gpio_export(DS); 
    gpio_export(ST);
    gpio_export(SH);

    gpio_set_dir(DS,1);
    gpio_set_dir(SH,1);
    gpio_set_dir(ST,1);

    gpio_set_level(DS,0);
    gpio_set_level(SH,0);
    gpio_set_level(ST,0);

    while(1)
    {
        int read_res = read_light_fifo(&led_data);
        if(read_res == 0)
        {
            // 读取成功，刷新移位寄存器LED
            SEND(led_data);
        }
        usleep(30000);
    }
}