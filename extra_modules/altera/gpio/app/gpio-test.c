#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


/* 以下宏驱动规定 */
#define GPIO_DIR_IN 0
#define GPIO_DIR_OUT 1
#define GPIO_DIR_NONE -1

#define GPIO_VAL_LOW 0
#define GPIO_VAL_HIGH 1
#define GPIO_VAL_NONE -1

#define EXPAND_OFFSET	(29 + 29 + 27)

/* hps gpio */
#define LOCK_CHIP_EN 	0
#define SDCARD_INSTRUCT 1
#define EN_24V 			2
#define PH_EN 			3
#define RESERVE 		4
#define IC_EN 			5
#define BOARD_INDEX 	6
#define PHY1_INIT 		7
#define PHY2_INIT 		8
#define PCA9535_INIT 	9
#define LOS_INDEX 		10

/* expand gpio */
#define EXPAND_GPIO0	(EXPAND_OFFSET + 0)
#define EXPAND_GPIO1	(EXPAND_OFFSET + 1)
#define EXPAND_GPIO2	(EXPAND_OFFSET + 2)
#define EXPAND_GPIO3	(EXPAND_OFFSET + 3)
#define EXPAND_GPIO4	(EXPAND_OFFSET + 4)
#define EXPAND_GPIO5	(EXPAND_OFFSET + 5)
#define EXPAND_GPIO6	(EXPAND_OFFSET + 6)
#define EXPAND_GPIO7	(EXPAND_OFFSET + 7)
#define EXPAND_GPIO8	(EXPAND_OFFSET + 8)
#define EXPAND_GPIO9	(EXPAND_OFFSET + 9)
#define EXPAND_GPIO10	(EXPAND_OFFSET + 10)
#define EXPAND_GPIO11	(EXPAND_OFFSET + 11)
#define EXPAND_GPIO12	(EXPAND_OFFSET + 12)
#define EXPAND_GPIO13	(EXPAND_OFFSET + 13)
#define EXPAND_GPIO14	(EXPAND_OFFSET + 14)
#define EXPAND_GPIO15	(EXPAND_OFFSET + 15)



/* 定义和驱动协议的gpio描述格式 */
typedef struct han_gpio_desc {
	char *name;		
	int ind;		
	int dir;		
	int val;
} han_gpio_desc_t;

//#define TEST_WRITE
#define TEST_READ

int main(void)
{
	int ret;
	int fd;
	fd = open("/dev/gpios", O_RDWR);

	han_gpio_desc_t descs;

	/* 设置[gpio3:gpio0]为1011 */
#ifdef TEST_WRITE	
	/* 设置gpio0 */
	descs.name = "expand-gpio0";		
	descs.ind = EXPAND_GPIO0;
	descs.dir = GPIO_DIR_OUT;
	descs.val = GPIO_VAL_HIGH;	
	ret = write(fd, &descs, sizeof(descs));
	if (ret != sizeof(descs))
		printf("write gpio0 error!\n");
	printf("1st gpio write: name: %s, value: %d\n", descs.name, descs.val);

	/* 设置gpio1 */
	descs.name = "expand-gpio1";		
	descs.ind = EXPAND_GPIO1;	
	descs.dir = GPIO_DIR_OUT;
	descs.val = GPIO_VAL_HIGH;
	ret = write(fd, &descs, sizeof(descs));
	if (ret != sizeof(descs))
		printf("write gpio1 error!\n");
	printf("2nd gpio write: name: %s, value: %d\n", descs.name, descs.val);

	/* 设置gpio2 */
	descs.name = "expand-gpio2";		
	descs.ind = EXPAND_GPIO2;	
	descs.dir = GPIO_DIR_OUT;
	descs.val = GPIO_VAL_LOW;	
	ret = write(fd, &descs, sizeof(descs));
	if (ret != sizeof(descs))
		printf("write gpio2 error!\n");
	printf("3th gpio write: name: %s, value: %d\n", descs.name, descs.val);

	/* 设置gpio3 */
	descs.name = "expand-gpio3";		
	descs.ind = EXPAND_GPIO3;	
	descs.dir = GPIO_DIR_OUT;
	descs.val = GPIO_VAL_HIGH;
	ret = write(fd, &descs, sizeof(descs));
	if (ret != sizeof(descs))
		printf("write gpio3 error!\n");
	printf("4th gpio write: name: %s, value: %d\n", descs.name, descs.val);
#endif

	/* 读取[gpio3:gpio0] */
#ifdef TEST_READ
	/* 读取gpio0 */
	descs.name = "expand-gpio0";		
	descs.ind = EXPAND_GPIO0;
	descs.dir = GPIO_DIR_NONE;
	descs.val = GPIO_VAL_NONE;
	ret = read(fd, &descs, sizeof(descs));
	if (ret != sizeof(descs)) 
		printf("read gpio0 error!\n");
	printf("1st gpio read: name: %s, value: %d\n", descs.name, descs.val);

	/* 读取gpio1 */
	descs.name = "expand-gpio1";		
	descs.ind = EXPAND_GPIO1;
	descs.dir = GPIO_DIR_NONE;
	descs.val = GPIO_VAL_NONE;
	ret = read(fd, &descs, sizeof(descs));
	if (ret != sizeof(descs)) 
		printf("read gpio1 error!\n");
	printf("1st gpio read: name: %s, value: %d\n", descs.name, descs.val);

	/* 读取gpio2 */
	descs.name = "expand-gpio2";		
	descs.ind = EXPAND_GPIO2;
	descs.dir = GPIO_DIR_NONE;
	descs.val = GPIO_VAL_NONE;
	ret = read(fd, &descs, sizeof(descs));
	if (ret != sizeof(descs)) 
		printf("read gpio2 error!\n");
	printf("1st gpio read: name: %s, value: %d\n", descs.name, descs.val);

	/* 读取gpio3 */
	descs.name = "expand-gpio3";		
	descs.ind = EXPAND_GPIO3;
	descs.dir = GPIO_DIR_NONE;
	descs.val = GPIO_VAL_NONE;
	ret = read(fd, &descs, sizeof(descs));
	if (ret != sizeof(descs)) 
		printf("read gpio3 error!\n");
	printf("1st gpio read: name: %s, value: %d\n", descs.name, descs.val);

#endif

	close(fd);
	return 0;
}

