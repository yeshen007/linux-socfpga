#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* gpio2019 - 2047 : GPIO[0:28] (GPIO A [28:0])
 * gpio1990 - 2018 : GPIO[29:57] (GPIO B [28:0])
 * gpio1963 - 1989 : GPIO[58:66] ... HLGPI[13:0] (GPIO C [8:0] ... GPIO C [26:13])
 */
#define GPIO_DIRECTION_PATH "/sys/class/gpio/gpio%d/direction"
#define GPIO_VALUE_PATH "/sys/class/gpio/gpio%d/value"


#define ON 1
#define OFF 0

#define IN 1
#define OUT 0

//echo <gpionum> > /sys/class/gpio/export
int hanglory_alloc_gpio(unsigned int gpionum)
{
	int fd;
	int ret;
	char *devpath = "/sys/class/gpio/export";
	char gpiochar[16] = {0};

	if (gpionum <= 28) {	 	//GPIO[0:28]
		gpionum += 2019;
	} else if (gpionum <= 57) {	//GPIO[29:57]
		gpionum += 1990 - 29;
	} else if (gpionum <= 66) {	//GPIO[58:66]
		gpionum += 1963 - 58;
	} else if ((gpionum >= 71) && (gpionum <= 84)) {		//HLGPI[13:0]
		gpionum += 1963 - 58;
	} else {					//illegal
		perror("alloc gpionum");
		return -1;
	}

	snprintf(gpiochar, sizeof(gpiochar), "%d", gpionum);

	fd = open(devpath, O_WRONLY);
	if (fd < 0) {
		perror("alloc gpio open");
		return -1;
	}
	
	ret = write(fd, gpiochar, strlen(gpiochar));
	if (ret != strlen(gpiochar)) {
		perror("alloc gpio write");
		return -1;
	}
	
	return 0;
}

//echo <gpionum> > /sys/class/gpio/unexport
int hanglory_free_gpio(unsigned int gpionum)
{
	int fd;
	int ret;
	char *devpath = "/sys/class/gpio/unexport";
	char gpiochar[16] = {0};

	if (gpionum <= 28) {	 	//GPIO[0:28]
		gpionum += 2019;
	} else if (gpionum <= 57) {	//GPIO[29:57]
		gpionum += 1990 - 29;
	} else if (gpionum <= 66) {	//GPIO[58:66]
		gpionum += 1963 - 58;
	} else if ((gpionum >= 71) && (gpionum <= 84)) {		//HLGPI[13:0]
		gpionum += 1963 - 58;
	} else {					//illegal
		perror("free gpionum");
		return -1;
	}

	snprintf(gpiochar, sizeof(gpiochar), "%d", gpionum);

	fd = open(devpath, O_WRONLY);
	if (fd < 0) {
		perror("free gpio open");
		return -1;
	}
	
	ret = write(fd, gpiochar, strlen(gpiochar));
	if (ret != strlen(gpiochar)) {
		perror("free gpio write");
		return -1;
	}
	
	return 0;

}

//echo <direction> > /sys/class/gpio/gpio<gpionum>/direction
int hanglory_set_direction(unsigned int gpionum, unsigned int mode)
{
	int fd;
	int ret;
	char devpath[128];
	char *in = "in\n";
	char *out = "out\n";
	char *m = NULL;

	if (gpionum <= 28) {	 	//GPIO[0:28]
		gpionum += 2019;
	} else if (gpionum <= 57) {	//GPIO[29:57]
		gpionum += 1990 - 29;
	} else if (gpionum <= 66) {	//GPIO[58:66]
		gpionum += 1963 - 58;
	} else if ((gpionum >= 71) && (gpionum <= 84)) {		//HLGPI[13:0]
		gpionum += 1963 - 58;
	} else {					//illegal
		perror("set direction gpionum");
		return -1;
	}	
	
	snprintf(devpath, sizeof(devpath), GPIO_DIRECTION_PATH, gpionum);

	fd = open(devpath, O_WRONLY);
	if (fd < 0) {
		perror("set direction open");
		return -1;
	}

	if (mode == IN)
		m = in;
	else if (mode == OUT)
		m = out;
	else {
		perror("set direction mode");
		return -1;
	}	
	
	ret = write(fd, m, strlen(m));
	if (ret != strlen(m)) {
		perror("set direction write");
		return -1;
	}
	
	return 0;	

}

//cat /sys/class/gpio/gpio<gpionum>/direction
int hanglory_get_direction(unsigned int gpionum)
{
	int fd;
	int ret;
	char devpath[128];
	char direction[8];	
	char *in = "i";
	char *out = "o";

	if (gpionum <= 28) {	 	//GPIO[0:28]
		gpionum += 2019;
	} else if (gpionum <= 57) {	//GPIO[29:57]
		gpionum += 1990 - 29;
	} else if (gpionum <= 66) {	//GPIO[58:66]
		gpionum += 1963 - 58;
	} else if ((gpionum >= 71) && (gpionum <= 84)) {		//HLGPI[13:0]
		gpionum += 1963 - 58;
	} else {					//illegal
		printf("get direction gpionum error\n");
		return -1;
	}	

	snprintf(devpath, sizeof(devpath), GPIO_DIRECTION_PATH, gpionum);
	
	fd = open(devpath, O_RDONLY);
	if (fd < 0) {
		printf("get direction open error\n");
		return -1;
	}

	ret = read(fd, direction, 1);
	direction[1] = 0;
	if (ret != 1) {
		printf("get direction write error\n");
		return -1;
	}

	if (strncmp(in, direction, 1) == 0) 
		return IN;
	else if (strncmp(out, direction, 1) == 0)
		return OUT;
	else 
		return -1;

}

//cat /sys/class/gpio/gpio<gpionum>/direction
//echo <value> > /sys/class/gpio/gpio<gpionum>/value
int hanglory_set_output_value(unsigned int gpionum, unsigned int value)
{
	int fd;
	int ret;
	char devpath[128];
	char direction[8];	
	char *on = "1\n";
	char *off = "0\n";
	char *m = NULL;	

	ret = hanglory_get_direction(gpionum);
	if (ret != OUT) {
		printf("not output direction\n");
		return -1;
	}

	if (gpionum <= 28) {	 	//GPIO[0:28]
		gpionum += 2019;
	} else if (gpionum <= 57) {	//GPIO[29:57]
		gpionum += 1990 - 29;
	} else if (gpionum <= 66) {	//GPIO[58:66]
		gpionum += 1963 - 58;
	} else if ((gpionum >= 71) && (gpionum <= 84)) {		//HLGPI[13:0]
		gpionum += 1963 - 58;
	} else {					//illegal
		perror("set output value gpionum");
		return -1;
	}	

	snprintf(devpath, sizeof(devpath), GPIO_VALUE_PATH, gpionum);

	fd = open(devpath, O_WRONLY);
	if (fd < 0) {
		perror("set output value open");
		return -1;
	}

	if (value == ON)
		m = on;
	else if (value == OFF)
		m = off;
	else {
		perror("mode");
		return -1;
	}

	ret = write(fd, m, strlen(m));
	if (ret != strlen(m)) {
		perror("set output value write");
		return -1;
	}	

	return 0;

}

//cat /sys/class/gpio/gpio<gpionum>/direction
//cat /sys/class/gpio/gpio<gpionum>/value
int hanglory_get_output_value(unsigned int gpionum)
{
	int fd;
	int ret;
	char devpath[128];
	char value[8];	
	char *on = "1\n";
	char *off = "0\n";
	char *m = NULL;		
	
	ret = hanglory_get_direction(gpionum);
	if (ret != OUT) {
		printf("not output direction\n");
		return -1;
	}

	if (gpionum <= 28) {	 	//GPIO[0:28]
		gpionum += 2019;
	} else if (gpionum <= 57) {	//GPIO[29:57]
		gpionum += 1990 - 29;
	} else if (gpionum <= 66) {	//GPIO[58:66]
		gpionum += 1963 - 58;
	} else if ((gpionum >= 71) && (gpionum <= 84)) {		//HLGPI[13:0]
		gpionum += 1963 - 58;
	} else {					//illegal
		perror("get output value gpionum");
		return -1;
	}	

	snprintf(devpath, sizeof(devpath), GPIO_VALUE_PATH, gpionum);

	fd = open(devpath, O_RDONLY);
	if (fd < 0) {
		perror("get output value open");
		return -1;
	}	

	ret = read(fd, value, 1);
	value[1] = 0;
	if (ret != 1) {
		perror("get output value write");
		return -1;
	}	

	if (strncmp(on, value, 1) == 0) 
		return ON;
	else if (strncmp(off, value, 1) == 0)
		return OFF;
	else 
		return -1;	
	
}

//cat /sys/class/gpio/gpio<gpionum>/direction
//cat /sys/class/gpio/gpio<gpionum>/value
int hanglory_get_input_value(unsigned int gpionum)
{
	int fd;
	int ret;
	char devpath[128];
	char value[8];	
	char *on = "1\n";
	char *off = "0\n";
	char *m = NULL;		
	
	ret = hanglory_get_direction(gpionum);
	if (ret != IN) {
		printf("not input direction\n");
		return -1;
	}

	if (gpionum <= 28) {	 	//GPIO[0:28]
		gpionum += 2019;
	} else if (gpionum <= 57) {	//GPIO[29:57]
		gpionum += 1990 - 29;
	} else if (gpionum <= 66) {	//GPIO[58:66]
		gpionum += 1963 - 58;
	} else if ((gpionum >= 71) && (gpionum <= 84)) {		//HLGPI[13:0]
		gpionum += 1963 - 58;
	} else {					//illegal
		perror("get intput value gpionum");
		return -1;
	}	

	snprintf(devpath, sizeof(devpath), GPIO_VALUE_PATH, gpionum);	

	fd = open(devpath, O_RDONLY);
	if (fd < 0) {
		perror("get output value open");
		return -1;
	}	

	ret = read(fd, value, 1);
	value[1] = 0;
	if (ret != 1) {
		perror("get output value write");
		return -1;
	}	

	if (strncmp(on, value, 1) == 0) 
		return ON;
	else if (strncmp(off, value, 1) == 0)
		return OFF;
	else 
		return -1;		
	
}


int main(int argc, char *argv[])
{
	int ret;

	/* 申请操作gpio44 */
	ret = hanglory_alloc_gpio(44);
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}

	/* 获取gpio44方向,此时默认输入 */
	ret = hanglory_get_direction(44);
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}	
	printf("gpio44 direct: %d\n", ret);

	ret = hanglory_get_input_value(44);	
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}	
	printf("gpio44 input value: %d\n", ret);

	getchar();

	/* 设置gpio44为输出 */
	ret = hanglory_set_direction(44, OUT);
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}
	/* 获取gpio44方向 */
	ret = hanglory_get_direction(44);
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}	
	printf("gpio44 direct: %d\n", ret);	

	/* gpio44输出1 */
	ret = hanglory_set_output_value(44, ON);	
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}
	ret = hanglory_get_output_value(44);	
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}	
	printf("gpio44 output value: %d\n", ret);	

	getchar();

	/* gpio44输出0 */
	ret = hanglory_set_output_value(44, OFF);	
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}	
	ret = hanglory_get_output_value(44);	
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}	
	printf("gpio44 output value: %d\n", ret);	

	getchar();

	/* 释放gpio44 */
	ret = hanglory_free_gpio(44);
	if (ret == -1) {
		fprintf(stderr, "Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}	
	
	return 0;
}


