#include "tools.h"

#include "led.h"

static const char *TAG = "tools";

void filesys_init()
{
	// Initialize NVS.
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		// 1.OTA app partition table has a smaller NVS partition size than the non-OTA
		// partition table. This size mismatch may cause NVS initialization to fail.
		// 2.NVS partition contains data in new format and cannot be recognized by this version of code.
		// If this happens, we erase NVS partition and initialize NVS again.
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	esp_vfs_spiffs_conf_t conf = {.base_path = "/spiffs",
								  .partition_label = "spiffs",
								  .max_files = 5,
								  .format_if_mount_failed = true};

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK)
	{
		if (ret == ESP_FAIL)
		{
			ESP_LOGI(TAG, "Failed to mount or format filesystem");
		}
		else if (ret == ESP_ERR_NOT_FOUND)
		{
			ESP_LOGI(TAG, "Failed to find SPIFFS partition");
		}
		else
		{
			ESP_LOGI(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return;
	}

	ESP_LOGI(TAG, "Performing SPIFFS_check().");
	ret = esp_spiffs_check(conf.partition_label);
	if (ret != ESP_OK)
	{
		ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
		return;
	}
	else
	{
		ESP_LOGI(TAG, "SPIFFS_check() successful");
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK)
	{
		ESP_LOGI(TAG,
				 "Failed to get SPIFFS partition information (%s). Formatting...",
				 esp_err_to_name(ret));
		esp_spiffs_format(conf.partition_label);
		return;
	}
	else
	{
		ESP_LOGI(TAG, "Partition size: total: %zu, used: %zu", total, used);
	}

	// Check consistency of reported partition size info.
	if (used > total)
	{
		ESP_LOGW(TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
		ret = esp_spiffs_check(conf.partition_label);
		// Could be also used to mend broken files, to clean unreferenced pages, etc.
		// More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
		if (ret != ESP_OK)
		{
			ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
			return;
		}
		else
		{
			ESP_LOGI(TAG, "SPIFFS_check() successful");
		}
	}
}

// 创建ota升级tag
void create_ota_tag()
{
	// Check if destination file exists
	struct stat st;
	if (stat("/spiffs/ota.txt", &st) == 0)
		return;

	// Use POSIX and C standard library functions to work with files.
	// First create a file.
	ESP_LOGI(TAG, "crate ota tag file");
	FILE *f = fopen("/spiffs/ota.txt", "w");
	if (f == NULL)
	{
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}
	fprintf(f, "1");
	fclose(f);
	ESP_LOGI(TAG, "File written");
	esp_restart();
}

// 移除ota升级tag
void remove_ota_tag()
{
	// Check if destination file exists
	struct stat st;
	if (stat("/spiffs/ota.txt", &st) == 0)
	{
		// Delete it if it exists
		unlink("/spiffs/ota.txt");
	}
}

// 检查ota升级tag
int is_exist_ota_tag()
{
	// Check if destination file exists
	struct stat st;
	if (stat("/spiffs/ota.txt", &st) == 0)
	{
		return 1;
	}
	return 0;
}

char *get_len_str(char *original, int len)
{
	char *dest = (char *)calloc(sizeof(char), (++len));
	snprintf(dest, len, "%s", original);
	dest[len] = '\0';
	// strncpy(dest, original, len);
	return dest;
}
/*
 *获取mac芯片id*/
char *get_chip_id()
{
	const uint8_t mac_str_len = 7;
	char *mac_str = (char *)calloc(sizeof(char), mac_str_len);
	if (mac_str == NULL)
		return NULL;
	uint8_t mac[6] = {0};
	// Get MAC address for WiFi Station interface
	ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
	snprintf(mac_str, mac_str_len, "%02X%02X%02X", mac[0], mac[2], mac[4]);
	mac_str[mac_str_len] = 0x00;
	return mac_str;
}

/*
 *打印系统信息*/
void print_sys_info()
{
	printf("\n\n------ Get Systrm Info------\n");
	// 获取IDF版本
	printf("SDK version:%s\n", esp_get_idf_version());
	// 获取芯片可用内存
	printf("esp_get_free_heap_size : %ld  \n", (unsigned long int) esp_get_free_heap_size());
	// 获取从未使用过的最小内存
	printf("esp_get_minimum_free_heap_size : %ld  \n", (unsigned long int) esp_get_minimum_free_heap_size());
	uint8_t mac[6];
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	printf("esp_read_mac(): %02x:%02x:%02x:%02x:%02x:%02x \n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	printf("------------------------------\n\n");
}

/*
 *计算16进制数据的2byte crc code*/
unsigned int crc_cal(const char *pBuff, int len)
{
	unsigned int mid = 0;
	unsigned char times = 0, Data_index = 0;
	unsigned int cradta = 0xFFFF;
	while (len)
	{
		cradta = pBuff[Data_index] ^ cradta; // 把数据帧中的第一个字节的8位与CRC寄存器中的低字节进行异或运算，结果存回CRC寄存器
		for (times = 0; times < 8; times++)
		{
			mid = cradta;
			cradta = cradta >> 1;
			if (mid & 0x0001)
			{
				cradta = cradta ^ 0xA001;
			}
		}
		Data_index++;
		len--;
	}
	return cradta;
}

// Function to return single precision float from from 8 hex chars, 4 bytes in 4 value integer array
float float_from_8hex(int arr[])
{
	uint16_t digit;
	uint8_t ptr; // Start output at lhs of string
	uint8_t sign;
	float answer;
	float mult;

	char result[33] = "00000000000000000000000000000000"; // 32-bit ASCII string (32 chars + 1 null terminator)
	result[32] = '\0';									  // Ensure null termination

	// Convert 4 integers (each 8-bit) to binary representation
	ptr = 0;
	for (int j = 0; j < 4; j++)
	{ // Loop over input array
		for (int h = 0; h < 2; h++)
		{
			digit = (h == 0) ? (arr[j] / 16) : (arr[j] % 16);

			// Convert each hex digit to binary
			result[ptr++] = (digit & 8) ? '1' : '0';
			result[ptr++] = (digit & 4) ? '1' : '0';
			result[ptr++] = (digit & 2) ? '1' : '0';
			result[ptr++] = (digit & 1) ? '1' : '0';
		}
	}

	// ********** Set sign flag **********
	sign = (result[0] == '1') ? 1 : 0;

	// ********* Extract exponent (8 bits) **********
	int expow = 0;
	for (int i = 1; i <= 8; i++)
	{ // Calculate exponent value
		if (result[i] == '1')
		{
			expow += (1 << (8 - i));
		}
	}
	expow -= 127; // Subtract bias (127)

	// ******** Evaluate multiplication factor (2^exponent) **********
	mult = pow(2, expow);
	if (sign)
	{
		mult = -mult;
	}

	// ********* Extract fraction (23 bits) **********
	answer = 1.0; // Implicit leading 1
	float fraction = 0.5;
	for (int i = 9; i < 32; i++)
	{ // From bit 9 to 31
		if (result[i] == '1')
		{
			answer += fraction;
		}
		fraction *= 0.5;
	}

	// ********* Compute final result **********
	return mult * answer;
}

/*
 *解析电表电量数据*/
void print_ddsu666_params(uint8_t bytes[], volatile float *voltage, volatile float *current,
						  volatile float *a_power, volatile float *r_power, volatile float *ap_power,
						  volatile float *power_factor, volatile float *power_frequency)
{
	uint8_t ptr;
	int ints_4[4];
	// int data_len = sizeof(bytes);

	// A 相电压
	ptr = 3; // Point to voltage data
	for (int z = 0; z < 4; z++)
	{
		ints_4[z] = bytes[ptr + z]; // Copy 4 bytes from vector array to int array
	}
	float v = float_from_8hex(ints_4); // Decode the 4 byte single precision float
	printf("Volts = %f\r\n", v);	   // Log converted float value (optional).
	*voltage = v;

	// A 相电流
	ptr = 7; // Point to current data
	for (int z = 0; z < 4; z++)
	{
		ints_4[z] = bytes[ptr + z]; // Copy 4 bytes from vector array to int array
	}
	float c = float_from_8hex(ints_4); // Decode the 4 byte single precision float
	printf("Current = %f\r\n", c);	   // Log converted float value (optional).
	*current = c;

	// 总有功功率
	ptr = 11; // Point to power data
	for (int z = 0; z < 4; z++)
	{
		ints_4[z] = bytes[ptr + z]; // Copy 4 bytes from vector array to int array
	}
	float ap = float_from_8hex(ints_4);	 // Decode the 4 byte single precision float
	printf("active Power = %f\r\n", ap); // Log converted float value (optional).
	*a_power = ap;

	// 总无功功率
	ptr = 15; // Point to power data
	for (int z = 0; z < 4; z++)
	{
		ints_4[z] = bytes[ptr + z]; // Copy 4 bytes from vector array to int array
	}
	float rp = float_from_8hex(ints_4);	   // Decode the 4 byte single precision float
	printf("reactive power = %f\r\n", rp); // Log converted float value (optional).
	*r_power = rp;

	// 总视在功率
	ptr = 19; // Point to power data
	for (int z = 0; z < 4; z++)
	{
		ints_4[z] = bytes[ptr + z]; // Copy 4 bytes from vector array to int array
	}
	float ap_p = float_from_8hex(ints_4);	 // Decode the 4 byte single precision float
	printf("apparent power = %f\r\n", ap_p); // Log converted float value (optional).
	*ap_power = ap_p;

	// 功率因素
	ptr = 23; // Point to power data
	for (int z = 0; z < 4; z++)
	{
		ints_4[z] = bytes[ptr + z]; // Copy 4 bytes from vector array to int array
	}
	float p_f = float_from_8hex(ints_4);  // Decode the 4 byte single precision float
	printf("power factor = %f\r\n", p_f); // Log converted float value (optional).
	*power_factor = p_f;

	// 电网频率
	ptr = 31;
	for (int z = 0; z < 4; z++)
	{
		ints_4[z] = bytes[ptr + z]; // Copy 4 bytes from vector array to int array
	}
	float p_fr = float_from_8hex(ints_4);	  // Decode the 4 byte single precision float
	printf("power frequency = %f\r\n", p_fr); // Log converted float value (optional).
	*power_frequency = p_fr;
}

/*
 *解析电表累计电量数据*/
void print_ddsu666_total_energy(uint8_t bytes[], volatile float *total_energy)
{
	uint8_t ptr;
	int ints_4[4];
	ptr = 3; // Point to total incoming energy data
	for (int z = 0; z < 4; z++)
	{
		ints_4[z] = bytes[ptr + z]; // Copy 4 bytes from vector array to int array
	}
	float energy = float_from_8hex(ints_4);
	printf("Total Energy = %f\r\n", energy); // Log converted float value (optional).
	*total_energy = energy;
}

/*
 *切换debug 模式*/
void debug_switch()
{
	extern int debug;
	debug = !debug;
	printf("debug mode is %d\r\n", debug);
}

void led_loop(int times)
{
	for (int i = 0; i < times; i++)
	{
		led_blink();
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}
}
