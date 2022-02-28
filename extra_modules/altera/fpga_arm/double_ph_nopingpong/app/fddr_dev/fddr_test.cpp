
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>
//#include <linux/types.h>
//#include <asm/string.h>
#include <assert.h>
#include <string.h>
#include <asm/io.h>		    // For ioremap and ioread32 and iowrite32
//#include <linux/io.h>

#include "alt_f2sm_regs.h"
#include "F2smIntf.h"
//#include "utils.h"

//#define TEST_TIMES 300
//#define TEST_TIMES 30


int do_test(void)
{
    int seed = 0;
    srand((unsigned)time(NULL));
    seed = (unsigned int)rand();  // 0~32767
    printf("seed: 0x%08x \n",seed);

    CF2smIntf f2sm;
    f2sm.Init();

    if(!f2sm.IsReady())
    {
        f2sm.Close();
        return -1;
    }

    int the_fd = f2sm.m_fd;


    f2sm.testinit();
    f2sm.StartWrite(seed);

    int read_time = rand()%30 + 1;
    int times = 0;  // read times for one write
    bool isWrite = true;

    int ret;
	fd_set rd_fds;
    int counter = 0;

	while (1) {
		// 清空文件描述符集合
		FD_ZERO(&rd_fds);
		FD_SET(f2sm.m_fd, &rd_fds);

		// ret 待处理事件总数
		ret = select(the_fd+1, &rd_fds, NULL, NULL, NULL);
		if (ret > 0) {
			// 检测读管道文件描述符是否存储在tmp_fds中
			// 检测此文件描述符中是否有事件发生
			if (FD_ISSET(the_fd, &rd_fds)) 
            {
				int nRead = read(the_fd, &counter, sizeof(int));
                if(nRead == sizeof(int))
                {
    				//printf("current event count  is: %d \n", counter);
                    if(isWrite)
                    {
                        isWrite = false;
                    }
                    else
                    {
                        if(!f2sm.CheckResult())   //check error
                        {
                            break;
                        }
                        times++;
                        printf("read OK %d\n",times);
                        fflush(stdout); 
                        if(times == read_time)
                            break;
                    }
                    
    			f2sm.testinit();
                    f2sm.StartRead();
                }
                else
                {
                    printf("Read error!\n");
                    break;
                }
			}
		}
	}

    f2sm.Close();
    return 0;
}

int fddr_test(void)
{
    while(true)
    {
        do_test();
    }
}

int main(void)
{
    fddr_test();

    return 0;
}
