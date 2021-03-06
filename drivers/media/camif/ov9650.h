#define	OVC9650_I2C_ADDR  0x30

struct ov9650_regval_list {
	unsigned char  reg_num;
	unsigned char  value;
};


struct ov9650_regval_list ov9650_regs[] = {

	{0x12, 0x80}, 		//Camera Soft reset. Self cleared after reset.
	{0xFF, 0x00},		//delay
	{0x01, 0x80}, 		//BLUE
	{0x02, 0x80}, 		//RED
	{0x0E, 0xa0},		//COM5
	{0x0F, 0x42},		//COM6
	{0x13, 0xe7},		//COM8 11100111
	{0x24, 0x74},		//AEW
	{0x25, 0x68},		//AEB
	{0x3B, 0x00},		//COM11
	{0x6A, 0x3e},		//MBD
	{0xA3, 0x41},		//BD60ST
	{0x42, 0x09},		//COM17
	{0x14, 0x2e},		//COM9
	{0x26, 0xC3},		//VPT

	{0x00, 0x00},
	{0x03, 0x09},
	{0x16, 0x06},

	{0x32, 0x80},
	{0x17, 0x1D},
	{0x18, 0xBD},
	{0x19, 0x01},
	{0x1A, 0x81},
	{0x1B, 0x00},
	{0x1E, 0x34},
	{0x29, 0x3f},
	{0x2A, 0x00},
	{0x2B, 0x00},

	{0x33, 0xe2},
	{0x34, 0xbf},
	{0x35, 0x91},
	{0x3C, 0x77},
	{0x3D, 0x92},
	{0x3E, 0x02},
	{0x3F, 0xa6},
	{0x41, 0x02},
	{0x43, 0xf0},
	{0x44, 0x10},
	{0x45, 0x68},
	{0x46, 0x96},
	{0x47, 0x60},
	{0x48, 0x80},

	{0x4F, 0x3a},
	{0x50, 0x3d},
	{0x51, 0x03},
	{0x52, 0x12},
	{0x53, 0x26},
	{0x54, 0x38},
	{0x55, 0x40},
	{0x56, 0x40},
	{0x57, 0x40},
	{0x58, 0x0d},
	{0x59, 0xc0},
	{0x5a, 0xaf},
	{0x5b, 0x55},
	{0x5C, 0xb9},
	{0x5D, 0x96},
	{0x5E, 0x10},
	{0x5F, 0xe0},
	{0x60, 0x8c},
	{0x61, 0x20},

	{0x69, 0x40},

	{0x6C, 0x40},
	{0x6D, 0x30},
	{0x6E, 0x4b},
	{0x6F, 0x60},
	{0x70, 0x70},
	{0x71, 0x70},
	{0x72, 0x70},
	{0x73, 0x70},
	{0x74, 0x60},
	{0x75, 0x60},
	{0x76, 0x50},
	{0x77, 0x48},
	{0x78, 0x3a},
	{0x79, 0x2e},
	{0x7A, 0x28},
	{0x7B, 0x22},
	{0x7C, 0x04},
	{0x7D, 0x07},
	{0x7E, 0x10},
	{0x7F, 0x28},
	{0x80, 0x36},
	{0x81, 0x44},
	{0x82, 0x52},
	{0x83, 0x60},
	{0x84, 0x6c},
	{0x85, 0x78},
	{0x86, 0x8c},
	{0x87, 0x9e},
	{0x88, 0xbb},
	{0x89, 0xd2},
	{0x8A, 0xe6},

	{0x8B, 0x06},
	{0x8C, 0x00},
	{0x8D, 0x00},
	{0x8E, 0x00},
	{0x8F, 0x0F},
	{0x90, 0x00},
	{0x91, 0x00},

	{0x94, 0x88},
	{0x95, 0x88},
	{0x96, 0x04},
	{0xA0, 0x00},
	{0xA4, 0x74},
	{0xA5, 0xd9},
	{0xa9, 0xb8},
	{0xaa, 0x92},
	{0xab, 0x0a},

	// OV9650_ITU_656
	{0x04, 0x40},  //VGA
	{0x40, 0x00},
	{0x3A, 0x09}, //YUYV

	{0xA2, 0x4b},

	{0x0C, 0x04},
	{0x0D, 0x80},

	//{0x11, 0x81}, //30 fps
	{0x11, 0x81}, //20 fps

	{0x12, 0x40},
	{0x37, 0x91},
	{0x38, 0x12},
	{0x39, 0x43},

	{0x15, 0x1A}, //normal
	{0xFF, 0x00},
};




//sensor registers


