#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>


#define GPIO0_BASE 0xFF708000
#define GPIO1_BASE 0xFF709000
#define GPIO2_BASE 0xFF70A000

#define GPIO0_OFFSET 0x0
#define GPIO1_OFFSET 0x1000
#define GPIO2_OFFSET 0x2000


#define GPIO0_SPAN 0x1000
#define GPIO1_SPAN 0x1000
#define GPIO2_SPAN 0x1000

#define GPIO_BASE GPIO0_BASE
#define GPIO_SPAN (GPIO0_SPAN + GPIO1_SPAN + GPIO2_SPAN)

#define MMAP_BASE GPIO_BASE
#define MMAP_SPAN GPIO_SPAN

#define SD_GPIO_DIR_REG_OFFSET 0x4
#define SD_GPIO_VAL_REG_OFFSET 0x0

#define SD_GPIO_INDEX	22
#define SD_GPIO_MASK	(0x1UL << SD_GPIO_INDEX)


#define han_read_word(ADDR) \
  *(unsigned long*)(ADDR)
#define han_write_word(ADDR, DATA) \
  *(unsigned long*)(ADDR) = (DATA)


static void *gpio_mmap_base = NULL;
static void *gpio0_mmap_base = NULL;
static void *gpio1_mmap_base = NULL;
static void *gpio2_mmap_base = NULL;


int main(void)
{
	int fd;

	/* 打开地址空间文件/dev/mem，它表示包括内存，外设区域一起的地址空间 */
	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd == -1) {
		fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
		exit(-1);
	}

	gpio_mmap_base = mmap(NULL, MMAP_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, MMAP_BASE);
	if (gpio_mmap_base == MAP_FAILED) {
		fprintf(stderr, "Mmap Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno));
		close(fd);
		exit(-1);
	}
	gpio0_mmap_base = gpio_mmap_base + GPIO0_OFFSET;
	gpio1_mmap_base = gpio_mmap_base + GPIO1_OFFSET;
	gpio2_mmap_base = gpio_mmap_base + GPIO2_OFFSET;

	close(fd);

	/* 
	 * 操作gpio
	 */
	unsigned long reg;
	unsigned long val;
	/* 设置gpio44为输入 */
	reg = (unsigned long)gpio1_mmap_base + SD_GPIO_DIR_REG_OFFSET;
	val = han_read_word(reg);
	val &= ~SD_GPIO_MASK;	
	/* 读取gpio44的值 */
	reg = (unsigned long)gpio1_mmap_base + SD_GPIO_VAL_REG_OFFSET;
	val = han_read_word(reg);
	val &= SD_GPIO_MASK;
	val >>= SD_GPIO_INDEX;
	printf("gpio44 val: 0X%08x\n", val);
	

	/* unmap */
	if (munmap(gpio_mmap_base, MMAP_SPAN) != 0) {
        printf("ERROR: munmap() failed...\n");
        return -1;
    }

	return 0;
}

