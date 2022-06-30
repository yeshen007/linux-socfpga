#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <poll.h>


#define UART_DEV "/dev/ttyAL0"

#define TEST_COMD_1
#define TEST_TIMEOUT_BEFORE_REAV
#define TEST_TIMEOUT_AFTER_REAV

//#define WDS
#define ZDYZ

static struct termios old_cfg; 	//用于保存终端的配置参数
static int fd; 					//串口终端对应的文件描述符

/*
 * 串口初始化操作
 * 参数 device 表示串口终端的设备节点
 */
static int uart_init(const char *device) 
{
	/* 打开串口终端,   默认阻塞 */
	fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (0 > fd) {
		fprintf(stderr, "open error: %s: %s\n", device, strerror(errno));
		return -1;
	}
	
	/* 获取串口当前的配置参数 */
	if (0 > tcgetattr(fd, &old_cfg)) {
		fprintf(stderr, "tcgetattr error: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	/* 设置串口为阻塞状态*/
	if (fcntl(fd, F_SETFL, 0) < 0) {
		fprintf(stderr, "fcntl failed: %s\n", strerror(errno));
		return -1;
	}
	
	return 0; 
}


/*
 * 串口配置
 * 对于altera的fpga uart很多参数设置不会生效,是固定的
 * 主要设置串口tty行规层为原始模式
 */
#ifdef WDS
static int uart_cfg(void) 
{
	struct termios new_cfg; 

	bzero(&new_cfg, sizeof(struct termios));

	/* 设置原始模式和一些除波特率数据位停止位校验位之外的设置 */
	new_cfg.c_cflag |= CLOCAL | CREAD; 
	new_cfg.c_cflag &= ~CSIZE; 
	new_cfg.c_lflag  &= ~(ICANON | ECHO | ECHOE | ISIG);  /*Input*/
	new_cfg.c_oflag  &= ~OPOST;   						/*Output*/	


	/* 设置波特率 */
	cfsetispeed(&new_cfg, B9600);
	cfsetospeed(&new_cfg, B9600);	

	/* 设置设置数据位 */
	new_cfg.c_cflag |= CS8;
	
	/* 设置校验位 */
 	new_cfg.c_cflag &= ~PARENB;

	/* 设置停止位 */
	new_cfg.c_cflag &= ~CSTOPB;

	/* MIN和TIME */	
	new_cfg.c_cc[VTIME] = 0;
#ifdef TEST_TIMEOUT_AFTER_REAV
	new_cfg.c_cc[VTIME] = 1;
#endif
	new_cfg.c_cc[VMIN] = 1;
#ifdef TEST_COMD_1
	new_cfg.c_cc[VMIN] = 14;
#endif

	/* 清空缓冲区 */
	tcflush(fd, TCIOFLUSH);


	/* 写入配置、使配置生效 */
	if (0 != tcsetattr(fd, TCSANOW, &new_cfg)) {
		fprintf(stderr, "tcsetattr error: %s\n", strerror(errno));
		return -1;
	}
	
	/* 配置 OK 退出 */
	return 0; 
}

#elif defined(ZDYZ)

static int uart_cfg(void) 
{
	struct termios new_cfg; 

	bzero(&new_cfg, sizeof(struct termios));

	/* 设置原始模式和使能接收 */
	cfmakeraw(&new_cfg);
	new_cfg.c_cflag |= CREAD;


	/* 设置波特率 */
	cfsetspeed(&new_cfg, B9600);
	

	/* 设置设置数据位 */
	new_cfg.c_cflag &= ~CSIZE;
	new_cfg.c_cflag |= CS8;
	
	/* 设置校验位 */
 	new_cfg.c_cflag &= ~PARENB;
	new_cfg.c_iflag &= ~INPCK;

	/* 设置停止位 */
	new_cfg.c_cflag &= ~CSTOPB;

	/* MIN和TIME */	
	new_cfg.c_cc[VTIME] = 0;
#ifdef TEST_TIMEOUT_AFTER_REAV
	new_cfg.c_cc[VTIME] = 1;
#endif	
	new_cfg.c_cc[VMIN] = 1;
#ifdef TEST_COMD_1
	new_cfg.c_cc[VMIN] = 14;
#endif

	/* 清空缓冲区 */
	tcflush(fd, TCIOFLUSH);


	/* 写入配置、使配置生效 */
	if (0 != tcsetattr(fd, TCSANOW, &new_cfg)) {
		fprintf(stderr, "tcsetattr error: %s\n", strerror(errno));
		return -1;
	}
	
	/* 配置 OK 退出 */
	return 0; 
}

#else
	#error must define WDS or ZDYZ! if define both,then set WDS.
#endif


int main(void)
{
	int i;
	int ret;
	unsigned char r_buf[14];
	unsigned char w_buf[8] = {0xFE, 0xFE, 0xFE, 4, 0x1, 0, 0, 0xFB};

	printf("begin to open uart\n");
	
	/* 串口初始化 */
	if (uart_init(UART_DEV))
		exit(-1);

	printf("begin to config uart\n");
	
	/* 串口配置 */
	if (uart_cfg()) {
		tcsetattr(fd, TCSANOW, &old_cfg); //恢复到之前的配置
		close(fd);
		exit(-1);
	}

	printf("begin to send comd to ph\n");

	/* 发命令给喷头 */
	ret = write(fd, w_buf, sizeof(w_buf));
	if (ret != sizeof(w_buf)) {
		fprintf(stderr, "uart write error: %s\n", strerror(errno));
		close(fd);
		exit(-1);
	}

	printf("begin to read data from ph\n");	

#ifdef TEST_TIMEOUT_BEFORE_REAV
	/* poll测试参数 */
	struct pollfd fds;
	nfds_t nfds = 1;
	memset(&fds,0,sizeof(fds));
	fds.fd = fd;
	fds.events |= POLLIN;		
	poll(&fds, nfds, 100);
	if (!(fds.revents & POLLIN)) 
		printf("gpio test poll timeout\n"); 	
#endif

	/* 读取喷头返回的数据打印出来 */
	ret = read(fd, r_buf, sizeof(r_buf));
	if (ret != sizeof(r_buf)) {
		fprintf(stderr, "uart read error: get %d bytes\n", ret);
		close(fd);
		exit(-1);
	}	
	for (i = 0; i < ret; i++)
		printf("r_buf[%d] -- %x\n", i, r_buf[i]);
	

 	/* 退出 */
 	tcsetattr(fd, TCSANOW, &old_cfg); //恢复到之前的配置
 	close(fd);	

	return 0;
}
