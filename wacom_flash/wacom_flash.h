/* Wacom I2C Firmware Flash Program*/
/* Copyright (c) 2013 Tatsunosuke Tobita, Wacom. */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <syslog.h>

#include <sys/stat.h>
#include <sys/types.h>
#include "i2c-dev.h"

#define msleep(time)({usleep(time * 1000);})

#define WACOM_VENDOR1           0x56a
#define WACOM_VENDOR2           0x2d1f
#define MAX_POLL_DEV            20
#define NUM_OF_RETRY            5

#define I2C_DEVICE              "/dev/i2c-"
#define I2C_TARGET              0x09

#define DATA_SIZE               (65536 * 2)
#define HEX_READ_ERR            -1
#define ASCII_EOF               0x1A


#define WACOM_QUERY_SIZE        19
#define ACK			 0

#define MPU_W9013              0x2e

#define FLASH_BLOCK_SIZE        64
#define DATA_SIZE       (65536 * 2)
#define BLOCK_NUM               127
#define W9013_START_ADDR     0x2000
#define W9013_END_ADDR      0x1ffff

/*Added for using this prog. in Linux user-space program*/
#define CMD_GET_FEATURE	         2
#define CMD_SET_FEATURE	         3

#define RTYPE_FEATURE           0x03 /*: Report type -> feature(11b)*/
#define GFEATURE_SIZE           6
#define SFEATURE_SIZE           8

#define COMM_REG                0x04
#define DATA_REG                0x05
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
#define WACOM_CMD_QUERY0	0x04
#define WACOM_CMD_QUERY1	0x00
#define WACOM_CMD_QUERY2	0x33
#define WACOM_CMD_QUERY3	0x02
#define WACOM_CMD_THROW0	0x05
#define WACOM_CMD_THROW1	0x00

/*Chrome OS specific flag*/
#define FLAGS_RECOVERY "0"

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
#define EXIT_SAME_FIRMWARE                   (47)
#define EXIT_NOFILE                             (48)


#define HID_DESC_REGISTER 1

typedef __u8 u8;
typedef __u16 u16;
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
