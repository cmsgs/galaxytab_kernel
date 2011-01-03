/* Fuel_guage.c */
#define MAX17042
#define TEMPERATURE_FROM_MAX17042

/* Slave address */
#define MAX17042_SLAVE_ADDR	0x6D

/* Register address */
#define STATUS_REG				0x00
#define VALRT_THRESHOLD_REG	0x01
#define TALRT_THRESHOLD_REG	0x02
#define SALRT_THRESHOLD_REG	0x03
#define REMCAP_REP_REG		0x05
#define SOCREP_REG			0x06
#define TEMPERATURE_REG		0x08
#define VCELL_REG				0x09
#define CURRENT_REG			0x0A
#define AVG_CURRENT_REG		0x0B
#define SOCMIX_REG			0x0D
#define SOCAV_REG				0x0E
#define REMCAP_MIX_REG		0x0F
#define FULLCAP_REG			0x10
#define CONFIG_REG			0x1D
#define REMCAP_AV_REG		0x1F
#define MISCCFG_REG			0x2B
#define RCOMP_REG				0x38
#define VFOCV_REG				0xFB
#define VFSOC_REG				0xFF

// SDI Battery Data
#define SDI_Capacity			0x1F40  // 4000mAh
#define SDI_VFCapacity		0x29AA  // 5333mAh

// ATL Battery Data
#define ATL_Capacity		0x1F68  // 4020mAh
#define ATL_VFCapacity		0x29E0  // 5360mAh

// For low battery compensation.
// SDI type threshold
#define SDI_Range3_1_Threshold	3360
#define SDI_Range3_3_Threshold	3450
#define SDI_Range2_1_Threshold	3408
#define SDI_Range2_3_Threshold	3463
#define SDI_Range1_1_Threshold	3456
#define SDI_Range1_3_Threshold	3530
// ATL type threshold
#define ATL_Range3_1_Threshold	3312
#define ATL_Range3_3_Threshold	3330
#define ATL_Range2_1_Threshold	3317
#define ATL_Range2_3_Threshold	3344
#define ATL_Range1_1_Threshold	3370
#define ATL_Range1_3_Threshold	3385


typedef enum {
	POSITIVE = 0,
	NEGATIVE = 1
} sign_type_t;

typedef enum {
	UNKNOWN_TYPE = 0,
	SDI_BATTERY_TYPE,
	ATL_BATTERY_TYPE
} battery_type_t;


static int pr_cnt = 0;
static u32 battery_type = UNKNOWN_TYPE;
static u16 Capacity = 0;
static u16 VFCapacity = 0;

int fuel_guage_init = 0;
EXPORT_SYMBOL(fuel_guage_init);

extern unsigned char maxim_lpm_chg_status(void);


static struct i2c_driver fg_i2c_driver;
static struct i2c_client *fg_i2c_client = NULL;


struct fg_state{
	struct i2c_client	*client;	
};
struct fg_state *fg_state;


static int fg_i2c_read(struct i2c_client *client, u8 reg, u8 *data, u8 length)
{
	u16 value;

#if 0  // not used.
	value = swab16(i2c_smbus_read_word_data(client, (u8)reg));
	
	if (value < 0)
		pr_err("%s: Failed to fg_i2c_read\n", __func__);
	
	*data = (value &0xff00) >>8;
	*(data+1) = value & 0x00ff;
#endif
	value = i2c_smbus_read_i2c_block_data(client, (u8)reg, length, data);
	if (value < 0)
		pr_err("%s: Failed to fg_i2c_read\n", __func__);
	
	return 0;
}

static int fg_i2c_write(struct i2c_client *client, u8 reg, u8 *data, u8 length)
{
	u16 value;
	value=(*(data+1)) | (*(data)<< 8) ;
		
	i2c_smbus_write_word_data(client, (u8)reg, swab16(value));
	
	return 0;
}


int fg_read_vcell(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	u32 vcell = 0;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	if (fg_i2c_read(client, VCELL_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to read VCELL\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	vcell += (temp2 << 4);

	if(!(pr_cnt % 30))
		printk("%s : VCELL(%d), data(0x%04x)\n", __func__, vcell, (data[1]<<8) | data[0]);

	return vcell;
}


int fg_read_vfocv(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	u32 vfocv = 0;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	if (fg_i2c_read(client, VFOCV_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to read VFOCV\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vfocv = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	vfocv += (temp2 << 4);

	return vfocv;
}


s32 fg_read_temp(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	s32 temper = 0;
	s32 trim1 = 15385;
	s32 trim2 = 22308;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	if (fg_i2c_read(client, TEMPERATURE_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to read SOCREP\n", __func__);
		return -1;
	}


	if(data[1]&(0x1 << 7)) //Negative
	{
		temper = ((~(data[1]))&0xFF)+1;
		temper *= (-1000);
	}
	else
	{
		temper = data[1] & 0x7f;
		temper *= 1000;
		temper += data[0] * 39 / 10;
		if(temper > 47000)
			temper = temper * trim1/10000 - trim2;
	}
		
	return temper;
}


s32 fg_read_temp2(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	s32 temper = 0;
	s32 trim1_1 = 122;
	s32 trim1_2 = 8950;
	s32 trim2_1 = 200;
	s32 trim2_2 = 51000;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	if (fg_i2c_read(client, TEMPERATURE_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to read SOCREP\n", __func__);
		return -1;
	}


	if(data[1]&(0x1 << 7)) //Negative
	{
		temper = ((~(data[1]))&0xFF)+1;
		temper *= (-1000);
	}
	else
	{
		temper = data[1] & 0x7f;
		temper *= 1000;
		temper += data[0] * 39 / 10;
		if(temper >= 47000 && temper <60000)
			temper = temper * trim1_1/100 - trim1_2;
		else if(temper >=60000)
			temper = temper * trim2_1/100 - trim2_2;
	}

	return temper;
}


s32 fg_read_temp3(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	s32 temper = 0;
	s32 trim1_1 = 257;
	s32 trim1_2 = 126785;
	s32 trim2_1 = 88;
	s32 trim2_2 = 24911;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	if (fg_i2c_read(client, TEMPERATURE_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to read SOCREP\n", __func__);
		return -1;
	}

	if(data[1]&(0x1 << 7)) //Negative
	{
		temper = ((~(data[1]))&0xFF)+1;
		temper *= (-1000);
		temper = temper * trim2_1/100 - trim2_2;
	}
	else
	{
		temper = data[1] & 0x7f;
		temper *= 1000;
		temper += data[0] * 39 / 10;

		if(temper >=60000)
			temper = temper * trim1_1/100 - trim1_2;
		else
			temper = temper * trim2_1/100 - trim2_2;
	}

	return temper;
}


s32 fg_read_batt_temp(void)
{
	if(battery_type == SDI_BATTERY_TYPE)
		return fg_read_temp2();
	else if(battery_type == ATL_BATTERY_TYPE)
		return fg_read_temp3();
	else
		return (s32)30000;
}


int fg_read_soc(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	u32 soc = 0;
	u32 temp = 0;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	if (fg_i2c_read(client, SOCREP_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to read SOCREP\n", __func__);
		return -1;
	}
#if 0  // Not Used
	if (fg_i2c_read(client, SOCMIX_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to read SOCMIX\n", __func__);
		return -1;
	}
	if (fg_i2c_read(client, SOCAV_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to read SOCAV\n", __func__);
		return -1;
	}
#endif

	temp = data[0] * 39 / 1000;

	soc = data[1];
//	if(temp >= 5)  // over 0.5 %
//		soc += 1;

	if(!(pr_cnt % 30))
		printk("%s : SOC(%d), data(0x%04x)\n", __func__, soc, (data[1]<<8) | data[0]);

	return soc;
}


int fg_read_vfsoc(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	u32 vfsoc = 0;
	u32 temp = 0;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	if (fg_i2c_read(client, VFSOC_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to read VFSOC\n", __func__);
		return -1;
	}

	temp = data[0] * 39 / 1000;

	vfsoc = data[1];
	if(vfsoc == 0)
	{
		if(temp > 1)  // over 0.1 %
			vfsoc = 1;
	}

	if(!(pr_cnt % 30))
		printk("%s : VfSOC(%d), data(0x%04x)\n", __func__, vfsoc, (data[1]<<8) | data[0]);

	return vfsoc;
}


int fg_read_current(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data1[2], data2[2];
	u32 temp, sign;
	s32 i_current = 0;
	s32 avg_current = 0;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	if (fg_i2c_read(client, CURRENT_REG, data1, (u8)2) < 0) {
		pr_err("%s: Failed to read CURRENT\n", __func__);
		return -1;
	}

	if (fg_i2c_read(client, AVG_CURRENT_REG, data2, (u8)2) < 0) {
		pr_err("%s: Failed to read AVERAGE CURRENT\n", __func__);
		return -1;
	}

	temp = ((data1[1]<<8) | data1[0]) & 0xFFFF;
	if(temp & (0x1 << 15))
		{
		sign = NEGATIVE;
		temp = (~(temp) & 0xFFFF) + 1;
		}
	else
		sign = POSITIVE;
//	printk("%s : temp(0x%08x), data1(0x%04x)\n", __func__, temp, (data1[1]<<8) | data1[0]);

	temp = temp * 15625;
	i_current = temp / 100000;
	if(sign)
		i_current *= -1;

	temp = ((data2[1]<<8) | data2[0]) & 0xFFFF;
	if(temp & (0x1 << 15))
		{
		sign = NEGATIVE;
		temp = (~(temp) & 0xFFFF) + 1;
		}
	else
		sign = POSITIVE;
//	printk("%s : temp(0x%08x), data2(0x%04x)\n", __func__, temp, (data2[1]<<8) | data2[0]);

	temp = temp * 15625;
	avg_current = temp / 100000;
	if(sign)
		avg_current *= -1;

	if(!(pr_cnt++ % 30))
	{
		printk("%s : CURRENT(%dmA), AVG_CURRENT(%dmA)\n", __func__, i_current, avg_current);
		pr_cnt = 1;
	}

	return i_current;
}


int fg_read_avg_current(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8  data2[2];
	u32 temp, sign;
	s32 avg_current = 0;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}
	
	if (fg_i2c_read(client, AVG_CURRENT_REG, data2, (u8)2) < 0) {
		pr_err("%s: Failed to read AVERAGE CURRENT\n", __func__);
		return -1;
	}


	temp = ((data2[1]<<8) | data2[0]) & 0xFFFF;
	if(temp & (0x1 << 15))
	{
		sign = NEGATIVE;
		temp = (~(temp) & 0xFFFF) + 1;
	}
	else
		sign = POSITIVE;

	temp = temp * 15625;
	avg_current = temp / 100000;
	
	if(sign)
		avg_current *= -1;

	return avg_current;
}


// Simple function of read/write
int fg_read_register(u8 addr)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];

	if (fg_i2c_read(client, addr, data, (u8)2) < 0) {
		pr_err("%s: Failed to read addr(0x%x)\n", __func__, addr);
		return -1;
	}

	return ( (data[1] << 8) | data[0] );
}


int fg_write_register(u8 addr, u16 w_data)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];

	data[0] = w_data & 0xFF;
	data[1] = (w_data >> 8);

	if (fg_i2c_write(client, addr, data, (u8)2) < 0) {
		pr_err("%s: Failed to write addr(0x%x)\n", __func__, addr);
		return -1;
	}

	return 0;
}


int fg_reset_soc(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	s32 ret = 0;

	printk("%s : Before quick-start - VfOCV(%d), VfSOC(%d), RepSOC(%d)\n",
					__func__, fg_read_vfocv(), fg_read_vfsoc(), fg_read_soc());

	if(maxim_lpm_chg_status()) {
		printk("%s : Return by DCIN input (TA or USB)\n", __func__);
		return 0;
	}

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	// cycle 0
	fg_write_register(0x17, (u16)(0x0));
	
	if (fg_i2c_read(client, MISCCFG_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to read MiscCFG\n", __func__);
		return -1;
	}

	data[1] |= (0x1 << 2);  // Set bit10 makes quick start

	if (fg_i2c_write(client, MISCCFG_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to write MiscCFG\n", __func__);
		return -1;
	}

	msleep(250);
#if defined(CONFIG_TARGET_LOCALE_NTT)
	msleep(250);
#endif /* CONFIG_TARGET_LOCALE_NTT */

	fg_write_register(0x10, Capacity);  // FullCAP

	msleep(500);

	printk("%s : After quick-start - VfOCV(%d), VfSOC(%d), RepSOC(%d)\n",
					__func__, fg_read_vfocv(), fg_read_vfsoc(), fg_read_soc());

	// cycle 160
	fg_write_register(0x17, (u16)(0x00a0));

	return ret;
}


int fg_reset_capacity(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	s32 ret = 0;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	ret = fg_write_register(0x18, VFCapacity-1);  // DesignCAP

	return ret;
}


int fg_check_chip_state(void)
{
	u32 vcell, soc;

	vcell = fg_read_vcell();
	soc = fg_read_soc();

	printk("%s : vcell(%d), soc(%d)\n", __func__, vcell, soc);
	
	// if read operation fails, then it's not alive status
	if( (vcell < 0) || (soc < 0) )
		return 0;
	else
		return 1;
}


int fg_adjust_capacity(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	s32 ret = 0;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	data[0] = 0;
	data[1] = 0;

	// 1. Write RemCapREP(05h)=0;
	if (fg_i2c_write(client, REMCAP_REP_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to write RemCap_REP\n", __func__);
		return -1;
	}

	// 2. Write RemCapMIX(0Fh)=0;
	if (fg_i2c_write(client, REMCAP_MIX_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to write RemCap_MIX\n", __func__);
		return -1;
	}

	// 3. Write RemCapAV(1Fh)=0;
	if (fg_i2c_write(client, REMCAP_AV_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to write RemCap_AV\n", __func__);
		return -1;
	}

	//4. Write RepSOC(06h)=0;
	if (fg_i2c_write(client, SOCREP_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to write SOC_REP\n", __func__);
		return -1;
	}

	//5. Write MixSOC(0Dh)=0;
	if (fg_i2c_write(client, SOCMIX_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to write SOC_MIX\n", __func__);
		return -1;
	}

	//6. Write SOCAV(0Eh)=Table_SOC;
	if (fg_i2c_write(client, SOCAV_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to write SOC_AV\n", __func__);
		return -1;
	}

	msleep(200);

	printk("%s : After adjust - RepSOC(%d)\n", __func__, fg_read_soc());

	return ret;
}


extern int low_batt_comp_flag;
void fg_low_batt_compensation(u32 level)
{
	u16 read_val = 0;
	u32 temp = 0;
	u16 tempVal=0;

	printk("%s : Adjust SOCrep to %d!!\n", __func__, level);

	//1) RemCapREP (05h) = FullCap(10h) x 0.034 (or 0.014)
	read_val = fg_read_register(0x10);
	temp = read_val * (level*10 + 4) / 1000;
	fg_write_register(0x05, (u16)temp);

	//2) RemCapMix(0Fh) = RemCapREP
	fg_write_register(0x0f, (u16)temp);

	//3) RemCapAV(1Fh) = RemCapREP; 
	fg_write_register(0x1f, (u16)temp);

	//4) RepSOC (06h) = 3.4% or 1.4%
	tempVal=(u16)((level << 8) | 0x67);  // 103(0x67) * 0.0039 = 0.4%
	fg_write_register(0x06, tempVal);

	//5) MixSOC (0Dh) = RepSOC
	fg_write_register(0x0D, tempVal);

	//6) AVSOC (0Eh) = RepSOC; 
	fg_write_register(0x0E, tempVal);	

	low_batt_comp_flag = 1;  // Set flag
	
}


void fg_test_read(void)
{
	u8 data[2], reg;
	struct i2c_client *client = fg_i2c_client;

	for(reg = 0; reg < 0x40; reg++)
	{
		if(reg != 0x0C && reg != 0x13 && reg != 0x15 && reg != 0x20 && reg != 0x22
			&& reg != 0x26 && reg != 0x28 && reg != 0x30 && reg != 0x31 && reg != 0x34
			&& reg != 0x35 && reg != 0x3C && reg != 0x3E)
		{
			fg_i2c_read(client, reg, data, (u8)2);
 			printk("%s - addr(0x%02x), data(0x%04x)\n", __func__, reg, (data[1]<<8) | data[0]);
		}
	}

	printk("%s - addr(0x%02x), data(0x%04x)\n", __func__, (u8)VFOCV_REG, (u16)fg_read_register((u8)VFOCV_REG));
	printk("%s - addr(0x%02x), data(0x%04x)\n", __func__, (u8)VFSOC_REG, (u16)fg_read_register((u8)VFSOC_REG));
}


#ifdef MAX17042
int fg_alert_init(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 misccgf_data[2];
	u8 salrt_data[2];
	u8 config_data[2];
	u8 valrt_data[2];
	u8 talrt_data[2];
	u16 read_data = 0;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	// Using RepSOC
	if (fg_i2c_read(client, MISCCFG_REG, misccgf_data, (u8)2) < 0) {
		pr_err("%s: Failed to read MISCCFG_REG\n", __func__);
		return -1;
	}
	misccgf_data[0] = misccgf_data[0] & ~(0x03);	
	
	if(fg_i2c_write(client, MISCCFG_REG, misccgf_data, (u8)2))
	{
		pr_info("%s: Failed to write MISCCFG_REG\n", __func__);
		return -1;
	}

	// SALRT Threshold setting
	salrt_data[1]=0xff;
	salrt_data[0]=0x01; //1%
	if(fg_i2c_write(client, SALRT_THRESHOLD_REG, salrt_data, (u8)2))
	{
		pr_info("%s: Failed to write SALRT_THRESHOLD_REG\n", __func__);
		return -1;	
	}

rewrite_valrt:
	// Reset VALRT Threshold setting (disable)
	valrt_data[1] = 0xFF;
	valrt_data[0] = 0x00;
	if(fg_i2c_write(client, VALRT_THRESHOLD_REG, valrt_data, (u8)2))
	{
		pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = fg_read_register((u8)VALRT_THRESHOLD_REG);
	if(read_data != 0xff00) {
		printk(KERN_ERR "%s : VALRT_THRESHOLD_REG is not valid (0x%x)\n", __func__, read_data);
//		goto rewrite_valrt;
	}

rewrite_talrt:
	// Reset TALRT Threshold setting (disable)
	talrt_data[1] = 0x7F;
	talrt_data[0] = 0x80;
	if(fg_i2c_write(client, TALRT_THRESHOLD_REG, talrt_data, (u8)2))
	{
		pr_info("%s: Failed to write TALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = fg_read_register((u8)TALRT_THRESHOLD_REG);
	if(read_data != 0x7f80) {
		printk(KERN_ERR "%s : TALRT_THRESHOLD_REG is not valid (0x%x)\n", __func__, read_data);
//		goto rewrite_talrt;
	}

	mdelay(100);
	
	// Enable SOC alerts
	if (fg_i2c_read(client, CONFIG_REG, config_data, (u8)2) < 0) {
		pr_err("%s: Failed to read CONFIG_REG\n", __func__);
		return -1;
	}
	config_data[0] = config_data[0] | (0x1 << 2);	
	
	if(fg_i2c_write(client, CONFIG_REG, config_data, (u8)2))
	{
		pr_info("%s: Failed to write CONFIG_REG\n", __func__);
		return -1;
	}
		
	return 1;
}


int fg_check_status_reg(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 status_data[2];
	int ret = 0;
	
	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	// 1. Check Smn was generatedread
	if (fg_i2c_read(client, STATUS_REG, status_data, (u8)2) < 0) {
		pr_err("%s: Failed to read STATUS_REG\n", __func__);
		return -1;
	}
	printk("%s - addr(0x00), data(0x%04x)\n", __func__, (status_data[1]<<8) | status_data[0]);

	if(status_data[1] & (0x1 << 2))
		ret = 1;

	// 2. clear Status reg
	status_data[1] = 0;
	if(fg_i2c_write(client, STATUS_REG, status_data, (u8)2))
	{
		pr_info("%s: Failed to write STATUS_REG\n", __func__);
		return -1;
	}
	
	return ret;
}


int fg_check_battery_present(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 status_data[2];
	int ret = 1;
	
	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}

	// 1. Check Bst bit
	if (fg_i2c_read(client, STATUS_REG, status_data, (u8)2) < 0) {
		pr_err("%s: Failed to read STATUS_REG\n", __func__);
		return -1;
	}

	if(status_data[0] & (0x1 << 3))
	{
		printk("%s - addr(0x01), data(0x%04x)\n", __func__, (status_data[1]<<8) | status_data[0]);
		printk("%s : battery is absent!!\n", __func__);
		ret = 0;
	}

	return ret;
}


void fg_fullcharged_compensation(void)
{
//	struct i2c_client *client = fg_i2c_client;
	u16 fullcap_data;
	u16 NewFullCap_data;
	
	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return ;
	}

#ifndef CONFIG_TARGET_LOCALE_USAGSM
	/* Full Charge compensation algoritm 091310*/
	//1. Write RemCapREP(05h)=FullCap;
	fullcap_data = fg_read_register(FULLCAP_REG);
	fg_write_register(REMCAP_REP_REG, (u16)(fullcap_data));
	printk("%s : current fullcap = 0x%04x\n", __func__,fullcap_data);

	//2. Write RemCapMIX(0Fh)=FullCap;
	fg_write_register(REMCAP_MIX_REG, (u16)(fullcap_data));

	//3. Write RemCapAV(1Fh)=FullCap;
	fg_write_register(REMCAP_AV_REG, (u16)(fullcap_data));
#endif

#if defined(CONFIG_TARGET_LOCALE_USAGSM)
	/* Full Charge compensation algoritm 101310*/
	//1. NewFullCap=RemCapREP ;
	NewFullCap_data = fg_read_register(REMCAP_REP_REG);

	//2. If abs(FullCap-NewFullCap)<0.1*FullCap
	//3. FullCap=NewFullCap ;
	fullcap_data = fg_read_register(FULLCAP_REG);
	printk("%s : FullCap = 0x%04x, RemCapREP = 0x%04x \n", __func__,fullcap_data,NewFullCap_data);

	if(fullcap_data>NewFullCap_data)
	{
		if((fullcap_data-NewFullCap_data) < (fullcap_data/10))
		{
			fg_write_register(FULLCAP_REG, (u16)(NewFullCap_data));
			printk("%s : fullcap is updated to 0x%04x !!!\n", __func__,NewFullCap_data);
		}
	}
	else
	{
		if((NewFullCap_data-fullcap_data) < (fullcap_data/10))
		{
			fg_write_register(FULLCAP_REG, (u16)(NewFullCap_data));
			printk("%s : fullcap is updated to 0x%04x !!!\n", __func__,NewFullCap_data);
		}
	}
#endif

	//4. Write RepSOC(06h)=100%;
	fg_write_register(SOCREP_REG, (u16)(0x64 << 8));

	//5. Write MixSOC(0Dh)=100%;
	fg_write_register(SOCMIX_REG, (u16)(0x64 << 8));

	//6. Write AVSOC(0Eh)=100%;
	fg_write_register(SOCAV_REG, (u16)(0x64 << 8));

}
#endif


void fg_set_battery_type(void)
{
	u16 data = 0;
	u8 type_str[10];

	data = fg_read_register(0x18);

#ifndef CONFIG_TARGET_LOCALE_USAGSM
	if((data == SDI_VFCapacity) || (data == SDI_VFCapacity-1))
		battery_type = SDI_BATTERY_TYPE;
	else if((data == ATL_VFCapacity) || (data == ATL_VFCapacity-1))
		battery_type = ATL_BATTERY_TYPE;
#else
		battery_type = SDI_BATTERY_TYPE;
#endif

	if(battery_type == SDI_BATTERY_TYPE)
		sprintf(type_str, "SDI");
	else if(battery_type == ATL_BATTERY_TYPE)
		sprintf(type_str, "ATL");
	else
		sprintf(type_str, "Unknown");

	printk("%s : DesignCAP(0x%04x), Battery type(%s)\n", __func__, data, type_str);

	switch(battery_type) {
	case ATL_BATTERY_TYPE:
		Capacity = ATL_Capacity;
		VFCapacity = ATL_VFCapacity;
		break;

	case SDI_BATTERY_TYPE:
	default:
		Capacity = SDI_Capacity;
		VFCapacity = SDI_VFCapacity;
		break;
	}

}


void fuel_gauge_rcomp(void)
{
#ifndef CONFIG_MACH_S5PC110_P1
	struct i2c_client *client = fg_i2c_client;
	u8 rst_cmd[2];
	s32 ret = 0;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return ;
	}
#if 0
	rst_cmd[0] = 0xa0;
	rst_cmd[1] = 0x00;

	ret = fg_i2c_write(client, RCOMP_REG, rst_cmd, (u8)2);
	if (ret)
		pr_info("%s: failed fuel_gauge_rcomp(%d)\n", __func__, ret);
	
	msleep(500);
#endif
#endif
}


int fg_read_rcomp(void)
{
#ifndef CONFIG_MACH_S5PC110_P1
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	u16 rcomp = 0;

	if(!fuel_guage_init) {
		printk("%s : fuel guage IC is not initialized!!\n", __func__);
		return -1;
	}
#if 0
	if (fg_i2c_read(client, RCOMP_REG, data, (u8)2) < 0) {
		pr_err("%s: Failed to read RCOMP\n", __func__);
		return -1;
	}

	rcomp = (data[1]<<8) | data[0];
#endif
	return rcomp;
#else
	return 0;
#endif
}


static int fg_i2c_remove(struct i2c_client *client)
{
	struct fg_state *fg = i2c_get_clientdata(client);

	kfree(fg);
	return 0;
}

static int fg_i2c_probe(struct i2c_client *client,  const struct i2c_device_id *id)
{
	struct fg_state *fg;

	fg = kzalloc(sizeof(struct fg_state), GFP_KERNEL);
	if (fg == NULL) {		
		printk("failed to allocate memory \n");
		return -ENOMEM;
	}
	
	fg->client = client;
	i2c_set_clientdata(client, fg);
	
	/* rest of the initialisation goes here. */
	
	printk("Fuel guage attach success!!!\n");

	fg_i2c_client = client;

	fuel_guage_init = 1;

	fg_set_battery_type();

	fg_test_read();
	
	return 0;
}


static const struct i2c_device_id fg_device_id[] = {
	{"fuelgauge", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, fg_device_id);


static struct i2c_driver fg_i2c_driver = {
	.driver = {
		.name = "fuelgauge",
		.owner = THIS_MODULE,
	},
	.probe	= fg_i2c_probe,
	.remove	= fg_i2c_remove,
	.id_table	= fg_device_id,
};

