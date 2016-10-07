/* Wacom I2C Firmware Flash Program*/
/* Copyright (c) 2013 Tatsunosuke Tobita, Wacom. */
#ifndef H_WACFLASH
#define H_WACFLASH

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>   /*for type of bool*/
#include <unistd.h>    /*system calls, read, write, close...*/
#include <string.h>
#include <time.h>      /*for nanosleep()*/
#include <fcntl.h>     /*O_RDONLY, O_RDWR etc...*/
#include <libgen.h>    /*for readlink()*/
#include <errno.h>
#include <sys/stat.h>  /*for struct stat*/
#include <sys/types.h>
#include "i2c-dev.h"

/*---------------------------------------*/
/*---------------------------------------*/
/*--WACOM common-------------------------*/
/*---------------------------------------*/
/*---------------------------------------*/
#define msleep(time)({usleep(time * 1000);})

#define WACOM_VENDOR1           0x56a
#define WACOM_VENDOR2           0x2d1f

#define FLAGS_RECOVERY_TRUE    "0"
#define FLAGS_RECOVERY_FALSE   "1"

/*HID over I2C spec*/
#define HID_DESC_REGISTER       0x01
#define USAGE_PAGE              0x05
#define USAGE_PAGE_DIGITIZERS   0x0d
#define USAGE_PAGE_DESKTOP      0x01
#define USAGE                   0x09
#define USAGE_PEN               0x02
#define USAGE_MOUSE             0x02
#define USAGE_TOUCHSCREEN       0x04
#define USAGE_X                 0x30
#define USAGE_TIPPRESSURE       0x30
#define USAGE_Y                 0x31

#define FW_LINK_PATH            "/lib/firmware/wacom_firmware.hex"
#define DEVFILE_PATH            "/dev/"
#define PARSE_SYMBOL            '_'

/*Added for using this prog. in Linux user-space program*/
#define CMD_GET_FEATURE	         2
#define CMD_SET_FEATURE	         3
#define RTYPE_FEATURE            0x03 /*: Report type -> feature(11b)*/
#define GFEATURE_SIZE            6
#define SFEATURE_SIZE            8
#define COMM_REG                 0x04
#define DATA_REG                 0x05

#define ASCII_EOF               0x1A

/*Flags for testing*/
//#define WACOM_DEBUG_LV1
//#define WACOM_DEBUG_LV2
//#define WACOM_DEBUG_LV3
//#define AES_DEBUG
//#define EMR_DEBUG
#define FILE_READ
#define I2C_OPEN
#define GETDATA
#define CONDUCT_FLASH

/*Added parameters*/
#define MILLI             1000    //corresponding to usleep()

typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;

typedef enum {
	WACOM_EMR,
	WACOM_AES,
	WACOM_ADDR_MAX,
} WACOM_ADDR;

typedef struct hid_descriptor {
	u16 wHIDDescLength;
	u16 bcdVersion;
	u16 wReportDescLength;
	u16 wReportDescRegister;
	u16 wInputRegister;
	u16 wMaxInputLength;
	u16 wOutputRegister;
	u16 wMaxOutputLength;
	u16 wCommandRegister;
	u16 wDataRegister;
	u16 wVendorID;
	u16 wProductID;
	u16 wVersion;
	u16 RESERVED_HIGH;
	u16 RESERVED_LOW;
} HID_DESC;

struct wacom_features {
	unsigned char *report_desc;
	char device_type;
	int x_max;
	int y_max;
	int pressure_max;
	int x_touch;
	int y_touch;
	unsigned char finger_max;
	int product_version;
	int fw_version;

};

u32 g_ErrorCode;

/*---------------------------------------*/
/*---------------------------------------*/
/*--WACOM EMR Technology-----------------*/
/*---------------------------------------*/
/*---------------------------------------*/
#define TECH_EMR                0x01
#define EMR_I2C_ADDR            0x09
#define MPU_W9013              0x2e
#define EMR_HEAD_NAME           "W9013"
#define EMR_FW_BLK              1

#define MAX_POLL_DEV            20
#define NUM_OF_RETRY            5

#define DATA_SIZE               (65536 * 2)
#define HEX_READ_ERR            -1
#define ASCII_EOF               0x1A
#define ACK			0

#define FLASH_BLOCK_SIZE        64
#define BLOCK_NUM               127
#define W9013_START_ADDR        0x2000
#define W9013_END_ADDR          0x1ffff

#define REPORT_ID_1             0x07
#define REPORT_ID_2             0x08
#define BOOT_QUERY_SIZE         5

#define FLASH_CMD_REPORT_ID     2
#define BOOT_CMD_SIZE	        78
#define BOOT_RSP_SIZE	         6
#define BOOT_CMD_REPORT_ID	 7
#define BOOT_ERASE_DATAMEM    0x0e
#define BOOT_ERASE_FLASH	 0
#define BOOT_WRITE_FLASH	 1
#define BOOT_EXIT		 3
#define BOOT_BLVER		 4
#define BOOT_MPU		 5
#define BOOT_QUERY		 7

#define QUERY_CMD             0x07
#define BOOT_CMD              0x04
#define MPU_CMD               0x05
#define ERS_CMD               0x00
#define WRITE_CMD             0x01

#define QUERY_RSP             0x06
#define ERS_RSP               0x00
#define WRITE_RSP             0x00

#define BUF_SIZE             1024
#define CMD_SIZE             (72 + 6)
#define RSP_SIZE                 6

/*Sector Nos for erasing datamem*/
#define DATAMEM_SECTOR0          0
#define DATAMEM_SECTOR1          1
#define DATAMEM_SECTOR2          2
#define DATAMEM_SECTOR3          3
#define DATAMEM_SECTOR4          4
#define DATAMEM_SECTOR5          5
#define DATAMEM_SECTOR6          6
#define DATAMEM_SECTOR7          7

#define RTRN_CMD                 1
#define RTRN_ECH                 2
#define RTRN_RSP                 3

/*query command*/
#define WACOM_CMD_QUERY          0x03
#define WACOM_QUERY_SIZE         19

//
// exit codes
//
#define EXIT_OK					(0)
#define EXIT_REBOOT				(1)
#define EXIT_FAIL				(2)
#define EXIT_USAGE				(3)
#define EXIT_NO_SUCH_FILE			(4)
#define EXIT_NO_INTEL_HEX			(5)
#define EXIT_FAIL_OPEN_COM_PORT			(6)
#define EXIT_FAIL_ENTER_FLASH_MODE		(7)
#define EXIT_FAIL_FLASH_QUERY			(8)
#define EXIT_FAIL_BAUDRATE_CHANGE		(9)
#define EXIT_FAIL_WRITE_FIRMWARE		(10)
#define EXIT_FAIL_EXIT_FLASH_MODE		(11)
#define EXIT_CANCEL_UPDATE			(12)
#define EXIT_SUCCESS_UPDATE			(13)
#define EXIT_FAIL_HID2SERIAL			(14)
#define EXIT_FAIL_VERIFY_FIRMWARE		(15)
#define EXIT_FAIL_MAKE_WRITING_MARK		(16)
#define EXIT_FAIL_ERASE_WRITING_MARK            (17)
#define EXIT_FAIL_READ_WRITING_MARK		(18)
#define EXIT_EXIST_MARKING			(19)
#define EXIT_FAIL_MISMATCHING			(20)
#define EXIT_FAIL_ERASE				(21)
#define EXIT_FAIL_GET_BOOT_LOADER_VERSION	(22)
#define EXIT_FAIL_GET_MPU_TYPE			(23)
#define EXIT_MISMATCH_BOOTLOADER		(24)
#define EXIT_MISMATCH_MPUTYPE			(25)
#define EXIT_FAIL_ERASE_BOOT			(26)
#define EXIT_FAIL_WRITE_BOOTLOADER		(27)
#define EXIT_FAIL_SWAP_BOOT			(28)
#define EXIT_FAIL_WRITE_DATA			(29)
#define EXIT_FAIL_GET_FIRMWARE_VERSION	        (30)
#define EXIT_FAIL_GET_UNIT_ID			(31)
#define EXIT_FAIL_SEND_STOP_COMMAND		(32)
#define EXIT_FAIL_SEND_QUERY_COMMAND	        (33)
#define EXIT_NOT_FILE_FOR_535			(34)
#define EXIT_NOT_FILE_FOR_514			(35)
#define EXIT_NOT_FILE_FOR_503			(36)
#define EXIT_MISMATCH_MPU_TYPE			(37)
#define EXIT_NOT_FILE_FOR_515			(38)
#define EXIT_NOT_FILE_FOR_1024			(39)
#define EXIT_FAIL_VERIFY_WRITING_MARK	        (40)
#define EXIT_DEVICE_NOT_FOUND			(41)
#define EXIT_FAIL_WRITING_MARK_NOT_SET	        (42)
#define EXIT_FAIL_SET_PDCT                      (43)
#define ERR                                     (44)
#define ERR_WRITE                               (45)
#define EXIT_FAIL_FWCMP                         (46)
#define EXIT_VERSION_CHECK                      (47)
#define EXIT_NOFILE                             (48)
#define EXIT_NOSUCH_OPTION                      (49)
#define EXIT_FAIL_READLINK                      (50)

/*---------------------------------------*/
/*---------------------------------------*/
/*--WACOM AES Technology-----------------*/
/*---------------------------------------*/
/*---------------------------------------*/
/*Added for using this prog. in Linux user-space program*/
#define TECH_AES                0x02
#define AES_I2C_ADDR            0x0a
#define TOUCH_CMD_QUERY         0x04
#define TOUCH_QUERY_SIZE        16
#define GET_AES_QUERY     0x05
#define AES_QUERY_SIZE    9

#define AES_FW_BASE             0x100
#define AES_FW_BLK              4
#define AES_REV_BLK             5

#define UBL_MAIN_ADDRESS	        0x8000

#define UBL_MAIN_SIZE		        (0x2bfff + 1)
#define UBL_ROM_SIZE		        0x30000	
#define UBL_CMD_SIZE_G11T	        (256 + 1)	// with report id
#define UBL_RSP_SIZE_G11T	        (135 + 1)	// with report id
#define UBL_G11T_CMD_DATA_SIZE	        128     // writing in 128 byte chunks

#define UBL_TIMEOUT_WRITE	        1000
#define UBL_RETRY_NUM		        3
#define UBL_G11T_MODE_UBL		0x06
#define DEVICETYPE_UBL		        0x02

#define DEVICETYPE_REPORT_ID	        0x02
#define UBL_CMD_REPORT_ID	        7
#define UBL_RSP_REPORT_ID	        8

#define UBL_RES_BUSYBIT			0x80
#define UBL_RES_ERRORBIT		0x40


#define UBL_WRITE			0x01	// regular writing
#define UBL_FORCEWRITE			0x02	// force-writing by ignoring device states

// Returned values
#define UBL_OK				0x00
#define UBL_ERROR			0x01

#define UBL_G11T_UBL_PID	        0x0094

//! Base address for merged FW
#define UBL_G11T_BASE_FLASH_ADDRESS	0x8000
#define UBL_G11T_MAX_FLASH_ADDRESS	0x2bfff

// bootloader commands
#define UBL_COM_WRITE			0x01
#define UBL_COM_EXIT			0x03
#define UBL_COM_GETBLVER		0x04
#define UBL_COM_GETMPUTYPE		0x05
#define UBL_COM_CHECKMODE		0x07
#define UBL_COM_ALLERASE		0x90

// bootloader responses
#define UBL_RES_OK			0x00
#define UBL_RES_BUSY			0x80
#define UBL_RES_MCUTYPE_ERROR	        0x0C
#define UBL_RES_PID_ERROR		0x0D
#define UBL_RES_VERSION_ERROR	        0x0E


// struct(s)
typedef struct{
	unsigned char status[128];
	unsigned char error[128];
	unsigned int ret;
	unsigned char command;
	unsigned char response;
	unsigned long progress;
	unsigned int process_id; //SY indicate current process. =0: blank, =1:connecting, =2:Now updating, =3:Program, =4:verify, =5:Complete
	unsigned int pid;
	unsigned int version;
	unsigned char connect;
	unsigned int usage;
	unsigned int usage_page;
	bool checksum;
	unsigned int mputype;
	unsigned int ubl_ver;
	unsigned int protect;
	unsigned int ubl_type;
	unsigned int hid_if_index;
} UBL_STATUS;

typedef struct{
	unsigned int process;
	unsigned char data[UBL_MAIN_SIZE];
	unsigned long start_adrs;
	unsigned long size;
	unsigned int pid;
	unsigned int version;
	unsigned int checksum;
	bool data_en;
	bool dev_detected;
	unsigned int protect;
	bool silent;
	bool sec;
	bool quit;
	bool log;
	bool enc;
	bool auto_conf;
	bool statusbar;
	bool dialog;
} UBL_PROCESS;


#pragma pack(push,1)
typedef struct
{
	unsigned char reportId;
	unsigned char cmd;		/* command code, see BOOT_xxx constants */
	unsigned char echo;		/* echo is used to link between command and response */
	unsigned char op[4];
} boot_cmd_header;


/*
* WRITE_FLASH - write flash memory
*/
typedef struct
{
	unsigned char reportId;
	unsigned char cmd;		/* command code, see BOOT_xxx constants */
	unsigned char echo;		/* echo is used to link between command and response */
	u32 addr;		/* address must be divisible by 2 */
	unsigned char size8;		/* size must be divisible by 8*/
	unsigned char data[UBL_CMD_SIZE_G11T - 1 - 3 - sizeof(u32)];
} boot_cmd_write_flash;	// must be 256+1 bytes


/*
 * ERASE_FLASH - erase flash memory
 */
typedef struct
{
	unsigned char reportId;
	unsigned char cmd;		/* command code, see BOOT_xxx constants */
	unsigned char echo;		/* echo is used to link between command and response */
	unsigned char blkNo;		/* block No. */
} boot_cmd_erase_flash;


typedef union
{
/*
 * data field is used to make all commands the same length
 */
	unsigned char data[UBL_CMD_SIZE_G11T];
	boot_cmd_header header;
	boot_cmd_write_flash write_flash;
	boot_cmd_erase_flash erase_flash;
} boot_cmd;

/*
 * common for all responses fields
 */
typedef struct
{
	unsigned char reportId;
	unsigned char cmd;		/* command code, see BOOT_xxx constants */
	unsigned char echo;		/* echo is used to link between command and response */
	unsigned char resp;
} boot_rsp_header;

/*
* WRITE_FLASH - write flash memory
*/
typedef struct
{
	unsigned char reportId;
	unsigned char cmd;		/* command code, see BOOT_xxx constants */
	unsigned char echo;		/* echo is used to link between command and response */
	unsigned char resp;
} boot_rsp_write_flash;

/*
* ERASE_FLASH - erase flash memory
*/
typedef struct
{
	unsigned char reportId;
	unsigned char cmd;		/* command code, see BOOT_xxx constants */
	unsigned char echo;		/* echo is used to link between command and response */
	unsigned char resp;
} boot_rsp_erase_flash;


typedef union
{
/*
 * data field is used to make all responses the same length
 */
	unsigned char data[UBL_RSP_SIZE_G11T];
	boot_rsp_header header;
	boot_rsp_write_flash write_flash;
	boot_rsp_erase_flash erase_flash;
} boot_rsp;
#pragma pack(pop)

bool wacom_i2c_set_feature(int fd, u8 report_id, unsigned int buf_size, u8 *data, 
			   u16 cmdreg, u16 datareg);
bool wacom_i2c_get_feature(int fd, u8 report_id, unsigned int buf_size, u8 *data, 
			   u16 cmdreg, u16 datareg, char addr);
int read_hex(FILE *fp, char *flash_data, size_t data_size, unsigned long *max_address,
	     UBL_PROCESS *pUBLProcess, UBL_STATUS *pUBLStatus, int tech);
int wacom_flash_aes(int fd, char *data, UBL_STATUS *pUBLStatus, UBL_PROCESS *pUBL_PROCESS);
#endif
