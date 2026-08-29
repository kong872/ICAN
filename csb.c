#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#define FIFO_PATH "/temp/csb_fifo"
#define TRIG_GPIO_NUM  180      //GPIO5_C4
#define ECHO_GPIO_NUM  182      //GPIO5_C6

static int run_flag=1;
static int fifo_wfd=-1;
float dist_cm;
void sigint_handler(int sig)
{
    run_flag=0;
    printf("\n收到退出信号，准备释放硬件与管道\n");
}

static int gpio_export(int gpio)
{
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if(fd < 0) return -1;
    char buf[16];
    sprintf(buf, "%d", gpio);
    write(fd, buf, strlen(buf));
    close(fd);
    return 0;
}

static int gpio_unexport(int gpio)
{
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if(fd < 0) return -1;
    char buf[16];
    sprintf(buf, "%d", gpio);
    write(fd, buf, strlen(buf));
    close(fd);
    return 0;
}

static int gpio_set_dir(int gpio, const char *dir)
{
    char path[64];
    sprintf(path, "/sys/class/gpio/gpio%d/direction", gpio);
    int fd = open(path, O_WRONLY);
    if(fd < 0) return -1;
    write(fd, dir, strlen(dir));
    close(fd);
    return 0;
}

static int gpio_set_val(int gpio, int val)
{
    char path[64];
    sprintf(path, "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(path, O_WRONLY);
    if(fd < 0) return -1;
    char buf[2] = {val ? '1' : '0', 0};
    write(fd, buf, 1);
    close(fd);
    return 0;
}

static int gpio_get_val(int gpio)
{
    char path[64];
    sprintf(path, "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(path, O_RDONLY);
    if(fd < 0) return -1;
    char ch;
    read(fd, &ch, 1);
    close(fd);
    return ch == '1' ? 1 : 0;
}

int main(void)
{
    signal(SIGINT,sigint_handler);
    struct stat stat_buf;
   fifo_wfd=open(FIFO_PATH,O_WRONLY);
   if(fifo_wfd<0)
   {
       printf("打开管道写端失败：%s\n", strerror(errno));
        return -1;
   }
    printf("管道写端打开成功，等待读取程序连接\n");
    // 导出GPIO
    gpio_export(TRIG_GPIO_NUM);
    gpio_export(ECHO_GPIO_NUM);
    usleep(200000);

    // 设置方向：Trig输出，Echo输入
    gpio_set_dir(TRIG_GPIO_NUM, "out");
    gpio_set_dir(ECHO_GPIO_NUM, "in");

    struct timespec t_start, t_end;
    long ns_diff;
    

    while(1)
    {
        // 发射10us触发脉冲
        gpio_set_val(TRIG_GPIO_NUM, 1);
        usleep(30);
        gpio_set_val(TRIG_GPIO_NUM, 0);

        // 等待Echo高电平开始
        while(gpio_get_val(ECHO_GPIO_NUM) == 0);
        if(!run_flag)break;
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        // 等待Echo变低，结束计时
        while(gpio_get_val(ECHO_GPIO_NUM) == 1);
        if(!run_flag)break;
        clock_gettime(CLOCK_MONOTONIC, &t_end);

        ns_diff = (t_end.tv_sec - t_start.tv_sec) * 1000000000L + (t_end.tv_nsec - t_start.tv_nsec);
        dist_cm = ns_diff * 0.000017f;

        if(dist_cm > 400) dist_cm = 0; // HC-SR04最大测距4m
        
        write(fifo_wfd,&dist_cm,sizeof(float));
        sleep(1);
        
    }
    close(fifo_wfd);
    gpio_unexport(TRIG_GPIO_NUM);
    gpio_unexport(ECHO_GPIO_NUM);
    return 0;
}