#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define LED0_DEV_PATH "/sys/class/leds/FPGA_CFG/brightness"
#define LED1_DEV_PATH "/sys/class/leds/Boot_State/brightness"
#define ON 1
#define OFF 0

int hanglory_set_led(unsigned int lednum, unsigned int mode)
{
	int fd;
	int ret;
	char devpath[128];
	char *on = "1\n";
	char *off = "0\n";
	char *m = NULL;

	if (lednum == 0)
		snprintf(devpath, sizeof(devpath), LED0_DEV_PATH);
	else if (lednum ==1)
		snprintf(devpath, sizeof(devpath), LED1_DEV_PATH);
	else {
		perror("lednum");
		return -1;
	}
	
	fd = open(devpath, O_WRONLY);
	if (fd < 0) {
		perror("open led");
		return -1;
	}

	if (mode == ON)
		m = on;
	else if (mode == OFF)
		m = off;
	else {
		perror("mode");
		return -1;
	}
	
	ret = write(fd, m, strlen(m));
	if (ret != strlen(m)) {
		close(fd);
		perror("write led");
		return -1;
	}

	close(fd);
	return 0;
}

int hanglory_get_led(unsigned int lednum)
{
	int fd;
	int ret;
	char devpath[128];
	char m[8];

	if (lednum == 0)
		snprintf(devpath, sizeof(devpath), LED0_DEV_PATH);
	else if (lednum ==1)
		snprintf(devpath, sizeof(devpath), LED1_DEV_PATH);
	else {
		perror("lednum");
		return -1;
	}

	fd = open(devpath, O_RDONLY);
	if (fd < 0) {
		perror("open led");
		return -1;
	}

	ret = read(fd, m, 1);
	m[1] = 0;
	if (ret < 0) {
		close(fd);
		perror("read led");
		return -1;
	}

	return (int)atoi(m);
		
}


int main(int argc, char *argv[])
{
	unsigned int lednum = 0;
	int ret;

	/* 循环点亮关闭led0和led1 */
	while (1) {
		hanglory_set_led(lednum, ON);
		ret = hanglory_get_led(lednum);
		if (ret != ON) {
			perror("NOT ON");
			return -1;
		}
		
		usleep(500000);
		
		hanglory_set_led(lednum, OFF);
		ret = hanglory_get_led(lednum);
		if (ret != OFF) {
			perror("NOT OFF");	
			return -1;
		}
		
		usleep(500000);	
		
		lednum ^= 1;
	}

	return 0;
}
