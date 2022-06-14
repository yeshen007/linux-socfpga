#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>


#include "utils.h"

static inline void do_nothing(void)
{
	__asm__ __volatile__("nop");
}


static u8 linenum[16] = {15, 4, 9, 2, 12, 7, 10, 1, 14, 5, 8, 3, 13, 6, 11, 0};


/* 每个喷头block_cnt */
int build_fpga_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt)
{
	u8 *buf_ph0 = (u8 *)ph0_data;
	u8 *buf_ph1 = (u8 *)ph1_data;

	/* check size */
	if (block_size != TEST_BLOCK_SIZE) {
		fprintf(stderr, "Block Size Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}
	
	/* build data */
	u8 row;
	u8 line;
	u32 block;
	for (block = 0; block < block_cnt; block++) {
		for (row = 0; row < 192; row++) {
			for (line = 0; line < 16; line++) {
				*buf_ph0++ = row; 
				*buf_ph1++ = (linenum[line] & 0xf) | (((u8)block << 4) & 0xf0);
			}
		}
	}
	
	return 0;

}

int check_fpga_data(void *ph0_data, void *ph1_data, u32 block_size, u32 block_cnt)
{	
	u8 *buf_ph0 = (u8 *)ph0_data;
	u8 *buf_ph1 = (u8 *)ph1_data;

	/* check size */
	if (block_size != TEST_BLOCK_SIZE) {
		fprintf(stderr, "Block Size Error at line %d, file %s\n", __LINE__, __FILE__);
		exit(-1);
	}

	/* check data */
	u8 row;
	u8 line;
	u32 block;
	for (block = 0; block < block_cnt; block++) {
		for (row = 0; row < 192; row++) {
			for (line = 0; line < 16; line++) {
				if ((*buf_ph0 != row) || (*buf_ph1 != ((linenum[line] & 0xf) | (((u8)block << 4) & 0xf0)))) {
					fprintf(stderr, "check data Error at line %d, file %s\n", __LINE__, __FILE__);
					exit(-1);
				}
				buf_ph0++; 
				buf_ph1++;
			}
		}
	}	

	return 0;
}



int Open(const char *pathname, int flags)
{
	int ret;
	ret = open(pathname, flags);
	if (ret < 0) {
		switch (errno) {
		case HAN_E_INT:
			printf("Open interrupted\n");
			break;
		case HAN_E_AGAIN:
			printf("Open try again\n");
			break;			
		default:
			printf("Open other error, errno:%d\n", errno);
			break; 
		}
	}
	return ret;
}


int Read(int fd, void *buf, unsigned long count)
{
	int ret;
	ret = read(fd, buf, count);
	if (ret == (int)count) {
		do_nothing(); 
	} else if ((ret < (int)count) && (ret > 0)) {
		printf("Read impossible get here, error\n");
	} else if (ret == HAN_E_PT_FIN) {
		do_nothing();
	} else {	//ret == -1
		switch (errno) {
		case HAN_E_PT_EXPT:
			printf("Read fpga accident stop\n");
			break;			
		case HAN_E_INT:
			printf("Read block interrupt\n");
			break;
		case HAN_E_AGAIN:
			printf("Read noblock out,try again\n");
			break;
		case HAN_E_BAD_ADDR:
			printf("Read bad addr\n");
			break;
		case HAN_E_BAD_ARG:
			printf("Read bad agr\n");
			break;
		case HAN_E_SYS_ERR:
			printf("Read system wrong in driver\n");
			break;
		default:
			printf("Read other error, errno:%d\n", errno);
			break;
		}
	}
	return ret;
}

int Write(int fd, const void *buf, unsigned long count)
{
	int ret;
	ret = write(fd, buf, count);	
	if (ret == (int)count) {
		do_nothing(); 
	} else if ((ret < (int)count) && (ret > 0)) {
		printf("Write impossible get here, error\n");
	} else {
		switch (errno) {			
		case HAN_E_INT:
			printf("Write block interrupt\n");
			break;
		case HAN_E_AGAIN:
			printf("Write noblock out,try again\n");
			break;
		case HAN_E_BAD_ADDR:
			printf("Write bad addr\n");
			break;
		case HAN_E_BAD_ARG:
			printf("Write bad agr\n");
			break;
		case HAN_E_SYS_ERR:
			printf("Write system wrong in driver\n");
			break;
		default:
			printf("Write other error, errno:%d\n", errno);
			break;
		}
	}
	return ret;
}

int Close(int fd)
{
	int ret;
	ret = close(fd);	
	if (ret != HAN_E_SUCCESS) {
		switch (errno) {
		case HAN_E_INT:
			printf("Close interrupted\n");
			break;
		case HAN_E_AGAIN:
			printf("Close try again\n");
			break;			
		default:
			printf("Close other error, errno:%d\n", errno);
			break;
		}
	}
	return ret;	
}

int Ioctl(int fd, unsigned long cmd, void *arg)
{
	int ret;
	ret = ioctl(fd, cmd, (unsigned long)arg);
	if (ret == -1) {
		switch (errno) {
		case HAN_E_INT:
			printf("Ioctl interrupted\n");
			break;
		case HAN_E_AGAIN:
			printf("Ioctl try again\n");
			break;			
		case HAN_E_BAD_ADDR:
			printf("Ioctl bad addr\n");
			break;
		case HAN_E_BAD_ARG:
			printf("Ioctl bad cmd %lx\n", cmd);
			break;
		case HAN_E_SYS_ERR:
			printf("Ioctl system wrong in driver\n");
			break;
		default:
			printf("Ioctl other error, errno:%d\n", errno);
			break;
		}
	}
	return ret;
}

int Poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	int ret;
	ret = poll(fds, nfds, timeout);
	if (ret > 0) {
		do_nothing();
	} else if (ret == 0) {
		printf("Poll time out...\n");
	} else {
		printf("Poll error, errno:%d\n", errno);
	}
	return ret;
}




int Read_again(int fd, void *buf, unsigned long count)
{
	int ret;
READ_AGAIN:	
	ret = read(fd, buf, count);
	if (ret == (int)count) {
		do_nothing(); 
	} else if ((ret < (int)count) && (ret > 0)) {
		printf("Read impossible get here, error\n");
	} else if (ret == HAN_E_PT_FIN) {
		do_nothing();
	} else {
		switch (errno) {
		case HAN_E_PT_EXPT:
			printf("Read fpga accident stop\n");
			break;			
		case HAN_E_INT:
			printf("Read block interrupt\n");
			break;
		case HAN_E_AGAIN:
			goto READ_AGAIN;
			break;
		case HAN_E_BAD_ADDR:
			printf("Read bad addr\n");
			break;
		case HAN_E_BAD_ARG:
			printf("Read bad agr\n");
			break;
		case HAN_E_SYS_ERR:
			printf("Read system wrong in driver\n");
			break;
		default:
			printf("Read other error, errno:%d\n", errno);
			break;
		}
	}
	return ret;

}

int Write_again(int fd, const void *buf, unsigned long count)
{
	int ret;
WRITE_AGAIN:	
	ret = write(fd, buf, count);	
	if (ret == (int)count) {
		do_nothing(); 
	} else if ((ret < (int)count) && (ret > 0)) {
		printf("Write impossible get here, error\n");
	} else {
		switch (errno) {			
		case HAN_E_INT:
			printf("Write block interrupt\n");
			break;
		case HAN_E_AGAIN:
			goto WRITE_AGAIN;
			break;
		case HAN_E_BAD_ADDR:
			printf("Write bad addr\n");
			break;
		case HAN_E_BAD_ARG:
			printf("Write bad agr\n");
			break;
		case HAN_E_SYS_ERR:
			printf("Write system wrong in driver\n");
			break;
		default:
			printf("Write other error, errno:%d\n", errno);
			break;
		}
	}
	return ret;
}


