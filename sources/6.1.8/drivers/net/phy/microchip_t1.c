// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Microchip Technology

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/ethtool.h>
#include <linux/ethtool_netlink.h>
#include <linux/bitfield.h>
#include <linux/microchipphy.h>
#include <linux/of.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/ptp_clock.h>
#include <linux/ptp_classify.h>
#include <linux/net_tstamp.h>

#define PHY_ID_LAN87XX				0x0007c150
#define PHY_ID_LAN937X				0x0007c180

/* External Register Control Register */
#define LAN87XX_EXT_REG_CTL                     (0x14)
#define LAN87XX_EXT_REG_CTL_RD_CTL              (0x1000)
#define LAN87XX_EXT_REG_CTL_WR_CTL              (0x0800)
#define LAN87XX_REG_BANK_SEL_MASK		GENMASK(10, 8)
#define LAN87XX_REG_ADDR_MASK			GENMASK(7, 0)

/* External Register Read Data Register */
#define LAN87XX_EXT_REG_RD_DATA                 (0x15)

/* External Register Write Data Register */
#define LAN87XX_EXT_REG_WR_DATA                 (0x16)

/* Interrupt Source Register */
#define LAN87XX_INTERRUPT_SOURCE                (0x18)
#define LAN87XX_INTERRUPT_SOURCE_2              (0x08)

/* Interrupt Mask Register */
#define LAN87XX_INTERRUPT_MASK                  (0x19)
#define LAN87XX_MASK_LINK_UP                    (0x0004)
#define LAN87XX_MASK_LINK_DOWN                  (0x0002)

#define LAN87XX_INTERRUPT_MASK_2                (0x09)
#define LAN87XX_MASK_COMM_RDY			BIT(10)

/* MISC Control 1 Register */
#define LAN87XX_CTRL_1                          (0x11)
#define LAN87XX_MASK_RGMII_TXC_DLY_EN           (0x4000)
#define LAN87XX_MASK_RGMII_RXC_DLY_EN           (0x2000)

/* phyaccess nested types */
#define	PHYACC_ATTR_MODE_READ		0
#define	PHYACC_ATTR_MODE_WRITE		1
#define	PHYACC_ATTR_MODE_MODIFY		2
#define	PHYACC_ATTR_MODE_POLL		3

#define	PHYACC_ATTR_BANK_SMI		0
#define	PHYACC_ATTR_BANK_MISC		1
#define	PHYACC_ATTR_BANK_PCS		2
#define	PHYACC_ATTR_BANK_AFE		3
#define	PHYACC_ATTR_BANK_DSP		4
#define	PHYACC_ATTR_BANK_MAX		7

/* measurement defines */
#define	LAN87XX_CABLE_TEST_OK		0
#define	LAN87XX_CABLE_TEST_OPEN	1
#define	LAN87XX_CABLE_TEST_SAME_SHORT	2

/* T1 Registers */
#define T1_AFE_PORT_CFG1_REG		0x0B
#define T1_POWER_DOWN_CONTROL_REG	0x1A
#define T1_SLV_FD_MULT_CFG_REG		0x18
#define T1_CDR_CFG_PRE_LOCK_REG		0x05
#define T1_CDR_CFG_POST_LOCK_REG	0x06
#define T1_LCK_STG2_MUFACT_CFG_REG	0x1A
#define T1_LCK_STG3_MUFACT_CFG_REG	0x1B
#define T1_POST_LCK_MUFACT_CFG_REG	0x1C
#define T1_TX_RX_FIFO_CFG_REG		0x02
#define T1_TX_LPF_FIR_CFG_REG		0x55
#define T1_COEF_CLK_PWR_DN_CFG		0x04
#define T1_COEF_RW_CTL_CFG		0x0D
#define T1_SQI_CONFIG_REG		0x2E
#define T1_SQI_CONFIG2_REG		0x4A
#define T1_DCQ_SQI_REG			0xC3
#define T1_DCQ_SQI_MSK			GENMASK(3, 1)
#define T1_MDIO_CONTROL2_REG		0x10
#define T1_INTERRUPT_SOURCE_REG		0x18
#define T1_INTERRUPT2_SOURCE_REG	0x08
#define T1_EQ_FD_STG1_FRZ_CFG		0x69
#define T1_EQ_FD_STG2_FRZ_CFG		0x6A
#define T1_EQ_FD_STG3_FRZ_CFG		0x6B
#define T1_EQ_FD_STG4_FRZ_CFG		0x6C
#define T1_EQ_WT_FD_LCK_FRZ_CFG		0x6D
#define T1_PST_EQ_LCK_STG1_FRZ_CFG	0x6E

#define T1_MODE_STAT_REG		0x11
#define T1_LINK_UP_MSK			BIT(0)

/* SQI defines */
#define LAN87XX_MAX_SQI			0x07

#define PHY_ID_LAN887X		(0x0007C1F2)
#define LAN887X_DEF_MASK	(0xFFFF)

#define PHY_ID_LAN887X_ALL	(0x0007C002)
#define PHY_ID_LAN887X_MSK	(0xfffff002)
#define PHY_ID_LAN887X_EXACT	(0xfffffff2)

#define IS_LAN887X_B0_PROTOS(id) (((id) & PHY_ID_LAN887X_EXACT) == PHY_ID_LAN887X_ALL)
#define IS_LAN887X_B0(id) (((id) & 0xfffffff2) == PHY_ID_LAN887X)

#define LAN887X_CHIPTOP_PMA_EXTABLE2_100	BIT(0)

/* Start:: DEV-0x1 Registers */
/* 100T1 PMA Commmon Registers */
#define		LAN887X_PMA_COMM_100T1_CTL_T1			(0x834)
#define		LAN887X_PMA_COMM_100T1_CTL_T1_MAS_SLV_CFG_VAL   BIT(14)
#define		LAN887X_PMA_COMM_100T1_CTL_T1_TYPE_MASK		(0xF)
#define		LAN887X_PMA_COMM_100T1_CTL_T1_TYPE_100		(0x0)
#define		LAN887X_PMA_COMM_100T1_CTL_T1_TYPE_1000		(0x1)

/* Start:: DEV-0x1 Registers */
/* 1000T1 PMA Registers */
#define		LAN887X_PMA_1000T1_DSP_PMA_CTL_REG	(0x810e)
#define		LAN887X_PMA_1000T1_DSP_PMA_LNK_SYNC	BIT(4)

/* t1_phy_port_afe_regs Registers */
#define LAN887X_T1_AFE				(0x8000)
#define LAN887X_T1_AFE_PORT_ZQCAL_OFFSET_REG	(LAN887X_T1_AFE | 0x93)
#define LAN887X_T1_AFE_PORT_TESTBUS_CTRL2_REG	(LAN887X_T1_AFE | 0x89)
#define LAN887X_T1_AFE_PORT_TESTBUS_CTRL4_REG	(LAN887X_T1_AFE | 0x8B)
#define LAN887X_T1_AFE_PORT_TX_AMPLT_1000T1_REG	(LAN887X_T1_AFE | 0xB0)
#define LAN887X_T1_AFE_PORT_TX_FIR_CTRL_STATUS	(LAN887X_T1_AFE | 0x94)
#define LAN887X_T1_AFE_PORT_COEFF_2_1_1000T1_REG (LAN887X_T1_AFE | 0x95)

/* t1_phy_common_afe_regs Registers */
#define LAN887X_COMMON_AFE_COMMON_BLOCK_CONTROL_2	(0xF001)

/* End:: DEV-0x1 Registers */

/* Start:: DEV-0x3 Registers */
/* 1000T1 PCS Regsiters */
#define        LAN887X_1000T1_PCS_CTL_REG		(0x900)
#define        LAN887X_1000T1_PCS_LPBK			BIT(14)

#define        LAN887X_1000T1_PCS_DBG_BUS_CFG		(0x802F)
/* End:: DEV-0x3 Registers */

/* Start:: DEV-0x7 Registers */
/* aneg_regs */
#define LAN887X_ANEG_VEND_DBG_CTRL_STAT_REG	(0x8013)
#define LAN887X_ANEG_VEND_DBG_CTRL_STAT_REG_LD_CONF_FAULT	BIT(10)
#define LAN887X_ANEG_VEND_DBG_CTRL_STAT_REG_LD_CONF_SLAVE	BIT(9)
#define LAN887X_ANEG_VEND_DBG_CTRL_STAT_REG_LD_CONF_MASTER	BIT(8)
/* End::   DEV-0x7 Registers */

/* Start:: DEV-0x1E Registers */
/* 1000T1 DSP Regsiters */
#define		LAN887X_1000T1_DSP_PROT_DIS_REG	        (0x825)
#define		LAN887X_1000T1_DSP_EDBG_G3_G0_MASK	(0x99f)
#define		LAN887X_1000T1_DSP_EDBG_G5_G4_MASK	(0x9a0)

/* SQI Registers*/
#define LAN887X_DSP_REGS_COEFF_MOD_CONFIG		(0x80d)
#define LAN887X_DSP_REGS_COEFF_MOD_CONFIG_DCQ_COEFF_EN	BIT(8)
#define LAN887X_DSP_REGS_DCQ_SQI_STATUS			(0x8b2)

/* CABLE DIAGONISTCS Registers */
#define LAN887X_DSP_REGS_MAX_PGA_GAIN_100		(0x44F)
#define LAN887X_DSP_REGS_MIN_PGA_GAIN_100		(0x450)
#define LAN887X_DSP_REGS_START_CBL_DIAG_100		(0x45A)
#define LAN887X_DSP_REGS_CBL_DIAG_TDR_THRESH_100	(0x45B)
#define LAN887X_DSP_REGS_CBL_DIAG_AGC_THRESH_100	(0x45C)
#define LAN887X_DSP_REGS_CBL_DIAG_MIN_WAIT_CONFIG_100   (0x45D)
#define LAN887X_DSP_REGS_CBL_DIAG_MAX_WAIT_CONFIG_100   (0x45E)
#define LAN887X_DSP_REGS_CBL_DIAG_CYC_CONFIG_100        (0x45F)
#define LAN887X_DSP_REGS_CBL_DIAG_TX_PULSE_CONFIG_100   (0x460)
#define LAN887X_DSP_REGS_CBL_DIAG_MIN_PGA_GAIN_100	(0x462)

/* Chiptop Common Registers */
#define        LAN887X_CHIPTOP_COMM_LED1_LED0                (0xc04)
#define        LAN887X_CHIPTOP_COMM_LED3_LED2                (0xc05)
#define        LAN887X_CHIPTOP_COMM_LED2_MASK                GENMASK(4, 0)

#define        LAN887X_CHIPTOP_LED_LINK_ACT_ANY_SPEED        0x0

/* MIS Regsiters */
#define        LAN887X_MIS_100T1_SMI_REG26		(0x1A)
#define        LAN887X_MIS_100T1_SMI_FORCE_TX_EN	BIT(9)
#define        LAN887X_MIS_100T1_SMI_HW_INIT_SEQ_EN	BIT(8)

#define        LAN887X_MIS_CFG_REG0			(0xa00)
#define        LAN887X_MIS_RCLKOUT_DIS			BIT(5)
#define        LAN887X_MIS_CFG_REG0_MAC_MAC_MODE_SEL	GENMASK(1, 0)
#define        LAN887X_MIS_CFG_REG0_MAC_MODE_RGMII	(0x01)
#define        LAN887X_MIS_CFG_REG0_MAC_MODE_SGMII	(0x03)

#define        LAN887X_MIS_TX_DLL_CFG_REG0		(0xa01)
#define        LAN887X_MIS_RX_DLL_CFG_REG1		(0xa02)
#define        LAN887X_MIS_DLL_DELAY_EN			BIT(15)
#define        LAN887X_MIS_DLL_EN			BIT(0)
#define        LAN887X_MIS_DLL_EN_	\
			(LAN887X_MIS_DLL_DELAY_EN |\
			 LAN887X_MIS_DLL_EN)

#define        LAN887X_MIS_CFG_REG2			(0xa03)
#define        LAN887X_MIS_CFG_REG2_FE_LPBK_EN		BIT(2)
#define        LAN887X_MIS_CFG_REG2_NE_LPBK_EN		BIT(1)

#define        LAN887X_MIS_PKT_STAT_REG0		(0xa06)
#define        LAN887X_MIS_PKT_STAT_REG1		(0xa07)
#define        LAN887X_MIS_PKT_STAT_REG3		(0xa09)
#define        LAN887X_MIS_PKT_STAT_REG4		(0xa0a)
#define        LAN887X_MIS_PKT_STAT_REG5		(0xa0b)
#define        LAN887X_MIS_PKT_STAT_REG6		(0xa0c)
#define        LAN887X_MIS_EPG_CFG1_REG			(0xa0d)

/* Common */
#define        LAN887X_COMM_PORT_INTC_REG		(0xc10)
#define        LAN887X_COMM_PORT_INTS_REG		(0xc11)

#define        LAN887X_COMM_PORT_INT_PHY_EN		BIT(15)
#define        LAN887X_COMM_PORT_INT_RECV_LPS_EN	BIT(5)
#define        LAN887X_COMM_PORT_INT_ANEG_EN		BIT(3)
#define        LAN887X_COMM_PORT_INT_MIS_EN		BIT(2)
#define        LAN887X_COMM_PORT_INT_S100		BIT(1)
#define        LAN887X_COMM_PORT_INT_S1000		BIT(0)

#define        LAN887X_COMM_PORT_INT_ALL (LAN887X_COMM_PORT_INT_PHY_EN |\
					  LAN887X_COMM_PORT_INT_MIS_EN |\
					  LAN887X_COMM_PORT_INT_S1000)

/* mx_chip_top_regs */
#define LAN887X_MX_CHIP_TOP_REG_INT_STS			(0xF000)
#define LAN887X_MX_CHIP_TOP_REG_INT_MSK			(0xF001)
#define LAN887X_MX_CHIP_TOP_REG_CONTROL1		(0xF002)
#define LAN887X_MX_CHIP_TOP_REG_CONTROL1_EVT_EN		BIT(8)
#define LAN887X_MX_CHIP_TOP_REG_CONTROL1_REF_CLK	BIT(9)
#define LAN887X_MX_CHIP_TOP_REG_CONTROL1_GPIO2_EN	BIT(5)

#define LAN887X_MX_CHIP_TOP_P1588_COM_INT_STS		BIT(8)
#define LAN887X_MX_CHIP_TOP_P1588_MOD_INT_STS		BIT(3)
#define LAN887X_MX_CHIP_TOP_T1_PHY_INT_MSK		BIT(2)
#define LAN887X_MX_CHIP_TOP_LINK_UP_MSK			BIT(1)
#define LAN887X_MX_CHIP_TOP_LINK_DOWN_MSK		BIT(0)

#define LAN887X_MX_CHIP_TOP_LINK_MSK	(LAN887X_MX_CHIP_TOP_LINK_UP_MSK |\
					 LAN887X_MX_CHIP_TOP_LINK_DOWN_MSK)

#define LAN887X_MX_CHIP_TOP_ALL_MSK	(LAN887X_MX_CHIP_TOP_T1_PHY_INT_MSK |\
					 LAN887X_MX_CHIP_TOP_LINK_MSK |\
					 LAN887X_MX_CHIP_TOP_P1588_MOD_INT_STS)

#define LAN887X_MX_CHIP_TOP_REG_HARD_RST		(0xF03E)
#define LAN887X_MX_CHIP_TOP_REG_SOFT_RST		(0xF03F)
#define LAN887X_MX_CHIP_TOP_RESET_			BIT(0)

#define LAN887X_MX_CHIP_TOP_REG_SGMII_CTL		(0xF01A)
#define LAN887X_MX_CHIP_TOP_REG_SGMII_MUX_EN		BIT(0)

#define LAN887X_MX_CHIP_TOP_SGMII_PCS_CFG		(0xF034)
#define LAN887X_MX_CHIP_TOP_SGMII_PCS_ENA		BIT(9)

/* End:: DEV-0x1E Registers */

/* PTP PRT Registers */
/* PTP Command Control Register */
#define LAN887X_PTP_CMD_CTL				(0xE000)
#define LAN887X_PTP_CMD_CTL_PTP_LTC_STEP_NANOSECONDS	BIT(6)
#define LAN887X_PTP_CMD_CTL_PTP_LTC_STEP_SECONDS	BIT(5)
#define LAN887X_PTP_CMD_CTL_CLOCK_LOAD			BIT(4)
#define LAN887X_PTP_CMD_CTL_CLOCK_READ			BIT(3)
#define LAN887X_PTP_CMD_CTL_EN				BIT(1)
#define LAN887X_PTP_CMD_CTL_DIS				BIT(0)

#define LAN887X_PTP_REF_CLK_CFG				(0xE002)
#define LAN887X_PTP_REF_CLK_SRC_250MHZ                 (0x0)
#define LAN887X_PTP_REF_CLK_SRC_200MHZ                 (0x1 << 10)
#define LAN887X_PTP_REF_CLK_SRC_125MHZ                 (0x2 << 10)
#define LAN887X_PTP_REF_CLK_SRC_RX                     (0x3 << 10)
#define LAN887X_PTP_REF_CLK_SRC_EXT                    (0x4 << 10)
#define LAN887X_PTP_REF_CLK_SRC_SGMII_RX               (0x5 << 10)
#define LAN887X_PTP_REF_CLK_PERIOD_OVERRIDE		BIT(9)
#define LAN887X_PTP_REF_CLK_PERIOD_MSK			GENMASK(8, 0)
/* Period is 8 for 125Mhz clock */
#define LAN887X_PTP_REF_CLK_PERIOD			(4)
#define LAN887X_PTP_REF_CLK_CFG_SET    \
		(LAN887X_PTP_REF_CLK_SRC_250MHZ |\
		 LAN887X_PTP_REF_CLK_PERIOD_OVERRIDE |\
		 LAN887X_PTP_REF_CLK_PERIOD)

/* Represents 1ppm adjustment in 2^32 format with
 * each nsec contains 8 clock cycles in 125MHz.
 * The value is calculated as following: (1/1000000)/((2^-32)/8)
 */
#define LAN887X_1PPM_FORMAT                 17179

/* PTP LTC Seconds Registers */
#define LAN887X_PTP_LTC_SEC_HI				(0xE005)
#define LAN887X_PTP_LTC_SEC_MID				(0xE006)
#define LAN887X_PTP_LTC_SEC_LO				(0xE007)

/* PTP LTC Nanoseconds Registers */
#define LAN887X_PTP_LTC_NS_HI				(0xE008)
#define LAN887X_PTP_LTC_NS_LO				(0xE009)

/* PTP LTC Read seconds registers */
#define LAN887X_PTP_LTC_READ_SEC_HI			(0xE029)
#define LAN887X_PTP_LTC_READ_SEC_MID			(0xE02A)
#define LAN887X_PTP_LTC_READ_SEC_LO			(0xE02B)

/* PTP LTC Read nanoseconds registers */
#define LAN887X_PTP_LTC_READ_NS_HI			(0xE02C)
#define LAN887X_PTP_LTC_READ_NS_LO			(0xE02D)

/* PTP LTC Hard Reset Register */
#define LAN887X_PTP_LTC_HARD_RESET			(0xE03F)
#define LAN887X_PTP_LTC_HARD_RESET_			BIT(0)

/* PTP LTC Adjustment Registers */
#define LAN887X_PTP_LTC_RATE_ADJ_HI			(0xE00C)
#define LAN887X_PTP_LTC_RATE_ADJ_HI_DIR			BIT(15)
#define LAN887X_PTP_LTC_RATE_ADJ_LO			(0xE00D)

/* PTP Step Adjustment registers */
#define LAN887X_PTP_LTC_STEP_ADJ_HI			(0xE012)
#define LAN887X_PTP_LTC_STEP_ADJ_HI_DIR			BIT(15)
#define LAN887X_PTP_LTC_STEP_ADJ_LO			(0xE013)

/* PTP Operational modes */
#define LAN887X_PTP_OP_MODE				(0xE041)
#define LAN887X_PTP_OP_MODE_DIS				(0)
#define LAN887X_PTP_OP_MODE_STANDALONE			(1)

/* PTP Latency corrections */
#define LAN887X_PTP_LATENCY_CORRECTION_CTL		(0xE044)
#define LAN887X_PTP_PREDICTOR_EN			BIT(6)
#define LAN887X_PTP_BRPHY_TX_PREDICTOR_DIS		BIT(5)
#define LAN887X_PTP_BRPHY_RX_PREDICTOR_DIS		BIT(4)
#define LAN887X_PTP_SW_CTRL_TX_LINK_LAT			BIT(3)
#define LAN887X_PTP_SW_CTRL_RX_LINK_LAT			BIT(2)
#define LAN887X_PTP_TX_PRED_DIS				BIT(1)
#define LAN887X_PTP_RX_PRED_DIS				BIT(0)

//0x43
#define LAN887X_PTP_LATENCY_SETTING	(LAN887X_PTP_PREDICTOR_EN | \
					 LAN887X_PTP_TX_PRED_DIS | \
					 LAN887X_PTP_RX_PRED_DIS)

/* UNG_MOLINEUX-637 - workaround is subtract 4 from 0xe846 to 0xe8AE */
/* PTP RX Parsing Configuration Register */
#define LAN887X_PTP_RX_PARSE_CONFIG			(0xE842)
/* PTP TX Parsing Configuration Register */
#define LAN887X_PTP_TX_PARSE_CONFIG			(0xE882)
//Bit configurations
#define LAN887X_PTP_PARSE_CONFIG_LAYER2_EN		BIT(0)
#define LAN887X_PTP_PARSE_CONFIG_IPV4_EN		BIT(1)
#define LAN887X_PTP_PARSE_CONFIG_IPV6_EN		BIT(2)

/* PTP RX L2 Address Enable Register */
#define LAN887X_PTP_RX_PARSE_L2_ADDR_EN			(0xE844)
/* PTP TX L2 Address Enable Register */
#define LAN887X_PTP_TX_PARSE_L2_ADDR_EN			(0xE884)

/* PTP TX?RX IPv4 Address Enable Register */
#define LAN887X_PTP_RX_PARSE_IPV4_ADDR_EN		(0xE845)
#define LAN887X_PTP_TX_PARSE_IPV4_ADDR_EN		(0xE885)

/* PTP TX/RX Timestamp Config */
#define PTP_RX_TIMESTAMP_CONFIG				(0xE84E)
#define PTP_RX_TIMESTAMP_CONFIG_PTP_FCS_DIS		BIT(0)

#define LAN887X_PTP_RX_VERSION		(0xE848)
#define LAN887X_PTP_TX_VERSION		(0xE888)
#define PTP_MAX_VERSION(x)		(((x) & GENMASK(7, 0)) << 8)
#define PTP_MIN_VERSION(x)		((x) & GENMASK(7, 0))

/* PTP Tx Timestamp Config */
#define PTP_TX_TIMESTAMP_CONFIG				(0xE88E)
#define PTP_TX_TIMESTAMP_CONFIG_PTP_FCS_DIS		BIT(0)

/* RX and TX Message Header 2 Register */
#define LAN887X_PTP_RX_MSG_HEADER2			(0xE859)
#define LAN887X_PTP_TX_MSG_HEADER2			(0xE899)

/* PTP RX Ingress Time Nanoseconds Registers */
#define LAN887X_PTP_RX_INGRESS_NS_HI			(0xE854)
#define LAN887X_PTP_RX_INGRESS_NS_HI_PTP_RX_TS_VALID	BIT(15)

#define LAN887X_PTP_RX_INGRESS_NS_LO			(0xE855)

/* PTP RX Ingress Time Seconds Registers */
#define LAN887X_PTP_RX_INGRESS_SEC_HI			(0xE856)
#define LAN887X_PTP_RX_INGRESS_SEC_LO			(0xE857)

#define LAN887X_PTP_RX_TIMESTAMP_EN			(0xE84D)
#define LAN887X_PTP_TX_TIMESTAMP_EN			(0xE88D)
#define PTP_TIMESTAMP_EN_SYNC				BIT(0)
#define PTP_TIMESTAMP_EN_DREQ				BIT(1)
#define PTP_TIMESTAMP_EN_PDREQ				BIT(2)
#define PTP_TIMESTAMP_EN_PDRES				BIT(3)
#define PTP_TIMESTAMP_EN_ALL_	\
		(PTP_TIMESTAMP_EN_SYNC |\
		 PTP_TIMESTAMP_EN_DREQ |\
		 PTP_TIMESTAMP_EN_PDREQ |\
		 PTP_TIMESTAMP_EN_PDRES)

/* PTP TX Modification register */
#define LAN887X_PTP_TX_MOD					(0xE88F)
#define LAN887X_PTP_TX_MOD_PTP_SYNC_TS_INSERT			BIT(12)
#define LAN887X_PTP_TX_MOD_PTP_FU_TS_INSERT			BIT(11)

/* PTP TX Egress Time Nanoseconds Registers */
#define LAN887X_PTP_TX_EGRESS_NS_HI			(0xE894)
#define LAN887X_PTP_TX_EGRESS_NS_HI_PTP_TX_TS_VALID	BIT(15)
#define LAN887X_PTP_TX_EGRESS_NS_LO			(0xE895)

/* PTP TX Egress Time Seconds Registers */
#define LAN887X_PTP_TX_EGRESS_SEC_HI			(0xE896)
#define LAN887X_PTP_TX_EGRESS_SEC_LO			(0xE897)

/* PTP TSU General configuration register */
#define LAN887X_TSU_GEN_CONFIG				(0xE8C0)
#define LAN887X_TSU_GEN_CFG_TSU_EN			BIT(0)

/* PTP TSU Hard reset register */
#define LAN887X_TSU_HARD_RESET				(0xE8C1)
#define LAN887X_PTP_TSU_HARD_RESET			BIT(0)

#define LAN887X_PTP_RX_LATENCY_100_REG			(0xE826)
#define LAN887X_PTP_TX_LATENCY_100_REG			(0xE827)
#define LAN887X_PTP_RX_LATENCY_1000_REG			(0xE828)
#define LAN887X_PTP_TX_LATENCY_1000_REG			(0xE829)

#define LAN887X_PTP_RX_LATENCY_1000			(2573)
#define LAN887X_PTP_TX_LATENCY_1000			(2573)

#define LAN887X_PTP_RX_LATENCY_100			(700)
#define LAN887X_PTP_TX_LATENCY_100			(1079)

#define FIFO_SIZE					8
#define LAN887X_MAX_ADJ					31249999

/**********************************/
//     PTP PRT Registers
/**********************************/

/* PTP Interrupt Enable Register */
#define LAN887X_PTP_INT_EN				(0xE800)
/* PTP Interrupt Status Register */
#define LAN887X_PTP_INT_STS				(0xE801)
#define LAN887X_PTP_INT_TX_TS_OVRFL_EN			BIT(3)
#define LAN887X_PTP_INT_TX_TS_EN			BIT(2)
#define LAN887X_PTP_INT_RX_TS_OVRFL_EN			BIT(1)
#define LAN887X_PTP_INT_RX_TS_EN			BIT(0)
#define LAN887X_PTP_INT_ALL_MSK		(LAN887X_PTP_INT_TX_TS_OVRFL_EN | \
					 LAN887X_PTP_INT_TX_TS_EN | \
					 LAN887X_PTP_INT_RX_TS_OVRFL_EN |\
					 LAN887X_PTP_INT_RX_TS_EN)

/* PTP Capture Information Register */
#define LAN887X_MX_PTP_PRT_CAP_INFO_REG			(0xE82E)
#define LAN887X_MX_PTP_PRT_TX_TS_CNT			GENMASK(11, 8)
#define LAN887X_MX_PTP_PRT_TX_TS_CNT_GET(v)    \
		(((v) & LAN887X_MX_PTP_PRT_TX_TS_CNT) >> 8)
#define LAN887X_MX_PTP_PRT_RX_TS_CNT			GENMASK(3, 0)
#define LAN887X_MX_PTP_PRT_RX_TS_CNT_GET(v)    \
		((v) & LAN887X_MX_PTP_PRT_RX_TS_CNT)

/* PTP GPIO Registers */
#define LAN887X_PTP_CLOCK_TARGET_SEC_HI_X(event)	(event ? 0xE01F : 0xE015)
#define LAN887X_PTP_CLOCK_TARGET_SEC_LO_X(event)	(event ? 0xE020 : 0xE016)
#define LAN887X_PTP_CLOCK_TARGET_NS_HI_X(event)		(event ? 0xE021 : 0xE017)
#define LAN887X_PTP_CLOCK_TARGET_NS_LO_X(event)		(event ? 0xE022 : 0xE018)

#define LAN887X_PTP_CLOCK_TARGET_RELOAD_SEC_HI_X(event)	(event ? 0xE023 : 0xE019)
#define LAN887X_PTP_CLOCK_TARGET_RELOAD_SEC_LO_X(event)	(event ? 0xE024 : 0xE01A)
#define LAN887X_PTP_CLOCK_TARGET_RELOAD_NS_HI_X(event)	(event ? 0xE025 : 0xE01B)
#define LAN887X_PTP_CLOCK_TARGET_RELOAD_NS_LO_X(event)	(event ? 0xE026 : 0xE01C)

#define LAN887X_PTP_GENERAL_CONFIG			0xE001
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_X_MASK_(event) \
					((event) ? GENMASK(11, 8) : GENMASK(7, 4))

#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_X_SET_(event, value) \
					(((value) & 0xF) << (4 + ((event) << 2)))
#define LAN887X_PTP_GENERAL_CONFIG_RELOAD_ADD_X_(event)	((event) ? BIT(2) : BIT(0))
#define LAN887X_PTP_GENERAL_CONFIG_POLARITY_X_(event)	((event) ? BIT(3) : BIT(1))

#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_200MS_     13
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_100MS_     12
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_50MS_      11
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_10MS_      10
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_5MS_       9
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_1MS_       8
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_500US_     7
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_100US_     6
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_50US_      5
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_10US_      4
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_5US_       3
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_1US_       2
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_500NS_     1
#define LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_100NS_     0

#define LAN887X_N_GPIO				4
#define LAN887X_N_PEROUT			2
#define LAN887X_EVENT_A				0
#define LAN887X_EVENT_B				1
#define LAN887X_BUFFER_TIME			2

/**
 * PHY states in TC10 Mode are
 * 0: PHY start
 * 1: PHY Normal
 * 2: SLEEP ACK - When PHY receives LPS(Low Power Sleep) signal from Link partner
 * 3: SLEEP_REQ - When Sleep request is asserted on PHY.
 * 4: SLEEP_FAIL - If Sleep handshaking is not done before sleep_req_timer timeout.
 * 5: SLEEP_SILENT - If the PHY detects that it has sent and received LPS commands.
 * 6: PHY_SLEEP - PHY is in SLEEP state.
 * 7: PHY_UNKNOWN state
 *
 */
#define LAN887X_TC10_DEV30_COMMON_MISC34		0x0C26
#define LAN887X_TC10_DEV30_COMMON_MISC34_CUR_STATE	GENMASK(2, 0)

#define LAN887X_MISC_REGS_REG16					0x10
#define LAN887X_MISC_REGS_REG16_IGNORE_IDLE_WITH_WUR_LPS	BIT(5)

#define LAN887X_MISC_REGS_MISC37				0x0125
#define LAN887X_MISC_REGS_MISC37_EN_TC10_SLEEP_SILENT		BIT(5)

#define LAN887X_DEV30_COMMON_TC10_REG_REG15		0x0C20
#define LAN887X_DEV30_COMMON_TC10_REG_REG15_WK_OUT_PIN_REQ	BIT(0)

#define LAN887X_DEV30_COMMON_TC10_REG_REG16		0x0C21
#define LAN887X_DEV30_COMMON_TC10_REG_REG16_RW_SEND_LPS	BIT(12)
#define LAN887X_DEV30_COMMON_TC10_REG_REG16_RW_SEND_WUR	BIT(13)

#define LAN887X_DEV30_COMMON_TC10_MISC32			0x0C24
#define LAN887X_DEV30_COMMON_TC10_MISC32_WK_IN_EN		BIT(0)
#define LAN887X_DEV30_COMMON_TC10_MISC32_INH_EN_TC10	BIT(1)
#define LAN887X_DEV30_COMMON_TC10_MISC32_SLEEP_EN		BIT(2)
#define LAN887X_DEV30_COMMON_TC10_MISC32_ROSC_EN		BIT(5)
#define LAN887X_DEV30_COMMON_TC10_MISC32_ROSC_STS		BIT(6)
#define LAN887X_DEV30_COMMON_TC10_MISC32_WK_OUT_AUTO_FWD_EN BIT(7)
#define LAN887X_DEV30_COMMON_TC10_MISC32_VBAT_COM_WR	BIT(15)
#define LAN887X_DEV30_COMMON_TC10_MISC32_VAL \
					(LAN887X_DEV30_COMMON_TC10_MISC32_WK_IN_EN |\
					 LAN887X_DEV30_COMMON_TC10_MISC32_INH_EN_TC10 |\
					 LAN887X_DEV30_COMMON_TC10_MISC32_SLEEP_EN |\
					 LAN887X_DEV30_COMMON_TC10_MISC32_ROSC_EN |\
					 LAN887X_DEV30_COMMON_TC10_MISC32_ROSC_STS |\
					 LAN887X_DEV30_COMMON_TC10_MISC32_WK_OUT_AUTO_FWD_EN |\
					 LAN887X_DEV30_COMMON_TC10_MISC32_VBAT_COM_WR)

#define LAN887X_DEV30_COMMON_TC10_MISC33			0x0C25
#define LAN887X_DEV30_COMMON_TC10_MISC33_WK_OUT_LEN	0x4F
#define LAN887X_DEV30_COMMON_TC10_MISC33_WK_DEB_VAL	0x27

#define LAN887X_DEV30_COMMON_TC10_MISC36			0x0C27
#define LAN887X_DEV30_COMMON_TC10_MISC36_WUP_DEB_VAL	0x10
#define LAN887X_DEV30_COMMON_TC10_MISC36_WK_MDI_EN		BIT(8)
#define LAN887X_DEV30_COMMON_TC10_MISC36_WUP_AUTO_FWD_EN	BIT(12)
#define LAN887X_DEV30_COMMON_TC10_MISC36_WUP_DET_FILT_LEN	(2 << 13)
#define LAN887X_DEV30_COMMON_TC10_MISC36_VBAT_PORT_WR	BIT(15)
#define LAN887X_DEV30_COMMON_TC10_MISC36_VAL \
				(LAN887X_DEV30_COMMON_TC10_MISC36_WK_MDI_EN |\
				 LAN887X_DEV30_COMMON_TC10_MISC36_WUP_AUTO_FWD_EN | \
				 LAN887X_DEV30_COMMON_TC10_MISC36_WUP_DEB_VAL |\
				 LAN887X_DEV30_COMMON_TC10_MISC36_WUP_DET_FILT_LEN |\
				 LAN887X_DEV30_COMMON_TC10_MISC36_VBAT_PORT_WR)

#define LAN887X_DEV30_COMMON_TC10_MISC37			0x0C28
#define LAN887X_DEV30_COMMON_TC10_MISC37_FORCE_SLEEP	BIT(0)

#define LAN887X_DEV30_COMMON_TC10_MISC46			0x0C29
#define LAN887X_DEV30_COMMON_TC10_MISC46_WK_PORT_TEST_MASK	GENMASK(7, 0)
#define LAN887X_DEV30_COMMON_TC10_MISC46_WK_PORT_TEST_VAL	0x04

#define LAN887X_DEV30_COMMON_TC10_SENDZ_MINWAIT_TMR_CFG		0xC2F

/* LAN887X End of mcro definitions */

#define DRIVER_AUTHOR	"Nisar Sayed <nisar.sayed@microchip.com>"
#define DRIVER_DESC	"Microchip LAN87XX/LAN937x T1 PHY driver"

struct access_ereg_val {
	u8  mode;
	u8  bank;
	u8  offset;
	u16 val;
	u16 mask;
};

static int lan937x_dsp_workaround(struct phy_device *phydev, u16 ereg, u8 bank)
{
	u8 prev_bank;
	int rc = 0;
	u16 val;

	mutex_lock(&phydev->lock);
	/* Read previous selected bank */
	rc = phy_read(phydev, LAN87XX_EXT_REG_CTL);
	if (rc < 0)
		goto out_unlock;

	/* store the prev_bank */
	prev_bank = FIELD_GET(LAN87XX_REG_BANK_SEL_MASK, rc);

	if (bank != prev_bank && bank == PHYACC_ATTR_BANK_DSP) {
		val = ereg & ~LAN87XX_REG_ADDR_MASK;

		val &= ~LAN87XX_EXT_REG_CTL_WR_CTL;
		val |= LAN87XX_EXT_REG_CTL_RD_CTL;

		/* access twice for DSP bank change,dummy access */
		rc = phy_write(phydev, LAN87XX_EXT_REG_CTL, val);
	}

out_unlock:
	mutex_unlock(&phydev->lock);

	return rc;
}

static int access_ereg(struct phy_device *phydev, u8 mode, u8 bank,
		       u8 offset, u16 val)
{
	u16 ereg = 0;
	int rc = 0;

	if (mode > PHYACC_ATTR_MODE_WRITE || bank > PHYACC_ATTR_BANK_MAX)
		return -EINVAL;

	if (bank == PHYACC_ATTR_BANK_SMI) {
		if (mode == PHYACC_ATTR_MODE_WRITE)
			rc = phy_write(phydev, offset, val);
		else
			rc = phy_read(phydev, offset);
		return rc;
	}

	if (mode == PHYACC_ATTR_MODE_WRITE) {
		ereg = LAN87XX_EXT_REG_CTL_WR_CTL;
		rc = phy_write(phydev, LAN87XX_EXT_REG_WR_DATA, val);
		if (rc < 0)
			return rc;
	} else {
		ereg = LAN87XX_EXT_REG_CTL_RD_CTL;
	}

	ereg |= (bank << 8) | offset;

	/* DSP bank access workaround for lan937x */
	if (phydev->phy_id == PHY_ID_LAN937X) {
		rc = lan937x_dsp_workaround(phydev, ereg, bank);
		if (rc < 0)
			return rc;
	}

	rc = phy_write(phydev, LAN87XX_EXT_REG_CTL, ereg);
	if (rc < 0)
		return rc;

	if (mode == PHYACC_ATTR_MODE_READ)
		rc = phy_read(phydev, LAN87XX_EXT_REG_RD_DATA);

	return rc;
}

static int access_ereg_modify_changed(struct phy_device *phydev,
				      u8 bank, u8 offset, u16 val, u16 mask)
{
	int new = 0, rc = 0;

	if (bank > PHYACC_ATTR_BANK_MAX)
		return -EINVAL;

	rc = access_ereg(phydev, PHYACC_ATTR_MODE_READ, bank, offset, val);
	if (rc < 0)
		return rc;

	new = val | (rc & (mask ^ 0xFFFF));
	rc = access_ereg(phydev, PHYACC_ATTR_MODE_WRITE, bank, offset, new);

	return rc;
}

static int access_smi_poll_timeout(struct phy_device *phydev,
				   u8 offset, u16 mask, u16 clr)
{
	int val;

	return phy_read_poll_timeout(phydev, offset, val, (val & mask) == clr,
				     150, 30000, true);
}

static int lan87xx_config_rgmii_delay(struct phy_device *phydev)
{
	int rc;

	if (!phy_interface_is_rgmii(phydev))
		return 0;

	rc = access_ereg(phydev, PHYACC_ATTR_MODE_READ,
			 PHYACC_ATTR_BANK_MISC, LAN87XX_CTRL_1, 0);
	if (rc < 0)
		return rc;

	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_RGMII:
		rc &= ~LAN87XX_MASK_RGMII_TXC_DLY_EN;
		rc &= ~LAN87XX_MASK_RGMII_RXC_DLY_EN;
		break;
	case PHY_INTERFACE_MODE_RGMII_ID:
		rc |= LAN87XX_MASK_RGMII_TXC_DLY_EN;
		rc |= LAN87XX_MASK_RGMII_RXC_DLY_EN;
		break;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		rc &= ~LAN87XX_MASK_RGMII_TXC_DLY_EN;
		rc |= LAN87XX_MASK_RGMII_RXC_DLY_EN;
		break;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		rc |= LAN87XX_MASK_RGMII_TXC_DLY_EN;
		rc &= ~LAN87XX_MASK_RGMII_RXC_DLY_EN;
		break;
	default:
		return 0;
	}

	return access_ereg(phydev, PHYACC_ATTR_MODE_WRITE,
			   PHYACC_ATTR_BANK_MISC, LAN87XX_CTRL_1, rc);
}

static int lan87xx_phy_init(struct phy_device *phydev)
{
	static const struct access_ereg_val init[] = {
		/* TXPD/TXAMP6 Configs */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_AFE,
		  T1_AFE_PORT_CFG1_REG,       0x002D,  0 },
		/* HW_Init Hi and Force_ED */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_SMI,
		  T1_POWER_DOWN_CONTROL_REG,  0x0308,  0 },
		/* Equalizer Full Duplex Freeze - T1 Slave */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_EQ_FD_STG1_FRZ_CFG,     0x0002,  0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_EQ_FD_STG2_FRZ_CFG,     0x0002,  0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_EQ_FD_STG3_FRZ_CFG,     0x0002,  0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_EQ_FD_STG4_FRZ_CFG,     0x0002,  0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_EQ_WT_FD_LCK_FRZ_CFG,    0x0002,  0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_PST_EQ_LCK_STG1_FRZ_CFG, 0x0002,  0 },
		/* Slave Full Duplex Multi Configs */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_SLV_FD_MULT_CFG_REG,     0x0D53,  0 },
		/* CDR Pre and Post Lock Configs */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_CDR_CFG_PRE_LOCK_REG,    0x0AB2,  0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_CDR_CFG_POST_LOCK_REG,   0x0AB3,  0 },
		/* Lock Stage 2-3 Multi Factor Config */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_LCK_STG2_MUFACT_CFG_REG, 0x0AEA,  0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_LCK_STG3_MUFACT_CFG_REG, 0x0AEB,  0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_POST_LCK_MUFACT_CFG_REG, 0x0AEB,  0 },
		/* Pointer delay */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_RX_FIFO_CFG_REG, 0x1C00, 0 },
		/* Tx iir edits */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1000, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1861, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1061, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1922, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1122, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1983, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1183, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1944, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1144, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x18c5, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x10c5, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1846, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1046, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1807, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1007, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1808, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1008, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1809, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1009, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x180A, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x100A, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x180B, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x100B, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x180C, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x100C, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x180D, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x100D, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x180E, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x100E, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x180F, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x100F, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1810, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1010, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1811, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1011, 0 },
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_TX_LPF_FIR_CFG_REG, 0x1000, 0 },
		/* Setup SQI measurement */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_COEF_CLK_PWR_DN_CFG,	0x16d6, 0 },
		/* SQI enable */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_SQI_CONFIG_REG,		0x9572, 0 },
		/* SQI select mode 5 */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_SQI_CONFIG2_REG,		0x0001, 0 },
		/* Throws the first SQI reading */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP,
		  T1_COEF_RW_CTL_CFG,		0x0301,	0 },
		{ PHYACC_ATTR_MODE_READ, PHYACC_ATTR_BANK_DSP,
		  T1_DCQ_SQI_REG,		0,	0 },
		/* Flag LPS and WUR as idle errors */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_SMI,
		  T1_MDIO_CONTROL2_REG,		0x0014, 0 },
		/* HW_Init toggle, undo force ED, TXPD off */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_SMI,
		  T1_POWER_DOWN_CONTROL_REG,	0x0200, 0 },
		/* Reset PCS to trigger hardware initialization */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_SMI,
		  T1_MDIO_CONTROL2_REG,		0x0094, 0 },
		/* Poll till Hardware is initialized */
		{ PHYACC_ATTR_MODE_POLL, PHYACC_ATTR_BANK_SMI,
		  T1_MDIO_CONTROL2_REG,		0x0080, 0 },
		/* Tx AMP - 0x06  */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_AFE,
		  T1_AFE_PORT_CFG1_REG,		0x000C, 0 },
		/* Read INTERRUPT_SOURCE Register */
		{ PHYACC_ATTR_MODE_READ, PHYACC_ATTR_BANK_SMI,
		  T1_INTERRUPT_SOURCE_REG,	0,	0 },
		/* Read INTERRUPT_SOURCE Register */
		{ PHYACC_ATTR_MODE_READ, PHYACC_ATTR_BANK_MISC,
		  T1_INTERRUPT2_SOURCE_REG,	0,	0 },
		/* HW_Init Hi */
		{ PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_SMI,
		  T1_POWER_DOWN_CONTROL_REG,	0x0300, 0 },
	};
	int rc, i;

	/* phy Soft reset */
	rc = genphy_soft_reset(phydev);
	if (rc < 0)
		return rc;

	/* PHY Initialization */
	for (i = 0; i < ARRAY_SIZE(init); i++) {
		if (init[i].mode == PHYACC_ATTR_MODE_POLL &&
		    init[i].bank == PHYACC_ATTR_BANK_SMI) {
			rc = access_smi_poll_timeout(phydev,
						     init[i].offset,
						     init[i].val,
						     init[i].mask);
		} else {
			rc = access_ereg(phydev, init[i].mode, init[i].bank,
					 init[i].offset, init[i].val);
		}
		if (rc < 0)
			return rc;
	}

	return lan87xx_config_rgmii_delay(phydev);
}

static int lan87xx_phy_config_intr(struct phy_device *phydev)
{
	int rc, val = 0;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		/* clear all interrupt */
		rc = phy_write(phydev, LAN87XX_INTERRUPT_MASK, val);
		if (rc < 0)
			return rc;

		rc = phy_read(phydev, LAN87XX_INTERRUPT_SOURCE);
		if (rc < 0)
			return rc;

		rc = access_ereg(phydev, PHYACC_ATTR_MODE_WRITE,
				 PHYACC_ATTR_BANK_MISC,
				 LAN87XX_INTERRUPT_MASK_2, val);
		if (rc < 0)
			return rc;

		rc = access_ereg(phydev, PHYACC_ATTR_MODE_READ,
				 PHYACC_ATTR_BANK_MISC,
				 LAN87XX_INTERRUPT_SOURCE_2, 0);
		if (rc < 0)
			return rc;

		/* enable link down and comm ready interrupt */
		val = LAN87XX_MASK_LINK_DOWN;
		rc = phy_write(phydev, LAN87XX_INTERRUPT_MASK, val);
		if (rc < 0)
			return rc;

		val = LAN87XX_MASK_COMM_RDY;
		rc = access_ereg(phydev, PHYACC_ATTR_MODE_WRITE,
				 PHYACC_ATTR_BANK_MISC,
				 LAN87XX_INTERRUPT_MASK_2, val);
	} else {
		rc = phy_write(phydev, LAN87XX_INTERRUPT_MASK, val);
		if (rc < 0)
			return rc;

		rc = phy_read(phydev, LAN87XX_INTERRUPT_SOURCE);
		if (rc < 0)
			return rc;

		rc = access_ereg(phydev, PHYACC_ATTR_MODE_WRITE,
				 PHYACC_ATTR_BANK_MISC,
				 LAN87XX_INTERRUPT_MASK_2, val);
		if (rc < 0)
			return rc;

		rc = access_ereg(phydev, PHYACC_ATTR_MODE_READ,
				 PHYACC_ATTR_BANK_MISC,
				 LAN87XX_INTERRUPT_SOURCE_2, 0);
	}

	return rc < 0 ? rc : 0;
}

static irqreturn_t lan87xx_handle_interrupt(struct phy_device *phydev)
{
	int irq_status;

	irq_status  = access_ereg(phydev, PHYACC_ATTR_MODE_READ,
				  PHYACC_ATTR_BANK_MISC,
				  LAN87XX_INTERRUPT_SOURCE_2, 0);
	if (irq_status < 0) {
		phy_error(phydev);
		return IRQ_NONE;
	}

	irq_status = phy_read(phydev, LAN87XX_INTERRUPT_SOURCE);
	if (irq_status < 0) {
		phy_error(phydev);
		return IRQ_NONE;
	}

	if (irq_status == 0)
		return IRQ_NONE;

	phy_trigger_machine(phydev);

	return IRQ_HANDLED;
}

static int lan87xx_config_init(struct phy_device *phydev)
{
	int rc = lan87xx_phy_init(phydev);

	return rc < 0 ? rc : 0;
}

static int microchip_cable_test_start_common(struct phy_device *phydev)
{
	int bmcr, bmsr, ret;

	/* If auto-negotiation is enabled, but not complete, the cable
	 * test never completes. So disable auto-neg.
	 */
	bmcr = phy_read(phydev, MII_BMCR);
	if (bmcr < 0)
		return bmcr;

	bmsr = phy_read(phydev, MII_BMSR);

	if (bmsr < 0)
		return bmsr;

	if (bmcr & BMCR_ANENABLE) {
		ret =  phy_modify(phydev, MII_BMCR, BMCR_ANENABLE, 0);
		if (ret < 0)
			return ret;
		ret = genphy_soft_reset(phydev);
		if (ret < 0)
			return ret;
	}

	/* If the link is up, allow it some time to go down */
	if (bmsr & BMSR_LSTATUS)
		msleep(1500);

	return 0;
}

static int lan87xx_cable_test_start(struct phy_device *phydev)
{
	static const struct access_ereg_val cable_test[] = {
		/* min wait */
		{PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP, 93,
		 0, 0},
		/* max wait */
		{PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP, 94,
		 10, 0},
		/* pulse cycle */
		{PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP, 95,
		 90, 0},
		/* cable diag thresh */
		{PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP, 92,
		 60, 0},
		/* max gain */
		{PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP, 79,
		 31, 0},
		/* clock align for each iteration */
		{PHYACC_ATTR_MODE_MODIFY, PHYACC_ATTR_BANK_DSP, 55,
		 0, 0x0038},
		/* max cycle wait config */
		{PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP, 94,
		 70, 0},
		/* start cable diag*/
		{PHYACC_ATTR_MODE_WRITE, PHYACC_ATTR_BANK_DSP, 90,
		 1, 0},
	};
	int rc, i;

	rc = microchip_cable_test_start_common(phydev);
	if (rc < 0)
		return rc;

	/* start cable diag */
	/* check if part is alive - if not, return diagnostic error */
	rc = access_ereg(phydev, PHYACC_ATTR_MODE_READ, PHYACC_ATTR_BANK_SMI,
			 0x00, 0);
	if (rc < 0)
		return rc;

	/* master/slave specific configs */
	rc = access_ereg(phydev, PHYACC_ATTR_MODE_READ, PHYACC_ATTR_BANK_SMI,
			 0x0A, 0);
	if (rc < 0)
		return rc;

	if ((rc & 0x4000) != 0x4000) {
		/* DUT is Slave */
		rc = access_ereg_modify_changed(phydev, PHYACC_ATTR_BANK_AFE,
						0x0E, 0x5, 0x7);
		if (rc < 0)
			return rc;
		rc = access_ereg_modify_changed(phydev, PHYACC_ATTR_BANK_SMI,
						0x1A, 0x8, 0x8);
		if (rc < 0)
			return rc;
	} else {
		/* DUT is Master */
		rc = access_ereg_modify_changed(phydev, PHYACC_ATTR_BANK_SMI,
						0x10, 0x8, 0x40);
		if (rc < 0)
			return rc;
	}

	for (i = 0; i < ARRAY_SIZE(cable_test); i++) {
		if (cable_test[i].mode == PHYACC_ATTR_MODE_MODIFY) {
			rc = access_ereg_modify_changed(phydev,
							cable_test[i].bank,
							cable_test[i].offset,
							cable_test[i].val,
							cable_test[i].mask);
			/* wait 50ms */
			msleep(50);
		} else {
			rc = access_ereg(phydev, cable_test[i].mode,
					 cable_test[i].bank,
					 cable_test[i].offset,
					 cable_test[i].val);
		}
		if (rc < 0)
			return rc;
	}
	/* cable diag started */

	return 0;
}

static int lan87xx_cable_test_report_trans(u32 result)
{
	switch (result) {
	case LAN87XX_CABLE_TEST_OK:
		return ETHTOOL_A_CABLE_RESULT_CODE_OK;
	case LAN87XX_CABLE_TEST_OPEN:
		return ETHTOOL_A_CABLE_RESULT_CODE_OPEN;
	case LAN87XX_CABLE_TEST_SAME_SHORT:
		return ETHTOOL_A_CABLE_RESULT_CODE_SAME_SHORT;
	default:
		/* DIAGNOSTIC_ERROR */
		return ETHTOOL_A_CABLE_RESULT_CODE_UNSPEC;
	}
}

static int lan87xx_cable_test_report(struct phy_device *phydev)
{
	int pos_peak_cycle = 0, pos_peak_in_phases = 0, pos_peak_phase = 0;
	int neg_peak_cycle = 0, neg_peak_in_phases = 0, neg_peak_phase = 0;
	int noise_margin = 20, time_margin = 89, jitter_var = 30;
	int min_time_diff = 96, max_time_diff = 96 + time_margin;
	bool fault = false, check_a = false, check_b = false;
	int gain_idx = 0, pos_peak = 0, neg_peak = 0;
	int pos_peak_time = 0, neg_peak_time = 0;
	int pos_peak_in_phases_hybrid = 0;
	int detect = -1;

	gain_idx = access_ereg(phydev, PHYACC_ATTR_MODE_READ,
			       PHYACC_ATTR_BANK_DSP, 151, 0);
	/* read non-hybrid results */
	pos_peak = access_ereg(phydev, PHYACC_ATTR_MODE_READ,
			       PHYACC_ATTR_BANK_DSP, 153, 0);
	neg_peak = access_ereg(phydev, PHYACC_ATTR_MODE_READ,
			       PHYACC_ATTR_BANK_DSP, 154, 0);
	pos_peak_time = access_ereg(phydev, PHYACC_ATTR_MODE_READ,
				    PHYACC_ATTR_BANK_DSP, 156, 0);
	neg_peak_time = access_ereg(phydev, PHYACC_ATTR_MODE_READ,
				    PHYACC_ATTR_BANK_DSP, 157, 0);

	pos_peak_cycle = (pos_peak_time >> 7) & 0x7F;
	/* calculate non-hybrid values */
	pos_peak_phase = pos_peak_time & 0x7F;
	pos_peak_in_phases = (pos_peak_cycle * 96) + pos_peak_phase;
	neg_peak_cycle = (neg_peak_time >> 7) & 0x7F;
	neg_peak_phase = neg_peak_time & 0x7F;
	neg_peak_in_phases = (neg_peak_cycle * 96) + neg_peak_phase;

	/* process values */
	check_a =
		((pos_peak_in_phases - neg_peak_in_phases) >= min_time_diff) &&
		((pos_peak_in_phases - neg_peak_in_phases) < max_time_diff) &&
		pos_peak_in_phases_hybrid < pos_peak_in_phases &&
		(pos_peak_in_phases_hybrid < (neg_peak_in_phases + jitter_var));
	check_b =
		((neg_peak_in_phases - pos_peak_in_phases) >= min_time_diff) &&
		((neg_peak_in_phases - pos_peak_in_phases) < max_time_diff) &&
		pos_peak_in_phases_hybrid < neg_peak_in_phases &&
		(pos_peak_in_phases_hybrid < (pos_peak_in_phases + jitter_var));

	if (pos_peak_in_phases > neg_peak_in_phases && check_a)
		detect = 2;
	else if ((neg_peak_in_phases > pos_peak_in_phases) && check_b)
		detect = 1;

	if (pos_peak > noise_margin && neg_peak > noise_margin &&
	    gain_idx >= 0) {
		if (detect == 1 || detect == 2)
			fault = true;
	}

	if (!fault)
		detect = 0;

	ethnl_cable_test_result(phydev, ETHTOOL_A_CABLE_PAIR_A,
				lan87xx_cable_test_report_trans(detect));

	return 0;
}

static int lan87xx_cable_test_get_status(struct phy_device *phydev,
					 bool *finished)
{
	int rc = 0;

	*finished = false;

	/* check if cable diag was finished */
	rc = access_ereg(phydev, PHYACC_ATTR_MODE_READ, PHYACC_ATTR_BANK_DSP,
			 90, 0);
	if (rc < 0)
		return rc;

	if ((rc & 2) == 2) {
		/* stop cable diag*/
		rc = access_ereg(phydev, PHYACC_ATTR_MODE_WRITE,
				 PHYACC_ATTR_BANK_DSP,
				 90, 0);
		if (rc < 0)
			return rc;

		*finished = true;

		return lan87xx_cable_test_report(phydev);
	}

	return 0;
}

static int lan87xx_read_status(struct phy_device *phydev)
{
	int rc = 0;

	rc = phy_read(phydev, T1_MODE_STAT_REG);
	if (rc < 0)
		return rc;

	if (rc & T1_LINK_UP_MSK)
		phydev->link = 1;
	else
		phydev->link = 0;

	phydev->speed = SPEED_UNKNOWN;
	phydev->duplex = DUPLEX_UNKNOWN;
	phydev->pause = 0;
	phydev->asym_pause = 0;

	rc = genphy_read_master_slave(phydev);
	if (rc < 0)
		return rc;

	rc = genphy_read_status_fixed(phydev);
	if (rc < 0)
		return rc;

	return rc;
}

static int lan87xx_config_aneg(struct phy_device *phydev)
{
	u16 ctl = 0;

	switch (phydev->master_slave_set) {
	case MASTER_SLAVE_CFG_MASTER_FORCE:
		ctl |= CTL1000_AS_MASTER;
		break;
	case MASTER_SLAVE_CFG_SLAVE_FORCE:
		break;
	case MASTER_SLAVE_CFG_UNKNOWN:
	case MASTER_SLAVE_CFG_UNSUPPORTED:
		return 0;
	default:
		phydev_warn(phydev, "Unsupported Master/Slave mode\n");
		return -EOPNOTSUPP;
	}

	return phy_modify_changed(phydev, MII_CTRL1000, CTL1000_AS_MASTER, ctl);
}

static int lan87xx_get_sqi(struct phy_device *phydev)
{
	u8 sqi_value = 0;
	int rc;

	rc = access_ereg(phydev, PHYACC_ATTR_MODE_WRITE,
			 PHYACC_ATTR_BANK_DSP, T1_COEF_RW_CTL_CFG, 0x0301);
	if (rc < 0)
		return rc;

	rc = access_ereg(phydev, PHYACC_ATTR_MODE_READ,
			 PHYACC_ATTR_BANK_DSP, T1_DCQ_SQI_REG, 0x0);
	if (rc < 0)
		return rc;

	sqi_value = FIELD_GET(T1_DCQ_SQI_MSK, rc);

	return sqi_value;
}

static int lan87xx_get_sqi_max(struct phy_device *phydev)
{
	return LAN87XX_MAX_SQI;
}

/**
 * LAN887X Start
 */
/* LAN887X Start */
struct lan887x_hw_stat {
	const char *string;
	u8 mmd;
	u8 bits;
	u16 reg;
};

static const struct lan887x_hw_stat lan887x_hw_stats[] = {
	//MMD registers
	{ "TX Good Count",                      MDIO_MMD_VEND1, 14, LAN887X_MIS_PKT_STAT_REG0},
	{ "RX Good Count",                      MDIO_MMD_VEND1, 14, LAN887X_MIS_PKT_STAT_REG1},
	{ "RX ERR Count detected by PCS",       MDIO_MMD_VEND1, 16, LAN887X_MIS_PKT_STAT_REG3},
	{ "TX CRC ERR Count",                   MDIO_MMD_VEND1, 8, LAN887X_MIS_PKT_STAT_REG4},
	{ "RX CRC ERR Count",                   MDIO_MMD_VEND1, 8, LAN887X_MIS_PKT_STAT_REG5},
	{ "RX ERR Count for SGMII MII2GMII",    MDIO_MMD_VEND1, 8, LAN887X_MIS_PKT_STAT_REG6},
};

struct lan887x_regwr_map {
	u8  mmd;
	u16 reg;
	u16 val;
};

struct lan887x_ptp_priv {
	struct mii_timestamper mii_ts;
	struct phy_device *phydev;

	struct sk_buff_head tx_queue;
	struct sk_buff_head rx_queue;

	struct list_head rx_ts_list;
	/* Lock for Rx ts fifo */
	spinlock_t rx_ts_lock;

	int hwts_tx_type;
	enum hwtstamp_rx_filters rx_filter;
	int layer;
	int version;

	struct ptp_clock *ptp_clock;
	struct ptp_clock_info caps;

	int lan887x_event_a;
	int lan887x_event_b;

	struct ptp_pin_desc *pin_config;

	/* Lock for phc */
	struct mutex ptp_lock;
};

struct lan887x_priv {
	struct lan887x_ptp_priv ptp_priv;
	const struct lan887x_type *type;
	u64 stats[ARRAY_SIZE(lan887x_hw_stats)];
};

struct lan887x_ptp_rx_ts {
	struct list_head list;
	u32 seconds;
	u32 nsec;
	u16 seq_id;
};

static int lan887x_cd_reset(struct phy_device *phydev, bool cd_done);
/**********************************************/
// Internal APIs to be called with-in driver
/**********************************************/
static int lan887x_config_advert(struct phy_device *phydev)
{
	u16 adv_r2_mask = 0;
	u16 adv_r1 = 0;
	u16 adv_r2 = 0;
	int ret;

	linkmode_and(phydev->advertising, phydev->advertising,
		     phydev->supported);

	adv_r2_mask = (MDIO_AN_T1_ADV_M_B1000 |
			MDIO_AN_T1_ADV_M_B100 |
			MDIO_AN_T1_ADV_M_MST);

	// Advertise master/slave mode
	switch (phydev->master_slave_set) {
	case MASTER_SLAVE_CFG_MASTER_FORCE:
		adv_r2 |= MDIO_AN_T1_ADV_M_MST;
		fallthrough;
	case MASTER_SLAVE_CFG_SLAVE_FORCE:
		adv_r1 |= MDIO_AN_T1_ADV_L_FORCE_MS;
		break;
	case MASTER_SLAVE_CFG_MASTER_PREFERRED:
		adv_r2 |= MDIO_AN_T1_ADV_M_MST;
		fallthrough;
	case MASTER_SLAVE_CFG_SLAVE_PREFERRED:
		break;
	case MASTER_SLAVE_CFG_UNKNOWN:
	case MASTER_SLAVE_CFG_UNSUPPORTED:
		//If unsupported mode is set then do not do anything
		return 0;
	default:
		phydev_warn(phydev, "Unsupported Master/Slave mode\n");
		return -EOPNOTSUPP;
	}

	//Errata-949: 100M master issue. Dual speed in Aneg is not supported.
	// Speed selection
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_1000baseT1_Full_BIT, phydev->advertising)) {
		linkmode_clear_bit(ETHTOOL_LINK_MODE_100baseT1_Full_BIT, phydev->advertising);
		phydev->speed = SPEED_1000;
		adv_r2 |= MDIO_AN_T1_ADV_M_B1000;
	} else if (linkmode_test_bit(ETHTOOL_LINK_MODE_100baseT1_Full_BIT, phydev->advertising)) {
		linkmode_clear_bit(ETHTOOL_LINK_MODE_1000baseT1_Full_BIT, phydev->advertising);
		phydev->speed = SPEED_100;
		adv_r2 |= MDIO_AN_T1_ADV_M_B100;
	} else {
		//If unsupported speeds are set then do not do anything
		return 0;
	}

	ret = phy_modify_mmd(phydev, MDIO_MMD_AN, MDIO_AN_T1_ADV_M, adv_r2_mask, adv_r2);
	if (ret < 0)
		return ret;

	/* Ref. 802.3bp-2016 : Section 45.2.7.14e
	 *      The Base Page value is transferred to mr_adv_ability when register 7.514 is written.
	 *      Therefore, registers 7.515 and 7.516 should be written before 7.514.
	 */
	ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_T1_ADV_L, adv_r1);
	if (ret < 0)
		return ret;

	return 0;
}

static int lan887x_rgmii_init(struct phy_device *phydev)
{
	int ret;

	// SGMII Mux disable
	ret = phy_clear_bits_mmd(phydev, MDIO_MMD_VEND1,
				 LAN887X_MX_CHIP_TOP_REG_SGMII_CTL,
				 LAN887X_MX_CHIP_TOP_REG_SGMII_MUX_EN);
	if (ret < 0)
		return ret;

	// Enable MAC_MODE = RGMII
	ret = phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MIS_CFG_REG0,
			     LAN887X_MIS_CFG_REG0_MAC_MAC_MODE_SEL,
			     LAN887X_MIS_CFG_REG0_MAC_MODE_RGMII);
	if (ret < 0)
		return ret;

	// PCS_ENA  = 0
	ret = phy_clear_bits_mmd(phydev, MDIO_MMD_VEND1,
				 LAN887X_MX_CHIP_TOP_SGMII_PCS_CFG,
				 LAN887X_MX_CHIP_TOP_SGMII_PCS_ENA);
	if (ret < 0)
		return ret;

	//UNG_MOLINEUX-964: RGMII Clock is still active during SGMII mode
	ret = phy_clear_bits_mmd(phydev, MDIO_MMD_VEND1,
				 LAN887X_MIS_CFG_REG0,
				 LAN887X_MIS_RCLKOUT_DIS);
	if (ret < 0)
		return ret;

	return 0;
}

static int lan887x_sgmii_init(struct phy_device *phydev)
{
	int ret;

	// SGMII Mux enable
	ret = phy_set_bits_mmd(phydev, MDIO_MMD_VEND1,
			       LAN887X_MX_CHIP_TOP_REG_SGMII_CTL,
			       LAN887X_MX_CHIP_TOP_REG_SGMII_MUX_EN);
	if (ret < 0)
		return ret;

	// Enable MAC_MODE = SGMII
	ret = phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MIS_CFG_REG0,
			     LAN887X_MIS_CFG_REG0_MAC_MAC_MODE_SEL,
			     LAN887X_MIS_CFG_REG0_MAC_MODE_SGMII);
	if (ret < 0)
		return ret;

	//UNG_MOLINEUX-964: RGMII Clock is still active during SGMII mode
	ret = phy_set_bits_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MIS_CFG_REG0,
			       LAN887X_MIS_RCLKOUT_DIS);
	if (ret < 0)
		return ret;

	// PCS_ENA  = 1
	ret = phy_set_bits_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MX_CHIP_TOP_SGMII_PCS_CFG,
			       LAN887X_MX_CHIP_TOP_SGMII_PCS_ENA);
	if (ret < 0)
		return ret;

	return 0;
}

static int lan887x_config_rgmii_delay(struct phy_device *phydev)
{
	u16 txc = 0;
	u16 rxc = 0;
	int ret = 0;

	ret = lan887x_rgmii_init(phydev);
	if (ret < 0)
		return ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MIS_TX_DLL_CFG_REG0);
	if (ret < 0)
		goto err_ret;

	txc |= ret & LAN887X_DEF_MASK;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MIS_RX_DLL_CFG_REG1);
	if (ret < 0)
		goto err_ret;

	rxc |= ret & LAN887X_DEF_MASK;

	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_RGMII:
		//phydev_dbg(phydev, "RGMII\n");
		txc &= ~LAN887X_MIS_DLL_EN_;
		rxc &= ~LAN887X_MIS_DLL_EN_;
		break;
	case PHY_INTERFACE_MODE_RGMII_ID:
		//phydev_dbg(phydev, "RGMII_ID\n");
		txc |= LAN887X_MIS_DLL_EN_;
		rxc |= LAN887X_MIS_DLL_EN_;
		break;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		//phydev_dbg(phydev, "RGMII_RXID\n");
		txc &= ~LAN887X_MIS_DLL_EN_;
		rxc |= LAN887X_MIS_DLL_EN_;
		break;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		//phydev_dbg(phydev, "RGMII_TXID\n");
		txc |= LAN887X_MIS_DLL_EN_;
		rxc &= ~LAN887X_MIS_DLL_EN_;
		break;
	default:
		ret = 0;
		goto err_ret;
	}

	// Set RX DELAY
	ret = phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MIS_RX_DLL_CFG_REG1,
			     LAN887X_MIS_DLL_EN_, rxc);
	if (ret < 0)
		goto err_ret;

	// Set TX DELAY
	ret = phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MIS_TX_DLL_CFG_REG0,
			     LAN887X_MIS_DLL_EN_, txc);
	if (ret < 0)
		goto err_ret;

	return 0;

err_ret:
	return ret;
}

static int lan887x_config_mac(struct phy_device *phydev)
{
	int ret;

	if (!phy_interface_is_rgmii(phydev)) {
		if (phydev->interface == PHY_INTERFACE_MODE_SGMII) {
			ret = lan887x_sgmii_init(phydev);
			if (ret < 0)
				return ret;
		} else {
			// Disable RGMII
			ret = phy_clear_bits_mmd(phydev, MDIO_MMD_VEND1,
						 LAN887X_MIS_CFG_REG0,
						 LAN887X_MIS_CFG_REG0_MAC_MAC_MODE_SEL);
			if (ret < 0)
				return ret;
		}
	} else {
		ret = lan887x_config_rgmii_delay(phydev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void lan887x_ptp_flush_fifo(struct lan887x_ptp_priv *ptp_priv, bool egress)
{
	struct phy_device *phydev = ptp_priv->phydev;
	int i;

	for (i = 0; i < FIFO_SIZE; ++i) {
		phy_read_mmd(phydev, MDIO_MMD_VEND1,
			     egress ? LAN887X_PTP_TX_MSG_HEADER2 :
			     LAN887X_PTP_RX_MSG_HEADER2);
	}
	phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_INT_STS);
}

static void lan887x_ptp_config_intr(struct lan887x_ptp_priv *ptp_priv,
				    bool enable)
{
	struct phy_device *phydev = ptp_priv->phydev;

	if (enable) {
		/* Enable interrupts */
		phy_write_mmd(phydev, MDIO_MMD_VEND1,
			      LAN887X_PTP_INT_EN,
			      LAN887X_PTP_INT_ALL_MSK);
	} else {
		/* Disable interrupts */
		phy_write_mmd(phydev, MDIO_MMD_VEND1,
			      LAN887X_PTP_INT_EN, 0);
	}
}

static bool is_sync(struct sk_buff *skb, int type)
{
	struct ptp_header *hdr;

	hdr = ptp_parse_header(skb, type);
	if (!hdr)
		return false;

	return ((ptp_get_msgtype(hdr, type) & 0xf) == 0);
}

static void lan887x_txtstamp(struct mii_timestamper *mii_ts,
			     struct sk_buff *skb, int type)
{
	struct lan887x_ptp_priv *ptp_priv = container_of(mii_ts,
							 struct lan887x_ptp_priv,
							 mii_ts);

	switch (ptp_priv->hwts_tx_type) {
	case HWTSTAMP_TX_ONESTEP_SYNC:
		if (is_sync(skb, type)) {
			kfree_skb(skb);
			return;
		}
		fallthrough;
	case HWTSTAMP_TX_ON:
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
		skb_queue_tail(&ptp_priv->tx_queue, skb);
		break;
	case HWTSTAMP_TX_OFF:
	default:
		kfree_skb(skb);
		break;
	}
}

static void lan887x_get_sig_rx(struct sk_buff *skb, u16 *sig)
{
	struct ptp_header *ptp_header;
	u32 type;

	skb_push(skb, ETH_HLEN);
	type = ptp_classify_raw(skb);
	ptp_header = ptp_parse_header(skb, type);
	skb_pull_inline(skb, ETH_HLEN);

	*sig = ntohs(ptp_header->sequence_id);
}

static bool lan887x_match_skb(struct lan887x_ptp_priv *ptp_priv,
			      struct lan887x_ptp_rx_ts *rx_ts)
{
	struct skb_shared_hwtstamps *shhwtstamps;
	struct sk_buff *skb, *skb_tmp;
	unsigned long flags;
	bool ret = false;
	u16 skb_sig;

	spin_lock_irqsave(&ptp_priv->rx_queue.lock, flags);
	skb_queue_walk_safe(&ptp_priv->rx_queue, skb, skb_tmp) {
		lan887x_get_sig_rx(skb, &skb_sig);

		if (memcmp(&skb_sig, &rx_ts->seq_id, sizeof(rx_ts->seq_id)))
			continue;

		__skb_unlink(skb, &ptp_priv->rx_queue);

		ret = true;
		break;
	}
	spin_unlock_irqrestore(&ptp_priv->rx_queue.lock, flags);

	if (ret) {
		shhwtstamps = skb_hwtstamps(skb);
		memset(shhwtstamps, 0, sizeof(*shhwtstamps));
		shhwtstamps->hwtstamp = ktime_set(rx_ts->seconds, rx_ts->nsec);
		netif_rx(skb);
	}

	return ret;
}

static void lan887x_match_rx_ts(struct lan887x_ptp_priv *ptp_priv,
				struct lan887x_ptp_rx_ts *rx_ts)
{
	unsigned long flags;

	/* If we failed to match the skb add it to the queue for when
	 * the frame will come
	 */
	if (!lan887x_match_skb(ptp_priv, rx_ts)) {
		spin_lock_irqsave(&ptp_priv->rx_ts_lock, flags);
		list_add(&rx_ts->list, &ptp_priv->rx_ts_list);
		spin_unlock_irqrestore(&ptp_priv->rx_ts_lock, flags);
	} else {
		kfree(rx_ts);
	}
}

static void lan887x_match_rx_skb(struct lan887x_ptp_priv *ptp_priv,
				 struct sk_buff *skb)
{
	struct skb_shared_hwtstamps *shhwtstamps;
	struct lan887x_ptp_rx_ts *rx_ts, *tmp;
	unsigned long flags;
	bool ret = false;
	u16 skb_sig;

	lan887x_get_sig_rx(skb, &skb_sig);

	/* Iterate over all RX timestamps and match it with the received skbs */
	spin_lock_irqsave(&ptp_priv->rx_ts_lock, flags);
	list_for_each_entry_safe(rx_ts, tmp, &ptp_priv->rx_ts_list, list) {
		/* Check if we found the signature we were looking for. */
		if (memcmp(&skb_sig, &rx_ts->seq_id, sizeof(rx_ts->seq_id)))
			continue;

		shhwtstamps = skb_hwtstamps(skb);
		memset(shhwtstamps, 0, sizeof(*shhwtstamps));
		shhwtstamps->hwtstamp = ktime_set(rx_ts->seconds,
						  rx_ts->nsec);
		netif_rx(skb);

		list_del(&rx_ts->list);
		kfree(rx_ts);

		ret = true;
		break;
	}
	spin_unlock_irqrestore(&ptp_priv->rx_ts_lock, flags);

	if (!ret)
		skb_queue_tail(&ptp_priv->rx_queue, skb);
}

static bool lan887x_rxtstamp(struct mii_timestamper *mii_ts,
			     struct sk_buff *skb, int type)
{
	int ret = false;
	struct lan887x_ptp_priv *ptp_priv = container_of(mii_ts,
							 struct lan887x_ptp_priv,
							 mii_ts);

	if (ptp_priv->rx_filter == HWTSTAMP_FILTER_NONE)
		goto ret_err;

	if ((type & ptp_priv->version) == 0 || (type & ptp_priv->layer) == 0)
		goto ret_err;

	ret = true;
	/* Here if match occurs skb is sent to application, If not skb is added to queue
	 * and sending skb to application will get handled when interrupt occurs i.e.,
	 * it get handles in intterupt handler. By anymeans skb will reach the application
	 * so we should not return false here if skb doesn't matches.
	 */
	lan887x_match_rx_skb(ptp_priv, skb);

ret_err:
	return ret;
}

static void lan887x_ptp_config_latency(struct phy_device *phydev)
{
	switch (phydev->speed) {
	case SPEED_1000:
		phy_write_mmd(phydev, MDIO_MMD_VEND1,
			      LAN887X_PTP_RX_LATENCY_1000_REG,
			      LAN887X_PTP_RX_LATENCY_1000);
		phy_write_mmd(phydev, MDIO_MMD_VEND1,
			      LAN887X_PTP_TX_LATENCY_1000_REG,
			      LAN887X_PTP_TX_LATENCY_1000);
		break;
	case SPEED_100:
		phy_write_mmd(phydev, MDIO_MMD_VEND1,
			      LAN887X_PTP_RX_LATENCY_100_REG,
			      LAN887X_PTP_RX_LATENCY_100);
		phy_write_mmd(phydev, MDIO_MMD_VEND1,
			      LAN887X_PTP_TX_LATENCY_100_REG,
			      LAN887X_PTP_TX_LATENCY_100);
		break;
	default:
		return;
	}
}

static int lan887x_hwtstamp(struct mii_timestamper *mii_ts, struct ifreq *ifr)
{
	struct lan887x_ptp_priv *ptp_priv = container_of(mii_ts, struct lan887x_ptp_priv, mii_ts);
	struct phy_device *phydev = ptp_priv->phydev;
	struct lan887x_ptp_rx_ts *rx_ts, *tmp;
	struct hwtstamp_config config;
	int txcfg = 0, rxcfg = 0;

	if (copy_from_user(&config, ifr->ifr_data, sizeof(config)))
		return -EFAULT;

	ptp_priv->hwts_tx_type = config.tx_type;
	ptp_priv->rx_filter = config.rx_filter;

	lan887x_ptp_config_latency(phydev);

	switch (config.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		ptp_priv->layer = 0;
		ptp_priv->version = 0;
		break;
	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
		ptp_priv->layer = PTP_CLASS_L4;
		ptp_priv->version = PTP_CLASS_V2;
		break;
	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
		ptp_priv->layer = PTP_CLASS_L2;
		ptp_priv->version = PTP_CLASS_V2;
		break;
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
		ptp_priv->layer = PTP_CLASS_L4 | PTP_CLASS_L2;
		ptp_priv->version = PTP_CLASS_V2;
		break;
	default:
		return -ERANGE;
	}

	/* Setup parsing of the frames and enable the timestamping for ptp
	 * frames
	 */
	if (ptp_priv->layer & PTP_CLASS_L2) {
		rxcfg = LAN887X_PTP_PARSE_CONFIG_LAYER2_EN;
		txcfg = LAN887X_PTP_PARSE_CONFIG_LAYER2_EN;
	}
	if (ptp_priv->layer & PTP_CLASS_L4) {
		rxcfg |= LAN887X_PTP_PARSE_CONFIG_IPV4_EN | LAN887X_PTP_PARSE_CONFIG_IPV6_EN;
		txcfg |= LAN887X_PTP_PARSE_CONFIG_IPV4_EN | LAN887X_PTP_PARSE_CONFIG_IPV6_EN;
	}
	phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_RX_PARSE_CONFIG,
		      rxcfg);

	phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_TX_PARSE_CONFIG,
		      txcfg);

	phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_RX_TIMESTAMP_EN,
		      PTP_TIMESTAMP_EN_ALL_);

	phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_TX_TIMESTAMP_EN,
		      PTP_TIMESTAMP_EN_ALL_);

	if (ptp_priv->hwts_tx_type == HWTSTAMP_TX_ONESTEP_SYNC)
		/* Enable / disable of the TX timestamp in the SYNC frames */
		phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_TX_MOD,
			       LAN887X_PTP_TX_MOD_PTP_SYNC_TS_INSERT,
			       LAN887X_PTP_TX_MOD_PTP_SYNC_TS_INSERT);
	else
		phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_TX_MOD,
			       LAN887X_PTP_TX_MOD_PTP_FU_TS_INSERT |
				   LAN887X_PTP_TX_MOD_PTP_SYNC_TS_INSERT,
			       ptp_priv->hwts_tx_type == HWTSTAMP_TX_ON ?
			       LAN887X_PTP_TX_MOD_PTP_FU_TS_INSERT : 0);

	/* Now enable the timestamping interrupts */
	lan887x_ptp_config_intr(ptp_priv,
				config.rx_filter != HWTSTAMP_FILTER_NONE);

	/* In case of multiple starts and stops, these needs to be cleared */
	list_for_each_entry_safe(rx_ts, tmp, &ptp_priv->rx_ts_list, list) {
		list_del(&rx_ts->list);
		kfree(rx_ts);
	}
	skb_queue_purge(&ptp_priv->rx_queue);
	skb_queue_purge(&ptp_priv->tx_queue);

	lan887x_ptp_flush_fifo(ptp_priv, false);
	lan887x_ptp_flush_fifo(ptp_priv, true);

	return copy_to_user(ifr->ifr_data, &config, sizeof(config)) ? -EFAULT : 0;
}

static int lan887x_ts_info(struct mii_timestamper *mii_ts,
			   struct ethtool_ts_info *info)
{
	struct lan887x_ptp_priv *ptp_priv = container_of(mii_ts,
							 struct lan887x_ptp_priv,
							 mii_ts);

	info->phc_index =
		ptp_priv->ptp_clock ? ptp_clock_index(ptp_priv->ptp_clock) : -1;
	if (info->phc_index == -1) {
		info->so_timestamping |= SOF_TIMESTAMPING_TX_SOFTWARE |
					 SOF_TIMESTAMPING_RX_SOFTWARE |
					 SOF_TIMESTAMPING_SOFTWARE;
		return 0;
	}

	info->so_timestamping = SOF_TIMESTAMPING_TX_HARDWARE |
				SOF_TIMESTAMPING_RX_HARDWARE |
				SOF_TIMESTAMPING_RAW_HARDWARE;

	info->tx_types =
		(1 << HWTSTAMP_TX_OFF) |
		(1 << HWTSTAMP_TX_ON) |
		(1 << HWTSTAMP_TX_ONESTEP_SYNC);

	info->rx_filters =
		(1 << HWTSTAMP_FILTER_NONE) |
		(1 << HWTSTAMP_FILTER_PTP_V1_L4_EVENT) |
		(1 << HWTSTAMP_FILTER_PTP_V2_L4_EVENT) |
		(1 << HWTSTAMP_FILTER_PTP_V2_L2_EVENT) |
		(1 << HWTSTAMP_FILTER_PTP_V2_EVENT);

	return 0;
}

static int lan887x_set_clock_target(struct phy_device *phydev, s8 gpio_pin,
				    s64 start_sec, u32 start_nsec);

static int lan887x_ltc_adjtime(struct ptp_clock_info *info, s64 delta)
{
	struct lan887x_ptp_priv *ptp_priv = container_of(info,
							 struct lan887x_ptp_priv,
							 caps);
	struct phy_device *phydev = ptp_priv->phydev;
	struct timespec64 ts;
	bool add = true;
	int ret = 0;
	u32 nsec;
	s32 sec;

	/* The HW allows up to 15 sec to adjust the time, but here we limit to
	 * 10 sec the adjustment. The reason is, in case the adjustment is 14
	 * sec and 999999999 nsec, then we add 8ns to compansate the actual
	 * increment so the value can be bigger than 15 sec. Therefore limit the
	 * possible adjustments so we will not have these corner cases
	 */
	if (delta > 10000000000LL || delta < -10000000000LL) {
		/* The timeadjustment is too big, so fall back using set time */
		u64 now;

		info->gettime64(info, &ts);

		now = ktime_to_ns(timespec64_to_ktime(ts));
		ts = ns_to_timespec64(now + delta);

		info->settime64(info, &ts);
		return 0;
	}
	sec = div_u64_rem(delta < 0 ? -delta : delta, NSEC_PER_SEC, &nsec);
	if (delta < 0 && nsec != 0) {
		/* It is not allowed to adjust low the nsec part, therefore
		 * subtract more from second part and add to nanosecond such
		 * that would roll over, so the second part will increase
		 */
		sec--;
		nsec = NSEC_PER_SEC - nsec;
	}

	/* Calculate the adjustments and the direction */
	if (delta < 0)
		add = false;

	if (nsec > 0)
		/* add 8 ns to cover the likely normal increment */
		nsec += 8;

	if (nsec >= NSEC_PER_SEC) {
		/* carry into seconds */
		sec++;
		nsec -= NSEC_PER_SEC;
	}

	mutex_lock(&ptp_priv->ptp_lock);
	if (sec) {
		if (sec < 0)
			sec = -sec;

		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_STEP_ADJ_LO, sec);
		if (ret < 0)
			goto out_unlock;
		ret = phy_set_bits_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_STEP_ADJ_HI,
				       ((add ? LAN887X_PTP_LTC_STEP_ADJ_HI_DIR : 0) |
					((sec >> 16) & 0x3fff)));
		if (ret < 0)
			goto out_unlock;
		ret = phy_set_bits_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_CMD_CTL,
				       LAN887X_PTP_CMD_CTL_PTP_LTC_STEP_SECONDS);
		if (ret < 0)
			goto out_unlock;
	}

	if (nsec) {
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_STEP_ADJ_LO,
				    nsec & LAN887X_DEF_MASK);
		if (ret < 0)
			goto out_unlock;
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_STEP_ADJ_HI,
				    (nsec >> 16) & 0x3fff);
		if (ret < 0)
			goto out_unlock;
		ret = phy_set_bits_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_CMD_CTL,
				       LAN887X_PTP_CMD_CTL_PTP_LTC_STEP_NANOSECONDS);
		if (ret < 0)
			goto out_unlock;
	}
	mutex_unlock(&ptp_priv->ptp_lock);
	info->gettime64(info, &ts);
	mutex_lock(&ptp_priv->ptp_lock);

	/* Target update is required for pulse generation on events that are enabled */
	if (ptp_priv->lan887x_event_a >= 0)
		lan887x_set_clock_target(phydev, LAN887X_EVENT_A,
					 ts.tv_sec + LAN887X_BUFFER_TIME, 0);

	if (ptp_priv->lan887x_event_b >= 0)
		lan887x_set_clock_target(phydev, LAN887X_EVENT_B,
					 ts.tv_sec + LAN887X_BUFFER_TIME, 0);

out_unlock:
	mutex_unlock(&ptp_priv->ptp_lock);

	return ret;
}

static int lan887x_ltc_adjfine(struct ptp_clock_info *info, long scaled_ppm)
{
	struct lan887x_ptp_priv *ptp_priv = container_of(info,
							 struct lan887x_ptp_priv,
							 caps);
	struct phy_device *phydev = ptp_priv->phydev;
	u16 rate_lo, rate_hi;
	bool faster = true;
	u32 rate;

	if (!scaled_ppm)
		return 0;

	if (scaled_ppm < 0) {
		scaled_ppm = -scaled_ppm;
		faster = false;
	}

	rate = LAN887X_1PPM_FORMAT * (upper_16_bits(scaled_ppm));
	rate += (LAN887X_1PPM_FORMAT * (0xffff & scaled_ppm)) >> 16;

	rate_lo = rate & 0xffff;
	rate_hi = (rate >> 16) & 0x3fff;

	if (faster)
		rate_hi |= LAN887X_PTP_LTC_RATE_ADJ_HI_DIR;

	mutex_lock(&ptp_priv->ptp_lock);
	phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_RATE_ADJ_HI, rate_hi);
	phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_RATE_ADJ_LO, rate_lo);
	mutex_unlock(&ptp_priv->ptp_lock);

	return 0;
}

static int lan887x_ltc_gettime64(struct ptp_clock_info *info,
				 struct timespec64 *ts)
{
	struct lan887x_ptp_priv *ptp_priv = container_of(info,
							 struct lan887x_ptp_priv,
							 caps);
	struct phy_device *phydev = ptp_priv->phydev;
	time64_t secs;
	int ret = 0;
	s64 nsecs;

	mutex_lock(&ptp_priv->ptp_lock);
	/* Set READ bit to 1 to save current values of 1588 Local Time Counter
	 * into PTP LTC seconds and nanoseconds registers.
	 */
	ret = phy_set_bits_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_CMD_CTL,
			       LAN887X_PTP_CMD_CTL_CLOCK_READ);
	if (ret < 0) {
		phydev_err(phydev, "Failed to set PTP_CLOCK_READ bit\n");
		goto out_unlock;
	}

	/* Get LTC clock values */
	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_READ_SEC_HI);
	if (ret < 0) {
		phydev_err(phydev, "Failed to read PTP_LTC_READ_SEC_HI reg\n");
		goto out_unlock;
	}
	secs = (ret & LAN887X_DEF_MASK);
	secs <<= 16;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_READ_SEC_MID);
	if (ret < 0) {
		phydev_err(phydev, "Failed to read PTP_LTC_READ_SEC_MID reg\n");
		goto out_unlock;
	}
	secs |= (ret & LAN887X_DEF_MASK);
	secs <<= 16;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_READ_SEC_LO);
	if (ret < 0) {
		phydev_err(phydev, "Failed to read PTP_LTC_READ_SEC_LO reg\n");
		goto out_unlock;
	}
	secs |= (ret & LAN887X_DEF_MASK);

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_READ_NS_HI);
	if (ret < 0) {
		phydev_err(phydev, "Failed to read PTP_LTC_READ_NS_HI reg\n");
		goto out_unlock;
	}
	nsecs = (ret & 0x3fff);
	nsecs <<= 16;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_READ_NS_LO);
	if (ret < 0) {
		phydev_err(phydev, "Failed to read PTP_LTC_READ_NS_LO reg\n");
		goto out_unlock;
	}
	nsecs |= (ret & LAN887X_DEF_MASK);

	set_normalized_timespec64(ts, secs, nsecs);

out_unlock:
	mutex_unlock(&ptp_priv->ptp_lock);

	return 0;
}

static int lan887x_ltc_settime64(struct ptp_clock_info *info,
				 const struct timespec64 *ts)
{
	struct lan887x_ptp_priv *ptp_priv = container_of(info,
							 struct lan887x_ptp_priv,
							 caps);
	struct phy_device *phydev = ptp_priv->phydev;
	int ret;

	mutex_lock(&ptp_priv->ptp_lock);
	ret = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_SEC_LO,
			    lower_16_bits(ts->tv_sec));
	if (ret < 0) {
		phydev_err(phydev, "Failed to write PTP_LTC_SEC_LO reg\n");
		goto out_unlock;
	}
	ret = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_SEC_MID,
			    upper_16_bits(ts->tv_sec));
	if (ret < 0) {
		phydev_err(phydev, "Failed to write PTP_LTC_SEC_MID reg\n");
		goto out_unlock;
	}
	ret = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_SEC_HI,
			    upper_32_bits(ts->tv_sec) & 0xffff);
	if (ret < 0) {
		phydev_err(phydev, "Failed to write PTP_LTC_SEC_HI reg\n");
		goto out_unlock;
	}
	ret = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_NS_LO,
			    lower_16_bits(ts->tv_nsec));
	if (ret < 0) {
		phydev_err(phydev, "Failed to write PTP_LTC_NS_LO register\n");
		goto out_unlock;
	}
	ret = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_LTC_NS_HI,
			    upper_16_bits(ts->tv_nsec) & 0x3fff);
	if (ret < 0) {
		phydev_err(phydev, "Failed to write PTP_LTC_NS_HI register\n");
		goto out_unlock;
	}

	/* Set LOAD bit to 1 to write PTP LTC seconds and nanoseconds
	 * registers to 1588 Local Time Counter.
	 */
	ret = phy_set_bits_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_CMD_CTL,
			       LAN887X_PTP_CMD_CTL_CLOCK_LOAD);
	if (ret < 0) {
		phydev_err(phydev, "Failed to set PTP_CLOCK_LOAD bit\n");
		goto out_unlock;
	}

out_unlock:
	mutex_unlock(&ptp_priv->ptp_lock);

	return ret;
}

static int lan887x_gpio_config_ptp_out(struct lan887x_ptp_priv *ptp_priv, s8 gpio_pin)
{
	struct phy_device *phydev = ptp_priv->phydev;
	int val, rc = 0;

	if (gpio_pin == ptp_priv->lan887x_event_b) {
		/* Enable pin mux for GPIO 2 as ref clk(design team suggested bit for event b) */
		val = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MX_CHIP_TOP_REG_CONTROL1);
		val |= LAN887X_MX_CHIP_TOP_REG_CONTROL1_REF_CLK;
		rc = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MX_CHIP_TOP_REG_CONTROL1,
				   val);
	}

	if (gpio_pin == ptp_priv->lan887x_event_a) {
		/* Enable pin mux for EVT A */
		val = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MX_CHIP_TOP_REG_CONTROL1);
		val |= LAN887X_MX_CHIP_TOP_REG_CONTROL1_EVT_EN;
		rc = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MX_CHIP_TOP_REG_CONTROL1,
				   val);
	}

	return rc;
}

static int lan887x_gpio_release(struct lan887x_ptp_priv *ptp_priv, s8 gpio_pin)
{
	struct phy_device *phydev = ptp_priv->phydev;
	int val, rc;

	if (gpio_pin == ptp_priv->lan887x_event_b) {
		/* Disable pin mux for GPIO 2 as Ref clk(design team suggested bit for event b) */
		val = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MX_CHIP_TOP_REG_CONTROL1);
		val &= ~LAN887X_MX_CHIP_TOP_REG_CONTROL1_REF_CLK;
		rc = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MX_CHIP_TOP_REG_CONTROL1,
				   val);
	}

	if (gpio_pin == ptp_priv->lan887x_event_a) {
		/* Disable pin mux for EVT A */
		val = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MX_CHIP_TOP_REG_CONTROL1);
		val &= ~LAN887X_MX_CHIP_TOP_REG_CONTROL1_EVT_EN;
		rc = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MX_CHIP_TOP_REG_CONTROL1,
				   val);
	}

	return rc;
}

static int lan887x_get_pulsewidth(struct phy_device *phydev,
				  struct ptp_perout_request *perout_request,
				  int *pulse_width)
{
	struct timespec64 ts_period;
	s64 ts_on_nsec, period_nsec;
	struct timespec64 ts_on;

	ts_period.tv_sec = perout_request->period.sec;
	ts_period.tv_nsec = perout_request->period.nsec;

	ts_on.tv_sec = perout_request->on.sec;
	ts_on.tv_nsec = perout_request->on.nsec;
	ts_on_nsec = timespec64_to_ns(&ts_on);
	period_nsec = timespec64_to_ns(&ts_period);

	if (period_nsec < 200) {
		phydev_warn(phydev, "perout period too small, minimum is 200ns\n");
		return -EOPNOTSUPP;
	}

	if (ts_on_nsec >= period_nsec) {
		phydev_warn(phydev, "pulse width must be smaller than period\n");
		return -EINVAL;
	}

	switch (ts_on_nsec) {
	case 200000000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_200MS_;
		break;
	case 100000000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_100MS_;
		break;
	case 50000000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_50MS_;
		break;
	case 10000000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_10MS_;
		break;
	case 5000000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_5MS_;
		break;
	case 1000000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_1MS_;
		break;
	case 500000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_500US_;
		break;
	case 100000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_100US_;
		break;
	case 50000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_50US_;
		break;
	case 10000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_10US_;
		break;
	case 5000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_5US_;
		break;
	case 1000:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_1US_;
		break;
	case 500:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_500NS_;
		break;
	case 100:
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_100NS_;
		break;
	default:
		phydev_warn(phydev, "Using default pulse width of 200ms\n");
		*pulse_width = LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_200MS_;
		break;
	}
	return 0;
}

static int lan887x_general_event_config(struct phy_device *phydev, s8 event, int pulse_width)
{
	u16 general_config;

	general_config = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_GENERAL_CONFIG);
	general_config &= ~(LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_X_MASK_(event));
	general_config |= LAN887X_PTP_GENERAL_CONFIG_LTC_EVENT_X_SET_(event,
								      pulse_width);
	general_config &= ~(LAN887X_PTP_GENERAL_CONFIG_RELOAD_ADD_X_(event));
	general_config |= LAN887X_PTP_GENERAL_CONFIG_POLARITY_X_(event);

	return phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_GENERAL_CONFIG, general_config);
}

static int lan887x_set_clock_reload(struct phy_device *phydev, s8 event,
				    s64 period_sec, u32 period_nsec)
{
	int rc;

	rc = phy_write_mmd(phydev, MDIO_MMD_VEND1,
			   LAN887X_PTP_CLOCK_TARGET_RELOAD_SEC_LO_X(event),
			   lower_16_bits(period_sec));
	if (rc < 0)
		return rc;

	rc = phy_write_mmd(phydev, MDIO_MMD_VEND1,
			   LAN887X_PTP_CLOCK_TARGET_RELOAD_SEC_HI_X(event),
			   upper_16_bits(period_sec));
	if (rc < 0)
		return rc;

	rc = phy_write_mmd(phydev, MDIO_MMD_VEND1,
			   LAN887X_PTP_CLOCK_TARGET_RELOAD_NS_LO_X(event),
			   lower_16_bits(period_nsec));
	if (rc < 0)
		return rc;

	return phy_write_mmd(phydev, MDIO_MMD_VEND1,
			     LAN887X_PTP_CLOCK_TARGET_RELOAD_NS_HI_X(event),
			     upper_16_bits(period_nsec) & 0x3fff);
}

static int lan887x_set_clock_target(struct phy_device *phydev, s8 event,
				    s64 start_sec, u32 start_nsec)
{
	int rc;

	if (event < 0)
		return -1;

	/* Set the start time */
	rc = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_CLOCK_TARGET_SEC_LO_X(event),
			   lower_16_bits(start_sec));
	if (rc < 0)
		return rc;

	rc = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_CLOCK_TARGET_SEC_HI_X(event),
			   upper_16_bits(start_sec));
	if (rc < 0)
		return rc;

	rc = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_CLOCK_TARGET_NS_LO_X(event),
			   lower_16_bits(start_nsec));
	if (rc < 0)
		return rc;

	return phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_CLOCK_TARGET_NS_HI_X(event),
			     upper_16_bits(start_nsec) & 0x3fff);
}

static int lan887x_get_event(struct lan887x_ptp_priv *ptp_priv, int pin)
{
	if (ptp_priv->lan887x_event_a < 0 && pin == 3) {
		ptp_priv->lan887x_event_a = pin;
		return LAN887X_EVENT_A;
	}

	if (ptp_priv->lan887x_event_b < 0 && pin == 1) {
		ptp_priv->lan887x_event_b = pin;
		return LAN887X_EVENT_B;
	}

	return -1;
}

static int lan887x_ptp_perout_off(struct lan887x_ptp_priv *ptp_priv,
				  s8 gpio_pin)
{
	u16 general_config;
	int event = -1, rc;

	if (ptp_priv->lan887x_event_a == gpio_pin)
		event = LAN887X_EVENT_A;
	else if (ptp_priv->lan887x_event_b == gpio_pin)
		event = LAN887X_EVENT_B;

	/* Set target to too far in the future, effectively disabling it */
	rc = lan887x_set_clock_target(ptp_priv->phydev, gpio_pin, 0xFFFFFFFF, 0);
	if (rc < 0)
		return rc;

	general_config = phy_read_mmd(ptp_priv->phydev, MDIO_MMD_VEND1, LAN887X_PTP_GENERAL_CONFIG);
	general_config |= LAN887X_PTP_GENERAL_CONFIG_RELOAD_ADD_X_(event);
	rc = phy_write_mmd(ptp_priv->phydev, MDIO_MMD_VEND1,
			   LAN887X_PTP_GENERAL_CONFIG, general_config);
	if (rc < 0)
		return rc;

	if (event == LAN887X_EVENT_A)
		ptp_priv->lan887x_event_a = -1;

	if (event == LAN887X_EVENT_B)
		ptp_priv->lan887x_event_b = -1;

	return lan887x_gpio_release(ptp_priv, gpio_pin);
}

static int lan887x_ptp_perout(struct ptp_clock_info *ptpci,
			      struct ptp_perout_request *perout, int on)
{
	struct lan887x_ptp_priv *ptp_priv = container_of(ptpci, struct lan887x_ptp_priv,
							 caps);
	struct phy_device *phydev = ptp_priv->phydev;
	int pulsewidth;
	int ret, event;
	int gpio_pin;

	/* Reject requests with unsupported flags */
	if (perout->flags & ~PTP_PEROUT_DUTY_CYCLE)
		return -EOPNOTSUPP;

	gpio_pin = ptp_find_pin(ptp_priv->ptp_clock, PTP_PF_PEROUT, perout->index);
	if (gpio_pin < 0)
		return -EBUSY;

	if (!on) {
		lan887x_ptp_perout_off(ptp_priv, gpio_pin);
		return 0;
	}

	ret = lan887x_get_pulsewidth(phydev, perout, &pulsewidth);
	if (ret < 0)
		return ret;

	event = lan887x_get_event(ptp_priv, gpio_pin);
	if (event < 0)
		return event;

	/* Configure to pulse every period */
	ret = lan887x_general_event_config(phydev, event, pulsewidth);
	if (ret < 0)
		return ret;

	ret = lan887x_set_clock_target(phydev, event, perout->start.sec,
				       perout->start.nsec);
	if (ret < 0)
		return ret;

	ret = lan887x_set_clock_reload(phydev, event, perout->period.sec,
				       perout->period.nsec);
	if (ret < 0)
		return ret;

	return lan887x_gpio_config_ptp_out(ptp_priv, gpio_pin);
}

static int lan887x_ptpci_enable(struct ptp_clock_info *ptpci,
				struct ptp_clock_request *request, int on)
{
	switch (request->type) {
	case PTP_CLK_REQ_PEROUT:
		return lan887x_ptp_perout(ptpci, &request->perout, on);
	default:
		return -EINVAL;
	}
}

static int lan887x_ptpci_verify(struct ptp_clock_info *ptpci, unsigned int pin,
				enum ptp_pin_function func, unsigned int chan)
{
	/* 1 for event b and 3 for event a*/
	if (!(pin == 1 && chan == 1) && !(pin == 3 && chan == 0))
		return -1;

	switch (func) {
	case PTP_PF_NONE:
	case PTP_PF_PEROUT:
		break;
	default:
		return -1;
	}

	return 0;
}

static void lan887x_ptp_init(struct phy_device *phydev)
{
	int i;

	static const struct lan887x_regwr_map reg_wr[] = {
		/* Disable PTP */
		{MDIO_MMD_VEND1, LAN887X_PTP_CMD_CTL, LAN887X_PTP_CMD_CTL_DIS},
		/* Disable TSU */
		{MDIO_MMD_VEND1, LAN887X_TSU_GEN_CONFIG, 0},
		/* LTC Hard reset */
		{MDIO_MMD_VEND1, LAN887X_PTP_LTC_HARD_RESET, LAN887X_PTP_LTC_HARD_RESET_},
		/* TSU Hard reset */
		{MDIO_MMD_VEND1, LAN887X_TSU_HARD_RESET, LAN887X_PTP_TSU_HARD_RESET},
		// predictor enable
		{MDIO_MMD_VEND1, LAN887X_PTP_LATENCY_CORRECTION_CTL, LAN887X_PTP_LATENCY_SETTING},
		/* Configure PTP Operational mode */
		{MDIO_MMD_VEND1, LAN887X_PTP_OP_MODE, LAN887X_PTP_OP_MODE_STANDALONE},
		/* Ref_clk configuration */
		{MDIO_MMD_VEND1, LAN887X_PTP_REF_CLK_CFG, LAN887X_PTP_REF_CLK_CFG_SET},
		/* Classifier Configurations */
		{MDIO_MMD_VEND1, LAN887X_PTP_RX_PARSE_CONFIG, 0},
		{MDIO_MMD_VEND1, LAN887X_PTP_TX_PARSE_CONFIG, 0},
		{MDIO_MMD_VEND1, LAN887X_PTP_TX_PARSE_L2_ADDR_EN, 0},
		{MDIO_MMD_VEND1, LAN887X_PTP_RX_PARSE_L2_ADDR_EN, 0},
		{MDIO_MMD_VEND1, LAN887X_PTP_RX_PARSE_IPV4_ADDR_EN, 0},
		{MDIO_MMD_VEND1, LAN887X_PTP_TX_PARSE_IPV4_ADDR_EN, 0},
		{MDIO_MMD_VEND1, LAN887X_PTP_RX_VERSION,
			PTP_MAX_VERSION(0xff) | PTP_MIN_VERSION(0x0)},
		{MDIO_MMD_VEND1, LAN887X_PTP_TX_VERSION,
			PTP_MAX_VERSION(0xff) | PTP_MIN_VERSION(0x0)},
		/* Enable TSU */
		{MDIO_MMD_VEND1, LAN887X_TSU_GEN_CONFIG, LAN887X_TSU_GEN_CFG_TSU_EN},
		/* Enable PTP */
		{MDIO_MMD_VEND1, LAN887X_PTP_CMD_CTL, LAN887X_PTP_CMD_CTL_EN},
	};

	if (!IS_ENABLED(CONFIG_PTP_1588_CLOCK) ||
	    !IS_ENABLED(CONFIG_NETWORK_PHY_TIMESTAMPING))
		return;

	for (i = 0; i < ARRAY_SIZE(reg_wr); i++) {
		phy_write_mmd(phydev, reg_wr[i].mmd,
			      reg_wr[i].reg, reg_wr[i].val);
	}
}

static int lan887x_ptp_probe(struct phy_device *phydev)
{
	struct lan887x_priv *priv = phydev->priv;
	struct lan887x_ptp_priv *ptp_priv = &priv->ptp_priv;
	int i;

	if (!IS_ENABLED(CONFIG_PTP_1588_CLOCK) ||
	    !IS_ENABLED(CONFIG_NETWORK_PHY_TIMESTAMPING))
		return 0;

	ptp_priv->pin_config = devm_kmalloc_array(&phydev->mdio.dev,
						  LAN887X_N_GPIO,
						  sizeof(*ptp_priv->pin_config),
						  GFP_KERNEL);

	if (!ptp_priv->pin_config)
		return -ENOMEM;

	for (i = 0; i < LAN887X_N_GPIO; ++i) {
		struct ptp_pin_desc *p = &ptp_priv->pin_config[i];

		memset(p, 0, sizeof(*p));
		snprintf(p->name, sizeof(p->name), "pin%d", i);
		p->index = i;
		p->func = PTP_PF_NONE;
	}
	/* Register PTP clock */
	ptp_priv->caps.owner          = THIS_MODULE;
	snprintf(ptp_priv->caps.name, 30, "%s", phydev->drv->name);
	ptp_priv->caps.max_adj        = LAN887X_MAX_ADJ;
	ptp_priv->caps.n_alarm        = 0;
	ptp_priv->caps.n_ext_ts       = 0;
	ptp_priv->caps.n_pins         = LAN887X_N_GPIO;
	ptp_priv->caps.pps            = 0;
	ptp_priv->caps.n_per_out      = LAN887X_N_PEROUT;
	ptp_priv->caps.pin_config     = ptp_priv->pin_config;
	ptp_priv->caps.adjfine        = lan887x_ltc_adjfine;
	ptp_priv->caps.adjtime        = lan887x_ltc_adjtime;
	ptp_priv->caps.gettime64      = lan887x_ltc_gettime64;
	ptp_priv->caps.settime64      = lan887x_ltc_settime64;
	ptp_priv->caps.getcrosststamp = NULL;
	ptp_priv->caps.enable = lan887x_ptpci_enable;
	ptp_priv->caps.verify = lan887x_ptpci_verify;
	ptp_priv->ptp_clock = ptp_clock_register(&ptp_priv->caps,
						 &phydev->mdio.dev);
	if (IS_ERR(ptp_priv->ptp_clock)) {
		phydev_err(phydev, "ptp_clock_register failed %lu\n",
			   PTR_ERR(ptp_priv->ptp_clock));
		return -EINVAL;
	}

	if (!ptp_priv->ptp_clock)
		return 0;

	/* Initialize the SW */
	skb_queue_head_init(&ptp_priv->tx_queue);
	skb_queue_head_init(&ptp_priv->rx_queue);
	INIT_LIST_HEAD(&ptp_priv->rx_ts_list);
	spin_lock_init(&ptp_priv->rx_ts_lock);
	ptp_priv->phydev = phydev;
	mutex_init(&ptp_priv->ptp_lock);

	ptp_priv->mii_ts.rxtstamp = lan887x_rxtstamp;
	ptp_priv->mii_ts.txtstamp = lan887x_txtstamp;
	ptp_priv->mii_ts.hwtstamp = lan887x_hwtstamp;
	ptp_priv->mii_ts.ts_info = lan887x_ts_info;

	phydev->mii_ts = &ptp_priv->mii_ts;

	ptp_priv->lan887x_event_a = -1;
	ptp_priv->lan887x_event_b = -1;

	//phydev_dbg(phydev, "Configuration of PHY PTP Block is complete!\n");

	return 0;
}

static void lan887x_get_sig_tx(struct sk_buff *skb, u16 *sig)
{
	struct ptp_header *ptp_header;
	u32 type;

	type = ptp_classify_raw(skb);
	ptp_header = ptp_parse_header(skb, type);

	*sig = htons(ptp_header->sequence_id);
}

static void lan887x_match_tx_skb(struct lan887x_ptp_priv *ptp_priv,
				 u32 seconds, u32 nsec, u16 seq_id)
{
	struct skb_shared_hwtstamps shhwtstamps;
	struct sk_buff *skb, *skb_tmp;
	unsigned long flags;
	bool ret = false;
	u16 skb_sig;

	spin_lock_irqsave(&ptp_priv->tx_queue.lock, flags);
	skb_queue_walk_safe(&ptp_priv->tx_queue, skb, skb_tmp) {
		lan887x_get_sig_tx(skb, &skb_sig);

		if (memcmp(&skb_sig, &seq_id, sizeof(seq_id)))
			continue;

		__skb_unlink(skb, &ptp_priv->tx_queue);
		ret = true;
		break;
	}
	spin_unlock_irqrestore(&ptp_priv->tx_queue.lock, flags);

	if (ret) {
		memset(&shhwtstamps, 0, sizeof(shhwtstamps));
		shhwtstamps.hwtstamp = ktime_set(seconds, nsec);
		skb_complete_tx_timestamp(skb, &shhwtstamps);
	}
}

static struct lan887x_ptp_rx_ts *lan887x_ptp_get_rx_ts(struct lan887x_ptp_priv *ptp_priv)
{
	struct phy_device *phydev = ptp_priv->phydev;
	struct lan887x_ptp_rx_ts *rx_ts;
	u32 sec, nsec;
	u16 seq;
	int ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_RX_INGRESS_NS_HI);
	nsec = (ret & LAN887X_DEF_MASK);
	if (!(nsec & LAN887X_PTP_RX_INGRESS_NS_HI_PTP_RX_TS_VALID)) {
		phydev_err(phydev, "RX Timestamp is not valid!\n");
		return NULL;
	}
	nsec = (nsec & 0x3fff) << 16;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_RX_INGRESS_NS_LO);
	nsec |= (ret & LAN887X_DEF_MASK);

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_RX_INGRESS_SEC_HI);
	sec = (ret & LAN887X_DEF_MASK);
	sec <<= 16;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_RX_INGRESS_SEC_LO);
	sec |= (ret & LAN887X_DEF_MASK);

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_RX_MSG_HEADER2);
	seq = (ret & LAN887X_DEF_MASK);

	rx_ts = kzalloc(sizeof(*rx_ts), GFP_KERNEL);
	if (!rx_ts)
		return NULL;

	rx_ts->seconds = sec;
	rx_ts->nsec = nsec;
	rx_ts->seq_id = seq;

	return rx_ts;
}

static void lan887x_ptp_process_rx_ts(struct lan887x_ptp_priv *ptp_priv)
{
	struct phy_device *phydev = ptp_priv->phydev;

	do {
		struct lan887x_ptp_rx_ts *rx_ts;

		rx_ts = lan887x_ptp_get_rx_ts(ptp_priv);
		if (rx_ts)
			lan887x_match_rx_ts(ptp_priv, rx_ts);
	} while (LAN887X_MX_PTP_PRT_RX_TS_CNT_GET(phy_read_mmd(phydev, MDIO_MMD_VEND1,
							       LAN887X_MX_PTP_PRT_CAP_INFO_REG
							       )) > 0);
}

static bool lan887x_ptp_get_tx_ts(struct lan887x_ptp_priv *ptp_priv,
				  u32 *sec, u32 *nsec, u16 *seq)
{
	struct phy_device *phydev = ptp_priv->phydev;
	int ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_TX_EGRESS_NS_HI);
	*nsec = (ret & LAN887X_DEF_MASK);
	if (!(*nsec & LAN887X_PTP_TX_EGRESS_NS_HI_PTP_TX_TS_VALID))
		return false;
	*nsec = (*nsec & 0x3fff) << 16;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_TX_EGRESS_NS_LO);
	*nsec = *nsec | (ret & LAN887X_DEF_MASK);

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_TX_EGRESS_SEC_HI);
	*sec = (ret & LAN887X_DEF_MASK);
	*sec = *sec << 16;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_TX_EGRESS_SEC_LO);
	*sec = *sec | (ret & LAN887X_DEF_MASK);

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_TX_MSG_HEADER2);
	*seq = (ret & LAN887X_DEF_MASK);

	return true;
}

static void lan887x_ptp_process_tx_ts(struct lan887x_ptp_priv *ptp_priv)
{
	struct phy_device *phydev = ptp_priv->phydev;

	do {
		u32 sec, nsec;
		u16 seq;

		if (lan887x_ptp_get_tx_ts(ptp_priv, &sec, &nsec, &seq))
			lan887x_match_tx_skb(ptp_priv, sec, nsec, seq);
	} while (LAN887X_MX_PTP_PRT_TX_TS_CNT_GET(phy_read_mmd(phydev, MDIO_MMD_VEND1,
							       LAN887X_MX_PTP_PRT_CAP_INFO_REG
							       )) > 0);
}

static void lan887x_handle_ptp_interrupt(struct phy_device *phydev, int irq_status)
{
	struct lan887x_priv *priv = phydev->priv;
	struct lan887x_ptp_priv *ptp_priv = &priv->ptp_priv;

	if (irq_status & LAN887X_PTP_INT_RX_TS_EN)
		lan887x_ptp_process_rx_ts(ptp_priv);

	if (irq_status & LAN887X_PTP_INT_TX_TS_EN)
		lan887x_ptp_process_tx_ts(ptp_priv);

	if (irq_status & LAN887X_PTP_INT_TX_TS_OVRFL_EN) {
		lan887x_ptp_flush_fifo(ptp_priv, true);
		skb_queue_purge(&ptp_priv->tx_queue);
	}

	if (irq_status & LAN887X_PTP_INT_RX_TS_OVRFL_EN) {
		lan887x_ptp_flush_fifo(ptp_priv, false);
		skb_queue_purge(&ptp_priv->rx_queue);
	}
}

/**********************************************/
// External/Driver APIs to be called from MAC
/**********************************************/

static int lan887x_get_features(struct phy_device *phydev)
{
	linkmode_zero(phydev->supported);

	/* Enable twisted pair */
	linkmode_set_bit(ETHTOOL_LINK_MODE_TP_BIT, phydev->supported);
	/* Enable  auto-neg */
	linkmode_set_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, phydev->supported);
	/* Enable  1G speed */
	linkmode_set_bit(ETHTOOL_LINK_MODE_1000baseT1_Full_BIT, phydev->supported);
	/* Enable  100Mbps */
	linkmode_set_bit(ETHTOOL_LINK_MODE_100baseT1_Full_BIT, phydev->supported);

	return 0;
}

static void lan887x_config_led(struct phy_device *phydev)
{
	phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_CHIPTOP_COMM_LED3_LED2,
		       LAN887X_CHIPTOP_COMM_LED2_MASK,
		       LAN887X_CHIPTOP_LED_LINK_ACT_ANY_SPEED);
}

static void lan887x_tc10_init(struct phy_device *phydev)
{
	phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MISC_REGS_REG16,
		       LAN887X_MISC_REGS_REG16_IGNORE_IDLE_WITH_WUR_LPS,
		       LAN887X_MISC_REGS_REG16_IGNORE_IDLE_WITH_WUR_LPS);
	phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_DEV30_COMMON_TC10_REG_REG15,
		       LAN887X_DEV30_COMMON_TC10_REG_REG15_WK_OUT_PIN_REQ,
		       LAN887X_DEV30_COMMON_TC10_REG_REG15_WK_OUT_PIN_REQ);
	phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_DEV30_COMMON_TC10_MISC33,
		      (LAN887X_DEV30_COMMON_TC10_MISC33_WK_DEB_VAL << 8) |
		      LAN887X_DEV30_COMMON_TC10_MISC33_WK_OUT_LEN);
	phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_DEV30_COMMON_TC10_MISC32,
		       LAN887X_DEV30_COMMON_TC10_MISC32_VAL,
		       LAN887X_DEV30_COMMON_TC10_MISC32_VAL);
	phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_DEV30_COMMON_TC10_MISC46,
		       LAN887X_DEV30_COMMON_TC10_MISC46_WK_PORT_TEST_MASK,
		       LAN887X_DEV30_COMMON_TC10_MISC46_WK_PORT_TEST_VAL);
	phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_DEV30_COMMON_TC10_MISC36,
		       LAN887X_DEV30_COMMON_TC10_MISC36_VAL,
		       LAN887X_DEV30_COMMON_TC10_MISC36_VAL);
	phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MISC_REGS_MISC37,
		       LAN887X_MISC_REGS_MISC37_EN_TC10_SLEEP_SILENT,
		       LAN887X_MISC_REGS_MISC37_EN_TC10_SLEEP_SILENT);
}

static int lan887x_phy_init(struct phy_device *phydev)
{
	int ret;

	//phydev_info(phydev, "PHY init\n");

	//Clear loopback
	ret = phy_clear_bits_mmd(phydev, MDIO_MMD_VEND1,
				 LAN887X_MIS_CFG_REG2,
				 LAN887X_MIS_CFG_REG2_FE_LPBK_EN);
	if (ret < 0)
		return ret;

	//MAC setup
	ret = lan887x_config_mac(phydev);
	if (ret < 0)
		return ret;

	//LED setup
	lan887x_config_led(phydev);

	lan887x_ptp_init(phydev);

	lan887x_tc10_init(phydev);

	//phydev_info(phydev, "PHY init done!\n");

	return 0;
}

static int lan887x_config_init(struct phy_device *phydev)
{
	/* Disable Pause frames */
	linkmode_clear_bit(ETHTOOL_LINK_MODE_Pause_BIT, phydev->supported);
	/* Disable Asym Pause */
	linkmode_clear_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, phydev->supported);

	phydev->interface = PHY_INTERFACE_MODE_RGMII_RXID;

	return lan887x_phy_init(phydev);
}

static int lan887x_phy_setup(struct phy_device *phydev)
{
	//Please do not change the sequence
	static const struct lan887x_regwr_map phy_cfg[] = {
		{MDIO_MMD_PMAPMD, 0x8080, 0x4008},
		{MDIO_MMD_PMAPMD, 0x8089, 0x0000},
		{MDIO_MMD_PMAPMD, 0x808D, 0x0040},
		{MDIO_MMD_PCS,	  0x8213, 0x0008},
		{MDIO_MMD_PCS,	  0x8204, 0x800D},
		{MDIO_MMD_VEND1,  0x0405, 0x0AB1},
		{MDIO_MMD_VEND1,  0x0411, 0x5274},
		{MDIO_MMD_VEND1,  0x0417, 0x0D74},
		{MDIO_MMD_VEND1,  0x041C, 0x0AEA},
		{MDIO_MMD_VEND1,  0x0425, 0x0360},
		{MDIO_MMD_VEND1,  0x0454, 0x0C30},
		{MDIO_MMD_VEND1,  0x0811, 0x2A78},
		{MDIO_MMD_VEND1,  0x0813, 0x1368},
		{MDIO_MMD_VEND1,  0x0825, 0x1354},
		{MDIO_MMD_VEND1,  0x0843, 0x3C84},
		{MDIO_MMD_VEND1,  0x0844, 0x3CA5},
		{MDIO_MMD_VEND1,  0x0845, 0x3CA5},
		{MDIO_MMD_VEND1,  0x0846, 0x3CA5},
		{MDIO_MMD_VEND1,  0x08EC, 0x0024},
		{MDIO_MMD_VEND1,  0x08EE, 0x227F},
		{MDIO_MMD_PCS,    0x8043, 0x1E00},
		{MDIO_MMD_PCS,    0x8048, 0x0FA1},
	};
	int ret;
	int i;

	//phydev_info(phydev, "PHY setup\n");

	for (i = 0; i < ARRAY_SIZE(phy_cfg); i++) {
		ret = phy_write_mmd(phydev, phy_cfg[i].mmd,
				    phy_cfg[i].reg, phy_cfg[i].val);
		if (ret < 0)
			return ret;
	}

	ret = lan887x_ptp_probe(phydev);
	if (ret < 0)
		return ret;

	//phydev_info(phydev, "PHY setup done\n");

	return 0;
}

static int lan887x_100m_scripts(struct phy_device *phydev)
{
	int ret;
	int i;

	if (phydev->autoneg == AUTONEG_DISABLE) {
		if (phydev->master_slave_set == MASTER_SLAVE_CFG_MASTER_FORCE ||
		    phydev->master_slave_set == MASTER_SLAVE_CFG_MASTER_PREFERRED){
			static const struct lan887x_regwr_map phy_cfg[] = {
				{MDIO_MMD_PMAPMD, 0x808B, 0x00B8},
				{MDIO_MMD_PMAPMD, 0x80B0, 0x0038},
				{MDIO_MMD_VEND1,  0x0422, 0x000F},
			};

			for (i = 0; i < ARRAY_SIZE(phy_cfg); i++) {
				ret = phy_write_mmd(phydev, phy_cfg[i].mmd,
						    phy_cfg[i].reg, phy_cfg[i].val);
				if (ret < 0)
					return ret;
			}
		} else {
			static const struct lan887x_regwr_map phy_cfg[] = {
				{MDIO_MMD_PMAPMD, 0x808B, 0x0038},
				{MDIO_MMD_VEND1, 0x0422, 0x0014},
			};

			for (i = 0; i < ARRAY_SIZE(phy_cfg); i++) {
				ret = phy_write_mmd(phydev, phy_cfg[i].mmd,
						    phy_cfg[i].reg, phy_cfg[i].val);
				if (ret < 0)
					return ret;
			}
		}
	} else { //Aneg
		static const struct lan887x_regwr_map phy_cfg[] = {
			{MDIO_MMD_PMAPMD, 0x808B, 0x00B8},
			{MDIO_MMD_PMAPMD, 0x80B0, 0x0038},
			{MDIO_MMD_VEND1, 0x0422, 0x000F},
		};

		for (i = 0; i < ARRAY_SIZE(phy_cfg); i++) {
			ret = phy_write_mmd(phydev, phy_cfg[i].mmd, phy_cfg[i].reg, phy_cfg[i].val);
			if (ret < 0)
				return ret;
		}
	}

	ret = phy_set_bits_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MIS_100T1_SMI_REG26,
			       LAN887X_MIS_100T1_SMI_HW_INIT_SEQ_EN);
	if (ret < 0)
		return ret;

	return 0;
}

static int lan887x_1000m_scripts(struct phy_device *phydev)
{
	static const struct lan887x_regwr_map phy_cfg[] = {
		{MDIO_MMD_PMAPMD, 0x80B0, 0x003F},
		{MDIO_MMD_PMAPMD, 0x808B, 0x00B8},
	};
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(phy_cfg); i++) {
		ret = phy_write_mmd(phydev, phy_cfg[i].mmd, phy_cfg[i].reg, phy_cfg[i].val);
		if (ret < 0)
			return ret;
	}

	if (phydev->autoneg == AUTONEG_ENABLE) {
		ret = phy_clear_bits_mmd(phydev, MDIO_MMD_PMAPMD,
					 LAN887X_PMA_1000T1_DSP_PMA_CTL_REG,
					 LAN887X_PMA_1000T1_DSP_PMA_LNK_SYNC);
		if (ret < 0)
			return ret;

		ret = phy_set_bits_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MIS_100T1_SMI_REG26,
				       LAN887X_MIS_100T1_SMI_HW_INIT_SEQ_EN);
		if (ret < 0)
			return ret;
	} else {
		ret = phy_set_bits_mmd(phydev, MDIO_MMD_PMAPMD, LAN887X_PMA_1000T1_DSP_PMA_CTL_REG,
				       LAN887X_PMA_1000T1_DSP_PMA_LNK_SYNC);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int lan887x_link_scripts(struct phy_device *phydev)
{
	int ret;

	//phydev_info(phydev, "PHY link_scripts\n");
	if (phydev->speed == SPEED_1000) {
		ret = lan887x_1000m_scripts(phydev);
		if (ret < 0)
			return ret;
	} else if (phydev->speed == SPEED_100) {
		ret = lan887x_100m_scripts(phydev);
		if (ret < 0)
			return ret;
	}
	//phydev_info(phydev, "PHY link(%s)_scripts done\n",
	//	    phy_speed_to_str(phydev->speed));

	return 0;
}

static int lan887x_phy_reset(struct phy_device *phydev)
{
	int ret, val;

	//phydev_info(phydev, "PHY soft-reset\n");
	// clear 1000m link sync
	ret = phy_clear_bits_mmd(phydev, MDIO_MMD_PMAPMD, LAN887X_PMA_1000T1_DSP_PMA_CTL_REG,
				 LAN887X_PMA_1000T1_DSP_PMA_LNK_SYNC);
	if (ret < 0)
		return ret;

	// clear 100m link sync
	ret = phy_clear_bits_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MIS_100T1_SMI_REG26,
				 LAN887X_MIS_100T1_SMI_HW_INIT_SEQ_EN);
	if (ret < 0)
		return ret;

	// disable aneg
	ret = genphy_c45_an_disable_aneg(phydev);
	if (ret < 0)
		return ret;

	// chiptop soft-reset to allow the speed/mode change
	ret = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MX_CHIP_TOP_REG_SOFT_RST,
			    LAN887X_MX_CHIP_TOP_RESET_);
	if (ret < 0)
		return ret;

	// CL22 soft-reset to let the link re-train
	ret = phy_modify(phydev, MII_BMCR, BMCR_RESET, BMCR_RESET);
	if (ret < 0)
		return ret;

	// Timeout if >10ms
	ret = phy_read_poll_timeout(phydev, MII_BMCR, val, !(val & BMCR_RESET),
				    5000, 10000, true);
	if (ret)
		return ret;

	//phydev_info(phydev, "PHY soft-reset done\n");

	return 0;
}

static int lan887x_phy_reconfig(struct phy_device *phydev)
{
	int ret;

	//phydev_info(phydev, "PHY reconfig\n");

	//Errata-949: soft-reset is required for switching speeds
	if (phydev->autoneg == AUTONEG_ENABLE) {
		ret = lan887x_config_advert(phydev);
	} else {
		linkmode_zero(phydev->advertising);
		ret = genphy_c45_config_aneg(phydev);
	}
	if (ret < 0)
		return ret;

	ret = lan887x_link_scripts(phydev);
	if (ret < 0)
		return ret;

	if (phydev->autoneg == AUTONEG_ENABLE) {
		ret = genphy_c45_restart_aneg(phydev);
		if (ret < 0)
			return ret;
	}

	//phydev_info(phydev, "PHY reconfig done!\n");

	return 0;
}

static int lan887x_config_aneg(struct phy_device *phydev)
{
	int ret;

	//phydev_info(phydev, "PHY config_aneg\n");
	phydev->duplex = DUPLEX_FULL;

	//Errata-949: soft-reset is required for switching speeds
	ret = lan887x_phy_reset(phydev);
	if (ret < 0)
		return ret;

	ret = lan887x_phy_reconfig(phydev);
	if (ret < 0)
		return ret;

	return 0;
}

static int lan887x_probe(struct phy_device *phydev)
{
	const struct lan887x_type *type = phydev->drv->driver_data;
	struct lan887x_priv *priv;
	int ret;

	//phydev_info(phydev, "PHY probe\n");

	//Proceed with further initializations
	priv = devm_kzalloc(&phydev->mdio.dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	memset(priv->stats, 0, sizeof(priv->stats));

	priv->type = type;
	phydev->priv = priv;
	phydev->duplex = DUPLEX_FULL;
	phydev->interrupts = PHY_INTERRUPT_ENABLED;
	phydev->autoneg = AUTONEG_ENABLE;
	phydev->master_slave_set = MASTER_SLAVE_CFG_MASTER_PREFERRED;
	phydev->speed = SPEED_1000;

	ret = lan887x_phy_setup(phydev);
	if (ret < 0)
		return ret;

	//phydev_info(phydev, "PHY probe done!\n");

	return 0;
}

static u64 lan887x_get_stat(struct phy_device *phydev, int i)
{
	struct lan887x_hw_stat stat = lan887x_hw_stats[i];
	struct lan887x_priv *priv = phydev->priv;
	int val;
	u64 ret;

	if (stat.mmd)
		val = phy_read_mmd(phydev, stat.mmd, stat.reg);
	else
		val = phy_read(phydev, stat.reg);

	if (val < 0) {
		ret = U64_MAX;
	} else {
		val = val & ((1 << stat.bits) - 1);
		priv->stats[i] += val;
		ret = priv->stats[i];
	}

	return ret;
}

static void lan887x_get_stats(struct phy_device *phydev,
			      struct ethtool_stats *stats, u64 *data)
{
	int i, idx = 0;

	for (i = 0; i < ARRAY_SIZE(lan887x_hw_stats); i++)
		data[idx++] = lan887x_get_stat(phydev, i);
}

static int lan887x_get_sset_count(struct phy_device *phydev)
{
	return ARRAY_SIZE(lan887x_hw_stats);
}

static void lan887x_get_strings(struct phy_device *phydev, u8 *data)
{
	int i;

	// Hardware stats
	for (i = 0; i < ARRAY_SIZE(lan887x_hw_stats); i++) {
		strncpy(data + i * ETH_GSTRING_LEN,
			lan887x_hw_stats[i].string, ETH_GSTRING_LEN);
	}
}

static int lan887x_config_intr(struct phy_device *phydev)
{
	int ret;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		/* unmask all source and clear them before enable */
		ret = phy_read_mmd(phydev, MDIO_MMD_VEND1,
				   LAN887X_MX_CHIP_TOP_REG_INT_STS);
		if (ret < 0)
			return ret;

		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1,
				    LAN887X_MX_CHIP_TOP_REG_INT_MSK,
				    (u16)~LAN887X_MX_CHIP_TOP_ALL_MSK);
	} else {
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1,
				    LAN887X_MX_CHIP_TOP_REG_INT_MSK, LAN887X_DEF_MASK);
		if (ret < 0)
			return ret;

		ret = phy_read_mmd(phydev, MDIO_MMD_VEND1,
				   LAN887X_MX_CHIP_TOP_REG_INT_STS);
	}
	if (ret < 0)
		return ret;

	return 0;
}

static irqreturn_t lan887x_handle_interrupt(struct phy_device *phydev)
{
	int irq_status, ptp_irq_status;

	irq_status = phy_read_mmd(phydev, MDIO_MMD_VEND1,
				  LAN887X_MX_CHIP_TOP_REG_INT_STS);
	if (irq_status < 0) {
		phy_error(phydev);
		return IRQ_NONE;
	}

	if (irq_status & LAN887X_MX_CHIP_TOP_LINK_MSK) {
		//phydev_info(phydev, "LINK_INT_MSK=0x%x\n", irq_status);
		phy_trigger_machine(phydev);
	}

	while ((ptp_irq_status = phy_read_mmd(phydev, MDIO_MMD_VEND1, LAN887X_PTP_INT_STS)) > 0)
		lan887x_handle_ptp_interrupt(phydev, ptp_irq_status);

	return IRQ_HANDLED;
}

static void lan887x_remove(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct lan887x_priv *priv = phydev->priv;
	struct lan887x_ptp_priv *ptp_priv = &priv->ptp_priv;
	int i;

	// unregister clock
	if (ptp_priv->ptp_clock)
		ptp_clock_unregister(ptp_priv->ptp_clock);

	// Flushout stats
	for (i = 0; i < ARRAY_SIZE(lan887x_hw_stats); i++)
		lan887x_get_stat(phydev, i);

	// delete private data
	if (priv)
		devm_kfree(dev, priv);
}

static int lan887x_loopback(struct phy_device *phydev, bool enable)
{
	// Enable/Disable far-end loopback
	if (enable)
		return phy_set_bits_mmd(phydev, MDIO_MMD_VEND1,
					LAN887X_MIS_CFG_REG2,
					LAN887X_MIS_CFG_REG2_FE_LPBK_EN);
	else
		return phy_clear_bits_mmd(phydev, MDIO_MMD_VEND1,
					  LAN887X_MIS_CFG_REG2,
					  LAN887X_MIS_CFG_REG2_FE_LPBK_EN);
}

static int lan887x_get_sqi_100m(struct phy_device *phydev)
{
	u16 temp;
	int i, j;
	u32 sqiavg = 0, linkavg = 0;
	u32 sqinum = 0;
	u16 rawtable[200];
	u16 linktable[200];

	memset(rawtable, 0, sizeof(rawtable));
	memset(linktable, 1, sizeof(linktable));

	// method 1 config
	phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x404, 0x16D6);
	phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x42E, 0x9572);

	temp = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x042E);

	if (temp != 0x9572) {
		pr_info("not configured properly\n");
		return 0;
	}

	phy_modify_mmd(phydev, MDIO_MMD_VEND1, 0x40d, 0x0001, 0x0001);

	msleep(50);

	// get 200 raw readings
	for (i = 0; i < 200; i++) {
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x40d, 0x0001);

		temp = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x483);
		rawtable[i] = temp;
		linktable[i] = phydev->link;
	}
	// sort raw sqi values in ascending order
	for (i = 0; i < 200; i++) {
		for (j = 0; j < 200; j++) {
			if (rawtable[j] > rawtable[i]) {
				u16 linktemp;

				temp = rawtable[i]; // sqi values
				rawtable[i] = rawtable[j];
				rawtable[j] = temp;

				linktemp = linktable[i]; // link statuses
				linktable[i] = linktable[j];
				linktable[j] = linktemp;
			}
		}
	}
	// sum middle 120 values
	for (i = 0; i < 200; i++) {
		if (i >= 40 && i < 160)
			sqiavg += rawtable[i];
		linkavg += linktable[i];
	}

	// get averages
	sqiavg /= 120;
	linkavg /= 200;

	if (sqiavg >= 299)
		sqinum = 0;
	else if ((sqiavg < 299) && (sqiavg >= 237))
		sqinum = 1;
	else if ((sqiavg < 237) && (sqiavg >= 189))
		sqinum = 2;
	else if ((sqiavg < 189) && (sqiavg >= 150))
		sqinum = 3;
	else if ((sqiavg < 150) && (sqiavg >= 119))
		sqinum = 4;
	else if ((sqiavg < 119) && (sqiavg >= 94))
		sqinum = 5;
	else if ((sqiavg < 94) && (sqiavg >= 75))
		sqinum = 6;
	else if ((sqiavg < 75) && (sqiavg >= 0))
		sqinum = 7;
	else
		sqinum = 8;

	if (linkavg < 1)
		sqinum = 0;

	return sqinum;
}

static int lan887x_get_sqi(struct phy_device *phydev)
{
	int rc, count = 0;
	u8 sqi_value = 0;

	// If link is down
	if (!phydev->link) {
		//phydev_info(phydev, "Link is Down\n");
		goto done;
	}

	if (phydev->speed != SPEED_1000 &&
	    phydev->speed != SPEED_100) {
		//phydev_info(phydev, "Invalid speed\n");
		goto done;
	}

	if (phydev->speed == SPEED_100)
		return lan887x_get_sqi_100m(phydev);

	rc = phy_set_bits_mmd(phydev, MDIO_MMD_VEND1,
			      LAN887X_DSP_REGS_COEFF_MOD_CONFIG,
			      LAN887X_DSP_REGS_COEFF_MOD_CONFIG_DCQ_COEFF_EN);
	if (rc < 0)
		return rc;

	/* Waiting time for register to get clear */
	do {
		usleep_range(10, 20);

		rc = phy_read_mmd(phydev, MDIO_MMD_VEND1,
				  LAN887X_DSP_REGS_COEFF_MOD_CONFIG);
		if (rc < 0)
			return rc;
		else if ((rc & 0x0100) != 0x0100)
			break;

		count++;
	} while (count < 10);

	if (count >= 10)
		goto done;

	rc = phy_read_mmd(phydev, MDIO_MMD_VEND1,
			  LAN887X_DSP_REGS_DCQ_SQI_STATUS);
	if (rc < 0)
		return rc;

	sqi_value = FIELD_GET(T1_DCQ_SQI_MSK, rc);

done:
	return sqi_value;
}

static int lan887x_cable_test_start_common(struct phy_device *phydev, bool is_hybrid)
{
	static const struct lan887x_regwr_map values[] = {
		{MDIO_MMD_VEND1, LAN887X_DSP_REGS_MAX_PGA_GAIN_100, 0x1F},
		{MDIO_MMD_VEND1, LAN887X_DSP_REGS_MIN_PGA_GAIN_100, 0x0},
		{MDIO_MMD_VEND1, LAN887X_DSP_REGS_CBL_DIAG_TDR_THRESH_100, 0x1},
		{MDIO_MMD_VEND1, LAN887X_DSP_REGS_CBL_DIAG_AGC_THRESH_100, 0x3c},
		{MDIO_MMD_VEND1, LAN887X_DSP_REGS_CBL_DIAG_MIN_WAIT_CONFIG_100, 0x0},
		{MDIO_MMD_VEND1, LAN887X_DSP_REGS_CBL_DIAG_MAX_WAIT_CONFIG_100, 0x46},
		{MDIO_MMD_VEND1, LAN887X_DSP_REGS_CBL_DIAG_CYC_CONFIG_100, 0x5A},
		{MDIO_MMD_VEND1, LAN887X_DSP_REGS_CBL_DIAG_TX_PULSE_CONFIG_100, 0x44D5},
		{MDIO_MMD_VEND1, LAN887X_DSP_REGS_CBL_DIAG_MIN_PGA_GAIN_100, 0x0},
	};
	int rc, i;

	rc = lan887x_cd_reset(phydev, false);
	if (rc < 0)
		return rc;

	/* Forcing DUT to master mode, avoids headaches and
	 * we don't care about mode during diagnostics
	 */
	//force master mode
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, LAN887X_PMA_COMM_100T1_CTL_T1,
		      LAN887X_PMA_COMM_100T1_CTL_T1_MAS_SLV_CFG_VAL);
	//100T1
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, 0x80B0, 0x0000);
	phy_modify_mmd(phydev, MDIO_MMD_VEND1, 0x437, 0x0, 0x0038);

	for (i = 0; i < ARRAY_SIZE(values); i++) {
		u16 val = values[i].val;

		rc = phy_write_mmd(phydev, values[i].mmd, values[i].reg, val);
		if (rc < 0)
			return rc;

		if (is_hybrid && LAN887X_DSP_REGS_CBL_DIAG_MAX_WAIT_CONFIG_100 == values[i].reg) {
			val = 0xA;
			rc = phy_write_mmd(phydev, values[i].mmd, values[i].reg, val);
			if (rc < 0)
				return rc;
		}
	}

	if (is_hybrid)
		phy_modify_mmd(phydev, MDIO_MMD_PMAPMD,
			       LAN887X_T1_AFE_PORT_TESTBUS_CTRL4_REG, 0x0001, 0x0001);

	/* HW_INIT 100T1, Get DUT running in 100T1 mode */
	rc = phy_modify_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MIS_100T1_SMI_REG26,
			    LAN887X_MIS_100T1_SMI_HW_INIT_SEQ_EN,
			    LAN887X_MIS_100T1_SMI_HW_INIT_SEQ_EN);
	if (rc < 0)
		return rc;

	/* wait 50ms */
	msleep(50);

	return 0;
}

static int lan887x_cable_test_en(struct phy_device *phydev, bool is_hybrid)
{
	int test_done = 0, time_out = 10;
	int rc = 0;

	rc = lan887x_cable_test_start_common(phydev, is_hybrid);
	if (rc < 0)
		return rc;

	/* start cable diag*/
	rc = phy_write_mmd(phydev, MDIO_MMD_VEND1,
			   LAN887X_DSP_REGS_START_CBL_DIAG_100, 0x01);
	if (rc < 0)
		return rc;

	/* wait for cable diag to complete */
	while (time_out) {
		msleep(50);
		rc = phy_read_mmd(phydev, MDIO_MMD_VEND1,
				  LAN887X_DSP_REGS_START_CBL_DIAG_100);
		if (rc < 0)
			return rc;

		if ((rc & 2) == 2) {
			test_done = 1;
			break;
		}
		time_out--;
	}

	if (!test_done) {
		rc = lan887x_cd_reset(phydev, true);
		if (rc < 0)
			return rc;

		phydev_info(phydev, "PHY cd_test not done!\n");
		return -1;
	}

	/* stop cable diag*/
	rc = phy_write_mmd(phydev, MDIO_MMD_VEND1,
			   LAN887X_DSP_REGS_START_CBL_DIAG_100, 0x00);
	if (rc < 0)
		return rc;

	return 0;
}

static int lan887x_cable_test_start(struct phy_device *phydev)
{
	return 0;
}

static int lan887x_cable_test_report(struct phy_device *phydev)
{
	u16 pos_peak_cycle = 0, pos_peak_cycle_hybrid = 0, pos_peak_in_phases = 0;
	u16 neg_peak_cycle = 0, neg_peak_in_phases = 0, neg_peak_phase = 0;
	u16 noise_margin = 20, time_margin = 89;
	u16 min_time_diff = 96, max_time_diff = 96 + time_margin;
	u16 pos_peak_time, pos_peak_time_hybrid, neg_peak_time;
	u16 pos_peak_phase = 0, pos_peak_phase_hybrid = 0;
	bool is_hybrid = true;
	u16 pos_peak_in_phases_hybrid = 0;
	u16 gain_idx, gain_idx_hybrid;
	u16 pos_peak, neg_peak;
	u16 distance = -1;
	u16 detect = -1;
	u16 rc;
	u32 length;

	/* read non-hybrid results */
	gain_idx = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x497);
	pos_peak = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x499);
	neg_peak = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x49A);
	pos_peak_time = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x49C);
	neg_peak_time = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x49D);

	/* calculate non-hybrid values */
	pos_peak_cycle = (pos_peak_time >> 7) & 0x7F;
	pos_peak_phase = pos_peak_time & 0x7F;
	pos_peak_in_phases = (pos_peak_cycle * 96) + pos_peak_phase;
	neg_peak_cycle = (neg_peak_time >> 7) & 0x7F;
	neg_peak_phase = neg_peak_time & 0x7F;
	neg_peak_in_phases = (neg_peak_cycle * 96) + neg_peak_phase;

	/* Deriving the status of cable */
	if (pos_peak > noise_margin && neg_peak > noise_margin && gain_idx >= 0) {
		if (pos_peak_in_phases > neg_peak_in_phases &&
		    ((pos_peak_in_phases - neg_peak_in_phases) >= min_time_diff) &&
		    ((pos_peak_in_phases - neg_peak_in_phases) < max_time_diff) &&
		    pos_peak_in_phases > 0) {
			detect = LAN87XX_CABLE_TEST_SAME_SHORT;
		} else if (neg_peak_in_phases > pos_peak_in_phases &&
			   ((neg_peak_in_phases - pos_peak_in_phases) >= min_time_diff) &&
			   ((neg_peak_in_phases - pos_peak_in_phases) < max_time_diff) &&
			   neg_peak_in_phases > 0) {
			detect = LAN87XX_CABLE_TEST_OPEN;
		} else {
			detect = LAN87XX_CABLE_TEST_OK;
		}
	} else {
		detect = LAN87XX_CABLE_TEST_OK;
	}

	if (detect == LAN87XX_CABLE_TEST_OK) {
		distance = 0;

		rc = lan887x_cd_reset(phydev, true);
		if (rc < 0)
			return rc;

		phydev_info(phydev, "PHY cd_test LINK_OK!\n");

		goto get_len;
	}

	// initate cable diag test
	rc = lan887x_cable_test_en(phydev, is_hybrid);
	if (rc < 0)
		return rc;

	/* read hybrid results */
	gain_idx_hybrid = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x497);
	pos_peak_time_hybrid = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x49C);

	/* calculate hybrid values */
	pos_peak_cycle_hybrid = (pos_peak_time_hybrid >> 7) & 0x7F;
	pos_peak_phase_hybrid = pos_peak_time_hybrid & 0x7F;
	pos_peak_in_phases_hybrid = (pos_peak_cycle_hybrid * 96) + pos_peak_phase_hybrid;

	/* float wavePropagationVelocity = 0.6811 * 2.9979;
	 * distance = (neg_peak_in_phases - pos_peak_in_phases) * 156.2499 *
	 * 0.0001 * wavePropagationVelocity * 0.5;
	 * distance = (neg_peak_in_phases - pos_peak_in_phases)
	 * * 0.0159520967437766;
	 */
	if (detect == LAN87XX_CABLE_TEST_OPEN) {
		distance = (((pos_peak_in_phases - pos_peak_in_phases_hybrid)
			    * 15953) / 10000);
	} else if (detect == LAN87XX_CABLE_TEST_SAME_SHORT) {
		distance = (((neg_peak_in_phases - pos_peak_in_phases_hybrid)
			    * 15953) / 10000);
	} else {
		distance = 0;
	}

	rc = lan887x_cd_reset(phydev, true);
	if (rc < 0)
		return rc;

get_len:
	length = ((u32)distance & 0xFFFF);
	ethnl_cable_test_result(phydev, ETHTOOL_A_CABLE_PAIR_A,
				lan87xx_cable_test_report_trans(detect));
	ethnl_cable_test_fault_length(phydev, ETHTOOL_A_CABLE_PAIR_A, length);

	return 0;
}

static int lan887x_cable_test_get_status(struct phy_device *phydev,
					 bool *finished)
{
	int rc = 0;

	*finished = false;

	if (phydev->link) {
		ethnl_cable_test_result(phydev, ETHTOOL_A_CABLE_PAIR_A,
					lan87xx_cable_test_report_trans(0));
		ethnl_cable_test_fault_length(phydev, ETHTOOL_A_CABLE_PAIR_A, 0);
	} else {
		// initate cable diag test
		rc = lan887x_cable_test_en(phydev, false);
		if (rc < 0)
			return rc;

		rc = lan887x_cable_test_report(phydev);
		if (rc < 0)
			return rc;
	}

	//cable diag test complete
	*finished = true;

	return 0;
}

static int lan887x_get_tc10_state(struct phy_device *phydev)
{
	int rc;

	rc = phy_read_mmd(phydev, MDIO_MMD_VEND1,
			  LAN887X_TC10_DEV30_COMMON_MISC34);
	if (rc < 0)
		return rc;

	return FIELD_GET(LAN887X_TC10_DEV30_COMMON_MISC34_CUR_STATE, rc) - 1;
}

static int lan887x_set_tc10(struct phy_device *phydev, u8 tc10)
{
	int rc;

	//phydev_dbg(phydev, "Mode value is %d\n", tc10);

	if (tc10 == ETHTOOL_A_TC10_SLEEP) {
		rc = phy_write_mmd(phydev, MDIO_MMD_VEND1,
				   LAN887X_DEV30_COMMON_TC10_REG_REG16,
				   LAN887X_DEV30_COMMON_TC10_REG_REG16_RW_SEND_LPS);
	} else {
		rc = phy_write_mmd(phydev, MDIO_MMD_VEND1,
				   LAN887X_DEV30_COMMON_TC10_REG_REG16,
				   LAN887X_DEV30_COMMON_TC10_REG_REG16_RW_SEND_WUR);
	}

	return rc;
}

static void lan887x_phy_presetup(struct phy_device *phydev)
{
	static const struct lan887x_regwr_map phy_init[] = {
		{MDIO_MMD_VEND1, 0xF003, 0x0008},
		{MDIO_MMD_VEND1, 0xF005, 0x0010},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(phy_init); i++) {
		phy_modify_mmd(phydev, phy_init[i].mmd,
			       phy_init[i].reg, phy_init[i].val, phy_init[i].val);
	}
}

static int lan887x_match_phy_device(struct phy_device *phydev)
{
	if (IS_LAN887X_B0(phydev->phy_id))
		goto ret_true;
	else if (IS_LAN887X_B0_PROTOS(phydev->phy_id))
		goto ret_prep;
	else
		goto ret_false; // !(vendor + revision) i.e.  SMSC + 1

ret_prep:
	lan887x_phy_presetup(phydev);

	return true;

ret_true:
	return true;

ret_false:
	return false;
}

static int lan887x_cd_reset(struct phy_device *phydev, bool cd_done)
{
	int rc;
	u16 val;

	//phydev_dbg(phydev, "PHY cd_reset\n");

	rc = phy_write_mmd(phydev, MDIO_MMD_VEND1, LAN887X_MX_CHIP_TOP_REG_HARD_RST,
			   LAN887X_MX_CHIP_TOP_RESET_);
	if (rc < 0)
		return rc;

	/* wait 50ms */
	rc = phy_read_poll_timeout(phydev, MII_PHYSID2, val, ((val & 0xf002U) == 0xC002U),
				   5000, 50000, true);
	phydev_info(phydev, "PHY cd_test poll: rc=%d, val=0x%x!\n", rc, val);
	if (rc < 0)
		return rc;

	if (cd_done) {
		//phydev_dbg(phydev, "PHY cd_done\n");
		if (IS_LAN887X_B0_PROTOS(phydev->phy_id))
			lan887x_phy_presetup(phydev);

		rc = lan887x_phy_setup(phydev);
		if (rc < 0)
			return rc;

		rc = lan887x_phy_init(phydev);
		if (rc < 0)
			return rc;

		rc = lan887x_config_intr(phydev);
		if (rc < 0)
			return rc;

		rc = lan887x_phy_reconfig(phydev);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int lan887x_read_status(struct phy_device *phydev)
{
	int ret;

	ret = genphy_c45_read_status(phydev);
	if (ret < 0)
		return ret;

	phydev->duplex = DUPLEX_FULL;
	phydev->pause = 0;
	phydev->asym_pause = 0;

	return 0;
}

/* LAN887X End */

static struct phy_driver microchip_t1_phy_driver[] = {
	{
		PHY_ID_MATCH_MODEL(PHY_ID_LAN87XX),
		.name           = "Microchip LAN87xx T1",
		.flags          = PHY_POLL_CABLE_TEST,
		.features       = PHY_BASIC_T1_FEATURES,
		.config_init	= lan87xx_config_init,
		.config_intr    = lan87xx_phy_config_intr,
		.handle_interrupt = lan87xx_handle_interrupt,
		.suspend        = genphy_suspend,
		.resume         = genphy_resume,
		.config_aneg    = lan87xx_config_aneg,
		.read_status	= lan87xx_read_status,
		.get_sqi	= lan87xx_get_sqi,
		.get_sqi_max	= lan87xx_get_sqi_max,
		.cable_test_start = lan87xx_cable_test_start,
		.cable_test_get_status = lan87xx_cable_test_get_status,
	},
	{
		PHY_ID_MATCH_MODEL(PHY_ID_LAN937X),
		.name		= "Microchip LAN937x T1",
		.flags          = PHY_POLL_CABLE_TEST,
		.features	= PHY_BASIC_T1_FEATURES,
		.config_init	= lan87xx_config_init,
		.config_intr    = lan87xx_phy_config_intr,
		.handle_interrupt = lan87xx_handle_interrupt,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.config_aneg    = lan87xx_config_aneg,
		.read_status	= lan87xx_read_status,
		.get_sqi	= lan87xx_get_sqi,
		.get_sqi_max	= lan87xx_get_sqi_max,
		.cable_test_start = lan87xx_cable_test_start,
		.cable_test_get_status = lan87xx_cable_test_get_status,
	},
	{
		//PHY_ID_MATCH_MODEL(PHY_ID_LAN887X),
		.phy_id         = PHY_ID_LAN887X_ALL,
		.phy_id_mask    = PHY_ID_LAN887X_MSK,
		.name		= "Microchip LAN8870 T1 PHY",
		.flags          = PHY_POLL_CABLE_TEST,
		/* PHY_GBIT_FEATURES */
		.probe		= lan887x_probe,
		.match_phy_device = lan887x_match_phy_device,
		.get_features	= lan887x_get_features,
		.config_init    = lan887x_config_init,
		.config_intr    = lan887x_config_intr,
		.handle_interrupt = lan887x_handle_interrupt,
		.config_aneg    = lan887x_config_aneg,
		.get_stats      = lan887x_get_stats,
		.get_sset_count = lan887x_get_sset_count,
		.get_strings    = lan887x_get_strings,
		.remove		= lan887x_remove,
		.set_loopback   = lan887x_loopback,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_status	= lan887x_read_status,
		.get_sqi	= lan887x_get_sqi,
		.get_sqi_max	= lan87xx_get_sqi_max,
		.cable_test_start = lan887x_cable_test_start,
		.cable_test_get_status = lan887x_cable_test_get_status,
		.get_tc10_state	= lan887x_get_tc10_state,
		.set_tc10       = lan887x_set_tc10,
	}
};

module_phy_driver(microchip_t1_phy_driver);

static struct mdio_device_id __maybe_unused microchip_t1_tbl[] = {
	{ PHY_ID_MATCH_MODEL(PHY_ID_LAN87XX) },
	{ PHY_ID_MATCH_MODEL(PHY_ID_LAN937X) },
	//{ PHY_ID_MATCH_MODEL(PHY_ID_LAN887X) },
	{ PHY_ID_LAN887X_ALL, PHY_ID_LAN887X_MSK },
	{ }
};

MODULE_DEVICE_TABLE(mdio, microchip_t1_tbl);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
