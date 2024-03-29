/* Wacom I2C Firmware Flash Program*/
/* Copyright (c) 2013-2021 Tatsunosuke Tobita, Wacom. */
/* Copyright (c) 2017-2019 Martin Chen, Wacom. */
//
#include "wacom_flash.h"
#define PROGRAM_NAME "wacom_flash"
#define VERSION_STRING "version 1.4.0"

//
// Release Note of Wacom_flash
//
// v1.4.0   2021/Sep/xx     (1) EMR W9021 flash compatibility added
//                              
//
// v1.3.0   2019/Sep/26     (1) Write the first sector after other sectors wrote OK, 
//                              Reduce possibility of broken firmware issue by interrupt write flash process
//                          (2) Add new HWID code for HWID in boot loader area, Check valid output when return code is 0
//                              output format vvvv_pppp, in hex, vvvv is 0x2D1F, ppp is PID
//
// v1.2.9   2019/Jul/30     (1) Make a special version for support AES only (This may can use for AES Android project open source)
//
// v1.2.8   2019/May/05     (1) Remove HWID support, but keep the code as macro define for reference
//                          (2) Add AES G14T support
//
// v1.2.7   2018/Dec/05     (1) Confirmed the response[RTRN_RSP] of these three command 
//                              BOOT_ERASE_DATAMEM, BOOT_ERASE_FLASH, BOOT_WRITE_FLASH
//                              only 0xff or 0x00, so remove useless check statement
//
// v1.2.6   2018/Mar/12     (1) Add support report check in wacom_hwid_from_firmware()
//                          (2) If enter_ubl fail in wacom_get_hwid(), don't call exit_ubl(), just out the function
//                          (3) If already in boot loader mode before program start, it is possible cause by
//                              last firmware write was failed, don't call exit_ubl()
//
// v1.2.5   2018/Mar/07     (1) Reduce the sleep time of enter_ubl() and exit_ubl() from 500 ms to 300 ms
//                          (2) If enter_ubl fail, change to just out the write function, not need to call exit_ubl()
//                          (3) Try read the hardware id from normal first, if fail, try the ubl method
//
// v1.2.4   2018/Feb/26     Fix bug, we shouldn't call exit_ubl() when wacom_write() failed
// v1.2.3   2018/Feb/14     Fix bug, we shouldn't call wacom_read_hwid() when product is EMR, it's AES only function
//
//
bool wacom_i2c_set_feature(int fd, u8 report_id, unsigned int buf_size, u8 *data, 
			   u16 cmdreg, u16 datareg)
{
	int i, ret = -1;
	int total = SFEATURE_SIZE + buf_size;
	u8 *sFeature = NULL;
	bool bRet = false;

	sFeature = (u8 *)malloc(sizeof(u8) * total);
	if (!sFeature) {
		fprintf(stderr, "%s cannot preserve memory \n", __func__);
		goto out;
	}
	memset(sFeature, 0, sizeof(u8) * total);

	sFeature[0] = (u8)(cmdreg & 0x00ff);
	sFeature[1] = (u8)((cmdreg & 0xff00) >> 8);
	sFeature[2] = (RTYPE_FEATURE << 4) | report_id;
	sFeature[3] = CMD_SET_FEATURE;
	sFeature[4] = (u8)(datareg & 0x00ff);
	sFeature[5] = (u8)((datareg & 0xff00) >> 8);

	if ( (buf_size + 2) > 255) {
		sFeature[6] = (u8)((buf_size + 2) & 0x00ff);
		sFeature[7] = (u8)(( (buf_size + 2) & 0xff00) >> 8);
	} else {
		sFeature[6] = (u8)(buf_size + 2);
		sFeature[7] = (u8)(0x00);
	}

	for (i = 0; i < buf_size; i++)
		sFeature[i + SFEATURE_SIZE] = *(data + i);

	ret = write(fd, sFeature, total);
	if (ret != total) {
		fprintf(stderr, "Sending Set_Feature failed sent bytes: %d \n", ret);
		goto err;
	}

	bRet = true;
 err:
	free(sFeature);
	sFeature = NULL;

 out:
	return bRet;
}

/*get_feature uses ioctl for using I2C restart method to communicate*/
/*and for that i2c_msg requires "char" for buf rather than unsinged char,*/
/*so storing data should be back as "unsigned char".*/
bool wacom_i2c_get_feature(int fd, u8 report_id, unsigned int buf_size, u8 *data, 
		 u16 cmdreg, u16 datareg, char addr)

{
	struct i2c_rdwr_ioctl_data packets;

	/*"+ 2", adding 2 more spaces for organizeing again later in the passed data, "data"*/
	unsigned int total = buf_size + 2;
	char *recv = NULL;
	bool bRet = false;
	u8 gFeature[] = {
		(u8)(cmdreg & 0x00ff),
		(u8)((cmdreg & 0xff00) >> 8),
		(RTYPE_FEATURE << 4) | report_id,
		CMD_GET_FEATURE,
		(u8)(datareg & 0x00ff),
		(u8)((datareg & 0xff00) >> 8)
	};

	recv = (char *)malloc(sizeof(char) * total);
	if (recv == NULL) {
		fprintf(stderr, "%s cannot preserve memory \n", __func__);
		goto out;
	}
	memset(recv, 0, sizeof(char) * total);

	{
		struct i2c_msg msgs[] = {
			{
				.addr = addr,
				.flags = 0,
				.len = GFEATURE_SIZE,
				.buf = (char *)gFeature,
			},
			{
				.addr = addr,
				.flags = I2C_M_RD,
				.len = total,
				.buf = recv,
			},
		};

		packets.msgs  = msgs;
		packets.nmsgs = 2;
		if (ioctl(fd, I2C_RDWR, &packets) < 0) {
			fprintf(stderr, "%s failed to send messages\n", __func__);
			goto err;
		}

		/*First two bytes in recv are length of 
		  the report and data doesn't need them*/
		memcpy(data, (unsigned char *)(recv + 2), buf_size);
	}

#ifdef WACOM_DEBUG_LV3
	{
		int ret = -1;
		fprintf(stderr, "Recved bytes: %d \n", ret);
		fprintf(stderr, "Expected bytes: %d \n", buf_size);
		fprintf(stderr, "1: %x, 2: %x, 3:%x, 4:%x 5:%x\n", data[0], data[1], data[2], data[3], data[4]);
	}
#endif
	
	bRet = true;
 err:
	free(recv);
	recv = NULL;
 out:
	return bRet;

}

int wacom_flash_cmd(int fd)
{
	int len = 0;
	u8 cmd[2];
	bool bRet = false;

	cmd[len++] = 0x02;
	cmd[len++] = 0x02;

	bRet = wacom_i2c_set_feature(fd, FLASH_CMD_REPORT_ID, len, cmd, COMM_REG, DATA_REG);
	if(!bRet){
		fprintf(stderr, "Sending flash command failed\n");
		return -EXIT_FAIL;
	}

	msleep(300);

	return 0;
}

int flash_query_emr(int fd)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ECH, len = 0;	

	command[len++] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
	command[len++] = BOOT_QUERY;			/* Report:Boot Query command */
	command[len++] = ECH = 7;				/* Report:echo */

	bRet = wacom_i2c_set_feature(fd, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "%s failed to set feature \n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, RSP_SIZE, response, COMM_REG, DATA_REG, EMR_I2C_ADDR);
	if (!bRet) {
		fprintf(stderr, "%s failed to get feature \n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	if ( (response[RTRN_CMD] != QUERY_CMD) ||
	     (response[RTRN_ECH] != ECH) ) {
		fprintf(stderr, "%s res3:%x res4:%x \n", __func__, response[3], response[RTRN_ECH]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	if (response[RTRN_RSP] != QUERY_RSP) {
		fprintf(stderr, "%s res5:%x \n", __func__, response[RTRN_RSP]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}
	
	return 0;
}

bool flash_blver_emr(int fd, int *blver)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ECH, len = 0;	

	command[len++] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
	command[len++] = BOOT_BLVER;			/* Report:Boot Version command */
	command[len++] = ECH = 7;				/* Report:echo */

	bRet = wacom_i2c_set_feature(fd, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "%s failed to set feature1\n", __func__);
		return bRet;
	}

	bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, RSP_SIZE, response, COMM_REG, DATA_REG, EMR_I2C_ADDR);
	if (!bRet) {
		fprintf(stderr, "%s 2 failed to set feature\n", __func__);
		return bRet;
	}

	if ( (response[RTRN_CMD] != BOOT_CMD) ||
	     (response[RTRN_ECH] != ECH) ) {
		fprintf(stderr, "%s res3:%x res4:%x \n", __func__, response[RTRN_CMD], response[RTRN_ECH]);
		return false;
	}
	
	*blver = (int)response[RTRN_RSP];
	
	return true;
}

bool flash_mputype_emr(int fd, int* pMpuType)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ECH, len = 0;		

	command[len++] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
	command[len++] = BOOT_MPU;				/* Report:Boot Query command */
	command[len++] = ECH = 7;				/* Report:echo */

	bRet = wacom_i2c_set_feature(fd, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "%s failed to set feature \n", __func__);
		return bRet;
	}

	bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, RSP_SIZE, response, COMM_REG, DATA_REG, EMR_I2C_ADDR);
	if (!bRet) {
		fprintf(stderr, "%s failed to get feature \n", __func__);
		return bRet;
	}

	if ( (response[RTRN_CMD] != MPU_CMD) ||
	     (response[RTRN_ECH] != ECH) ) {
		fprintf(stderr, "%s res3:%x res4:%x \n", __func__, response[RTRN_CMD], response[RTRN_ECH]);
		return false;
	}
	
	*pMpuType = (int)response[RTRN_RSP];	
	return true;
}

bool flash_end_emr(int fd)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	int ECH, len = 0;

	command[len++] = BOOT_CMD_REPORT_ID;
	command[len++] = BOOT_EXIT;
	command[len++] = ECH = 7;

	bRet = wacom_i2c_set_feature(fd, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "%s failed to set feature 1\n", __func__);
		return bRet;
	}

	return true;
}

/*--------------------------------------------erase--------------------------------------------------------------------------*/
bool erase_datamem(int fd)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[BOOT_RSP_SIZE];
	unsigned char sum = 0;
	unsigned char cmd_chksum;
	int ECH, j;
	int len = 0;

	command[len++] = BOOT_CMD_REPORT_ID;        /* Report:ReportID */
	command[len++] = BOOT_ERASE_DATAMEM;		/* Report:erase datamem command */
	command[len++] = ECH = BOOT_ERASE_DATAMEM;	/* Report:echo */
	command[len++] = DATAMEM_SECTOR0;			/* Report:erased block No. */

	/*Preliminarily store the data that cannnot appear here, but in wacom_set_feature()*/	
	sum = 0;
	sum += 0x05;
	sum += 0x07;
	for (j = 0; j < 4; j++)
		sum += command[j];

	cmd_chksum = ~sum + 1;						/* Report:check sum */
	command[len++] = cmd_chksum;

	bRet = wacom_i2c_set_feature(fd, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "%s failed to set feature 1 \n", __func__);
		return bRet;
	}
	
	usleep(50);

	do {

		bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, BOOT_RSP_SIZE, response, COMM_REG, DATA_REG, EMR_I2C_ADDR);
		if (!bRet) {
			fprintf(stderr, "%s failed to get feature \n", __func__);
			return bRet;
		}
		if ( (response[RTRN_CMD] != 0x0e || response[RTRN_ECH] != ECH) )
			return false;
		// Dec/05/2018, v1.2.7, Martin, Confirmed response[RTRN_RSP] only equal 0xff or 0x00
	} while (response[RTRN_CMD] == 0x0e && response[RTRN_ECH] == ECH && response[RTRN_RSP] == 0xff);


	return true;
}

bool erase_codemem(int fd, int *eraseBlock, int num)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[BOOT_RSP_SIZE];
	unsigned char sum = 0;
	unsigned char cmd_chksum;
	int ECH, len = 0;
	int i, j;

	for (i = 0; i < num; i++) {
		len = 0;		
		command[len++] = BOOT_CMD_REPORT_ID;    /* Report:ReportID */
		command[len++] = BOOT_ERASE_FLASH;		/* Report:erase command */
		command[len++] = ECH = i;				/* Report:echo */
		command[len++] = *eraseBlock;			/* Report:erased block No. */
		eraseBlock++;
		
		/*Preliminarily store the data that cannnot appear here, but in wacom_set_feature()*/	
		sum = 0;
		sum += 0x05;
		sum += 0x07;
		for (j = 0; j < 4; j++)
			sum += command[j];

		cmd_chksum = ~sum + 1;					/* Report:check sum */
		command[len++] = cmd_chksum;
	
		bRet = wacom_i2c_set_feature(fd, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
		if (!bRet) {
			fprintf(stderr, "%s failed to set feature \n", __func__);
			return bRet;
		}	

		usleep(50);

		do {	

			bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, BOOT_RSP_SIZE, response, COMM_REG, DATA_REG, EMR_I2C_ADDR);
			if (!bRet) {
				fprintf(stderr, "%s failed to get feature \n", __func__);
				return bRet;
			}			
			if ( (response[RTRN_CMD] != 0x00 || response[RTRN_ECH] != ECH) )
				return false;
			// Dec/05/2018, v1.2.7, Martin, Confirmed response[RTRN_RSP] only equal 0xff or 0x00
		} while (response[RTRN_CMD] == 0x00 && response[RTRN_ECH] == ECH && response[RTRN_RSP] == 0xff);
	}

	return true;
}

bool flash_erase_w9013(int fd, int *eraseBlock, int num)
{
	bool ret;

	ret = erase_datamem(fd);
	if (!ret) {
		fprintf(stderr, "%s erasing datamem failed \n", __func__);
		return false;
	}

	ret = erase_codemem(fd, eraseBlock, num);
	if (!ret) {
		fprintf(stderr, "%s erasing codemem failed \n", __func__);
		return false;
	}

	return true;
}

int check_progress(u8 *data, size_t size, u8 cmd, u8 ech)
{
	if (data[0] != cmd || data[1] != ech) {
		fprintf(stderr, "%s failed to erase \n", __func__);
		return -EXIT_FAIL;
	}

	switch (data[2]) {
	case PROCESS_CHKSUM1_ERR:
	case PROCESS_CHKSUM2_ERR:
	case PROCESS_TIMEOUT_ERR:
		fprintf(stderr, "%s error: %x \n", __func__, data[2]);
		return -EXIT_FAIL;
	}

	return data[2];
}

/*-----------------------------------------------------*/
/*--------------------- For W9021      ----------------*/
/*-----------------------------------------------------*/
bool flash_erase_w9021(int fd)
{
	bool bRet = false;
	u8 command[BOOT_CMD_SIZE] = {0};
	u8 response[BOOT_RSP_SIZE] = {0};
	int i = 0, len = 0;
	int ECH = 0, sum = 0;
	int ret = -1;

	command[len++] = 7;
	command[len++] = ERS_ALL_CMD;
	command[len++] = ECH = 2;
	command[len++] = ERS_ECH2;

	//Preliminarily stored data that cannnot appear here, but in wacom_set_feature()
	sum += 0x05;
	sum += 0x07;
	for (i = 0; i < len; i++)
		sum += command[i];

	command[len++] = ~sum + 1;

	bRet = wacom_i2c_set_feature(fd, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "%s failed to set feature \n", __func__);
		return bRet;
	}	

	do {
		bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, BOOT_RSP_SIZE, response, 
					     COMM_REG, DATA_REG, EMR_I2C_ADDR);
		if (!bRet) {
			fprintf(stderr, "%s failed to set feature \n", __func__);
			return bRet;
		}
		
		if ((ret = check_progress(&response[1], (BOOT_RSP_SIZE - 3), ERS_ALL_CMD, ECH)) < 0)
			return false;
	} while(ret == PROCESS_INPROGRESS);

	return true;
}

bool flash_erase_emr(int fd, int *eraseBlock, int num, int mpu_type)
{
	bool bRet = false;

	switch (mpu_type) {
	case MPU_W9013:
		bRet = flash_erase_w9013(fd, eraseBlock, num);
		break;

	case MPU_W9021:
		bRet = flash_erase_w9021(fd);
		break;

	default:
		/* If no MPU is matched, just return "false" */
		break;
		
	}

	return bRet;
}

/*--------------------------------------------erase--------------------------------------------------------------------------*/



/*-------------------------------------------- write & verify -----------------------------------------------------------------------*/
bool flash_write_block_emr(int fd, char *flash_data, 
			   unsigned long ulAddress, u8 *pcommand_id, int *ECH, int mpu_type, unsigned int block_size)
{
	const int MAX_COM_SIZE = (8 + block_size + 2);  //8: num of command[0] to command[7]
                                                              //FLASH_BLOCK_SIZE: unit to erase the block
                                                              //Num of Last 2 checksums
	bool bRet = false;
	u8 command[300] = {0};
        int boot_cmd_size = (mpu_type == MPU_W9013) ? BOOT_CMD_SIZE : BOOT_CMD_SIZE_W9021;
	unsigned char sum = 0;
	int i = 0;

	command[0] = BOOT_CMD_REPORT_ID;	    /* Report:ReportID */
	command[1] = BOOT_WRITE_FLASH;			/* Report:program  command */
	command[2] = *ECH = ++(*pcommand_id);	/* Report:echo */
	command[3] = ulAddress & 0x000000ff; 
	command[4] = (ulAddress & 0x0000ff00) >> 8;
	command[5] = (ulAddress & 0x00ff0000) >> 16;
	command[6] = (ulAddress & 0xff000000) >> 24;	/* Report:address(4bytes) */
	command[7] = (mpu_type == MPU_W9013) ? 0x08 : 0x20; /* If not W9013, that means W9021 this time*/

	/* Preliminarily store the data that cannnot appear here, but in wacom_set_feature() */	
	sum = 0;
	sum += 0x05;
	sum += ((mpu_type == MPU_W9013) ? 0x4c : 0x0d); /* If not W9013, that means W9021 this time*/

	
	for (i = 0; i < 8; i++)
		sum += command[i];
	
command[MAX_COM_SIZE - 2] = ~sum + 1;			/* Report:command checksum */
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "addr 0x%x Checksum1: 0x%x \n", ulAddress, sum);
#endif	
	
	sum = 0;
	for (i = 8; i < (block_size + 8); i++){
		command[i] = flash_data[ulAddress + (i - 8)];
		sum += flash_data[ulAddress+(i - 8)];
	}

command[MAX_COM_SIZE - 1] = ~sum + 1;				/* Report:data checksum */

#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "addr 0x%x Checksum2: 0x%x \n", ulAddress, sum);
#endif		
	/* Subtract 8 for the first 8 bytes*/
	bRet = wacom_i2c_set_feature(fd, REPORT_ID_1, (boot_cmd_size + 4 - 8), command, COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "%s failed to set feature \n", __func__);
		return bRet;
	}	

	usleep(50);

	return bRet;
}

bool flash_write_emr(int fd, char *flash_data,
		     unsigned long start_address, unsigned long *max_address, int mpu_type, unsigned int block_size)
{
	bool bRet = false;
	u8 command_id = 0;
	u8 response[BOOT_RSP_SIZE] = {0};
	int i, j, ECH = 0, ECH_len = 0;
	int ECH_ARRAY[3] = {0};
	unsigned long ulAddress = 0;
	int ret = -1;

	j = 0;
	for (ulAddress = start_address; ulAddress < *max_address; ulAddress += block_size) {
		for (i = 0; i < block_size; i++) {
			if ((u8)(flash_data[ulAddress+i]) != 0xFF)
				break;
		}
		if (i == (block_size))
			continue;

		bRet = flash_write_block_emr(fd, flash_data, ulAddress, &command_id, &ECH, mpu_type, block_size);
		if(!bRet)
			return bRet;
		if (ECH_len == 3)
			ECH_len = 0;

		ECH_ARRAY[ECH_len++] = ECH;
		if (ECH_len == 3) {
			for (j = 0; j < 3; j++) {
				do {

					bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, BOOT_RSP_SIZE, response, COMM_REG, DATA_REG, EMR_I2C_ADDR);
					if (!bRet) {
						fprintf(stderr, "%s failed to set feature \n", __func__);
						return bRet;
					}

					ret = check_progress(&response[1], (BOOT_RSP_SIZE - 3), 0x01, ECH_ARRAY[j]);
					if (ret < 0) {
						fprintf(stderr, "addr: 0x%x res1:0x%x res2:0x%x res3:0x%x \n",
							(unsigned int)ulAddress, response[RTRN_CMD], response[RTRN_ECH], response[RTRN_RSP]);
						return false;
					}
				} while (ret == PROCESS_INPROGRESS);
			}
		}
	}

return true;
}


/*-------------------------------------------- write & verify -----------------------------------------------------------------------*/




/***********************************************/
/***********************************************/
/* flash_cmd, flash_query, flash_blver, and    */
/* flash_mputype can be commonly used between  */
/* W9013 and W9021.                            */
/***********************************************/
/***********************************************/
int wacom_i2c_flash_emr(int fd, char *flash_data)
{
	bool bRet = false;
	int result, i;
	int retry_cnt = 0;
	int eraseBlock[200], eraseBlockNum;
	int iBLVer = 0, iMpuType = 0;
	unsigned long max_address = 0;			/* Max.address of Load data */
	unsigned long start_address = 0x2000;	/* Start.address of Load data */
	unsigned int block_num = 0;
	unsigned int block_size = 0;

	/*Obtain boot loader version*/
	if (!flash_blver_emr(fd, &iBLVer)) {
		fprintf(stderr, "%s failed to get Boot Loader version \n", __func__);
		return -EXIT_FAIL_GET_BOOT_LOADER_VERSION;
	}
	
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "BL version: %x \n", iBLVer);
#endif

	/*Obtain MPU type: this can be manually done in user space*/
	if (!flash_mputype_emr(fd, &iMpuType)) {
		fprintf(stderr, "%s failed to get MPU type \n", __func__);
		return -EXIT_FAIL_GET_MPU_TYPE;
	}
	if (iMpuType != MPU_W9013 && iMpuType != MPU_W9021) {
		fprintf(stderr, "MPU is not for W9013 / W9021 : 0x%x \n", iMpuType);
		return -EXIT_FAIL_GET_MPU_TYPE;
	}

#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "MPU num: 0x%x \n", iMpuType);
	fprintf(stderr, "MPU type: %s \n", (iMpuType == MPU_W9013) ? "W9013" : "W9021");
#endif	
	/*-----------------------------------*/
	/*Flashing operation starts from here*/

	/*Set start and end address and block numbers*/
	eraseBlockNum = 0;

	switch (iMpuType) {
	case MPU_W9013:
		start_address = W9013_START_ADDR;
		max_address = W9013_END_ADDR;
		block_num = BLOCK_NUM;
		block_size = FLASH_BLOCK_SIZE;

		for (i = block_num; i >= 8; i--) {
			eraseBlock[eraseBlockNum] = i;
			eraseBlockNum++;
		}	

		break;

	case MPU_W9021:
	default:
		start_address = W9021_START_ADDR;
		max_address = W9021_END_ADDR;
		block_num = BLOCK_NUM_W9021;
		block_size = FLASH_BLOCK_SIZE_W9021;
		break;
		
	}

 retry:









	
	/*Erase the old program*/

	/********* Note *********************/
	/* eraseBlock and eraseBlockNum are */
	/* required for only W9013          */
	/************************************/
	if (!erased) {
#ifdef WACOM_DEBUG_LV1
		fprintf(stderr, "%s erasing the current firmware \n", __func__);
#endif
	
		bRet = flash_erase_emr(fd, eraseBlock,  eraseBlockNum, iMpuType);
		if (!bRet) {
			fprintf(stderr, "%s failed to erase the user program \n", __func__);
			result = -EXIT_FAIL_ERASE;
			goto fail;
		}
	}

	erased = true;
	
	/*Write the new program*/
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "%s writing new firmware \n", __func__);
#endif
	
	bRet = flash_write_emr(fd, flash_data, start_address, &max_address, iMpuType, block_size);
	if (!bRet) {
		fprintf(stderr, "%s failed to write firmware \n", __func__);
		result = -EXIT_FAIL_WRITE_FIRMWARE;
		goto fail;
	}	
	
	/*Return to the user mode*/
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "%s closing the boot mode \n", __func__);
#endif
	bRet = flash_end_emr(fd);
	if (!bRet) {
		fprintf(stderr, "%s closing boot mode failed  \n", __func__);
		result = -EXIT_FAIL_EXIT_FLASH_MODE;
		goto fail;
	}

#ifdef WACOM_DEBUG_LV1	
	fprintf(stderr, "%s write and verify completed \n", __func__);
#endif
	result = EXIT_OK;

 fail:
	if (result != -EXIT_FAIL_EXIT_FLASH_MODE && result < 0 
	    && retry_cnt < NUM_OF_RETRY) {
		fprintf(stderr, "Flash failed; retrying...; count: %d\n\n\n", (retry_cnt + 1));
		retry_cnt++;
		msleep(300);
		goto retry;
	}

	return result;	
}

int wacom_i2c_flash(int fd, char *flash_data)
{
	int ret;

	ret = wacom_flash_cmd(fd);
	if (ret < 0) {
		fprintf(stderr, "%s cannot send flash command \n", __func__);
	}

	ret = flash_query_emr(fd);
	if(ret < 0) {
		fprintf(stderr, "%s Error: cannot send query \n", __func__);
		return -EXIT_FAIL;
	}
	
	ret = wacom_i2c_flash_emr(fd, flash_data);
	if (ret < 0) {
		fprintf(stderr, "%s Error: flash failed \n", __func__);
		return -EXIT_FAIL;
	}

	return ret;
}

int wacom_flash_emr(int fd, char *data)
{
	int ret = -1;
#ifdef CONDUCT_FLASH
	ret =  wacom_i2c_flash(fd, data);
	if (ret < 0) {
		fprintf(stderr, "%s failed to flash firmware\n", __func__);
		ret = -EXIT_FAIL;
		goto exit;
	}
	
	msleep(200);
#endif

 exit:
	return ret;
}

/*********************************************************************************************************/
void show_hid_descriptor(HID_DESC hid_descriptor)
{
	fprintf(stderr,  "Length:%d bcdVer:0x%x RPLength:0x%x \
RPRegister:%d InputReg:%d \n \
MaxInput:%d OutReg:%d MaxOut:%d \
ComReg:%d DataReg:%d \n VID:0x%x \
PID:0x%x wVer:0x%x ResvH:%d ResvL%d\n",
	       hid_descriptor.wHIDDescLength,
	       hid_descriptor.bcdVersion,
	       hid_descriptor.wReportDescLength,
	       hid_descriptor.wReportDescRegister,
	       hid_descriptor.wInputRegister,
	       hid_descriptor.wMaxInputLength,
	       hid_descriptor.wOutputRegister,
	       hid_descriptor.wMaxOutputLength,
	       hid_descriptor.wCommandRegister,
	       hid_descriptor.wDataRegister,
	       hid_descriptor.wVendorID,
	       hid_descriptor.wProductID,
	       hid_descriptor.wVersion,
	       hid_descriptor.RESERVED_HIGH,
	       hid_descriptor.RESERVED_LOW);
}

int get_hid_desc(int fd, char addr, unsigned int *pid)
{
	struct i2c_rdwr_ioctl_data packets;
	HID_DESC hid_descriptor;		
	int ret = -1;
	char cmd[] = {HID_DESC_REGISTER, 0x00};
	struct i2c_msg msgs[] = {
		{
			.addr = addr,
			.flags = 0,
			.len = sizeof(cmd),
			.buf = cmd,
		},
		{
			.addr = addr,
			.flags = I2C_M_RD,
			.len = sizeof(HID_DESC),
			.buf = (char *)&hid_descriptor,
		},
	};

	packets.msgs  = msgs;
	packets.nmsgs = 2;
	if (ioctl(fd, I2C_RDWR, &packets) < 0) {
		fprintf(stderr, "%s failed to send messages\n", __func__);
		ret = -EXIT_FAIL;
		goto out;
	}

	*pid = hid_descriptor.wProductID;

	show_hid_descriptor(hid_descriptor);

	ret = 0;
 out:
	return ret;
}

unsigned int parse_active_fw_version(char *data, int tech)
{
	unsigned int fw_ver = 0;

	if (tech == TECH_EMR) {
		fw_ver = (unsigned int)data[12] << 8 | data[11];
	} else if ( data[0] == TOUCH_CMD_QUERY ) {
		fw_ver = (unsigned int)data[12] << 8 | data[11];
		fw_ver = fw_ver * AES_FW_BASE + data[13];
	} else {
		fprintf(stderr, "data 0x%02x error, cannot parse fw version \n", data[0]);
		fw_ver = 0;
	}

	return fw_ver;
}

int wacom_gather_info(int fd, unsigned int *fw_ver, int tech)
{
	int ret = -1;
	size_t report_size = ((tech == TECH_EMR) ? WACOM_QUERY_SIZE : TOUCH_QUERY_SIZE);
	char report_id = ((tech == TECH_EMR) ? WACOM_CMD_QUERY : TOUCH_CMD_QUERY);
	char addr = ((tech == TECH_EMR) ? EMR_I2C_ADDR: AES_I2C_ADDR);
	char data[WACOM_QUERY_SIZE] = {0};
	bool bRet = false;

	bRet = wacom_i2c_get_feature(fd, report_id, report_size,
				     (u8 *)data, COMM_REG, DATA_REG, addr);
	if ( bRet == false ) {
		fprintf(stderr, "cannot get data query \n");
		goto out;
	}

	*fw_ver = parse_active_fw_version(data, tech);
	ret = 0;

 out:
	return ret;
}

int get_device(unsigned int *current_fw_ver, unsigned int *pid, char *device_num, int *tech)
{
	int fd = -1;
	int ret = -1;
	int i;
	char addr = EMR_I2C_ADDR;

	/*Wacom I2C device can be with an address either 0x09(EMR) or 0x0a(touch&AES)*/
	*tech = TECH_UNKNOWN;
	for (i = 0; i < 2; i++) {
		fd = open(device_num, O_RDWR);
		if (fd < 0) {
			fprintf(stderr, "cannot open %s \n", device_num);
			goto exit;
		}
	
		/*Use I2C_SLAVE_FORCE for DMA transfer*/
		ret = ioctl(fd, I2C_SLAVE_FORCE, addr);
		if (ret < 0) {
			fprintf(stderr, "Falied to set the slave address: %d \n", addr);
			close(fd);
			goto exit;
		}

		ret = get_hid_desc(fd, addr, pid);
		if (ret == 0) {
			*tech = (addr == EMR_I2C_ADDR) ? TECH_EMR : TECH_AES;
			fprintf(stderr, "%s found: addr 0x%x \n", (*tech == TECH_EMR) ? "EMR" : "AES", addr);

			break;
		}

		close(fd);
		addr = AES_I2C_ADDR;
	}

	if (*tech != TECH_UNKNOWN) {
	ret = wacom_gather_info(fd, current_fw_ver, *tech);
		if (ret == 0) {
			ret = fd;
			// even everything is OK, we should check if it is boot loader, then version should set to zero for process firmware update
			if ( (*tech == TECH_EMR ) && ( *pid == EMR_UBL_PID ) ) {
				*current_fw_ver = 0;
			} else if ( (*tech == TECH_AES ) && ( *pid == UBL_G11T_UBL_PID ) ) {
				*current_fw_ver = 0;
			} 
			goto exit;
		}
		close(fd);
	}
	fprintf(stderr, "Cannot get device infomation\n");

 exit:
	return ret;

}

int main(int argc, char *argv[])
{
	unsigned long maxAddr = 0;
	int fd = -1;
	int ret = -1;
	unsigned int pid = 0;
	unsigned int current_fw_ver = 0;
	int tech = TECH_EMR;
	size_t data_size = 0;
	char *data;
	char device_num[64] = {0};
	bool active_fw_check = false;
	bool force_flash = false;
	bool pid_check = false;
#ifdef HWID_SUPPORT
	unsigned long hwid = 0;
	bool hwid_check = false;
#endif
	FILE *fp;
	UBL_STATUS *pUBLStatus = NULL;
	UBL_PROCESS *pUBLProcess = NULL;

	if (argc != 4){
		fprintf(stderr,  "%s %s\n", PROGRAM_NAME, VERSION_STRING);
		fprintf(stderr,  "Usage: $%s [firmware filename] [type] [i2c-device path]\n", PROGRAM_NAME);
		fprintf(stderr,  "Ex: $%s W9017_492E_0012.hex -a i2c-1 \n", PROGRAM_NAME);
		ret = -EXIT_NOFILE;
		goto exit;
	}

	if (!strcmp(argv[2], "-a")) {
		fprintf(stderr,  "Returning active firmware version only\n");
		active_fw_check = true;
	} else if (!strcmp(argv[2], "-p")) {
		fprintf(stderr,  "Returning PID only\n");
		pid_check = true;
	}
#ifdef HWID_SUPPORT
    else if (!strcmp(argv[2], "-h")) {
		fprintf(stderr,  "Returning HWID only\n");
		hwid_check = true;
	}
#endif
    else if (!strcmp(argv[2], FLAGS_RECOVERY_TRUE)) {
		force_flash = true;
	} else if (!strcmp(argv[2], FLAGS_RECOVERY_FALSE)) {
		fprintf(stderr,  "Force flash is NOT set\n");
	} else {
		fprintf(stderr, "option is not valid \n");
		ret = -EXIT_NOSUCH_OPTION;
		goto exit;
	}

	sprintf(device_num, "%s%s", DEVFILE_PATH, argv[3]);

#ifdef I2C_OPEN
	/*Opening and setting file descriptor   */
	fd = get_device(&current_fw_ver, &pid, device_num, &tech);
	if (fd < 0) {
		fprintf(stderr, "cannot find Wacom i2c device\n");
		ret = fd;
		goto exit;
	}

	if (active_fw_check) {
		ret = 0;
		printf("%u\n", current_fw_ver);
		goto exit;
	} else if (pid_check) {
		ret = 0;
		printf("%04x\n", pid); // in hex digit, for ex: 4877 means 0x4877
		goto exit;
	} 
#ifdef HWID_SUPPORT
    else if (hwid_check) { // Feb/13/2018, Martin. This should only works for AES
		if (tech == TECH_AES) {
			ret = wacom_get_hwid(fd, pid, &hwid);
		}
		// EMR not support HWID now, just leave it 0, and return value is -1
		printf("%04x_%04x\n", (unsigned int)((hwid&0xFFFF0000)>>16), (unsigned int)(hwid&0x0000FFFF));
		goto exit;
	} 
#endif
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "%s current_fw: 0x%x \n", __func__, current_fw_ver);
#endif
#endif //I2C_OPEN

	if (tech == TECH_EMR) {
		data_size = DATA_SIZE;
	}  else {
		data_size = UBL_ROM_SIZE;
		pUBLStatus = (UBL_STATUS *)malloc(sizeof(UBL_STATUS));
		pUBLProcess = (UBL_PROCESS *)malloc(sizeof(UBL_PROCESS));
		if (pUBLStatus == NULL || pUBLProcess == NULL) {
			fprintf(stderr, "cannot preserve memories \n");
			return -ERR;
		}

		memset(pUBLStatus, 0, sizeof(UBL_STATUS) );
		memset(pUBLProcess, 0, sizeof(UBL_PROCESS) );
		pUBLProcess->start_adrs = 0;
		pUBLProcess->process = 0;	
	}

	data = (char *)malloc(sizeof(char) * data_size);
	memset(data, 0xff, data_size);

#ifdef FILE_READ
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "Reading hex file: %s \n", argv[1]);
#endif
	fp = fopen(argv[1], "rb");  /* FW_LINK_PATH */
	if (fp == NULL) {
		fprintf(stderr, "the file name is invalid or does not exist \n");
		goto err;
	}

	ret = read_hex(fp, data, data_size, &maxAddr, pUBLProcess, tech);
	if (ret == HEX_READ_ERR) {
		fprintf(stderr, "reading the hex file failed\n");
		fclose(fp);
		goto err;
	}
	fclose(fp);
#endif

	fprintf(stderr,  "*Flash started...... \n");
	if (tech == TECH_EMR) {
		ret = wacom_flash_emr(fd, data);
		if (ret < 0)
			fprintf(stderr, "wacom emr flash failed \n");
	} else {
		ret = wacom_flash_aes(fd, data, pUBLStatus, pUBLProcess);
		if (ret < 0)
			fprintf(stderr, "wacom aes flash failed \n");
	}

#ifdef CONDUCT_FLASH
	/*getting the active firmware version again*/
	ret = wacom_gather_info(fd, &current_fw_ver, tech);
	if (ret < 0) {
		fprintf(stderr, "cannot get firmware version \n");
		goto err;
	}

	fprintf(stderr,  "Flashed firmware : %d \n", current_fw_ver);
#endif

	ret = 0;
 err:
	if (tech == TECH_AES) {
		if (pUBLStatus != NULL) {
			free(pUBLStatus);
			pUBLStatus = NULL;
		}

		if (pUBLProcess != NULL) {
			free(pUBLProcess);
			pUBLProcess = NULL;
		}
	}

	if (data != NULL) {
		free(data);
		data = NULL;
	}

exit:
	close(fd);
 
	return -ret;
}
