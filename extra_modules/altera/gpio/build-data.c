#include <stdio.h>
#include <stdlib.h>

#define TEST_BLOCK_SIZE (3*1024)


int build_fpga_data(void *ph0_data, void *ph1_data, unsigned int block_size, unsigned int block_cnt)
{
	char *buf_ph0 = ph0_data;
	char *buf_ph1 = ph1_data;
	unsigned int buf_size = block_size * block_cnt;

	/* check size */
	if (block_size != TEST_BLOCK_SIZE) {
		fprintf(stderr, "Block Size Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}

	/* build data */
	unsigned int row;
	unsigned int line;
	unsigned int block;
	for (block = 0; block < block_cnt; block++) {
		for (line = 0; line < 16; line++) {
			for (row = 0; row < 192; row++) {
				*buf_ph0++ = row; 
				*buf_ph1++ = (line & 0xf) | (block & 0xf0);
			}
		}
	}
	
	return 0;
}

int check_fpga_data(void *ph0_data, void *ph1_data, unsigned int block_size, unsigned int block_cnt)
{
	char *buf_ph0 = ph0_data;
	char *buf_ph1 = ph1_data;
	unsigned int buf_size = block_size * block_cnt;

	/* check size */
	if (block_size != TEST_BLOCK_SIZE) {
		fprintf(stderr, "Block Size Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}

	/* check data */
	unsigned int row;
	unsigned int line;
	unsigned int block;
	for (block = 0; block < block_cnt; block++) {
		for (line = 0; line < 16; line++) {
			for (row = 0; row < 192; row++) {
				if ((*buf_ph0 != row) && (*buf_ph1 != (line & 0xf) | (block & 0xf0))) {
					fprintf(stderr, "check data Error at line %d, file %s\n", __LINE__, __FILE__);
					return -1;
				}
				buf_ph0++; 
				buf_ph1++;
			}
		}
	}	


	return 0;
}


int main(void)
{
	int ret;
	unsigned int block_size = 3 * 1024;
	unsigned int block_cnt = 64;
	char *ph0_data = malloc(block_size * block_cnt);
	char *ph1_data = malloc(block_size * block_cnt);

	/* build data into buffer */
	ret = build_fpga_data(ph0_data, ph1_data, block_size, block_cnt);
	if (ret == -1) {
		fprintf(stderr, "Build Data Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}

	/* check data */
	ret = check_fpga_data(ph0_data, ph1_data, block_size, block_cnt);
	if (ret == -1) {
		fprintf(stderr, "Check Data Error at line %d, file %s\n", __LINE__, __FILE__);
		return -1;
	}	

	free(ph0_data);
	free(ph1_data);
	printf("Data check ok!\n");
	return 0;
}

