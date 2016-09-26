#include "wacom_flash.h"

void show_result(int size, int start_addr, long max_addr, char *data)
{

#ifdef WACOM_DEBUG_LV3
	int i = 0;
	for (i = 0; i < size; i++) {
		if ((i % 16) == 0)
			fprintf(stderr, "\n");
		fprintf(stderr, "0x%x:%x,", i, data[i]);
	}
#endif
	fprintf(stderr, "\n%s size(int): %d \n",  __func__, (int)size);
	fprintf(stderr, "%s size(hex): 0x%x \n",  __func__, (unsigned int)size);
	fprintf(stderr, "%s max address: %x \n",  __func__, (unsigned int)max_addr);

	return;
}

//int read_hex(FILE *fp, unsigned char *flash_data, size_t data_size, unsigned long *max_address)
int read_hex(FILE *fp, char *flash_data, size_t data_size, unsigned long *max_address,
	     UBL_PROCESS *pUBLProcess, UBL_STATUS *pUBLStatus, int tech)
{
	int i, s;
	int fd = -1;
	int ret = -1;
	unsigned int checksum = 0;
	unsigned long expand_address = 0;
	unsigned long startLinearAddress = 0;
	unsigned long count = 0;
	unsigned long file_size = 0;  
	unsigned long start_address = (unsigned long)-1;
	struct stat stat;
	
	if (tech == TECH_AES)
		pUBLProcess->data_en = false;

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
		int ret = -1;
		int cr = 0, lf = 0;

		s = fgetc(fp);
		count++;

		if (s == ';') {
			while(1) {
				while(s != '\n')
					s = fgetc(fp);

				s = fgetc(fp);
				if (s == ':')
					break;
			}
		} else if (s != ':') {
			if (s == ASCII_EOF)
				return count;
			else if (s != 0)
				continue;
			else
				break;
		}

		ret = fscanf(fp, "%2lX", &byte_count);
		if (ret == EOF) {
			fprintf(stderr, "fscanf is error or EOF\n");
			return HEX_READ_ERR;
		}
		count += 2;

		ret = fscanf(fp, "%4lX", &address);
		if (ret == EOF) {
			fprintf(stderr, "fscanf is error or EOF\n");
			return HEX_READ_ERR;
		}
		count += 4;

		ret = fscanf(fp, "%2lX", &record_type);
		if (ret == EOF) {
			fprintf(stderr, "fscanf is error or EOF\n");
			return HEX_READ_ERR;
		}
		count += 2;

		switch (record_type) {
		case 0:
			total = byte_count;
			total += (unsigned char)(address);
			total += (unsigned char)(address >> 8);
			total += record_type;
			address += expand_address;

			if (tech == TECH_AES) {
				if (start_address == (unsigned long)(-1))
					start_address = address;
				address -= start_address;
			}

			if (address > *max_address) {
				*max_address = address;
				*max_address += (byte_count - 1);
			}
	  
			for (i = 0; i < byte_count; i++){
				ret = fscanf(fp, "%2lX", &data);
				if (ret == EOF) {
					fprintf(stderr, "fscanf is error or EOF\n");
					return HEX_READ_ERR;
				}
				count += 2;
				total += data;

				if (address + i < data_size){
					flash_data[address + i] = (unsigned char)data;
				}
			}
	  
			ret = fscanf(fp, "%2lX", &sum);
			if (ret == EOF) {
				fprintf(stderr, "fscanf is error or EOF\n");
				return HEX_READ_ERR;
			}
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

			ret = fscanf(fp, "%2lX", &sum);
			if (ret == EOF) {
				fprintf(stderr, "fscanf is error or EOF\n");
				return HEX_READ_ERR;
			}
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
			ret = fscanf(fp, "%4lX", &expand_address);
			if (ret == EOF) {
				fprintf(stderr, "fscanf is error or EOF\n");
				return HEX_READ_ERR;
			}
			count += 4;

			total = byte_count;
			total += (unsigned char)(address);
			total += (unsigned char)(address >> 8);
			total += record_type;
			total += (unsigned char)(expand_address);
			total += (unsigned char)(expand_address >> 8);

			ret = fscanf(fp, "%2lX", &sum);
			if (ret == EOF) {
				fprintf(stderr, "fscanf is error or EOF\n");
				return HEX_READ_ERR;
			}
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
			  
				ret = fscanf(fp, "%4lX", &cs);
				if (ret == EOF) {
					fprintf(stderr, "fscanf is error or EOF\n");
					return HEX_READ_ERR;
				}
				count += 4;

				ret = fscanf(fp, "%4lX", &ip);
				if (ret == EOF) {
					fprintf(stderr, "fscanf is error or EOF\n");
					return HEX_READ_ERR;
				}
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

				ret = fscanf(fp, "%2lX", &sum);
				if (ret == EOF) {
					fprintf(stderr, "fscanf is error or EOF\n");
					return HEX_READ_ERR;
				}
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
			ret = fscanf(fp, "%4lX", &expand_address);
			if (ret == EOF) {
				fprintf(stderr, "fscanf is error or EOF\n");
				return HEX_READ_ERR;
			}
			count += 4;

			total = byte_count;
			total += (unsigned char)(address);
			total += (unsigned char)(address >> 8);
			total += record_type;
			total += (unsigned char)(expand_address);
			total += (unsigned char)(expand_address >> 8);

			ret = fscanf(fp, "%2lX", &sum);
			if (ret == EOF) {
				fprintf(stderr, "fscanf is error or EOF\n");
				return HEX_READ_ERR;
			}
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
			ret = fscanf(fp, "%8lX", &startLinearAddress);
			if (ret == EOF) {
				fprintf(stderr, "fscanf is error or EOF\n");
				return HEX_READ_ERR;
			}
			count += 8;

			total = byte_count;
			total += (unsigned char)(address);
			total += (unsigned char)(address >> 8);
			total += record_type;
			total += (unsigned char)(startLinearAddress);
			total += (unsigned char)(startLinearAddress >> 8);
			total += (unsigned char)(startLinearAddress >> 16);
			total += (unsigned char)(startLinearAddress >> 24);

			ret = fscanf(fp, "%2lX", &sum);
			if (ret == EOF) {
				fprintf(stderr, "fscanf is error or EOF\n");
				return HEX_READ_ERR;
			}
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

	if (tech == TECH_AES) {	
		if ( *max_address >= UBL_MAIN_SIZE ){
			fprintf(stderr, "File size error. \n");
			return -ERR;
		}

		if ( start_address != UBL_MAIN_ADDRESS ){
			fprintf(stderr, "Start address error. \n");
			fprintf(stderr, "returning to main()\n");
			return -ERR;
		}

		pUBLProcess->start_adrs = start_address;
		pUBLProcess->size = *max_address + 1;

		//Calculating checksum
		checksum = 0;
		for ( i = 0; i < pUBLProcess->size; i++ ){
			checksum = checksum + flash_data[i];
		}

		checksum = checksum & 0xFFFF;
		pUBLProcess->checksum = checksum;
#ifdef WACOM_DEBUG_LV1
		fprintf(stderr, "Checksum: 0x%x \n", (unsigned int)checksum);
		show_result(pUBLProcess->size, start_address, *max_address, (char *)pUBLProcess->data);
#endif
		pUBLProcess->data_en = true;
	}

	return count;
}
/*********************************************************************************************************/
