#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>
#include <linux/kernel.h>//for printk


#include "kd_camera_hw.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"

#define PFX "[kd_camera_hw]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg)

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_ERR(fmt, arg...)         xlog_printk(ANDROID_LOG_ERR, PFX , fmt, ##arg)
#define PK_XLOG_INFO(fmt, args...) \
                do {    \
                   xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg); \
                } while(0)
#else
#define PK_DBG(a,...)
#define PK_ERR(a,...)
#define PK_XLOG_INFO(fmt, args...)
#endif

//[begin]456653 zhfan 2013-6-10 for imx135 and imx132 power sequence
#define MAIN_RST         GPIO_CAMERA_CMRST_PIN
#define MAIN_PDN         GPIO_CAMERA_CMPDN_PIN
#define MAIN_AF_EN_PIN   GPIO133
#define SUB_RST          GPIO_CAMERA_CMRST1_PIN
#define SUB_PDN          GPIO_CAMERA_CMPDN1_PIN

#define IMX135_DVDD_PIN  GPIO22 
#define MCLK1_PIN        GPIO210

#define MODE_GPIO        GPIO_MODE_00
#define MODE_CMMCLK      GPIO_MODE_01

extern void ISP_MCLK1_EN(BOOL En);
static void setCameraPin(u16 pinNum, u8 mode, u8 dir, u8 level);
static void initCameraPin(char *currSensorName);
static bool JRDPowerSwitchDVDDVOL1050(char *currSensorName, bool on); //for imx135 1.05V DVDD special requirement

//[end]456653 zhfan 2013-6-10


int kdCISModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx, char *currSensorName, BOOL On, char* mode_name)
{
u32 pinSetIdx = 0;//default main sensor

#define IDX_PS_CMRST 0
#define IDX_PS_CMPDN 4

#define IDX_PS_MODE 1
#define IDX_PS_ON   2
#define IDX_PS_OFF  3   
u32 pinSet[3][8] = {
                    //for main sensor
                    {GPIO_CAMERA_CMRST_PIN,
                        GPIO_CAMERA_CMRST_PIN_M_GPIO,   /* mode */
                        GPIO_OUT_ONE,                   /* ON state */
                        GPIO_OUT_ZERO,                  /* OFF state */
                     GPIO_CAMERA_CMPDN_PIN,
                        GPIO_CAMERA_CMPDN_PIN_M_GPIO,
                        GPIO_OUT_ONE,
                        GPIO_OUT_ZERO,
                    },
                    //for sub sensor
                    {GPIO_CAMERA_CMRST1_PIN,
                     GPIO_CAMERA_CMRST1_PIN_M_GPIO,
                        GPIO_OUT_ONE,
                        GPIO_OUT_ZERO,
                     GPIO_CAMERA_CMPDN1_PIN,
                        GPIO_CAMERA_CMPDN1_PIN_M_GPIO,
                        GPIO_OUT_ONE,
                        GPIO_OUT_ZERO,
                    },
                    //for main_2 sensor
                    {GPIO_CAMERA_2_CMRST_PIN,
                        GPIO_CAMERA_2_CMRST_PIN_M_GPIO,   /* mode */
                        GPIO_OUT_ONE,                   /* ON state */
                        GPIO_OUT_ZERO,                  /* OFF state */
                     GPIO_CAMERA_2_CMPDN_PIN,
                        GPIO_CAMERA_2_CMPDN_PIN_M_GPIO,
                        GPIO_OUT_ONE,
                        GPIO_OUT_ZERO,
                    }
                   };




    if (DUAL_CAMERA_MAIN_SENSOR == SensorIdx){
        pinSetIdx = 0;
    }
    else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx) {
        pinSetIdx = 1;
    }
    else if (DUAL_CAMERA_MAIN_2_SENSOR == SensorIdx) {
       // pinSetIdx = 2;
    }

    //power ON
    if (On) {


#if 0  //TODO: depends on HW layout. Should be notified by SA.

        PK_DBG("Set CAMERA_POWER_PULL_PIN for power \n");
        if (mt_set_gpio_pull_enable(GPIO_CAMERA_LDO_EN_PIN, GPIO_PULL_DISABLE)) {PK_DBG("[[CAMERA SENSOR] Set CAMERA_POWER_PULL_PIN DISABLE ! \n"); }
        if(mt_set_gpio_mode(GPIO_CAMERA_LDO_EN_PIN, GPIO_CAMERA_LDO_EN_PIN_M_GPIO)){PK_DBG("[[CAMERA SENSOR] set CAMERA_POWER_PULL_PIN mode failed!! \n");}
        if(mt_set_gpio_dir(GPIO_CAMERA_LDO_EN_PIN,GPIO_DIR_OUT)){PK_DBG("[[CAMERA SENSOR] set CAMERA_POWER_PULL_PIN dir failed!! \n");}
        if(mt_set_gpio_out(GPIO_CAMERA_LDO_EN_PIN,GPIO_OUT_ONE)){PK_DBG("[[CAMERA SENSOR] set CAMERA_POWER_PULL_PIN failed!! \n");}
#endif

		PK_DBG("kdCISModulePowerOn -on:currSensorName=%s\n",currSensorName);
		PK_DBG("kdCISModulePowerOn -on:pinSetIdx=%d\n",pinSetIdx);
        
	initCameraPin(currSensorName);

//[begin]496059 zhfan imx135 sunny module lens compatible 2013-07-26
        if ((DUAL_CAMERA_MAIN_SENSOR == SensorIdx) && currSensorName && ((0 == strcmp(SENSOR_DRVNAME_IMX135_LENS50016_MIPI_RAW, currSensorName))
              ||(0 == strcmp(SENSOR_DRVNAME_IMX135_MIPI_RAW, currSensorName))||(0 == strcmp(SENSOR_DRVNAME_IMX135TRULY_MIPI_RAW, currSensorName))))
//[end]496059 zhfan
        {
            PK_DBG("kdCISModulePowerOn get in---  SENSOR_DRVNAME_IMX135TRULY_MIPI_RAW \n");
            PK_DBG("[ON_general 2.8V]sensorIdx:%d \n",SensorIdx);

            setCameraPin(MCLK1_PIN, MODE_GPIO, GPIO_DIR_OUT, GPIO_OUT_ZERO);

            ISP_MCLK1_EN(0);

            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
                goto _kdCISModulePowerOn_exit_;
            }

	   /*  if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1200,mode_name))
	    {
		  PK_DBG("[CAMERA SENSOR] Fail to enable DOVDD power\n");
		  goto _kdCISModulePowerOn_exit_;
	    }*/
            if(TRUE != JRDPowerSwitchDVDDVOL1050(currSensorName, On))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable DOVDD power\n");
                goto _kdCISModulePowerOn_exit_;
            }

            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                goto _kdCISModulePowerOn_exit_;
            }

            mdelay(10);
            
            setCameraPin(MAIN_RST, MODE_GPIO, GPIO_DIR_OUT, GPIO_OUT_ONE);
            setCameraPin(MAIN_PDN, MODE_GPIO, GPIO_DIR_OUT, GPIO_OUT_ONE);

            setCameraPin(MCLK1_PIN, MODE_CMMCLK, GPIO_DIR_OUT, GPIO_OUT_ZERO);

            ISP_MCLK1_EN(1);

            //modified by zhfan, pr450349, porting diablox main camera imx135 VCM driver dw9714a 
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
                goto _kdCISModulePowerOn_exit_;
            }

            mdelay(13);
            
            setCameraPin(MAIN_AF_EN_PIN, MODE_GPIO, GPIO_DIR_OUT, GPIO_OUT_ONE); 
	}

		else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV8825_MIPI_RAW,currSensorName)))
		{
				PK_DBG("kdCISModulePowerOn get in---  SENSOR_DRVNAME_OV8825_MIPI_RAW \n");
				PK_DBG("[ON_general 2.8V]sensorIdx:%d \n",SensorIdx);

                //[begin]456653 zhfan 2013-6-10 for imx135 and imx132 power sequence
                setCameraPin(MCLK1_PIN, MODE_CMMCLK, GPIO_DIR_OUT, GPIO_OUT_ZERO);

                ISP_MCLK1_EN(1);
                //[end]456653 zhfan 2013-6-10

				if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
				{
					PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
					//return -EIO;
					goto _kdCISModulePowerOn_exit_;
				}


			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
			{
				PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
				//return -EIO;
				goto _kdCISModulePowerOn_exit_;
			}

			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1500,mode_name))
			{
				 PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
				 //return -EIO;
				 goto _kdCISModulePowerOn_exit_;
			}

			if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
			{
				PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
				//return -EIO;
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);

			//PDN/STBY pin
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST])
			{
				if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
				if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
				if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
				mdelay(10);
				if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
				mdelay(1);

				//RST pin
				if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
				if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
				if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
				mdelay(10);
				if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
				mdelay(1);
			}

        //disable inactive sensor
        if(pinSetIdx == 0 || pinSetIdx == 2) {//disable sub
	        if (GPIO_CAMERA_INVALID != pinSet[1][IDX_PS_CMRST]) {
	            if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	            if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	            if(mt_set_gpio_out(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
	            if(mt_set_gpio_out(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
	        }
        }
		else {
	        if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
	            if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	            if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	            if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
	            if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
	        }
	        if (GPIO_CAMERA_INVALID != pinSet[2][IDX_PS_CMRST]) {
               /*
	            if(mt_set_gpio_mode(pinSet[2][IDX_PS_CMRST],pinSet[2][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	            if(mt_set_gpio_mode(pinSet[2][IDX_PS_CMPDN],pinSet[2][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[2][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[2][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	            if(mt_set_gpio_out(pinSet[2][IDX_PS_CMRST],pinSet[2][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
	            if(mt_set_gpio_out(pinSet[2][IDX_PS_CMPDN],pinSet[2][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
               */
	        }
		}

	}
	else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K8AAYX_MIPI_YUV,currSensorName)))
	{
		PK_DBG("kdCISModulePowerOn get in---  SENSOR_DRVNAME_S5K8AAYX_MIPI_YUV \n");
		PK_DBG("[ON_general 2.8V]sensorIdx:%d \n",SensorIdx);

        //[begin]456653 zhfan 2013-6-10 for imx135 and imx132 power sequence
        setCameraPin(MCLK1_PIN, MODE_CMMCLK, GPIO_DIR_OUT, GPIO_OUT_ZERO);

        ISP_MCLK1_EN(1);
        //[end]456653 zhfan 2013-6-10
		//PDN/STBY pin
		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST])
		{
			PK_DBG("kdCISModulePowerOn get in---  SENSOR_DRVNAME_S5K8AAYX_MIPI_YUV -init set pdn/rst \n");
			
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
			mdelay(2);

			//RST pin
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
			mdelay(2);
		}


		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800/*VOL_2800*/,mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
			//return -EIO;
			goto _kdCISModulePowerOn_exit_;
		}
		mdelay(2);


		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
			//return -EIO;
			goto _kdCISModulePowerOn_exit_;
		}
		mdelay(2);

		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D_SUB, VOL_1200,mode_name))
		{
			 PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
			 //return -EIO;
			 goto _kdCISModulePowerOn_exit_;
		}
		mdelay(2);

		//if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
		//{
		//	PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
		//	//return -EIO;
		//	goto _kdCISModulePowerOn_exit_;
		//}
		//mdelay(2);

		//PDN/STBY pin
		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST])
		{
			//if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
			//if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
			//if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
			//mdelay(10);
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
			mdelay(5);

			//RST pin
			//if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			//if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			//if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
			//mdelay(10);
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
			mdelay(5);
		}

        //disable inactive sensor
        if(pinSetIdx == 0 || pinSetIdx == 2) {//disable sub
	        if (GPIO_CAMERA_INVALID != pinSet[1][IDX_PS_CMRST]) {
	            if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	            if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	            if(mt_set_gpio_out(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
	            if(mt_set_gpio_out(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
	        }
        }
		else {
	        if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
				PK_DBG("kdCISModulePowerOn 8AA close index[0]\n");
				
	            if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	            if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	            if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
	            if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
	        }
	        if (GPIO_CAMERA_INVALID != pinSet[2][IDX_PS_CMRST]) {
				PK_DBG("kdCISModulePowerOn 8AA close index[2]\n");
				
	            /*if(mt_set_gpio_mode(pinSet[2][IDX_PS_CMRST],pinSet[2][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	            if(mt_set_gpio_mode(pinSet[2][IDX_PS_CMPDN],pinSet[2][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[2][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[2][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	            if(mt_set_gpio_out(pinSet[2][IDX_PS_CMRST],pinSet[2][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
	            if(mt_set_gpio_out(pinSet[2][IDX_PS_CMPDN],pinSet[2][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
                */
	        }
		}
		mdelay(50);
	}
    else
    {
		PK_DBG("kdCISModulePowerOn get in---  other \n");
        //enable active sensor
        //RST pin
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(10);
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(1);

            //PDN pin
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        }

        //DOVDD
        if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV3640_YUV,currSensorName)))
        {
            PK_DBG("[ON_OV3640YUV case 1.5V]sensorIdx:%d \n",SensorIdx);
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
            }
        }
        else //general case on
        {
            PK_DBG("[ON_general 1.8V]sensorIdx:%d \n",SensorIdx);
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_2800,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
            }
        }
        mdelay(10);
        //AVDD
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
            //return -EIO;
            //goto _kdCISModulePowerOn_exit_;
        }
        mdelay(10);
        //DVDD
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1500,mode_name))
        {
             PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
             //return -EIO;
             //goto _kdCISModulePowerOn_exit_;
        }
        mdelay(10);

        //AF_VCC
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
            //return -EIO;
            goto _kdCISModulePowerOn_exit_;
        }

#if 1
        //enable active sensor
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(10);
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(1);

            //PDN pin
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        }
#endif

        //disable inactive sensor
        if(pinSetIdx == 0 || pinSetIdx == 2) {//disable sub
	        if (GPIO_CAMERA_INVALID != pinSet[1][IDX_PS_CMRST]) {
	            if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	            if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	            if(mt_set_gpio_out(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
	            if(mt_set_gpio_out(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
	        }
        }
		else {
	        if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
	            if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	            if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	            if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
	            if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
	        }
	        if (GPIO_CAMERA_INVALID != pinSet[2][IDX_PS_CMRST]) {
	            /*if(mt_set_gpio_mode(pinSet[2][IDX_PS_CMRST],pinSet[2][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	            if(mt_set_gpio_mode(pinSet[2][IDX_PS_CMPDN],pinSet[2][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[2][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[2][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	            if(mt_set_gpio_out(pinSet[2][IDX_PS_CMRST],pinSet[2][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
	            if(mt_set_gpio_out(pinSet[2][IDX_PS_CMPDN],pinSet[2][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
                */
	        }
		}
    }

  }
  else {//power OFF

#if 0 //TODO: depends on HW layout. Should be notified by SA.
        PK_DBG("Set GPIO 94 for power OFF\n");
        if (mt_set_gpio_pull_enable(GPIO_CAMERA_LDO_EN_PIN, GPIO_PULL_DISABLE)) {PK_DBG("[CAMERA SENSOR] Set GPIO94 PULL DISABLE ! \n"); }
        if(mt_set_gpio_mode(GPIO_CAMERA_LDO_EN_PIN, GPIO_CAMERA_LDO_EN_PIN_M_GPIO)){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
        if(mt_set_gpio_dir(GPIO_CAMERA_LDO_EN_PIN,GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
        if(mt_set_gpio_out(GPIO_CAMERA_LDO_EN_PIN,GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
#endif
		PK_DBG("kdCISModulePowerOn -off:currSensorName=%s\n",currSensorName);

	initCameraPin(currSensorName);

//[begin]496059 zhfan imx135 sunny module lens compatible 2013-07-26
        if ((DUAL_CAMERA_MAIN_SENSOR == SensorIdx) && currSensorName && ((0 == strcmp(SENSOR_DRVNAME_IMX135_LENS50016_MIPI_RAW, currSensorName))
              ||(0 == strcmp(SENSOR_DRVNAME_IMX135_MIPI_RAW, currSensorName)) ||(0 == strcmp(SENSOR_DRVNAME_IMX135TRULY_MIPI_RAW, currSensorName))))
//[end]496059 zhfan
        {
            PK_DBG("kdCISModulePower--off get in---SENSOR_DRVNAME_IMX135TRULY_MIPI_RAW \n");

            mdelay(2);

            setCameraPin(MCLK1_PIN, MODE_GPIO, GPIO_DIR_OUT, GPIO_OUT_ZERO);

            ISP_MCLK1_EN(0);

            mdelay(2);

            if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
                //return -EIO;
                goto _kdCISModulePowerOn_exit_;
            }
                        
            if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2, mode_name)) {
                PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
                //return -EIO;
                goto _kdCISModulePowerOn_exit_;
            }

          /* if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D, mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to OFF DOVDD power\n");
                goto _kdCISModulePowerOn_exit_;
            }*/
            if(TRUE != JRDPowerSwitchDVDDVOL1050(currSensorName, On))
            {
                PK_DBG("[CAMERA SENSOR] Fail to OFF DOVDD power\n");
                goto _kdCISModulePowerOn_exit_;
            }
            
            
            if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
                //return -EIO;
                goto _kdCISModulePowerOn_exit_;
            }
        }

		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV8825_MIPI_RAW,currSensorName)))
		{
			PK_DBG("kdCISModulePower--off get in---SENSOR_DRVNAME_OV8825_MIPI_RAW \n");

            //reset pull down
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
				if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
				if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
				if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
			}

			if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
				//return -EIO;
				goto _kdCISModulePowerOn_exit_;
			}

			if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
				//return -EIO;
				goto _kdCISModulePowerOn_exit_;
			}

			if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
			{
				PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
				//return -EIO;
				goto _kdCISModulePowerOn_exit_;
			}

			if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name))
			{
				PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
				//return -EIO;
				goto _kdCISModulePowerOn_exit_;
			}

	        //PDN pull down
	        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
	            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	    	    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
	        }
		}

		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K8AAYX_MIPI_YUV,currSensorName)))
		{
			PK_DBG("kdCISModulePower--off get in---SENSOR_DRVNAME_S5K8AAYX_MIPI_YUV \n");

			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
				if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
				if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
				if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
				mdelay(2);

				if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
				if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
				if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
				mdelay(2);
			}

			if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D_SUB, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
				//return -EIO;
				goto _kdCISModulePowerOn_exit_;
			}

			if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
				//return -EIO;
				goto _kdCISModulePowerOn_exit_;
			}

			if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
			{
				PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
				//return -EIO;
				goto _kdCISModulePowerOn_exit_;
			}

			if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name))
			{
				PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
				//return -EIO;
				goto _kdCISModulePowerOn_exit_;
			}
		}
		else
	    {
			PK_DBG("kdCISModulePower--off get in---other \n");

        //PK_DBG("[OFF]sensorIdx:%d \n",SensorIdx);
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
    	    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
        }

    	if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name)) {
            PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
            //return -EIO;
            goto _kdCISModulePowerOn_exit_;
        }
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
            //return -EIO;
            goto _kdCISModulePowerOn_exit_;
        }
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D, mode_name)) {
            PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
            //return -EIO;
            goto _kdCISModulePowerOn_exit_;
        }
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
            //return -EIO;
            goto _kdCISModulePowerOn_exit_;
        }
		if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D_SUB, mode_name)) {
			PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
			//return -EIO;
			goto _kdCISModulePowerOn_exit_;
		}

		
	}
  }
	return 0;

_kdCISModulePowerOn_exit_:
    return -EIO;
}

static bool JRDPowerSwitchDVDDVOL1050(char *currSensorName, bool on)
{
    bool ret = FALSE;
//[begin]496059 zhfan imx135 sunny module lens compatible 2013-07-26 
    if(currSensorName && ((0 == strcmp(SENSOR_DRVNAME_IMX135_LENS50016_MIPI_RAW, currSensorName))
              ||(0 == strcmp(SENSOR_DRVNAME_IMX135_MIPI_RAW, currSensorName))||(0 == strcmp(SENSOR_DRVNAME_IMX135TRULY_MIPI_RAW, currSensorName))))
//[end]496059 zhfan
    {
        if(mt_set_gpio_mode(IMX135_DVDD_PIN, MODE_GPIO))
        {
            PK_DBG("[IMX135_DVDD_PIN] set gpio mode failed!! \n");
        }
        
        if(on)
        {
            if(mt_set_gpio_out(IMX135_DVDD_PIN, GPIO_OUT_ONE))
            {
                PK_DBG("[IMX135_DVDD_PIN] set gpio dir failed!! \n");
            }
        }
        else
        {
            if(mt_set_gpio_out(IMX135_DVDD_PIN, GPIO_OUT_ZERO))
            {
                PK_DBG("[IMX135_DVDD_PIN] set gpio dir failed!! \n");
            }
        }

        if(mt_set_gpio_dir(IMX135_DVDD_PIN, GPIO_DIR_OUT))
        {
            PK_DBG("[IMX135_DVDD_PIN] set gpio failed!! \n");
        }
        else
        {
            ret = TRUE;
        }
    }
    else
    {
        PK_DBG("[JRDPowerSwitchDVDDVOL1050] It's for imx135 only, not used by other sensor!! \n");
    }
    return ret;
}
//[begin]456653 zhfan 2013-6-10 for imx135 and imx132 power sequence
static void setCameraPin(u16 pinNum, u8 mode, u8 dir, u8 level)
{
    if(mt_set_gpio_mode(pinNum, mode)){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
    if(mt_set_gpio_out(pinNum, level)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
    if(mt_set_gpio_dir(pinNum, dir)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
}
static void initCameraPin(char *currSensorName)
{
//[begin]496059 zhfan imx135 sunny module lens compatible 2013-07-26
    if(currSensorName && ((0 == strcmp(SENSOR_DRVNAME_IMX135_LENS50016_MIPI_RAW, currSensorName))
              ||(0 == strcmp(SENSOR_DRVNAME_IMX135_MIPI_RAW, currSensorName))||(0 == 
              strcmp(SENSOR_DRVNAME_IMX135TRULY_MIPI_RAW, currSensorName))))
//[end]496059 zhfan
    {
        setCameraPin(MAIN_RST, MODE_GPIO, GPIO_DIR_OUT, GPIO_OUT_ZERO);
        setCameraPin(MAIN_PDN, MODE_GPIO, GPIO_DIR_OUT, GPIO_OUT_ZERO);
        mdelay(1);
        setCameraPin(MAIN_AF_EN_PIN, MODE_GPIO, GPIO_DIR_OUT, GPIO_OUT_ZERO);
    }
    else
    {
        PK_DBG("[CAMERA SENSOR]Do not support this sensor!!\n");
    }
}

//[end]456653 zhfan 2013-6-10

EXPORT_SYMBOL(kdCISModulePowerOn);