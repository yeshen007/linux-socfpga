#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <mtd/mtd-user.h>
#include <sys/ioctl.h>

#define ERASE_BLOCK		(64 * 1024)
#define TEST_OFFSET		(0 * 1024 * 1024)
#define ZIMAGE_SIZE		(6 * 1024 * 1024)
#define IMAGE_START_OFF	(0xa0000)

int main(int argc, const char *argv[])
{
	int fd;
	int i = 0, j = 0;
	char* buff = NULL;
	int size = ZIMAGE_SIZE;
	FILE* fp = NULL;
	struct mtd_info_user mtd;
	struct erase_info_user e;
	int cmd = atoi(argv[1]);
	
	fd = open("/dev/mtd0", O_SYNC | O_RDWR);
	buff = (char*)malloc(size);
	
	if(fd < 0)
	{
	  perror("fail to open\n");
	  exit(-1);
	}
	
	if(ioctl(fd, MEMGETINFO, &mtd) < 0)
	{
		perror("fail to MEMGETINFO\n");
		close(fd);
		exit(-1);
	}
	printf("erase size is %d\r\n", mtd.erasesize);
	printf("part size is %d\r\n", mtd.size);
	printf("write size is %d\r\n", mtd.writesize);
	
	switch(cmd)
	{
		case 0:	//read
		{
			read(fd, buff, size);	
			fp = fopen("/dev/shm/zImage1", "wb");
			fwrite(buff, 1, size, fp);
			fclose(fp);
			break;
		}
		case 1:	//erase
		{
			e.start = IMAGE_START_OFF;
			e.length = size;
			if(ioctl(fd, MEMERASE, &e) < 0)
			{
				perror("fail to MEMERASE\n");
				close(fd);
				exit(-1);
			}
			break;
		}
		case 2:	//write
		{
			int ret = 0;
			memset(buff, 0xff, size);
			lseek(fd, IMAGE_START_OFF, SEEK_SET);
			fp = fopen(argv[2], "rb");
			ret = fread(buff, 1, size, fp);
			fclose(fp);
			
			printf("read size is %d\r\n", ret);
			
			write(fd, buff, size);
			break;
		}
	}

	close(fd);
	free(buff);

	return 0;
}


