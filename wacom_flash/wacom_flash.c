#include "wacom_flash.h"

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

int flash_query_w9013(int fd)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ECH, len = 0;	

	command[len++] = BOOT_CMD_REPORT_ID;	                /* Report:ReportID */
	command[len++] = BOOT_QUERY;				/* Report:Boot Query command */
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

bool flash_blver_w9013(int fd, int *blver)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ECH, len = 0;	

	command[len++] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
	command[len++] = BOOT_BLVER;					/* Report:Boot Version command */
	command[len++] = ECH = 7;							/* Report:echo */

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

bool flash_mputype_w9013(int fd, int* pMpuType)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ECH, len = 0;		

	command[len++] = BOOT_CMD_REPORT_ID;	                        /* Report:ReportID */
	command[len++] = BOOT_MPU;					/* Report:Boot Query command */
	command[len++] = ECH = 7;					/* Report:echo */

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

bool flash_end_w9013(int fd)
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

	command[len++] = BOOT_CMD_REPORT_ID;                 	/* Report:ReportID */
	command[len++] = BOOT_ERASE_DATAMEM;			        /* Report:erase datamem command */
	command[len++] = ECH = BOOT_ERASE_DATAMEM;					/* Report:echo */
	command[len++] = DATAMEM_SECTOR0;				/* Report:erased block No. */

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
		if ((response[RTRN_CMD] != 0x0e || response[RTRN_ECH] != ECH) || (response[RTRN_RSP] != 0xff && response[RTRN_RSP] != 0x00))
			return false;

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
		command[len++] = BOOT_CMD_REPORT_ID;                 	/* Report:ReportID */
		command[len++] = BOOT_ERASE_FLASH;			        /* Report:erase command */
		command[len++] = ECH = i;					/* Report:echo */
		command[len++] = *eraseBlock;				/* Report:erased block No. */
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
			if ((response[RTRN_CMD] != 0x00 || response[RTRN_ECH] != ECH) || (response[RTRN_RSP] != 0xff && response[5] != 0x00))
				return false;
			
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
/*--------------------------------------------erase--------------------------------------------------------------------------*/

bool flash_write_block_w9013(int fd, char *flash_data, 
				    unsigned long ulAddress, u8 *pcommand_id, int *ECH)
{
	const int MAX_COM_SIZE = (8 + FLASH_BLOCK_SIZE + 2);  //8: num of command[0] to command[7]
                                                              //FLASH_BLOCK_SIZE: unit to erase the block
                                                              //Num of Last 2 checksums
	bool bRet = false;
	u8 command[300];
	unsigned char sum = 0;
	int i;

	command[0] = BOOT_CMD_REPORT_ID;	                /* Report:ReportID */
	command[1] = BOOT_WRITE_FLASH;			        /* Report:program  command */
	command[2] = *ECH = ++(*pcommand_id);		        /* Report:echo */
	command[3] = ulAddress & 0x000000ff;
	command[4] = (ulAddress & 0x0000ff00) >> 8;
	command[5] = (ulAddress & 0x00ff0000) >> 16;
	command[6] = (ulAddress & 0xff000000) >> 24;			/* Report:address(4bytes) */
	command[7] = 8;						/* Report:size(8*8=64) */

	/*Preliminarily store the data that cannnot appear here, but in wacom_set_feature()*/	
	sum = 0;
	sum += 0x05;
	sum += 0x4c;
	for (i = 0; i < 8; i++)
		sum += command[i];
	command[MAX_COM_SIZE - 2] = ~sum + 1;					/* Report:command checksum */
	
	sum = 0;
	for (i = 8; i < (FLASH_BLOCK_SIZE + 8); i++){
		command[i] = flash_data[ulAddress+(i - 8)];
		sum += flash_data[ulAddress+(i - 8)];
	}

	command[MAX_COM_SIZE - 1] = ~sum+1;				/* Report:data checksum */
	
	/*Subtract 8 for the first 8 bytes*/
	bRet = wacom_i2c_set_feature(fd, REPORT_ID_1, (BOOT_CMD_SIZE + 4 - 8), command, COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "%s failed to set feature \n", __func__);
		return bRet;
	}	

	usleep(50);

	return true;
}

bool flash_write_w9013(int fd, char *flash_data,
			      unsigned long start_address, unsigned long *max_address)
{
	bool bRet = false;
	u8 command_id = 0;
	u8 response[BOOT_RSP_SIZE];
	int i, j, ECH = 0, ECH_len = 0;
	int ECH_ARRAY[3];
	unsigned long ulAddress;

	j = 0;
	for (ulAddress = start_address; ulAddress < *max_address; ulAddress += FLASH_BLOCK_SIZE) {
		for (i = 0; i < FLASH_BLOCK_SIZE; i++) {
			if ((u8)(flash_data[ulAddress+i]) != 0xFF)
				break;
		}
		if (i == (FLASH_BLOCK_SIZE))
			continue;

		bRet = flash_write_block_w9013(fd, flash_data, ulAddress, &command_id, &ECH);
		if(!bRet)
			return false;
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
					
					if ((response[RTRN_CMD] != 0x01 || response[RTRN_ECH] != ECH_ARRAY[j]) || (response[RTRN_RSP] != 0xff && response[RTRN_RSP] != 0x00)) {
						fprintf(stderr, "addr: %x res:%x \n", (unsigned int)ulAddress, response[RTRN_RSP]);
						return false;
					}
				
				} while (response[RTRN_CMD] == 0x01 && response[RTRN_ECH] == ECH_ARRAY[j] && response[RTRN_RSP] == 0xff);
			}
		}
	}
	
	return true;
}

int wacom_i2c_flash_w9013(int fd, char *flash_data)
{
	bool bRet = false;
	int result, i;
	int retry_cnt = 0;
	int eraseBlock[200], eraseBlockNum;
	int iBLVer = 0, iMpuType = 0;
	unsigned long max_address = 0;			/* Max.address of Load data */
	unsigned long start_address = 0x2000;	        /* Start.address of Load data */

	/*Obtain boot loader version*/
	if (!flash_blver_w9013(fd, &iBLVer)) {
		fprintf(stderr, "%s failed to get Boot Loader version \n", __func__);
		return -EXIT_FAIL_GET_BOOT_LOADER_VERSION;
	}
	
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "BL version: %x \n", iBLVer);
#endif

	/*Obtain MPU type: this can be manually done in user space*/
	if (!flash_mputype_w9013(fd, &iMpuType)) {
		fprintf(stderr, "%s failed to get MPU type \n", __func__);
		return -EXIT_FAIL_GET_MPU_TYPE;
	}
	if (iMpuType != MPU_W9013) {
		fprintf(stderr, "MPU is not for W9013 : 0x%x \n", iMpuType);
		return -EXIT_FAIL_GET_MPU_TYPE;
	}

#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "MPU type: 0x%x \n", iMpuType);	
#endif	
	/*-----------------------------------*/
	/*Flashing operation starts from here*/

	/*Set start and end address and block numbers*/
	eraseBlockNum = 0;
	start_address = W9013_START_ADDR;
	max_address = W9013_END_ADDR;
	for (i = BLOCK_NUM; i >= 8; i--) {
		eraseBlock[eraseBlockNum] = i;
		eraseBlockNum++;
	}	

 retry:
	msleep(300);

	/*Erase the old program*/
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "%s erasing the current firmware \n", __func__);
#endif
	bRet = flash_erase_w9013(fd, eraseBlock,  eraseBlockNum);
	if (!bRet) {
		fprintf(stderr, "%s failed to erase the user program \n", __func__);
		result = -EXIT_FAIL_ERASE;
		goto fail;
	}

	/*Write the new program*/
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "%s writing new firmware \n", __func__);
#endif
	bRet = flash_write_w9013(fd, flash_data, start_address, &max_address);
	if (!bRet) {
		fprintf(stderr, "%s failed to write firmware \n", __func__);
		result = -EXIT_FAIL_WRITE_FIRMWARE;
		goto fail;
	}	
	
	/*Return to the user mode*/
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "%s closing the boot mode \n", __func__);
#endif
	bRet = flash_end_w9013(fd);
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
		fprintf(stderr, "Flash failed; retrying...; count: %d\n", (retry_cnt + 1));
		retry_cnt++;
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

	ret = flash_query_w9013(fd);
	if(ret < 0) {
		fprintf(stderr, "%s Error: cannot send query \n", __func__);
		return -EXIT_FAIL;
	}
	
	ret = wacom_i2c_flash_w9013(fd, flash_data);
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

int get_hid_desc(int fd, char addr)
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

	show_hid_descriptor(hid_descriptor);

	ret = 0;
 out:
	return ret;
}

int parse_active_fw_version(char *data, int tech)
{
	int fw_ver = 0;

	if (tech == TECH_EMR) {
		fw_ver = (int)data[12] << 8 | data[11];
	} else {
		fw_ver = (int)data[12] << 8 | data[11];
		fw_ver = fw_ver * AES_FW_BASE + data[13];
	}

	return fw_ver;
}

int wacom_gather_info(int fd, int *fw_ver, int tech)
{
	int ret = -1;
	size_t report_size = ((tech == TECH_EMR) ? WACOM_QUERY_SIZE : TOUCH_QUERY_SIZE);
	char report_id = ((tech == TECH_EMR) ? WACOM_CMD_QUERY : TOUCH_CMD_QUERY);
	char addr = ((tech == TECH_EMR) ? EMR_I2C_ADDR: AES_I2C_ADDR);
	char data[WACOM_QUERY_SIZE] = {0};
	bool bRet = false;

	bRet = wacom_i2c_get_feature(fd, report_id, report_size,
				     (u8 *)data, COMM_REG, DATA_REG, addr);
	if ( bRet == false ){
		fprintf(stderr, "cannot get data query \n");
		goto out;
	}

	*fw_ver = parse_active_fw_version(data, tech);
	ret = 0;
 out:
	return ret;
}

int get_device(int *current_fw_ver, char *device_num, int *tech)
{
	int fd = -1;
	int ret = -1;
	int i;
	char addr = EMR_I2C_ADDR;

	/*Wacom I2C device can be with an address either 0x09(EMR) or 0x0a(touch&AES)*/
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

		ret = get_hid_desc(fd, addr);
		if (ret == 0) {
			*tech = (addr == EMR_I2C_ADDR) ? TECH_EMR : TECH_AES;
			fprintf(stderr, "%s found: addr 0x%x \n", (*tech == TECH_EMR) ? "EMR" : "AES", addr);

			break;
		}

		close(fd);
		addr = AES_I2C_ADDR;
	}

	ret = wacom_gather_info(fd, current_fw_ver, *tech);
	if (ret < 0) {
		fprintf(stderr, "Cannot get device infomation\n");
		close(fd);
		goto exit;
	}

	ret = fd;

 exit:
	return ret;

}
int main(int argc, char *argv[])
{
	unsigned long maxAddr = 0;
	int fd = -1;
	int ret = -1;
	int current_fw_ver = -1;
	int tech = TECH_EMR;
	size_t data_size = 0;
	char *data;
	char device_num[64] = {0};
	bool active_fw_check = false;
	bool force_flash = false;

	FILE *fp;
	UBL_STATUS *pUBLStatus = NULL;
	UBL_PROCESS *pUBLProcess = NULL;

	if (argc != 4){
		fprintf(stderr,  "Usage: $wacom_flash [firmware filename] [type] [i2c-device path]\n");
		fprintf(stderr,  "Ex: $wacom_flash W9013_056.hex -r i2c-1 \n");
		ret = -EXIT_NOFILE;
		goto exit;
	}

	if (!strcmp(argv[2], "-a")) {
		fprintf(stderr,  "Returning active firmware version only\n");
		active_fw_check = true;
	} else if (!strcmp(argv[2], FLAGS_RECOVERY_TRUE)) {
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
	fd = get_device(&current_fw_ver, device_num, &tech);
	if (fd < 0) {
		fprintf(stderr, "cannot find Wacom i2c device\n");
		ret = fd;
		goto exit;
	}

	if (active_fw_check) {
		ret = 0;
		printf("%d\n", current_fw_ver);
		goto exit;
	} 

#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "%s current_fw: 0x%x \n", __func__, current_fw_ver);
#endif

#endif
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
		pUBLProcess->start_adrs = 0;//UBL_G11T_BASE_FLASH_ADDRESS;
		pUBLProcess->process = 0;	
	}

	data = (char *)malloc(sizeof(char) * data_size);
	memset(data, 0xff, data_size);



#ifdef FILE_READ

#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "Reading hex file: %s... \n", argv[1]);
#endif
	fp = fopen(argv[1], "rb");  /* FW_LINK_PATH */
	if (fp == NULL) {
		fprintf(stderr, "the file name is invalid or does not exist \n");
		goto err;
	}

	ret = read_hex(fp, data, data_size, &maxAddr, pUBLProcess, pUBLStatus, tech);
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
