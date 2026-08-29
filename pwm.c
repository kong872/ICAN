#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

// PWM 配置
#define PWM_TOP       "/sys/class/pwm/pwmchip2/"
#define PWM0_CHANNEL  "/sys/class/pwm/pwmchip2/pwm0/"
#define PWM_PERIOD    "20000000"
#define DUTY_CLOSE    "2500000"  // 距离 >5cm
#define DUTY_OPEN     "1500000"  // 距离 <5cm
#define FIFO_PATH "/temp/csb_fifo"
#define FIFO_PATH "/temp/pwm_fifo"
float dist_cm;
int all_empty;
static int run_flag=1;
static int fifo_rfd=-1;
statc=ic int pwm_fifo_fd=-1;
void sigint_handler(int sig)
{
    run_flag = 0;
    printf("\n收到退出信号，关闭PWM与管道\n");
}
//---------------------- PWM 工具函数 ----------------------
static int write_to_file(const char *file_path, const char *content)
{
    int fd = open(file_path, O_WRONLY);
    if (fd < 0)
    {
        printf("打开失败:%s, 错误:%s\n", file_path, strerror(errno));
        return -1;
    }
    ssize_t ret = write(fd, content, strlen(content));
    if (ret < 0)
    {
        printf("写入失败:%s, 错误:%s\n", file_path, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int main(void)
{
    signal(SIGINT,sigint_handler);

    fifo_rfd=open(FIFO_PATH,O_RDONLY);
    pwm_fifo_fd=open(FIFO_PWM,O_RDONLY);

    if(fifo_rfd<0)
    {
        printf("管道打开失败！请先运行超声波csb程序\n");
        return -1;
    }
    printf("管道读取端就绪，等待超声波数据...\n");
    // 1. 初始化 PWM 通道
    printf("初始化 PWM0 通道\n");
    if (write_to_file(PWM_TOP "export", "0") != 0)
    {
        printf("PWM 导出失败，请使用 root 权限运行！\n");
        return -1;
    }
    usleep(600000);
    write_to_file(PWM0_CHANNEL "enable", "0");
    write_to_file(PWM0_CHANNEL "polarity", "normal");
    usleep(100000);
    write_to_file(PWM0_CHANNEL "period", PWM_PERIOD);
    write_to_file(PWM0_CHANNEL "duty_cycle", DUTY_CLOSE);
    write_to_file(PWM0_CHANNEL "enable", "1");
    printf("PWM 初始化完成\n");

    
    while (run_flag)
    {
        ssize_t read_len=read(fifo_rfd,&dist_cm,sizeof(float));
        if(read_len<=0)
        {
            printf("管道断开，等待重连...\n");
            usleep(500000);
            continue;
        }
        printf("收到距离：%.2f cm | ", dist_cm);
        if (dist_cm > 0 && dist_cm < 5.0f)
        {
            if(all_empty!0)
            {
                // 小于5cm，抬杆
                write_to_file(PWM0_CHANNEL "duty_cycle", DUTY_OPEN);
                printf("抬杆 占空比1500000\n");
            }
           
        }
        else
        {
            sleep(1);
            // 大于等于5cm，降杆
            write_to_file(PWM0_CHANNEL "duty_cycle", DUTY_CLOSE);
            printf("降杆 占空比2500000\n");
        }
        
    }
       // 退出释放资源
    close(fifo_rfd);
    write_to_file(PWM0_CHANNEL "enable", "0");
    write_to_file(PWM_TOP "unexport", "0");
    printf("PWM、管道全部释放完毕，程序退出\n");
    return 0;
}