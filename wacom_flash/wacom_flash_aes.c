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

	usleep(500 * MILLI);

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

	usleep(1000 * MILLI);

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
	if ( pUBLProcess->start_adrs != UBL_MAIN_ADDRESS ){
		fprintf(stderr, "%s failed \n", __func__);
		fprintf(stderr, "Data error. Start at 0x%05x.\n", (unsigned int)pUBLProcess->start_adrs );

		return false;
	}

	// checking the total firmware size
	if ( (pUBLProcess->start_adrs + pUBLProcess->size) > (UBL_MAIN_SIZE + 1) ){
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
	unsigned int max_block = UBL_END_BLOCK;
	int i = 0;

	memset(&command, 0, sizeof(boot_cmd));
	memset(&response, 0, sizeof(boot_rsp));

	command.erase_flash.reportId = UBL_CMD_REPORT_ID;
	command.erase_flash.cmd = UBL_COM_BLOCKERASE;	// erase all flash

	if (pUBLProcess->erase_all)
		max_block += 1;

	/*Erase blocks from 0 up to 70 (including) when not force_flash*/
	/*otehrwise set to 71*/
	for (i = 0; i <= max_block; i++) {
		command.erase_flash.echo = i;
		command.erase_flash.blkNo = i;

		bRet = wacom_i2c_set_feature(fd, UBL_CMD_REPORT_ID, UBL_CMD_SIZE_G11T, command.data,
					     COMM_REG, DATA_REG);
		if (!bRet) {
			fprintf(stderr, "%s failed \n", __func__);
			return false;
		}
		
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

		if(response.header.resp != UBL_RES_OK 
		   || response.header.echo != command.erase_flash.echo) {
			fprintf(stderr, "%s failed \n", __func__);
			return false;
		}
	}

	return true;
}

bool wacom_send_data(int fd, unsigned char com, unsigned char *data, unsigned long start_adrs, 
		     unsigned long size, bool force_flash, UBL_STATUS *pUBLStatus)
{
	unsigned int i, j;
	unsigned int max_size = UBL_NEW_SIZE;
	bool bRet = false;
	boot_cmd command;
	boot_rsp response;
	unsigned char command_id = 0;

	if (force_flash) {
		max_size =  UBL_MAIN_SIZE;
	}

	// g_FlashBlockSize - global variable containing size of one data block in read/write report
	for (i = 0; i < (max_size + 1) / UBL_G11T_CMD_DATA_SIZE; i++)	{
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
	if ( wacom_send_data(fd, UBL_COM_WRITE, pUBLProcess->data, pUBLProcess->start_adrs, 
			     pUBLProcess->size, pUBLProcess->force_flash, pUBLStatus ) == false ){
		fprintf(stderr, "%s failed \n", __func__);
		return false;
	}

	return true;
}

bool comparing_id(int fd, u32 address, unsigned char *id)
{
	bool bRet = false;
	u8 set_data[UBL_CMD_SETHWID_REPTID_SIZE] = {0};
	u8 get_data[UBL_CMD_GETHWID_REPTID_SIZE] = {0};

	set_data[0] = UBL_CMD_SET_HWID_REPORT_ID;
	set_data[1] = UBL_HWID_COMMAND;
	set_data[2] = UBL_HWID_ECHO;
	set_data[3] = address & 0x000000ff;
	set_data[4] = (address & 0x0000ff00) >> 8;
	set_data[5] = (address & 0x00ff0000) >> 16;
	set_data[6] = (address & 0xff000000) >> 24;
      	set_data[7] = 1; /*8 bytes * 1(data[7]) times to compare*/
	memcpy((set_data + 8), id, 8);

	bRet = wacom_i2c_set_feature(fd, UBL_CMD_SET_HWID_REPORT_ID,
				     UBL_CMD_SETHWID_REPTID_SIZE, set_data, COMM_REG, DATA_REG);
	if (!bRet) {
		fprintf(stderr, "Claiming Hardware ID report failed \n");
		goto out;
	}

	bRet = wacom_i2c_get_feature(fd, UBL_CMD_GET_HWID_REPORT_ID,
				     UBL_CMD_GETHWID_REPTID_SIZE, get_data, COMM_REG, DATA_REG, AES_I2C_ADDR);
	if (!bRet) {
		fprintf(stderr, "Getting Hardware ID report failed \n");
		goto out;
	}

	if ((get_data[0] != UBL_CMD_GET_HWID_REPORT_ID) || (get_data[1] != UBL_HWID_COMMAND) ||
	    (get_data[2] != UBL_HWID_ECHO) || (get_data[3] != 0)) {
		fprintf(stderr, "Data back from get-feature is not with correct values\n");
		bRet = false;
		goto out;
	}

 out:
	return bRet;
}

bool check_hw_compatibility(int fd, char *data, UBL_STATUS *pUBLStatus, 
			    UBL_PROCESS *pUBLProcess)
{
	bool bRet = false;
	unsigned int base_addr_pid = (UBL_HWID_PID_ADDRESS - UBL_MAIN_ADDRESS);
	unsigned char id[8] = {0};
	int i = 0;

	for (i = 0; i < 8; i ++)
		id[i] = data[base_addr_pid + i];

	if (wacom_enter_ubl(fd) == false ){
		fprintf(stderr, "entering user boot loader error. \n");
		goto err;
	}

	/*Check if the device successfully entered in UBL mode*/
	for ( i = 0; i < 5/*UBL_RETRY_NUM*/; i++ ){
		usleep(200 * MILLI);

		bRet = wacom_check_mode(fd);
		if (bRet)
			break;
		else
			goto out;

		pUBLStatus->ret = UBL_OK; /*Resetting the return status*/
		usleep(200 * MILLI);
	}

	bRet = comparing_id(fd, UBL_HWID_ADDRESS, hw_magicword);
	if (!bRet) {
		fprintf(stderr, "hwid is not supported; force-write is set \n");
		bRet = pUBLProcess->force_flash = true;
		goto out;
	}

	/*If hardware-id is supported, then check VID and PID*/
	bRet = comparing_id(fd, UBL_HWID_PID_ADDRESS, id);
	if (!bRet) {
		fprintf(stderr, "This Wacom hardware cannot be used with this firmware \n");
		goto out;
	}

	printf("Correct firmware to write on\n");
 out:
	for (i = 0; i < EXIT_RETRY; i++) {
		if (wacom_exit_ubl(fd)) {
			fprintf(stderr, "exiting bootloader succeeded\n");
			break;
		} else {
			fprintf(stderr, "exiting boot mode failed\n");
			fprintf(stderr, "%d st attempt\n", (i + 1));
		}
	}

 err:
	return bRet;
}

//! G11T programming thread
int wacom_flash_aes(int fd, char *data, UBL_STATUS *pUBLStatus, UBL_PROCESS *pUBLProcess)
{

	int i;
	int ret = -1;
	bool bRet = false;

	/*If a firmware on a device doesn't have the area of hardware id, then do not return*/
	/*, but write over. On the other hand, if a firmware on a device doesn't match to that*/
	/* on disk, then never try to write over, but just quit.*/
	bRet = check_hw_compatibility(fd, data, pUBLStatus, pUBLProcess);
	if (!bRet) {
		fprintf(stderr, "%s This Wacom hardware doesn't support firmware on disk\n",
			__func__);
		goto err;
	}

	if (pUBLProcess->force_flash) {
		/*If the connected device has the old firmware,*/
		/*erase all rom area and overwrite them all at the size*/
		pUBLProcess->size = (UBL_MAX_ROM_SIZE_OLD + 1);
		pUBLProcess->erase_all = true;
	}

	for ( i = 0; i < pUBLProcess->size; i++ ){
		pUBLProcess->data[i] = data[i];
	}

#ifdef GETDATA
	i = 0;
	if(pUBLStatus->pid != UBL_G11T_UBL_PID) {
		if (wacom_enter_ubl(fd) == false ){
			fprintf(stderr, "entering user boot loader error. \n");
			goto err;
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
		goto err;
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

	/*If writing is failed(including erase), do not call ExitG11TUBL*/
	return ret;
}
