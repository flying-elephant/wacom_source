#include "wacom_flash.h"

bool wacom_send_cmd(int fd, boot_cmd *command, boot_rsp *response)
{
	bool bRet = false;
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
				fprintf(stderr, "RESPONSE not match at command:%d \n", command->header.cmd);
				return false;
			}

			if ( response->header.resp != UBL_RES_BUSY ){
				// other than returned value is UBL_RES_BUSY, quit
				break;
			}
			usleep(1 * MILLI);
		}	// wait unti device processes the command

		if ( i == UBL_TIMEOUT_WRITE ){
			fprintf(stderr, "TIMEOUT at command:%d \n", command->header.cmd);
			return false;
		}

		// Command that generally returns "OK" response is treated as an error
		if ( ((command->header.cmd == UBL_COM_WRITE ) || (command->header.cmd == UBL_COM_ALLERASE))
			&& (response->header.resp != UBL_RES_OK) ){
			fprintf(stderr,"Error response:%d at command:%d \n", response->header.resp, command->header.cmd);
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
	usleep(300 * MILLI);	// Mar/07/2018, v1.2.5, Martin, Try reduce sleep time to minimum needed 300 ms

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
	usleep(300 * MILLI);	// Mar/07/2018, v1.2.5, Martin, Try reduce sleep time to minimum needed 300 ms

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
		fprintf(stderr, "%s Not in user boot loader mode. Mode:%d \n", __func__, response.header.resp);
		return false;
	}

	return true;
}

#ifdef GETDATA
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
#endif

bool wacom_check_data(UBL_PROCESS *pUBLProcess)
{
	// Checking start address
	if ( pUBLProcess->start_adrs != UBL_MAIN_ADDRESS ){
		fprintf(stderr, "%s failed \n", __func__);
		fprintf(stderr, "Data error. Start at 0x%05x.\n", (unsigned int)pUBLProcess->start_adrs);
		return false;
	}

	// checking the total firmware size
	if ( (pUBLProcess->start_adrs + pUBLProcess->size) > UBL_MAIN_SIZE ){
		fprintf(stderr, "%s failed \n", __func__);
		fprintf(stderr, "Data size error. Size is 0x%05x. \n", (unsigned int)pUBLProcess->size);
		return false;
	}

	return true;
}

bool wacom_erase_all(int fd, UBL_PROCESS *pUBLProcess)
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

bool wacom_send_data(int fd, unsigned char com, unsigned char *data, unsigned long start_adrs, unsigned long size, UBL_STATUS *pUBLStatus)
{
	unsigned int i, j;
	bool bRet = false;
	boot_cmd command;
	boot_rsp response;
	unsigned char command_id = 0;

	for (i = 0; i < UBL_MAIN_SIZE / UBL_G11T_CMD_DATA_SIZE; i++)	{
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

		// Copy data bytes to report buffer
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
						fprintf(stderr, "File error. PID not match. \n");
						break;

					case UBL_RES_VERSION_ERROR:
						fprintf(stderr, "File error. VERSION is older. \n");
						break;

					default:
						fprintf(stderr, "Error response@SendData:%d at block:%d \n", response.header.resp, i);
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
			fprintf(stderr, "TIMEOUT at block:%d \n", i);
			return false;
		}
	}

	return true;
}

bool wacom_write(int fd, UBL_PROCESS *pUBLProcess, UBL_STATUS *pUBLStatus)
{
	if ( wacom_check_data(pUBLProcess) == false ){
		fprintf(stderr, "%s check data failed \n", __func__);
		return false;
	}

	// Erasing 
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "Erasing flash. \n");
#endif
	if ( wacom_erase_all(fd, pUBLProcess) == false ){
		fprintf(stderr, "%s erase failed \n", __func__);
		return false;
	}

	// Writing firmware
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "Write start. \n");
	fprintf(stderr, "WRITE address:0x%x size:0x%x \n", (unsigned int)pUBLProcess->start_adrs, (unsigned int)pUBLProcess->size);
#endif
	// Sep/26/2019, v1.3.0, Martin Chen
	// Write the first sector after other sectors wrote OK, reduce posibility of broken firmware issue by interrupt write flash process
	if ( wacom_send_data(fd, UBL_COM_WRITE, pUBLProcess->data+UBL_SECTOR_SIZE, UBL_FIRST_SECTOR_ADDRES, pUBLProcess->size-UBL_SECTOR_SIZE, pUBLStatus ) == false ){
		fprintf(stderr, "%s write sectors failed \n", __func__);
		return false;
	}
	if ( wacom_send_data(fd, UBL_COM_WRITE, pUBLProcess->data, UBL_MAIN_ADDRESS, UBL_SECTOR_SIZE, pUBLStatus ) == false ){
		fprintf(stderr, "%s write sectors failed \n", __func__);
		return false;
	}

	return true;
}

//! G11T programming thread
int wacom_flash_aes(int fd, char *data, UBL_STATUS *pUBLStatus, UBL_PROCESS *pUBLProcess)
{
	int i = 0;
	int ret = -1;
	bool bRet = false;

	for ( i = 0; i < pUBLProcess->size; i++ ){
		pUBLProcess->data[i] = data[i];
	}

	if( pUBLStatus->pid != UBL_G11T_UBL_PID ) {
		if ( wacom_enter_ubl(fd) == false ){
			fprintf(stderr, "entering boot loader error. \n");
			// 2018/Mar/07, v1.2.5, Martin, If enter boot loader fail, just exit
			goto out_write_err;
		}

		/*Check if the device successfully entered in UBL mode*/
		for ( i = 0; i < UBL_RETRY_NUM; i++ ){
			bRet = wacom_check_mode(fd);
			if (bRet) {
				pUBLStatus->ret = UBL_OK;	// Reset return status
				break;
			}
			usleep(200 * MILLI);
		}
		if ( i == UBL_RETRY_NUM ){
			pUBLStatus->ret = UBL_ERROR;
			fprintf(stderr, "entering boot loader error(2). \n");
			// 2018/Mar/12, v1.2.6, Martin, If enter boot loader fail, just exit
			goto out_write_err;
		}
	}

#ifdef GETDATA
	// Obtaining the device's basic data; repeat the cout if failed
	for ( i = 0; i < UBL_RETRY_NUM; i++ ){
		bRet = wacom_get_data(fd, pUBLStatus);
		if (bRet) {
			pUBLStatus->ret = UBL_OK;	// Reset return status
			break;
		}
		usleep(10 * MILLI);
	}
	if ( i == UBL_RETRY_NUM ){
		pUBLStatus->ret = UBL_ERROR;
		fprintf(stderr, "Cannot correctly obtain data 2\n");
		goto err;
	}
#endif

#ifdef CONDUCT_FLASH
	/*Conducting erasing and writing operation, the main part of the flash*/
	bRet = wacom_write(fd, pUBLProcess, pUBLStatus);
	if (!bRet) {
		fprintf(stderr, "UBL_G11T_Write returned false \n");
		ret = -EXIT_FAIL;
		//2018/Feb/26, v1.2.4, Martin, If writing is failed(including erase), do not call ExitG11TUBL
		goto out_write_err;
	}
#endif
	ret = UBL_OK;

 err:
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "\nClosing device... \n");
#endif
	bRet = wacom_exit_ubl(fd);
	if (!bRet) {
		fprintf(stderr, "exiting boot mode failed\n");
		ret = -EXIT_FAIL_EXIT_FLASH_MODE;
	}
out_write_err:
	return ret;
}

// ========================================================
#ifdef HWID_SUPPORT
bool wacom_read_hwid(int fd, unsigned long *hwid)
{
	bool bRet = false;
	u32 address;
	boot_cmd command;
	boot_rsp response;
	u8 count8Bytes = BL_DATA_BYTES_CHECK/8;
	unsigned int hwpid = 0;

	// Clear response buffer
	memset(response.data, 0, UBL_RSP_SIZE_G11T);

#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "-----Read HWID begin\n");
#endif
	address = UBL_BL_CHECK_ADDR; // start address of data we want to retrieve
	command.verify_flash.reportId = UBL_CMD_REPORT_ID;
	command.verify_flash.cmd = UBL_COM_VERIFY; // verify
	command.verify_flash.echo = 1;
	command.verify_flash.address = address;
	command.verify_flash.size8 = count8Bytes; // how many 8 bytes
	bRet = wacom_i2c_set_feature(fd, UBL_CMD_REPORT_ID, UBL_CMD_SIZE_G11T, command.data,
			COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "read_HWID Set Feature error\n");
		return false;
	}

	usleep(5 * MILLI);
	response.verify_flash.reportId = UBL_RSP_REPORT_ID;
	bRet = wacom_i2c_get_feature(fd, UBL_RSP_REPORT_ID, UBL_RSP_SIZE_G11T, response.data,
						COMM_REG, DATA_REG, AES_I2C_ADDR);
	if (!bRet) {
		fprintf(stderr, "read_HWID Get Feature error\n");
		return false;
	}
	// new bootloader will response the correct address and the size8 is 0x1 (same as what we set in CMD report) 
	// But old boot loader only return all zero (0x00) in addr and size8
	// Invalid case one: no support
	if (response.verify_flash.size8 != count8Bytes) { // What we set in "command.verify_flash.size8"
		fprintf(stderr, "Old Bootloader not support HWID\n");
		return false;
	}
	// Get the SKU pid data
	hwpid = (response.verify_flash.data[7] << 8) + (response.verify_flash.data[6]);
	// Invalid case two: empty (0xFFFF) or invalid HWID (0x0000)
	if( (hwpid == 0xFFFF) || (hwpid == 0x0000) ) {
		fprintf(stderr, "HWID field empty or invalid, value = 0x%04x\n", hwpid);
		return false;
	}
	*hwid = (WACOM_VENDOR2 <<16) + hwpid;
#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "-----Read HWID End\n");
#endif

	return true;
}

int wacom_get_hwid(int fd, unsigned int pid, unsigned long *hwid)
{
	int i_retry = 0, ret = -1;
	bool bRet = false;

	#ifdef WACOM_DEBUG_LV1
	fprintf(stderr,  "Check HWID start......\n");
	#endif
	if(pid != UBL_G11T_UBL_PID) {
		if (wacom_enter_ubl(fd) == false ){
			fprintf(stderr, "%s Entering boot loader error. \n", __func__);
			// 2018/Mar/12, v1.2.6, Martin, If enter boot loader fail, just exit
			goto out_get_hwid;
		}

		//Check if the device successfully entered in UBL mode
		for ( i_retry = 0; i_retry<UBL_RETRY_NUM; i_retry++ ){
			bRet = wacom_check_mode(fd);
			if (bRet) {
				ret = UBL_OK; // Reset return status
				break;
			}
			usleep(200 * MILLI);
		}
		if ( i_retry == UBL_RETRY_NUM ){
			ret = UBL_ERROR;
			fprintf(stderr, "%s Entering boot loader error(2). \n", __func__);
			goto out_get_hwid;
		}
	}

	ret = UBL_OK;
	// Try read the HWID block data and check 
	bRet = wacom_read_hwid(fd, hwid);
	if (!bRet) {
		#ifdef WACOM_DEBUG_LV1
		fprintf(stderr, "wacom_read_hwid returned false \n");
		#endif
		ret = UBL_ERROR;
	}

#ifdef WACOM_DEBUG_LV1
	fprintf(stderr, "\nClosing device... \n");
#endif
	// 2018/Mar/12, v1.2.6, Martin, Must check if chip already in boot loader mode before program start
	// It is possible caused by last firmware write was failed
	// We should not call exit_ubl() in this condition, otherwise, firmware will crashed
	// We can know this by check if it is boot loader PID
	if ( pid != UBL_G11T_UBL_PID ) { // Not in boot loader when program start
		bRet = wacom_exit_ubl(fd);
		if (!bRet) {
			fprintf(stderr, "Exit boot mode failed\n");
			ret = -EXIT_FAIL_EXIT_FLASH_MODE;
		}
	}
out_get_hwid:
	#ifdef WACOM_DEBUG_LV1
	fprintf(stderr,  "Check HWID end......\n");
	#endif
	return ret;
}
#endif //HWID_SUPPORT
// ========================================================
