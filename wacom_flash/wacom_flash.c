#include "wacom_flash.h"

#include <unistd.h>

/*hex file read for 35D, 35G, and W9002*/
int read_hex(FILE *fp, unsigned char *flash_data, size_t data_size, unsigned long *max_address)
{
  int s;
  int ret;
  int fd = -1;
  unsigned long expand_address = 0;
  unsigned long startLinearAddress = 0;
  unsigned long count = 0;
  unsigned long file_size = 0;  
  struct stat stat;

  fd = fileno(fp);
  ret = fstat(fd, &stat);
  if (ret < 0) {
	  fprintf(stderr, "Cannot obtain stat \n");
	  return HEX_READ_ERR;
  }

  file_size = stat.st_size;

  while (!feof(fp)) {
	  s = fgetc(fp);
	  if (ferror(fp)) {
		  fprintf(stderr, "HEX_READ_ERR 1 \n ");
		  return HEX_READ_ERR;
	  }
   	  

	  if ( s == ':') {
		  s = fseek(fp, -1L, SEEK_CUR);
		  if (s) {
			  fprintf(stderr, "HEX_READ_ERR 2 \n ");
			  return HEX_READ_ERR;
		  }
		  break;
	  }
  }
  
  while(count < file_size) {
	  unsigned long address = 0;
	  unsigned long sum;
	  unsigned long byte_count;
	  unsigned long record_type;
	  unsigned long data;
	  unsigned int total;
	  unsigned int i;
	  int cr = 0, lf = 0;

	  s = fgetc(fp);
	  count++;

	  if (s != ':') {
		  if (s == ASCII_EOF)
			  return count;

		  fprintf(stderr, "HEX_READ_ERR 3 \n ");
		  return HEX_READ_ERR; /* header error */
	  }      

	  fscanf(fp, "%2lX", &byte_count);
	  count += 2;

	  fscanf(fp, "%4lX", &address);
	  count += 4;

	  fscanf(fp, "%2lX", &record_type);
	  count += 2;

	  switch (record_type) {
	  case 0:
		  total = byte_count;
		  total += (unsigned char)(address);
		  total += (unsigned char)(address >> 8);
		  total += record_type;
		  address += expand_address;
		  if (address > *max_address) {
			  *max_address = address;
			  *max_address += (byte_count-1);
		  }
	  
		  for (i = 0; i < byte_count; i++){
			  fscanf(fp, "%2lX", &data);


			  count += 2;
			  total += data;
	    
			  if (address + i < data_size){
				  flash_data[address + i] = (unsigned char)data;
			  }
		  }
	  
		  fscanf(fp, "%2lX", &sum);
		  count += 2;
	  
		  total += sum;
		  if ((unsigned char)(total & 0xff) != 0x00) {
			  fprintf(stderr, "HEX_READ_ERR 4 \n ");
			  return HEX_READ_ERR; /* check sum error */
		  }	  

		  cr = fgetc(fp);
		  count++;

		  lf = fgetc(fp);
		  count++;

		  if (cr != '\r' || lf != '\n') {
			  fprintf(stderr, "HEX_READ_ERR 5 \n ");
			  return HEX_READ_ERR;
		  }
	  
		  break;
	  case 1:
		  total = byte_count;
		  total += (unsigned char)(address);
		  total += (unsigned char)(address >> 8);
		  total += record_type;

		  fscanf(fp, "%2lX", &sum);
		  count += 2;	

		  total += sum;
		  if ((unsigned char)(total & 0xff) != 0x00) {
			  fprintf(stderr, "HEX_READ_ERR 6 \n ");
			  return HEX_READ_ERR; /* check sum error */
		  }	

		  cr = fgetc(fp);
		  count++;

		  lf = fgetc(fp);
		  count++;

		  if (cr != '\r' || lf != '\n') {
			  fprintf(stderr, "HEX_READ_ERR 7 \n ");
			  return HEX_READ_ERR;
		  }
		  
		  break;
	  case 2:
		  fscanf(fp, "%4lX", &expand_address);
		  count += 4;

		  total = byte_count;
		  total += (unsigned char)(address);
		  total += (unsigned char)(address >> 8);
		  total += record_type;
		  total += (unsigned char)(expand_address);
		  total += (unsigned char)(expand_address >> 8);

		  fscanf(fp, "%2lX", &sum);
		  count += 2;	

		  total += sum;
		  if ((unsigned char)(total & 0xff) != 0x00) {
			  fprintf(stderr, "HEX_READ_ERR 8 \n ");
			  return HEX_READ_ERR; /* check sum error */
		  }			

		  cr = fgetc(fp);
		  count++;

		  lf = fgetc(fp);
		  count++;

		  if (cr != '\r' || lf != '\n') {
			  fprintf(stderr, "HEX_READ_ERR 9 \n ");
			  return HEX_READ_ERR;
		  }
	
		  expand_address <<= 4;
	
		  break;

	  case 3:
		  {
			  unsigned long cs=0, ip=0;
			  
			  fscanf(fp, "%4lX", &cs);
			  count += 4;

			  fscanf(fp, "%4lX", &ip);
			  count += 4;

			  expand_address = (cs << 4) + ip;

			  total = byte_count;
			  total += (unsigned char)(address);
			  total += (unsigned char)(address >> 8);
			  total += record_type;
			  total += (unsigned char)(cs);
			  total += (unsigned char)(cs >> 8);
			  total += (unsigned char)(ip);
			  total += (unsigned char)(ip >> 8);

			  fscanf(fp, "%2lX", &sum);
			  count += 2;
			  total += sum;
			  
			  if ((unsigned char)(total & 0x0f) != 0x00)
				  return HEX_READ_ERR;

			  cr = fgetc(fp);
			  count++;
			  
			  lf = fgetc(fp);
			  count++;
			  
			  if (cr != '\r' || lf != '\n')
				  return HEX_READ_ERR;
			  
			  expand_address <<= 16;
					
			  break;
		  }

	  case 4:
		  fscanf(fp, "%4lX", &expand_address);
		  count += 4;

		  total = byte_count;
		  total += (unsigned char)(address);
		  total += (unsigned char)(address >> 8);
		  total += record_type;
		  total += (unsigned char)(expand_address);
		  total += (unsigned char)(expand_address >> 8);

		  fscanf(fp, "%2lX", &sum);
		  count += 2;
	
		  total += sum;

		  if ((unsigned char)(total & 0xff) != 0x00) {
			  fprintf(stderr, "HEX_READ_ERR 10 \n ");
			  return HEX_READ_ERR; /* check sum error */
		  }	

		  cr = fgetc(fp);
		  count++;

		  lf = fgetc(fp);
		  count++;

		  if (cr != '\r' || lf != '\n') {
			  fprintf(stderr, "HEX_READ_ERR 11 \n ");
			  return HEX_READ_ERR;
		  }
	
		  expand_address <<= 16;
	
		  break;

	  case 5:
		  fscanf(fp, "%8lX", &startLinearAddress);
		  count += 8;

		  total = byte_count;
		  total += (unsigned char)(address);
		  total += (unsigned char)(address >> 8);
		  total += record_type;
		  total += (unsigned char)(startLinearAddress);
		  total += (unsigned char)(startLinearAddress >> 8);
		  total += (unsigned char)(startLinearAddress >> 16);
		  total += (unsigned char)(startLinearAddress >> 24);

		  fscanf(fp, "%2lX", &sum);
		  count += 2;
		  total += sum;
		  
		  if ((unsigned char)(total & 0x0f) != 0x00)
			  return HEX_READ_ERR; /* check sum error */
		  
		  cr = fgetc(fp);
		  count++;
		  
		  lf = fgetc(fp);
		  count++;
		  
		  if (cr != '\r' || lf != '\n')
			  return HEX_READ_ERR;
		  
		  break;
		  
	  default:
		  fprintf(stderr, "HEX_READ_ERR 12 \n ");
		  return HEX_READ_ERR;
	  }
  }
  
  return count;
}
/*********************************************************************************************************/

bool wacom_i2c_set_feature(int fd, u8 report_id, unsigned int buf_size, u8 *data, 
			   u16 cmdreg, u16 datareg)
{
	int i, ret = -1;
	int total = SFEATURE_SIZE + buf_size;
	u8 *sFeature = NULL;
	bool bRet = false;

	sFeature = malloc(sizeof(u8) * total);
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

bool wacom_i2c_get_feature(int fd, u8 report_id, unsigned int buf_size, u8 *data, 
			   u16 cmdreg, u16 datareg, int delay)
{
	int ret = -1;
	u8 *recv = NULL;
	bool bRet = false;
	u8 gFeature[] = {
		(u8)(cmdreg & 0x00ff),
		(u8)((cmdreg & 0xff00) >> 8),
		(RTYPE_FEATURE << 4) | report_id,
		CMD_GET_FEATURE,
		(u8)(datareg & 0x00ff),
		(u8)((datareg & 0xff00) >> 8)
	};

	/*"+ 2", adding 2 more spaces for organizeing again later in the passed data, "data"*/
	recv = malloc(sizeof(u8) * (buf_size + 2));
	if (!recv) {
		fprintf(stderr, "%s cannot preserve memory \n", __func__);
		goto out;
	}

	memset(recv, 0, sizeof(u8) * (buf_size + 2)); /*Append 2 bytes for length low and high of the byte*/

	ret = write(fd, gFeature, GFEATURE_SIZE);
	if (ret != GFEATURE_SIZE) {
		fprintf(stderr, "%s Sending Get_Feature failed; sent bytes: %d \n", __func__, ret);
		goto err;
	}

	usleep(delay);

	ret = read(fd, recv, (buf_size + 2));
	if (ret != (buf_size + 2)) {
		fprintf(stderr, "%s Receiving data failed; recieved bytes: %d \n", __func__, ret);
		goto err;
	}

	/*Coppy data pointer, subtracting the first two bytes of the length*/
	memcpy(data, (recv + 2), buf_size);

#ifdef DEBUG
	fprintf(stderr, "Recved bytes: %d \n", ret);
	fprintf(stderr, "Expected bytes: %d \n", buf_size);
	fprintf(stderr, "1: %x, 2: %x, 3:%x, 4:%x 5:%x\n", data[0], data[1], data[2], data[3], data[4]);
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

	bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, RSP_SIZE, response, COMM_REG, DATA_REG, (10 * 1000));
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
	
	fprintf(stderr, "QUERY SUCCEEDED \n");
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
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, RSP_SIZE, response, COMM_REG, DATA_REG, (10 * 1000));
	if (!bRet) {
		fprintf(stderr, "%s 2 failed to set feature\n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	if ( (response[RTRN_CMD] != BOOT_CMD) ||
	     (response[RTRN_ECH] != ECH) ) {
		fprintf(stderr, "%s res3:%x res4:%x \n", __func__, response[RTRN_CMD], response[RTRN_ECH]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	if (response[RTRN_RSP] != QUERY_RSP) {
		fprintf(stderr, "%s res5:%x \n", __func__, response[RTRN_RSP]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
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
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, RSP_SIZE, response, COMM_REG, DATA_REG, (10 * 1000));
	if (!bRet) {
		fprintf(stderr, "%s failed to get feature \n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	if ( (response[RTRN_CMD] != MPU_CMD) ||
	     (response[RTRN_ECH] != ECH) ) {
		fprintf(stderr, "%s res3:%x res4:%x \n", __func__, response[RTRN_CMD], response[RTRN_ECH]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
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
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
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
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}
	
	usleep(50);

	do {

		bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, BOOT_RSP_SIZE, response, COMM_REG, DATA_REG, 50);
		if (!bRet) {
			fprintf(stderr, "%s failed to get feature \n", __func__);
			return -EXIT_FAIL_SEND_QUERY_COMMAND;
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
			return -EXIT_FAIL_SEND_QUERY_COMMAND;
		}	

		usleep(50);

		do {	

			bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, BOOT_RSP_SIZE, response, COMM_REG, DATA_REG, 50);
			if (!bRet) {
				fprintf(stderr, "%s failed to get feature \n", __func__);
				return -EXIT_FAIL_SEND_QUERY_COMMAND;
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
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}	

	usleep(50);

	return true;
}

bool flash_write_w9013(int fd, unsigned char *flash_data,
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
			if (flash_data[ulAddress+i] != 0xFF)
				break;
		}
		if (i == (FLASH_BLOCK_SIZE))
			continue;

		bRet = flash_write_block_w9013(fd, (char*)flash_data, ulAddress, &command_id, &ECH);
		if(!bRet)
			return false;
		if (ECH_len == 3)
			ECH_len = 0;

		ECH_ARRAY[ECH_len++] = ECH;
		if (ECH_len == 3) {
			for (j = 0; j < 3; j++) {
				do {

					bRet = wacom_i2c_get_feature(fd, REPORT_ID_2, BOOT_RSP_SIZE, response, COMM_REG, DATA_REG, 50);
					if (!bRet) {
						fprintf(stderr, "%s failed to set feature \n", __func__);
						return -EXIT_FAIL_SEND_QUERY_COMMAND;
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

int wacom_i2c_flash_w9013(int fd, unsigned char *flash_data)
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
	fprintf(stderr, "BL version: %x \n", iBLVer);

	/*Obtain MPU type: this can be manually done in user space*/
	if (!flash_mputype_w9013(fd, &iMpuType)) {
		fprintf(stderr, "%s failed to get MPU type \n", __func__);
		return -EXIT_FAIL_GET_MPU_TYPE;
	}
	if (iMpuType != MPU_W9013) {
		fprintf(stderr, "MPU is not for W9013 : %x \n", iMpuType);
		return -EXIT_FAIL_GET_MPU_TYPE;
	}
	fprintf(stderr, "MPU type: %x \n", iMpuType);
	
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
	fprintf(stderr, "%s erasing the current firmware \n", __func__);
	bRet = flash_erase_w9013(fd, eraseBlock,  eraseBlockNum);
	if (!bRet) {
		fprintf(stderr, "%s failed to erase the user program \n", __func__);
		result = -EXIT_FAIL_ERASE;
		goto fail;
	}

	/*Write the new program*/
	fprintf(stderr, "%s writing new firmware \n", __func__);
	bRet = flash_write_w9013(fd, flash_data, start_address, &max_address);
	if (!bRet) {
		fprintf(stderr, "%s failed to write firmware \n", __func__);
		result = -EXIT_FAIL_WRITE_FIRMWARE;
		goto fail;
	}	
	
	/*Return to the user mode*/
	fprintf(stderr, "%s closing the boot mode \n", __func__);
	bRet = flash_end_w9013(fd);
	if (!bRet) {
		fprintf(stderr, "%s closing boot mode failed  \n", __func__);
		result = -EXIT_FAIL_EXIT_FLASH_MODE;
		goto fail;
	}
	
	fprintf(stderr, "%s write and verify completed \n", __func__);
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

int wacom_i2c_flash(int fd, unsigned char *flash_data)
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

/*********************************************************************************************************/
void show_hid_descriptor(HID_DESC hid_descriptor)
{
	fprintf(stderr,  "%d 0x%x 0x%x %d %d \n %d %d %d %d %d \n 0x%x 0x%x 0x%x %d %d\n",
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

int wacom_gather_info(int fd, int *fw_ver)
{
	struct i2c_rdwr_ioctl_data packets;
	HID_DESC hid_descriptor;		
	int ret = -1;
	u16 cmd_reg;
	u16 data_reg;
	u8 cmd[] = {HID_DESC_REGISTER, 0x00};
	struct i2c_msg msgs[] = {
		{
			.addr = I2C_TARGET,
			.flags = 0,
			.len = sizeof(cmd),
			.buf = (char *)cmd,
		},
		{
			.addr = I2C_TARGET,
			.flags = I2C_M_RD,
			.len = sizeof(HID_DESC),
			.buf = (char *)(&hid_descriptor),
		},
	};

	packets.msgs  = msgs;
	packets.nmsgs = 2;
	if (ioctl(fd, I2C_RDWR, &packets) < 0) {
		fprintf(stderr, "%s failed to send messages\n", __func__);
		ret = -EXIT_FAIL;
		goto out;
	}

	if (hid_descriptor.wVendorID != WACOM_VENDOR1 &&
	    hid_descriptor.wVendorID != WACOM_VENDOR2) {
		fprintf(stderr, "the vendor ID is not Wacom ID, : %x \n", hid_descriptor.wVendorID);
		ret = -EXIT_FAIL;
		goto out;
	}

	*fw_ver = (int)hid_descriptor.wVersion;

#ifdef WACOM_DEBUG
	fprintf(stderr,  "Wacom device found:\n Vendor ID: %x obtained fw_ver:%x \n", 
	       hid_descriptor.wVendorID, *fw_ver);
#endif

	ret = 0;

 out:
	return ret;
}

int get_device(int *current_fw_ver, char *device_num)
{
	int fd = -1;
	int i = 0, ret = -1;

	fd = open(device_num, O_RDWR);
	if (fd < 0) {
		perror(device_num);
		fprintf(stderr, "xcannot open %s \n", device_num);
		goto exit;
	}
	
	/*If I2C_SLAVE makes "Segmentation fault" or the error, use I2C_SLAVE_FORCE instead*/
	ret = ioctl(fd, I2C_SLAVE_FORCE, I2C_TARGET);
	if (ret < 0) {
		fprintf(stderr, "Falied to set the slave address: %d \n", I2C_TARGET);
		goto exit;
	}

	ret = wacom_gather_info(fd, current_fw_ver);
	if (ret < 0) {
		fprintf(stderr, "Cannot get device infomation\n");
		goto exit;
	}

	ret = fd;
 exit:
	return ret;

}

int compare_fw_version(int fd, char *fw_file_name, int new_fw_ver, int current_fw_ver)
{
	int ret = -1;

	if (new_fw_ver < 0 || current_fw_ver < 0)
		return -EXIT_FAIL;

	return (new_fw_ver == current_fw_ver ? 1 : 0);
}

/*********************************************************************************************************/
int main(int argc, char *argv[])
{
	/*For read hex*/
	char *file_name;
	char device_num[64];
	unsigned long maxAddr;
	FILE *fp;

	/*For Flash*/
	bool bRet = false;

	unsigned char flash_data[DATA_SIZE];
	bool only_ver_check = false;
	bool active_fw_check = false;
	bool new_fw_check = false;
	bool force_flash = false;
	int i, ret = 0, fd = -1;
	int count = 0, cnt = 0;
	int current_fw_ver = -1;
	int new_fw_ver = -1;
	int updated_fw_ver = -1;

	/**************************************/
	/**************************************/
	/*From here starts reading hex file****/
	/**************************************/
	/**************************************/
	if (argc != 4){
		fprintf(stderr,  "Usage: $wac_flash [target file name] [type] [i2c-device path]\n");
		fprintf(stderr,  "Ex: $wac_flash W9013_056.hex -r i2c-1 \n");
		ret = -EXIT_NOFILE;
		goto exit;
	}

	if (!strcmp(argv[2], "-v")) {
		fprintf(stderr,  "Conducting only version check \n");
		only_ver_check = true;
	} else if (!strcmp(argv[2], "-a")) {
		fprintf(stderr,  "Returning active firmware version only\n");
		active_fw_check = true;
	} else if (!strcmp(argv[2], "-n")) {
		fprintf(stderr,  "Returning new firmware version only\n");
		new_fw_check = true;
	} else if (!strcmp(argv[2], FLAGS_RECOVERY_TRUE)) {
		fprintf(stderr,  "Force flash set\n");
		force_flash = true;
	} else if (!strcmp(argv[2], FLAGS_RECOVERY_FALSE)) {
		fprintf(stderr,  "Force flash is NOT set\n");
	} else {
		fprintf(stderr, "option is not valid \n");
		ret = -EXIT_NOSUCH_OPTION;
		goto exit;
	}

	/****************************************/
	/*Hex file parsing                      */
	/****************************************/
	file_name = argv[1];
	snprintf(device_num, sizeof(device_num), "%s%s", "/dev/", argv[3]);

	/*If active_fw_check is flagged; skip below file-read*/
	if (!active_fw_check) {
		memset(flash_data, 0xff, DATA_SIZE);	
		fp = fopen(argv[1], "rb");
		if (fp == NULL) {
			fprintf(stderr, "the file name is invalid or does not exist\n");
			ret = -EXIT_FAIL;
			goto exit;
		}

		cnt = read_hex(fp, flash_data, DATA_SIZE, &maxAddr);
		if (cnt == HEX_READ_ERR) {
			fprintf(stderr, "reading the hex file failed\n");
			fclose(fp);
			
			if (new_fw_check)
				ret = -EXIT_FAIL; //avoid confusion in shell script
			else
				ret = -EXIT_NO_INTEL_HEX;
			goto exit;
		}
		fclose(fp);

		new_fw_ver = (int)(flash_data[DATA_SIZE - 1] << 8) | (int)flash_data[DATA_SIZE -2];

		/*Checking if only new firmware version check is requested*/
		if (new_fw_check) {
			printf("%d\n", new_fw_ver);
			goto exit;
		}
	}

	/****************************************/
	/*Opening and setting file descriptor   */
	/****************************************/
	fd = get_device(&current_fw_ver, device_num);
	if (fd < 0) {
		fprintf(stderr, "cannot find Wacom i2c device\n");
		goto exit;
	}

	/****************************************/
	/*Checking option other than "-n"       */
	/****************************************/
	if (force_flash) {
		fprintf(stderr,  "No firmware check; now starting to flash");
		goto firmware_flash;
	} else if (active_fw_check) {
		printf("%d\n", current_fw_ver);
		goto exit;
	} 

	/****************************************/
	/*Check firmware version before flashing*/
	/****************************************/
	fprintf(stderr, "current_fw: %x new_fw: %x \n", current_fw_ver, new_fw_ver);

	ret = compare_fw_version(fd, file_name, new_fw_ver, current_fw_ver);
	if (ret || ret < 0) {
		fprintf(stderr, "Fw version check failed. Aborting the flash\n");
		ret = -EXIT_FAIL_FWCMP;
		goto exit;
	} else if (only_ver_check) {
		ret = -EXIT_VERSION_CHECK;
		goto exit;
	}

 firmware_flash:
	/****************************************/
	/*From here prepares for flash operation*/
	/****************************************/
	fprintf(stderr,  "*Flash started...... \n");
	fprintf(stderr,  "Firmware on disc: %x \n", new_fw_ver);
	fprintf(stderr,  "Flashed firmware : %x \n", updated_fw_ver);
	ret =  wacom_i2c_flash(fd, flash_data);
	if (ret < 0) {
		fprintf(stderr, "%s failed to flash firmware\n", __func__);
		ret = -EXIT_FAIL;
		goto exit;
	}
	
	msleep(200);

	if (!only_ver_check && !active_fw_check && !new_fw_check)
		fprintf(stderr, "\n3:######################\n");
	if (wacom_gather_info(fd, &updated_fw_ver) < 0) {
		fprintf(stderr, "cannot get updated information \n");
		goto exit;
	}

	if (updated_fw_ver == new_fw_ver) {
		fprintf(stderr,  "Firmware update successfully finished \n");
	} else {
		fprintf(stderr, "firmwrae version does not match;\n fw_file_ver: %d current_fw_ver: %d\n",
			new_fw_ver, updated_fw_ver);
	}

	fprintf(stderr,  "*Flash finished...... \n");
	fprintf(stderr,  "Firmware on disc: %x \n", new_fw_ver);
	fprintf(stderr,  "Flushed firmware : %x \n", updated_fw_ver);

 exit:
	if (fd > 0)
		close(fd);

	return -ret;
}
