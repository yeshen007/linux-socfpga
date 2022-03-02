
#ifndef __ALT_F2SM_REGS_H__
#define __ALT_F2SM_REGS_H__

//#include <io.h>
#define SYSTEM_BUS_WIDTH (32)
#define __IO_CALC_ADDRESS_NATIVE(BASE, REGNUM) \
  ((void *)(((unsigned char*)BASE) + ((REGNUM) * (SYSTEM_BUS_WIDTH/8))))
#define IORD(BASE, REGNUM) \
  *(unsigned long*)(__IO_CALC_ADDRESS_NATIVE ((BASE), (REGNUM)))
#define IOWR(BASE, REGNUM, DATA) \
  *(unsigned long*)(__IO_CALC_ADDRESS_NATIVE ((BASE), (REGNUM))) = (DATA)


#define alt_read_word(ADDR) \
  *(unsigned long*)(ADDR)
#define alt_write_word(ADDR, DATA) \
  *(unsigned long*)(ADDR) = (DATA)

#define alt_read_half_word(ADDR) \
  *(unsigned short*)(ADDR)
#define alt_write_half_word(ADDR, DATA) \
  *(unsigned short*)(ADDR) = (DATA)

#define alt_read_byte(ADDR) \
  *(unsigned char*)(ADDR)
#define alt_write_byte(ADDR, DATA) \
  *(unsigned char*)(ADDR) = (DATA)



//Where the FPGA Module is connected?Select one of the 2 situations
#define FPGA_MODULE_ON_LIGHTWEIGHT
//#define FPGA_MODULE_ON_HFBRIDGE

//Constants to do mmap and get access to FPGA through HPS-FPGA bridge
#ifdef FPGA_MODULE_ON_HFBRIDGE
  #define HPS_FPGA_BRIDGE_BASE 0xC0000000 //Beginning of H2F bridge
  #define MMAP_BASE ( HPS_FPGA_BRIDGE_BASE )
  #define MMAP_SPAN ( 0x04000000 )
  #define MMAP_MASK ( MMAP_SPAN - 1 )
  #define FPGA_MODULE_BASE 0 //FPGA Module address relative to H2F bridge
#endif
//Consts to do mmap and get access to FPGA through Lightweight HPS-FPGA bridge
#ifdef FPGA_MODULE_ON_LIGHTWEIGHT
  #define HPS_FPGA_BRIDGE_BASE 0xFF200000 //Beginning of LW H2F bridge
  #define MMAP_BASE ( HPS_FPGA_BRIDGE_BASE )
  #define MMAP_SPAN 0x00010000//( 0x00080000 )
  #define MMAP_MASK ( MMAP_SPAN - 1 )
  #define FPGA_MODULE_BASE 0 //FPGA Module address relative to LW H2F bridge
#endif


/* F2SM FPGA Read register */  // FR - FPGA READ
#define F2SM_FR_ADDR_REG              19                 // Mem phys Addr
#define IOADDR_F2SM_FR_ADDR_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_ADDR_REG)
#define IORD_F2SM_FR_ADDR_REG(base) \
  IORD(base, F2SM_FR_ADDR_REG) 
#define IOWR_F2SM_FR_ADDR_REG(base, data) \
  IOWR(base, F2SM_FR_ADDR_REG, data)

#define F2SM_FR_SIZE_REG              32                 // Mem phys Addr Size for F2SM
#define IOADDR_F2SM_FR_SIZE_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_SIZE_REG)
#define IORD_F2SM_FR_SIZE_REG(base) \
  IORD(base, F2SM_FR_SIZE_REG) 
#define IOWR_F2SM_FR_SIZE_REG(base, data) \
  IOWR(base, F2SM_FR_SIZE_REG, data)

#define F2SM_FR_START_REG              20                        // FR - FPGA READ
#define IOADDR_F2SM_FR_START_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_START_REG)
#define IORD_F2SM_FR_START_REG(base) \
  IORD(base, F2SM_FR_START_REG) 
#define IOWR_F2SM_FR_START_REG(base, data) \
  IOWR(base, F2SM_FR_START_REG, data)
#define _F2SM_FR_START_UP           (0x1)
#define _F2SM_FR_START_DOWN          (0x0)

#define F2SM_FR_DATA_MINL_REG              21                 // low 32 bit
#define IOADDR_F2SM_FR_DATA_MINL_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_DATA_MINL_REG)
#define IORD_F2SM_FR_DATA_MINL_REG(base) \
  IORD(base, F2SM_FR_DATA_MINL_REG) 
#define IOWR_F2SM_FR_DATA_MINL_REG(base, data) \
  IOWR(base, F2SM_FR_DATA_MINL_REG, data)

#define F2SM_FR_DATA_MINH_REG              22                // high 32 bit
#define IOADDR_F2SM_FR_DATA_MINH_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_DATA_MINH_REG)
#define IORD_F2SM_FR_DATA_MINH_REG(base) \
  IORD(base, F2SM_FR_DATA_MINH_REG) 
#define IOWR_F2SM_FR_DATA_MINH_REG(base, data) \
  IOWR(base, F2SM_FR_DATA_MINH_REG, data)

#define F2SM_FR_CHECK_REG              29                        // read check
#define IOADDR_F2SM_FR_CHECK_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_CHECK_REG)
#define IORD_F2SM_FR_CHECK_REG(base) \
  IORD(base, F2SM_FR_CHECK_REG) 
#define IOWR_F2SM_FR_CHECK_REG(base, data) \
  IOWR(base, F2SM_FR_CHECK_REG, data)
#define _F2SM_FR_CHECK_VALUE_OK          (0x0)

#define F2SM_FR_CHECK_REG_PH1              14                        // ph1 read check
#define IOADDR_F2SM_FR_CHECK_REG_PH1(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_CHECK_REG_PH1)
#define IORD_F2SM_FR_CHECK_REG_PH1(base) \
  IORD(base, F2SM_FR_CHECK_REG_PH1) 
#define IOWR_F2SM_FR_CHECK_REG_PH1(base, data) \
  IOWR(base, F2SM_FR_CHECK_REG_PH1, data)
#define _F2SM_FR_CHECK_VALUE_OK_PH1          (0x0)

#define F2SM_FR_CHECK_REG_PH2              21                        // ph2 read check
#define IOADDR_F2SM_FR_CHECK_REG_PH2(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_CHECK_REG_PH2)
#define IORD_F2SM_FR_CHECK_REG_PH2(base) \
  IORD(base, F2SM_FR_CHECK_REG_PH2) 
#define IOWR_F2SM_FR_CHECK_REG_PH2(base, data) \
  IOWR(base, F2SM_FR_CHECK_REG_PH2, data)
#define _F2SM_FR_CHECK_VALUE_OK_PH2          (0x0)

#define F2SM_FR_RDLEN_REG              27                        // read len return; Accumulate mode
#define IOADDR_F2SM_FR_RDLEN_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_RDLEN_REG)
#define IORD_F2SM_FR_RDLEN_REG(base) \
  IORD(base, F2SM_FR_RDLEN_REG) 
#define IOWR_F2SM_FR_RDLEN_REG(base, data) \
  IOWR(base, F2SM_FR_RDLEN_REG, data)
//#define _F2SM_FR_RDLEN_OK           (0x55) // 85  


#define F2SM_FR_DDLEN_REG              28                        // FPGA used len  return;Accumulate mode
#define IOADDR_F2SM_FR_DDLEN_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_DDLEN_REG)
#define IORD_F2SM_FR_DDLEN_REG(base) \
  IORD(base, F2SM_FR_DDLEN_REG) 
#define IOWR_F2SM_FR_DDLEN_REG(base, data) \
  IOWR(base, F2SM_FR_DDLEN_REG, data)
//#define _F2SM_FR_RDLEN_OK           (0x55) // 85

#define F2SM_FR_RDLEN_REG_PH1              12                        // ph1 read len return; Accumulate mode
#define IOADDR_F2SM_FR_RDLEN_REG_PH1(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_RDLEN_REG_PH1)
#define IORD_F2SM_FR_RDLEN_REG_PH1(base) \
  IORD(base, F2SM_FR_RDLEN_REG_PH1) 
#define IOWR_F2SM_FR_RDLEN_REG_PH1(base, data) \
  IOWR(base, F2SM_FR_RDLEN_REG_PH1, data)

#define F2SM_FR_DDLEN_REG_PH1              13                        // ph1 FPGA used len  return;Accumulate mode
#define IOADDR_F2SM_FR_DDLEN_REG_PH1(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_DDLEN_REG_PH1)
#define IORD_F2SM_FR_DDLEN_REG_PH1(base) \
  IORD(base, F2SM_FR_DDLEN_REG_PH1) 
#define IOWR_F2SM_FR_DDLEN_REG_PH1(base, data) \
  IOWR(base, F2SM_FR_DDLEN_REG_PH1, data)

#define F2SM_FR_RDLEN_REG_PH2              19                        // ph2 read len return; Accumulate mode
#define IOADDR_F2SM_FR_RDLEN_REG_PH2(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_RDLEN_REG_PH2)
#define IORD_F2SM_FR_RDLEN_REG_PH2(base) \
  IORD(base, F2SM_FR_RDLEN_REG_PH2) 
#define IOWR_F2SM_FR_RDLEN_REG_PH2(base, data) \
  IOWR(base, F2SM_FR_RDLEN_REG_PH2, data)

#define F2SM_FR_DDLEN_REG_PH2              20                        // ph2 FPGA used len  return;Accumulate mode
#define IOADDR_F2SM_FR_DDLEN_REG_PH2(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_DDLEN_REG_PH2)
#define IORD_F2SM_FR_DDLEN_REG_PH2(base) \
  IORD(base, F2SM_FR_DDLEN_REG_PH2) 
#define IOWR_F2SM_FR_DDLEN_REG_PH2(base, data) \
  IOWR(base, F2SM_FR_DDLEN_REG_PH2, data)



#define F2SM_FR_RESETLEN_REG              30                        // FR - FPGA READ
#define IOADDR_F2SM_FR_RESETLEN_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FR_RESETLEN_REG)
#define IORD_F2SM_FR_RESETLEN_REG(base) \
  IORD(base, F2SM_FR_RESETLEN_REG) 
#define IOWR_F2SM_FR_RESETLEN_REG(base, data) \
  IOWR(base, F2SM_FR_RESETLEN_REG, data)
#define _F2SM_FR_RESETLEN_UP           (0x1)
#define _F2SM_FR_RESETLEN_DOWN          (0x0)


/* F2SM FPGA Write register */     // FW - FPGA WRITE
#define F2SM_FW_ADDR_REG              23                        // Address
#define IOADDR_F2SM_FW_ADDR_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FW_ADDR_REG)
#define IORD_F2SM_FW_ADDR_REG(base) \
  IORD(base, F2SM_FW_ADDR_REG) 
#define IOWR_F2SM_FW_ADDR_REG(base, data) \
  IOWR(base, F2SM_FW_ADDR_REG, data)

#define F2SM_FW_START_REG              24                        // FW - FPGA WRITE
#define IOADDR_F2SM_FW_START_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FW_START_REG)
#define IORD_F2SM_FW_START_REG(base) \
  IORD(base, F2SM_FW_START_REG) 
#define IOWR_F2SM_FW_START_REG(base, data) \
  IOWR(base, F2SM_FW_START_REG, data)
#define _F2SM_FW_START_UP           (0x1)
#define _F2SM_FW_START_DOWN          (0x0)

#define F2SM_FW_DATA_MINL_REG              25                 // low 32 bit
#define IOADDR_F2SM_FW_DATA_MINL_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FW_DATA_MINL_REG)
#define IORD_F2SM_FW_DATA_MINL_REG(base) \
  IORD(base, F2SM_FW_DATA_MINL_REG) 
#define IOWR_F2SM_FW_DATA_MINL_REG(base, data) \
  IOWR(base, F2SM_FW_DATA_MINL_REG, data)

#define F2SM_FW_DATA_MINH_REG              26                // high 32 bit
#define IOADDR_F2SM_FW_DATA_MINH_REG(base) \
  __IO_CALC_ADDRESS_NATIVE(base, F2SM_FW_DATA_MINH_REG)
#define IORD_F2SM_FW_DATA_MINH_REG(base) \
  IORD(base, F2SM_FW_DATA_MINH_REG) 
#define IOWR_F2SM_FW_DATA_MINH_REG(base, data) \
  IOWR(base, F2SM_FW_DATA_MINH_REG, data)


#endif //__ALT_F2SM_REGS_H__

