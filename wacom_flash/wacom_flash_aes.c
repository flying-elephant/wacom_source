#include "wacom_flash.h"

bool wacom_send_cmd(int fd, boot_cmd *command, boot_rsp *response)
{
	bool bRet =false;
	unsigned int i;

	command->header.reportId = UBL_CMD_REPORT_ID;
	response->header.reportId = UBL_RSP_REPORT_ID;

#ifdef WACOM_DEBUG_LV3
        printf("Set Feature. command:0x%x \n", command->header.cmd);
#endif

	/*Set feature here*/
	bRet = wacom_i2c_set_feature(fd, UBL_CMD_REPORT_ID, UBL_CMD_SIZE_G11T, command->data,
				     COMM_REG, DATA_REG);
	if ( bRet == false ){
		if ( command->header.cmd == UBL_COM_EXIT ){
			return true;
		}

		fprintf(stderr, "%s failed \n", __func__);

		return false;
	}

	if ( command->header.cmd != UBL_COM_EXIT ){
		for ( i = 0; i < UBL_TIMEOUT_WRITE; i++ ){
			bRet = wacom_i2c_get_feature(fd, UBL_RSP_REPORT_ID, UBL_RSP_SIZE_G11T, response->data,
						     COMM_REG, DATA_REG, AES_I2C_ADDR);
			if ( bRet == false ){
				fprintf(stderr, "GET ERROR at command:%d \n", command->header.cmd);
				return false;
			}

#ifdef WACOM_DEBUG_LV3
			fprintf(stderr, "Get Feature. response:0x%X \n", response->header.resp);
#endif
			if ( (command->header.cmd != response->header.cmd) || (command->header.echo != response->header.echo) ){
				fprintf(stderr, "RESPONSE not match at command:%d \n", command->header.cmd );
				return false;
			}

			if ( response->header.resp != UBL_RES_BUSY ){
				// other than returned value is UBL_RES_BUSY, quit
				break;
			}
			usleep(1 * MILLI);
		}	// wait unti device processes the command

		if ( i == UBL_TIMEOUT_WRITE ){
			fprintf(stderr, "TIMEOUT at command:%d \n", command->header.cmd );
			return false;
		}

		// Command that generally returns "OK" response is treated as an error
		if ( ((command->header.cmd == UBL_COM_WRITE ) || (command->header.cmd == UBL_COM_ALLERASE))
			&& (response->header.resp != UBL_RES_OK) ){
			fprintf(stderr,"Error response:%d at command:%d \n", response->header.resp, command->header.cmd );
			return false;
		}
	}

	return true;
}

// Switch to bootloader
bool wacom_enter_ubl(int fd)
{
	bool bRet = false;
	unsigned char data[2];

	data[0] = DEVICETYPE_REPORT_ID;
	data[1] = DEVICETYPE_UBL;

	bRet = wacom_i2c_set_feature(fd, DEVICETYPE_REPORT_ID, 2, data,
				     COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "%s set feature failed\n", __func__);
		return false;
	}
	usleep(300 * MILLI);	// Mar/07/2018, v1.2.6, Martin, Try reduce sleep time to minimum needed 300 ms

	return true;
}

// Switch back to normal device
bool wacom_exit_ubl(int fd)
{
	boot_cmd command;
	boot_rsp response;

	memset(&command, 0, sizeof(boot_cmd));
	memset(&response, 0, sizeof(boot_rsp));	

	command.header.cmd = UBL_COM_EXIT;
	if ( wacom_send_cmd(fd, &command, &response) == false ){
		fprintf(stderr, "%s exiting user boot loader failed \n", __func__);
		return false;
	}
	usleep(300 * MILLI);	// Mar/07/2018, v1.2.6, Martin, Try reduce sleep time to minimum needed 300 ms

	return true;
}

// Check if switched to bootloader
bool wacom_check_mode(int fd)
{
	boot_cmd command;
	boot_rsp response;

	memset(&command, 0, sizeof(boot_cmd));
	memset(&response, 0, sizeof(boot_rsp));

	command.header.cmd = UBL_COM_CHECKMODE;
	if ( wacom_send_cmd(fd, &command, &response) == false ){
		fprintf(stderr, "%s Sending command failed \n", __func__);
		return false;
	}
	if ( response.header.resp != UBL_G11T_MODE_UBL ){
		fprintf(stderr, "%s Not in user boot loader mode. Mode:%d \n", __func__, response.header.resp );
		return false;
	}

	return true;
}

// Get info about device
bool wacom_get_data(int fd, UBL_STATUS *pUBLStatus)
{
	boot_cmd command;
	boot_rsp response;

	memset( &command, 0, sizeof( boot_cmd ) );
	memset( &response, 0, sizeof( boot_rsp ) );

	// MPUTYPE
	command.header.cmd = UBL_COM_GETMPUTYPE;
	if ( wacom_send_cmd(fd, &command, &response) == false ){
		fprintf(stderr, "%s reportid: %x, cmd: %x, echo: %x, resp: %x \n", __func__, 
			response.header.reportId, response.header.cmd, response.header.echo, response.header.resp);
		return false;
	}

	pUBLStatus->mputype = response.header.resp;

#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "MPU type: 0x%x \n", response.header.resp);
#endif

	// UBL Version
	command.header.cmd = UBL_COM_GETBLVER;
	if ( wacom_send_cmd(fd, &command, &response) == false ){
		fprintf(stderr, "%s Obtaining UBL version failed \n", __func__);
		return false;
	}

	pUBLStatus->ubl_ver = response.header.resp;
	pUBLStatus->protect = 0;

#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "UBL version: 0x%x \n", response.header.resp);
#endif

	return true;
}

bool wacom_check_data( UBL_PROCESS *pUBLProcess, UBL_STATUS *pUBLStatus)
{

	// Checking start address
	if ( pUBLProcess->start_adrs != UBL_G11T_BASE_FLASH_ADDRESS ){
		fprintf(stderr, "%s failed \n", __func__);
		fprintf(stderr, "Data error. Start at 0x%05x.\n", (unsigned int)pUBLProcess->start_adrs );

		return false;
	}

	// checking the total firmware size
	if ( (pUBLProcess->start_adrs + pUBLProcess->size) > (UBL_G11T_MAX_FLASH_ADDRESS + 1) ){
		fprintf(stderr, "%s failed \n", __func__);
		fprintf(stderr, "Data size error. Size is 0x%05x. \n", (unsigned int)pUBLProcess->size );

		return false;
	}

	return true;
}

bool wacom_erase_all(int fd,  UBL_PROCESS *pUBLProcess)
{
	bool bRet = false;
	boot_cmd command;
	boot_rsp response;

	memset(&command, 0, sizeof(boot_cmd));
	memset(&response, 0, sizeof(boot_rsp));

	command.erase_flash.reportId = UBL_CMD_REPORT_ID;
	command.erase_flash.cmd = UBL_COM_ALLERASE;	// erase all flash
	command.erase_flash.echo = 1;
	command.erase_flash.blkNo = 0;

		bRet = wacom_i2c_set_feature(fd, UBL_CMD_REPORT_ID, UBL_CMD_SIZE_G11T, command.data,
					     COMM_REG, DATA_REG);
	if ( bRet == false ){
			fprintf(stderr, "%s failed \n", __func__);
			return false;
		}
		
	usleep(2000 * MILLI);	// wait for 2 seconds

		response.erase_flash.reportId = UBL_RSP_REPORT_ID;
		response.header.resp = UBL_RES_BUSY;	// busy
		
		while(bRet && (response.header.resp == UBL_RES_BUSY)) {
			bRet = wacom_i2c_get_feature(fd, UBL_RSP_REPORT_ID, UBL_RSP_SIZE_G11T, response.data,
						     COMM_REG, DATA_REG, AES_I2C_ADDR);
		}

		if (!bRet) {
			fprintf(stderr, "%s failed \n", __func__);
			return false;
		}

	if(response.header.resp != UBL_RES_OK) {
			fprintf(stderr, "%s failed \n", __func__);
			return false;
		}

	return true;
}

bool wacom_send_data(int fd, unsigned char com, unsigned char *data, unsigned long start_adrs, unsigned long size, UBL_STATUS *pUBLStatus )
{
	unsigned int i, j;
	bool bRet = false;
	boot_cmd command;
	boot_rsp response;
	unsigned char command_id = 0;

	// g_FlashBlockSize - global variable containing size of one data block in read/write report
	for (i = 0; i < (UBL_G11T_MAX_FLASH_ADDRESS + 1) / UBL_G11T_CMD_DATA_SIZE; i++)	{
		if ( i * UBL_G11T_CMD_DATA_SIZE >= size ){
			break;
		}

		memset(&command, 0, sizeof(boot_cmd));
		memset(&response, 0, sizeof(boot_rsp));

		pUBLStatus->progress = i * UBL_G11T_CMD_DATA_SIZE;
		command.write_flash.reportId = UBL_CMD_REPORT_ID;
		command.write_flash.cmd = com;
		command.write_flash.echo = ++command_id;
		command.write_flash.addr = (u32)(start_adrs + i * UBL_G11T_CMD_DATA_SIZE);
		command.write_flash.size8 = UBL_G11T_CMD_DATA_SIZE / 8;

		// Copy g_FlashRepDataSize bytes to report
		for ( j = 0; j < UBL_G11T_CMD_DATA_SIZE; j++ ){
			command.write_flash.data[j] = *(data + i * UBL_G11T_CMD_DATA_SIZE + j);
		}

		bRet = wacom_i2c_set_feature(fd, UBL_CMD_REPORT_ID, UBL_CMD_SIZE_G11T, command.data,
				     COMM_REG, DATA_REG);
		if ( bRet == false ){
			pUBLStatus->ret = UBL_ERROR;
			fprintf(stderr, "SET ERROR at block:%d \n", i);
			return false;
		}

		response.write_flash.reportId = UBL_RSP_REPORT_ID;
		for ( j = 0; j < UBL_TIMEOUT_WRITE; j++ ){
			bRet = wacom_i2c_get_feature(fd, UBL_RSP_REPORT_ID, UBL_RSP_SIZE_G11T, response.data,
						     COMM_REG, DATA_REG, AES_I2C_ADDR);
			if (bRet == false){
				pUBLStatus->ret = UBL_ERROR;
				fprintf(stderr, "GET ERROR at block:%d \n", i);
				return false;

			}
#ifdef WACOM_DEBUG_LV1
				if ((i % 100) == 0)
					fprintf(stderr, "\n");
				fprintf(stderr, ".");
#endif	
			if ( (command.header.cmd != response.header.cmd) || (command.header.echo != response.header.echo) ){
				pUBLStatus->ret = UBL_ERROR;
				fprintf(stderr, "RESPONSE not match at block:%d \n", i);
				return false;
			}
			if ( (response.header.resp != UBL_RES_OK) && (response.header.resp != UBL_RES_BUSY) ){
				pUBLStatus->response = response.header.resp;
				pUBLStatus->ret = UBL_ERROR;

				switch ( response.header.resp ){
					case UBL_RES_MCUTYPE_ERROR:
						fprintf(stderr, "File error. MCUTYPE not match. \n");
						break;

					case UBL_RES_PID_ERROR:
						fprintf(stderr, "File error. PID not match. \n" );
						break;

					case UBL_RES_VERSION_ERROR:
						fprintf(stderr, "File error. VERSION is older. \n" );
						break;

					default:
						fprintf(stderr, "Error response@SendData:%d at block:%d \n", response.header.resp, i );
						break;
				}

				return false;
			}
			if ( response.header.resp == UBL_RES_OK ){
				break;
			}
			usleep(1 * MILLI);
		}

		if ( j == UBL_TIMEOUT_WRITE ){
			pUBLStatus->ret = UBL_ERROR;
			fprintf(stderr, "TIMEOUT at block:%d \n", i );
			return false;
		}
	}

	return true;
}

bool wacom_write(int fd, UBL_PROCESS *pUBLProcess, UBL_STATUS *pUBLStatus )
{
	if ( wacom_check_data(pUBLProcess, pUBLStatus) == false ){
		fprintf(stderr, "%s failed \n", __func__);
		return false;
	}

	// Erasing 
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "Erasing flash. \n");
#endif
	if (wacom_erase_all(fd, pUBLProcess) == false ){
		fprintf(stderr, "%s failed \n", __func__);
		return false;
	}

	// Writing firmware
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "Write start. \n");
	fprintf(stderr, "WRITE address:0x%x size:0x%x \n", (unsigned int)pUBLProcess->start_adrs, 
	       (unsigned int)pUBLProcess->size);
#endif
	if ( wacom_send_data(fd, UBL_COM_WRITE, pUBLProcess->data, pUBLProcess->start_adrs, pUBLProcess->size, pUBLStatus ) == false ){
		fprintf(stderr, "%s failed \n", __func__);
		return false;
	}

	return true;
}

//! G11T programming thread
int wacom_flash_aes(int fd, char *data, UBL_STATUS *pUBLStatus, UBL_PROCESS *pUBLProcess)
{
	int i;
	int ret = -1;
	bool bRet = false;

	for ( i = 0; i < pUBLProcess->size; i++ ){
		pUBLProcess->data[i] = data[i];
	}

	i = 0;
	if(pUBLStatus->pid != UBL_G11T_UBL_PID) {
		if (wacom_enter_ubl(fd) == false ){
			fprintf(stderr, "entering user boot loader error. \n");
			// 2018/Mar/07, v1.2.5, Martin, If enter boot loader fail, just exit
			goto out_write_err;
		}

		/*Check if the device successfully entered in UBL mode*/
		for ( i = 0; i < 5/*UBL_RETRY_NUM*/; i++ ){
			usleep(200 * MILLI);
		
			bRet = wacom_check_mode(fd);
			if (bRet)
				break;
			else
				goto err;
			
			pUBLStatus->ret = UBL_OK; /*Resetting the return status*/
			usleep(200 * MILLI);
		}
	}

	if ( i == UBL_RETRY_NUM ){
		pUBLStatus->ret = UBL_ERROR;
		fprintf(stderr, "entering user boot loader error(2). \n");
		goto err;
	}

#ifdef GETDATA
	//Obtaining the device's basic data; repeat the cout if failed
	for ( i = 0; i < UBL_RETRY_NUM; i++ ){
		bRet = wacom_get_data(fd, pUBLStatus );
		if (bRet == false) {
			fprintf(stderr, "Cannot correctly obtain data 1\n");
			break;
		}
		pUBLStatus->ret = UBL_OK;			//Resetting the returned status
		usleep(10 * MILLI);
	}
	if ( i == (UBL_RETRY_NUM + 1)){
		fprintf(stderr, "Cannot correctly obtain data 2\n");
		goto err;
	}
#endif

#ifdef CONDUCT_FLASH
	/*Conducting erasing and writing operation, the main part of the flash*/
	bRet = wacom_write(fd, pUBLProcess, pUBLStatus );
	if (!bRet) {
		fprintf(stderr, "UBL_G11T_Write returned false \n");
		ret = -EXIT_FAIL;
		//2018/Feb/26, v1.2.4, Martin, If writing is failed(including erase), do not call ExitG11TUBL
		goto out_write_err;
	}
#endif
	
	ret = 0;

 err:
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "\nClosing device... \n");
#endif
	bRet = wacom_exit_ubl(fd);
	if (!bRet) {
		fprintf(stderr, "exiting boot mode failed\n");
		ret = -EXIT_FAIL;
	}
out_write_err:
	return ret;
}

bool wacom_is_hwid_ok(u8 *hwid_buffer, unsigned long *hwid)
{
	unsigned long the_hw_id = 0;
	
	// If the magic number is not present, return false (incorrect hwid)
	if ((hwid_buffer[0] != 0x34) || (hwid_buffer[1] != 0x12) || (hwid_buffer[2] != 0x78) || (hwid_buffer[3] != 0x56) ||
		(hwid_buffer[4] != 0x65) || (hwid_buffer[5] != 0x87) || (hwid_buffer[6] != 0x21) || (hwid_buffer[7] != 0x43))
	{
		fprintf(stderr, "HWID - Incorrect magic number\n");
		return false;
	}

	the_hw_id = hwid_buffer[9];
	the_hw_id = (the_hw_id << 8)+ hwid_buffer[8];
	the_hw_id = (the_hw_id << 8) + hwid_buffer[11];
	the_hw_id = (the_hw_id << 8) + hwid_buffer[10];
	*hwid = the_hw_id;
	return true;
}

bool wacom_hwid_from_firmware(int fd, unsigned long *hwid)
{
	bool bRet = false;
	u8 data[MAINTAIN_REPORT_SIZE];
	u8 hwid_buffer[MAINTAIN_REPORT_SIZE];

#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "-----wacom_hwid_from_firmware\n");
#endif
	
	// Clear data and result
	memset(data, 0, MAINTAIN_REPORT_SIZE);
	memset(hwid_buffer, 0, MAINTAIN_REPORT_SIZE);

	data[0] = MAINTAIN_REPORT_ID;
	data[1] = 0x01;
	data[2] = 0x01;
	data[3] = 0x0F;

	bRet = wacom_i2c_set_feature(fd, MAINTAIN_REPORT_ID, MAINTAIN_REPORT_SIZE, data,
				     COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "%s set feature failed\n", __func__);
		return false;
	}
	bRet = wacom_i2c_get_feature(fd, MAINTAIN_REPORT_ID, MAINTAIN_REPORT_SIZE, hwid_buffer,
					 COMM_REG, DATA_REG, AES_I2C_ADDR);
	if (!bRet) {
		fprintf(stderr, "%s get feature failed\n", __func__);
		return false;
	}
	return wacom_is_hwid_ok(&(hwid_buffer[4]), hwid);
}

#define OUT_INFO(...)	printf(__VA_ARGS__)
//#define OUT_INFO(...)	fprintf(stderr,__VA_ARGS__)
bool wacom_read_hwid(int fd, unsigned long *hwid)
{
	bool bRet = false;
	int i=0;
#ifdef SHOW_HWID_BLOCK
	int count = (UBL_HWID_END_ADDR-UBL_HWID_BASE_ADDR)/UBL_G11T_CMD_DATA_SIZE; // 5 times
#endif
	u32 address;
	boot_cmd command;
	boot_rsp response;
	u8 hwid_buffer[UBL_HWID_BLOCK_SIZE];

	// Clear result
	memset(hwid_buffer, 0, UBL_HWID_BLOCK_SIZE);

#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "-----Read Block begin\n");
#endif
#ifdef SHOW_HWID_BLOCK
	for (i=0; i < count; i++)	{
#endif
		address = UBL_HWID_BASE_ADDR + i*UBL_G11T_CMD_DATA_SIZE;
		command.verify_flash.reportId = UBL_CMD_REPORT_ID;
		command.verify_flash.cmd = UBL_COM_VERIFY; // verify
		command.verify_flash.echo = 1;
		command.verify_flash.address = address;
		command.verify_flash.size8 = UBL_G11T_CMD_DATA_SIZE / 8;
		bRet = wacom_i2c_set_feature(fd, UBL_CMD_REPORT_ID, UBL_CMD_SIZE_G11T, command.data,
				COMM_REG, DATA_REG);

		usleep(5 * MILLI);
		response.verify_flash.reportId = UBL_RSP_REPORT_ID;
		bRet = wacom_i2c_get_feature(fd, UBL_RSP_REPORT_ID, UBL_RSP_SIZE_G11T, response.data,
						 COMM_REG, DATA_REG, AES_I2C_ADDR);
		//fprintf(stderr, "GET block:%02d ---bRet = %d\n", i, bRet);
		if (!bRet) {
			fprintf(stderr, "read_HWID Set Feature error\n");
			return false;
		}
		// new bootloader will response the correct address and the size8 is 0x10, 
		// But old boot loader only return all zero (0x00) in addr and size8
		if (response.verify_flash.size8 != 0x10) {// UBL_G11T_CMD_DATA_SIZE/8
			if( i==0 )
				fprintf(stderr, "Old Bootloader not support HWID\n");
			return false;
		}
		// copy 128 bytes read from flash
		memcpy((void *)(hwid_buffer + i*UBL_G11T_CMD_DATA_SIZE),
				response.verify_flash.data, UBL_G11T_CMD_DATA_SIZE); 
#ifdef SHOW_HWID_BLOCK
	}
#endif
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "-----Read Block End\n");
#endif
	// If hwid not OK, return false
	if( !wacom_is_hwid_ok(hwid_buffer, hwid) )
		return false;

	// Dump block data
#ifdef WACOM_DEBUG_LV1
#ifdef SHOW_HWID_BLOCK
	for (i = 0; i < UBL_HWID_BLOCK_SIZE; i++) {	// only 544 bytes used now
		if (i % 32 == 0)
			OUT_INFO("\n #%03x-%03x#", i, i + 31);
		if (i % 8 == 0)
			OUT_INFO(" ");
		OUT_INFO("%02x ", hwid_buffer[i]);
	}
	OUT_INFO("\n");
#endif
#ifdef SHOW_HWID
#define STR_NAME_MAX_SIZE	260
	{
		int offset;
		unsigned int hw_vid, hw_pid;
		unsigned long hw_version;
		char str_sku_name[STR_NAME_MAX_SIZE] = { 0 };
		char str_description[STR_NAME_MAX_SIZE] = { 0 };

		// Collect the SKU_name and Description strings
		offset = 16;
		for (i = 0; i < STR_NAME_MAX_SIZE; i++) {
			if (hwid_buffer[offset + i] == 0)
				break;
			str_sku_name[i] = hwid_buffer[offset + i];
		}
		offset = 16 + 256;
		for (i = 0; i < STR_NAME_MAX_SIZE; i++) {
			if (hwid_buffer[offset + i] == 0)
				break;
			str_description[i] = hwid_buffer[offset + i];
		}
		// Get the HWID & Version
		hw_vid = (hwid_buffer[9] << 8);
		hw_vid += hwid_buffer[8];
		hw_pid = (hwid_buffer[11] << 8);
		hw_pid += hwid_buffer[10];
		hw_version = hwid_buffer[15];
		hw_version = (hw_version << 8)+ hwid_buffer[14];
		hw_version = (hw_version << 8) + hwid_buffer[13];
		hw_version = (hw_version << 8) + hwid_buffer[12];
		//
		OUT_INFO("\n---Magic number = ");
		for (i = 0; i<8; i++) 
			OUT_INFO("%02x ", hwid_buffer[i]);
		OUT_INFO("\n");
		OUT_INFO("---HW VID = 0x%04x\n", hw_vid);
		OUT_INFO("---HW PID = 0x%04x\n", hw_pid);
		OUT_INFO("---HW HWID = 0x%08x\n", (*hwid));
		OUT_INFO("---HW Version = 0x%08lx\n", hw_version);
		OUT_INFO("---HW SKU Name = %s\n", str_sku_name);
		OUT_INFO("---HW Description = %s\n", str_description);
		OUT_INFO("\n");
	}
#endif
#endif
	return true;
}

int wacom_get_hwid(int fd, unsigned int pid, unsigned long *hwid)
{
	int i_retry, ret = -1;
	bool bRet = false;

	i_retry = 0;
	if(pid != UBL_G11T_UBL_PID) {
		// Mar/07/2018, v1.2.6, Martin, Try read from normal firmware first
		if( wacom_hwid_from_firmware(fd, hwid) )
			return UBL_OK;

		if (wacom_enter_ubl(fd) == false ){
			fprintf(stderr, "Entering user boot loader error. \n");
			goto err;
		}

		//Check if the device successfully entered in UBL mode
		for ( i_retry = 0; i_retry<UBL_RETRY_NUM; i_retry++ ){
			bRet = wacom_check_mode(fd);
			if (bRet) {
				ret = UBL_OK; //Resetting the return status
				break;
			}
			usleep(200 * MILLI);
		}
	}
	if ( i_retry == UBL_RETRY_NUM ){
		ret = UBL_ERROR;
		fprintf(stderr, "Entering user boot loader error(2). \n");
		goto err;
	}

	ret = UBL_OK;
	// Try read the HWID block data and check 
	bRet = wacom_read_hwid(fd, hwid);
	if (!bRet) {
		fprintf(stderr, "wacom_read_hwid returned false \n");
		ret = UBL_ERROR;
	}

err:
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "\nClosing device... \n");
#endif
	bRet = wacom_exit_ubl(fd);
	if (!bRet) {
		fprintf(stderr, "Exit boot mode failed\n");
		ret = -EXIT_FAIL;
	}

	return ret;
}
