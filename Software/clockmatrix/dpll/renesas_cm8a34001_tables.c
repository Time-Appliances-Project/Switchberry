
/* renesas_cm8a34001_tables.c - generated tables */
#include "renesas_cm8a34001_tables.h"
#include <string.h>
#include <stdio.h>

static const uint16_t cm_Status_bases[1] = {0xC03C};
static const cm_field_desc_t cm_Status_reg_0_fields[3] = {
  { CM_NAME_STR("RESERVED"), 4, 4 },
  { CM_NAME_STR("I2CM_SPEED"), 2, 2 },
  { CM_NAME_STR("I2CM_PORT_SEL"), 0, 2 },
};
static const cm_field_desc_t cm_Status_reg_1_fields[3] = {
  { CM_NAME_STR("RESERVED"), 3, 5 },
  { CM_NAME_STR("ADDRESS_SIZE"), 2, 1 },
  { CM_NAME_STR("MODE"), 0, 2 },
};
static const cm_field_desc_t cm_Status_reg_2_fields[5] = {
  { CM_NAME_STR("RESERVED"), 5, 3 },
  { CM_NAME_STR("SPI_SDO_DELAY"), 4, 1 },
  { CM_NAME_STR("SPI_CLOCK_SELECTION"), 3, 1 },
  { CM_NAME_STR("SPI_DUPLEX_MODE"), 2, 1 },
  { CM_NAME_STR("RESERVED_0"), 0, 2 },
};
static const cm_field_desc_t cm_Status_reg_3_fields[2] = {
  { CM_NAME_STR("RESERVED"), 7, 1 },
  { CM_NAME_STR("DEVICE_ADDRESS"), 0, 7 },
};
static const cm_field_desc_t cm_Status_reg_4_fields[3] = {
  { CM_NAME_STR("RESERVED"), 3, 5 },
  { CM_NAME_STR("ADDRESS_SIZE"), 2, 1 },
  { CM_NAME_STR("MODE"), 0, 2 },
};
static const cm_field_desc_t cm_Status_reg_5_fields[5] = {
  { CM_NAME_STR("RESERVED"), 5, 3 },
  { CM_NAME_STR("SPI_SDO_DELAY"), 4, 1 },
  { CM_NAME_STR("SPI_CLOCK_SELECTION"), 3, 1 },
  { CM_NAME_STR("SPI_DUPLEX_MODE"), 2, 1 },
  { CM_NAME_STR("RESERVED_0"), 0, 2 },
};
static const cm_field_desc_t cm_Status_reg_6_fields[2] = {
  { CM_NAME_STR("RESERVED"), 7, 1 },
  { CM_NAME_STR("DEVICE_ADDRESS"), 0, 7 },
};
static const cm_field_desc_t cm_Status_reg_7_fields[1] = { { CM_NAME_STR(""), 0, 0 } };
static const cm_field_desc_t cm_Status_reg_8_fields[4] = {
  { CM_NAME_STR("RESERVED"), 6, 2 },
  { CM_NAME_STR("DPLL_SYS_HOLDOVER_STATE_CHANGE_STICKY"), 5, 1 },
  { CM_NAME_STR("DPLL_SYS_LOCK_STATE_CHANGE_STICKY"), 4, 1 },
  { CM_NAME_STR("DPLL_SYS_STATE"), 0, 4 },
};
static const cm_field_desc_t cm_Status_reg_9_fields[2] = {
  { CM_NAME_STR("RESERVED"), 5, 3 },
  { CM_NAME_STR("DPLL{num}_INPUT"), 0, 5 },
};
static const cm_field_desc_t cm_Status_reg_10_fields[2] = {
  { CM_NAME_STR("RESERVED"), 5, 3 },
  { CM_NAME_STR("DPLL_SYS_INPUT"), 0, 5 },
};
static const cm_field_desc_t cm_Status_reg_13_fields[1] = { { CM_NAME_STR(""), 0, 0 } };
static const cm_field_desc_t cm_Status_reg_14_fields[1] = { { CM_NAME_STR(""), 0, 0 } };
static const cm_field_desc_t cm_Status_reg_15_fields[1] = {
  { CM_NAME_STR("FFO_7_0"), 0, 8 },
};
static const cm_field_desc_t cm_Status_reg_16_fields[2] = {
  { CM_NAME_STR("FFO_UNIT"), 6, 2 },
  { CM_NAME_STR("FFO_13:8"), 0, 6 },
};
static const cm_field_desc_t cm_Status_reg_11_fields[1] = {
  { CM_NAME_STR("FILTER_STATUS"), 0, 8 },
};
static const cm_field_desc_t cm_Status_reg_phase_status_fields[1] = {
  { CM_NAME_STR("PHASE_STATUS"), 0, 8 },
};
static const cm_field_desc_t cm_Status_reg_dpll_fields[3] = {
  { CM_NAME_STR("HOLDOVER_STATE_CHANGE_STICKY"), 5, 1 },
  { CM_NAME_STR("LOCK_STATE_CHANGE_STICKY"), 4, 1 },
  { CM_NAME_STR("DPLL_STATE"), 0, 4 }
};
static const cm_reg_desc_t cm_Status_regs[] = {
  { CM_NAME_STR("I2CM_STATUS"), 0x000, cm_Status_reg_0_fields, 3 },
  { CM_NAME_STR("SER0_STATUS"), 0x002, cm_Status_reg_1_fields, 3 },
  { CM_NAME_STR("SER0_SPI_STATUS"), 0x003, cm_Status_reg_2_fields, 5 },
  { CM_NAME_STR("SER0_I2C_STATUS"), 0x004, cm_Status_reg_3_fields, 2 },
  { CM_NAME_STR("SER1_STATUS"), 0x005, cm_Status_reg_4_fields, 3 },
  { CM_NAME_STR("SER1_SPI_STATUS"), 0x006, cm_Status_reg_5_fields, 5 },
  { CM_NAME_STR("SER1_I2C_STATUS"), 0x007, cm_Status_reg_6_fields, 2 },
  { CM_NAME_STR("IN{num}_MON_STATUS"), 0x008, cm_Status_reg_7_fields, 0 },
  { CM_NAME_STR("DPLL0_STATUS"), 0x018, cm_Status_reg_dpll_fields, 3 },
  { CM_NAME_STR("DPLL1_STATUS"), 0x019, cm_Status_reg_dpll_fields, 3 },
  { CM_NAME_STR("DPLL2_STATUS"), 0x01a, cm_Status_reg_dpll_fields, 3 },
  { CM_NAME_STR("DPLL3_STATUS"), 0x01b, cm_Status_reg_dpll_fields, 3 },
  { CM_NAME_STR("DPLL4_STATUS"), 0x01c, cm_Status_reg_dpll_fields, 3 },
  { CM_NAME_STR("DPLL5_STATUS"), 0x01d, cm_Status_reg_dpll_fields, 3 },
  { CM_NAME_STR("DPLL6_STATUS"), 0x01e, cm_Status_reg_dpll_fields, 3 },
  { CM_NAME_STR("DPLL7_STATUS"), 0x01f, cm_Status_reg_dpll_fields, 3 },
  { CM_NAME_STR("SYS_DPLL"), 0x020, cm_Status_reg_8_fields, 4 },
  { CM_NAME_STR("DPLL{num}_REF_STATUS"), 0x022, cm_Status_reg_9_fields, 2 },
  { CM_NAME_STR("DPLL_SYS_REF_STATUS"), 0x02A, cm_Status_reg_10_fields, 2 },
  { CM_NAME_STR("DPLL0_FILTER_STATUS"), 0x044, cm_Status_reg_11_fields, 1 },
  { CM_NAME_STR("DPLL1_FILTER_STATUS"), 0x04c, cm_Status_reg_11_fields, 1 },
  { CM_NAME_STR("DPLL2_FILTER_STATUS"), 0x054, cm_Status_reg_11_fields, 1 },
  { CM_NAME_STR("DPLL3_FILTER_STATUS"), 0x05c, cm_Status_reg_11_fields, 1 },
  { CM_NAME_STR("DPLL4_FILTER_STATUS"), 0x064, cm_Status_reg_11_fields, 1 },
  { CM_NAME_STR("DPLL5_FILTER_STATUS"), 0x06c, cm_Status_reg_11_fields, 1 },
  { CM_NAME_STR("DPLL6_FILTER_STATUS"), 0x074, cm_Status_reg_11_fields, 1 },
  { CM_NAME_STR("DPLL7_FILTER_STATUS"), 0x07c, cm_Status_reg_11_fields, 1 },
  { CM_NAME_STR("DPLL_SYS_FILTER_STATUS"), 0x084, cm_Status_reg_11_fields, 1 },



  { CM_NAME_STR("USER_GPIO0_TO_7_STATUS"), 0x08A, cm_Status_reg_13_fields, 0 },
  { CM_NAME_STR("USER_GPIO8_TO_15_STATUS"), 0x08B, cm_Status_reg_14_fields, 0 },
  { CM_NAME_STR("IN{num}_MON_FREQ_STATUS_0"), 0x08C, cm_Status_reg_15_fields, 1 },
  { CM_NAME_STR("IN{num}_MON_FREQ_STATUS_1"), 0x08D, cm_Status_reg_16_fields, 2 },
{ CM_NAME_STR("DPLL0_PHASE_STATUS"), 0x0DC, cm_Status_reg_phase_status_fields, 1 },
{ CM_NAME_STR("DPLL1_PHASE_STATUS"), 0x0E4, cm_Status_reg_phase_status_fields, 1 },
{ CM_NAME_STR("DPLL2_PHASE_STATUS"), 0x0EC, cm_Status_reg_phase_status_fields, 1 },
{ CM_NAME_STR("DPLL3_PHASE_STATUS"), 0x0F4, cm_Status_reg_phase_status_fields, 1 },
{ CM_NAME_STR("DPLL4_PHASE_STATUS"), 0x0FC, cm_Status_reg_phase_status_fields, 1 },
{ CM_NAME_STR("DPLL5_PHASE_STATUS"), 0x104, cm_Status_reg_phase_status_fields, 1 },
{ CM_NAME_STR("DPLL6_PHASE_STATUS"), 0x10C, cm_Status_reg_phase_status_fields, 1 },
{ CM_NAME_STR("DPLL7_PHASE_STATUS"), 0x114, cm_Status_reg_phase_status_fields, 1 },

};
const cm_module_desc_t cm_Status_module = { CM_NAME_STR("Status"), cm_Status_bases, 1, cm_Status_regs, (uint16_t)(sizeof(cm_Status_regs)/sizeof(cm_Status_regs[0])) };

static const uint16_t cm_PWMEncoder_bases[8] = {0xCB00, 0xCB08, 0xCB10, 0xCB18, 0xCB20, 0xCB28, 0xCB30, 0xCB38};
static const cm_field_desc_t cm_PWMEncoder_reg_0_fields[1] = {
  { CM_NAME_STR("ENCODER_ID"), 0, 8 },
};
static const cm_field_desc_t cm_PWMEncoder_reg_1_fields[3] = {
  { CM_NAME_STR("PPS_SEL"), 3, 1 },
  { CM_NAME_STR("SECONDARY_OUTPUT"), 2, 1 },
  { CM_NAME_STR("TOD_SEL"), 0, 2 },
};
static const cm_field_desc_t cm_PWMEncoder_reg_2_fields[4] = {
  { CM_NAME_STR("FIFTH_SYMBOL"), 6, 2 },
  { CM_NAME_STR("SIXTH_SYMBOL"), 4, 2 },
  { CM_NAME_STR("SEVENTH_SYMBOL"), 2, 2 },
  { CM_NAME_STR("EIGHTH_SYMBOL"), 0, 2 },
};
static const cm_field_desc_t cm_PWMEncoder_reg_3_fields[4] = {
  { CM_NAME_STR("FIRST_SYMBOL"), 6, 1 },
  { CM_NAME_STR("SECOND_SYMBOL"), 4, 2 },
  { CM_NAME_STR("THIRD_SYMBOL"), 2, 2 },
  { CM_NAME_STR("FOURTH_SYMBOL"), 0, 2 },
};
static const cm_field_desc_t cm_PWMEncoder_reg_4_fields[4] = {
  { CM_NAME_STR("TOD_AUTO_UPDATE"), 3, 1 },
  { CM_NAME_STR("TOD_TX"), 2, 1 },
  { CM_NAME_STR("SIGNATURE_MODE"), 1, 1 },
  { CM_NAME_STR("ENABLE"), 0, 1 },
};
static const cm_reg_desc_t cm_PWMEncoder_regs[5] = {
  { CM_NAME_STR("PWM_ENCODER_ID"), 0x000, cm_PWMEncoder_reg_0_fields, 1 },
  { CM_NAME_STR("PWM_ENCODER_CNFG"), 0x001, cm_PWMEncoder_reg_1_fields, 3 },
  { CM_NAME_STR("PWM_ENCODER_SIGNATURE_0"), 0x002, cm_PWMEncoder_reg_2_fields, 4 },
  { CM_NAME_STR("PWM_ENCODER_SIGNATURE_1"), 0x003, cm_PWMEncoder_reg_3_fields, 4 },
  { CM_NAME_STR("PWM_ENCODER_CMD"), 0x004, cm_PWMEncoder_reg_4_fields, 4 },
};
const cm_module_desc_t cm_PWMEncoder_module = { CM_NAME_STR("PWMEncoder"), cm_PWMEncoder_bases, 8, cm_PWMEncoder_regs, 5 };

static const uint16_t cm_PWMDecoder_bases[16] = {0xCB40, 0xCB48, 0xCB50, 0xCB58, 0xCB60, 0xCB68, 0xCB70, 0xCB80, 0xCB88, 0xCB90, 0xCB98, 0xCBA0, 0xCBA8, 0xCBB0, 0xCBB8, 0xCBC0};
static const cm_field_desc_t cm_PWMDecoder_reg_0_fields[1] = {
  { CM_NAME_STR("PPS_RATE_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_PWMDecoder_reg_1_fields[2] = {
  { CM_NAME_STR("GENERATE_PPS"), 7, 1 },
  { CM_NAME_STR("PPS_RATE_8_14"), 0, 7 },
};
static const cm_field_desc_t cm_PWMDecoder_reg_2_fields[1] = {
  { CM_NAME_STR("DECODER_ID"), 0, 8 },
};
static const cm_field_desc_t cm_PWMDecoder_reg_3_fields[4] = {
  { CM_NAME_STR("FIFTH_SYMBOL"), 6, 2 },
  { CM_NAME_STR("SIXTH_SYMBOL"), 4, 2 },
  { CM_NAME_STR("SEVENTH_SYMBOL"), 2, 2 },
  { CM_NAME_STR("EIGHTH_SYMBOL"), 0, 2 },
};
static const cm_field_desc_t cm_PWMDecoder_reg_4_fields[4] = {
  { CM_NAME_STR("FIRST_SYMBOL"), 6, 1 },
  { CM_NAME_STR("SECOND_SYMBOL"), 4, 2 },
  { CM_NAME_STR("THIRD_SYMBOL"), 2, 2 },
  { CM_NAME_STR("FOURTH_SYMBOL"), 0, 2 },
};
static const cm_field_desc_t cm_PWMDecoder_reg_5_fields[3] = {
  { CM_NAME_STR("TOD_FRAME_ACCESS_EN"), 2, 1 },
  { CM_NAME_STR("SIGNATURE_MODE"), 1, 1 },
  { CM_NAME_STR("ENABLE"), 0, 1 },
};
static const cm_reg_desc_t cm_PWMDecoder_regs[6] = {
  { CM_NAME_STR("PWM_DECODER_CNFG"), 0x000, cm_PWMDecoder_reg_0_fields, 1 },
  { CM_NAME_STR("PWM_DECODER_CNFG_1"), 0x001, cm_PWMDecoder_reg_1_fields, 2 },
  { CM_NAME_STR("PWM_DECODER_ID"), 0x002, cm_PWMDecoder_reg_2_fields, 1 },
  { CM_NAME_STR("PWM_DECODER_SIGNATURE_0"), 0x003, cm_PWMDecoder_reg_3_fields, 4 },
  { CM_NAME_STR("PWM_DECODER_SIGNATURE_1"), 0x004, cm_PWMDecoder_reg_4_fields, 4 },
  { CM_NAME_STR("PWM_DECODER_CMD"), 0x005, cm_PWMDecoder_reg_5_fields, 3 },
};
const cm_module_desc_t cm_PWMDecoder_module = { CM_NAME_STR("PWMDecoder"), cm_PWMDecoder_bases, 16, cm_PWMDecoder_regs, 6 };

static const uint16_t cm_TOD_bases[4] = {0xCBC8, 0xCBCC, 0xCBD0, 0xCBD2};
static const cm_field_desc_t cm_TOD_reg_0_fields[3] = {
  { CM_NAME_STR("TOD_EVEN_PPS_MODE"), 2, 1 },
  { CM_NAME_STR("TOD_OUT_SYNC_DISABLE"), 1, 1 },
  { CM_NAME_STR("TOD_ENABLE"), 0, 1 },
};
static const cm_reg_desc_t cm_TOD_regs[1] = {
  { CM_NAME_STR("TOD_CFG"), 0x000, cm_TOD_reg_0_fields, 3 },
};
const cm_module_desc_t cm_TOD_module = { CM_NAME_STR("TOD"), cm_TOD_bases, 4, cm_TOD_regs, 1 };

static const uint16_t cm_TODWrite_bases[4] = {0xCC00, 0xCC10, 0xCC20, 0xCC30};
static const cm_field_desc_t cm_TODWrite_reg_0_fields[1] = {
  { CM_NAME_STR("SUBNS"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_1_fields[1] = {
  { CM_NAME_STR("NS_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_2_fields[1] = {
  { CM_NAME_STR("NS_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_3_fields[1] = {
  { CM_NAME_STR("NS_16_23"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_4_fields[1] = {
  { CM_NAME_STR("NS_24_31"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_5_fields[1] = {
  { CM_NAME_STR("SECONDS_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_6_fields[1] = {
  { CM_NAME_STR("SECONDS_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_7_fields[1] = {
  { CM_NAME_STR("SECONDS_16_23"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_8_fields[1] = {
  { CM_NAME_STR("SECONDS_24_31"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_9_fields[1] = {
  { CM_NAME_STR("SECONDS_32_39"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_10_fields[1] = {
  { CM_NAME_STR("SECONDS_40_47"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_11_fields[1] = {
  { CM_NAME_STR("RESERVED"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_12_fields[1] = {
  { CM_NAME_STR("WRITE_COUNTER"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_13_fields[2] = {
  { CM_NAME_STR("PWM_DECODER_INDEX"), 4, 4 },
  { CM_NAME_STR("REF_INDEX"), 0, 4 },
};
static const cm_field_desc_t cm_TODWrite_reg_14_fields[1] = {
  { CM_NAME_STR("RESERVED"), 0, 8 },
};
static const cm_field_desc_t cm_TODWrite_reg_15_fields[2] = {
  { CM_NAME_STR("TOD_WRITE_TYPE"), 4, 2 },
  { CM_NAME_STR("TOD_WRITE_SELECTION"), 0, 4 },
};
static const cm_reg_desc_t cm_TODWrite_regs[16] = {
  { CM_NAME_STR("TOD_WRITE_SUBNS"), 0x000, cm_TODWrite_reg_0_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_NS_0_7"), 0x001, cm_TODWrite_reg_1_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_NS_8_15"), 0x002, cm_TODWrite_reg_2_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_NS_16_23"), 0x003, cm_TODWrite_reg_3_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_NS_24_31"), 0x004, cm_TODWrite_reg_4_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_SECONDS_0_7"), 0x005, cm_TODWrite_reg_5_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_SECONDS_8_15"), 0x006, cm_TODWrite_reg_6_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_SECONDS_16_23"), 0x007, cm_TODWrite_reg_7_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_SECONDS_24_31"), 0x008, cm_TODWrite_reg_8_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_SECONDS_32_39"), 0x009, cm_TODWrite_reg_9_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_SECONDS_40_47"), 0x00A, cm_TODWrite_reg_10_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_RESERVED_0"), 0x00B, cm_TODWrite_reg_11_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_COUNTER"), 0x00C, cm_TODWrite_reg_12_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_SELECT_CFG_0"), 0x00D, cm_TODWrite_reg_13_fields, 2 },
  { CM_NAME_STR("TOD_WRITE_RESERVED_1"), 0x00E, cm_TODWrite_reg_14_fields, 1 },
  { CM_NAME_STR("TOD_WRITE_CMD"), 0x00F, cm_TODWrite_reg_15_fields, 2 },
};
const cm_module_desc_t cm_TODWrite_module = { CM_NAME_STR("TODWrite"), cm_TODWrite_bases, 4, cm_TODWrite_regs, 16 };

static const uint16_t cm_TODReadPrimary_bases[4] = {0xCC40, 0xCC50, 0xCC60, 0xCC80};
static const cm_field_desc_t cm_TODReadPrimary_reg_0_fields[1] = {
  { CM_NAME_STR("SUBNS"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_1_fields[1] = {
  { CM_NAME_STR("NS_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_2_fields[1] = {
  { CM_NAME_STR("NS_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_3_fields[1] = {
  { CM_NAME_STR("NS_16_23"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_4_fields[1] = {
  { CM_NAME_STR("NS_24_31"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_5_fields[1] = {
  { CM_NAME_STR("SECONDS_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_6_fields[1] = {
  { CM_NAME_STR("SECONDS_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_7_fields[1] = {
  { CM_NAME_STR("SECONDS_16_23"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_8_fields[1] = {
  { CM_NAME_STR("SECONDS_24_31"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_9_fields[1] = {
  { CM_NAME_STR("SECONDS_32_39"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_10_fields[1] = {
  { CM_NAME_STR("SECONDS_40_47"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_11_fields[1] = {
  { CM_NAME_STR("READ_COUNTER"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_12_fields[2] = {
  { CM_NAME_STR("PWM_DECODER_INDEX"), 4, 4 },
  { CM_NAME_STR("REF_INDEX"), 0, 4 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_13_fields[1] = {
  { CM_NAME_STR("DPLL_INDEX"), 0, 3 },
};
static const cm_field_desc_t cm_TODReadPrimary_reg_14_fields[2] = {
  { CM_NAME_STR("TOD_READ_TRIGGER_MODE"), 4, 1 },
  { CM_NAME_STR("TOD_READ_TRIGGER"), 0, 4 },
};
static const cm_reg_desc_t cm_TODReadPrimary_regs[15] = {
  { CM_NAME_STR("TOD_READ_PRIMARY_SUBNS"), 0x000, cm_TODReadPrimary_reg_0_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_NS_0_7"), 0x001, cm_TODReadPrimary_reg_1_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_NS_8_15"), 0x002, cm_TODReadPrimary_reg_2_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_NS_16_23"), 0x003, cm_TODReadPrimary_reg_3_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_NS_24_31"), 0x004, cm_TODReadPrimary_reg_4_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_SECONDS_0_7"), 0x005, cm_TODReadPrimary_reg_5_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_SECONDS_8_15"), 0x006, cm_TODReadPrimary_reg_6_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_SECONDS_16_23"), 0x007, cm_TODReadPrimary_reg_7_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_SECONDS_24_31"), 0x008, cm_TODReadPrimary_reg_8_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_SECONDS_32_39"), 0x009, cm_TODReadPrimary_reg_9_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_SECONDS_40_47"), 0x00A, cm_TODReadPrimary_reg_10_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_COUNTER"), 0x00B, cm_TODReadPrimary_reg_11_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_SEL_CFG_0"), 0x00C, cm_TODReadPrimary_reg_12_fields, 2 },
  { CM_NAME_STR("TOD_READ_PRIMARY_SEL_CFG_1"), 0x00D, cm_TODReadPrimary_reg_13_fields, 1 },
  { CM_NAME_STR("TOD_READ_PRIMARY_CMD"), 0x00E, cm_TODReadPrimary_reg_14_fields, 2 },
};
const cm_module_desc_t cm_TODReadPrimary_module = { CM_NAME_STR("TODReadPrimary"), cm_TODReadPrimary_bases, 4, cm_TODReadPrimary_regs, 15 };

static const uint16_t cm_TODReadSecondary_bases[4] = {0xCC90, 0xCCA0, 0xCCB0, 0xCCC0};
static const cm_field_desc_t cm_TODReadSecondary_reg_0_fields[1] = {
  { CM_NAME_STR("SUBNS"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_1_fields[1] = {
  { CM_NAME_STR("NS_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_2_fields[1] = {
  { CM_NAME_STR("NS_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_3_fields[1] = {
  { CM_NAME_STR("NS_16_23"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_4_fields[1] = {
  { CM_NAME_STR("NS_24_31"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_5_fields[1] = {
  { CM_NAME_STR("SECONDS_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_6_fields[1] = {
  { CM_NAME_STR("SECONDS_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_7_fields[1] = {
  { CM_NAME_STR("SECONDS_16_23"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_8_fields[1] = {
  { CM_NAME_STR("SECONDS_24_31"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_9_fields[1] = {
  { CM_NAME_STR("SECONDS_32_39"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_10_fields[1] = {
  { CM_NAME_STR("SECONDS_40_47"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_11_fields[1] = {
  { CM_NAME_STR("READ_COUNTER"), 0, 8 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_12_fields[2] = {
  { CM_NAME_STR("PWM_DECODER_INDEX"), 4, 4 },
  { CM_NAME_STR("REF_INDEX"), 0, 4 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_13_fields[1] = {
  { CM_NAME_STR("DPLL_INDEX"), 0, 3 },
};
static const cm_field_desc_t cm_TODReadSecondary_reg_14_fields[2] = {
  { CM_NAME_STR("TOD_READ_TRIGGER_MODE"), 4, 1 },
  { CM_NAME_STR("TOD_READ_TRIGGER"), 0, 4 },
};
static const cm_reg_desc_t cm_TODReadSecondary_regs[15] = {
  { CM_NAME_STR("TOD_READ_SECONDARY_SUBNS"), 0x000, cm_TODReadSecondary_reg_0_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_NS_0_7"), 0x001, cm_TODReadSecondary_reg_1_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_NS_8_15"), 0x002, cm_TODReadSecondary_reg_2_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_NS_16_23"), 0x003, cm_TODReadSecondary_reg_3_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_NS_24_31"), 0x004, cm_TODReadSecondary_reg_4_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_SECONDS_0_7"), 0x005, cm_TODReadSecondary_reg_5_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_SECONDS_8_15"), 0x006, cm_TODReadSecondary_reg_6_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_SECONDS_16_23"), 0x007, cm_TODReadSecondary_reg_7_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_SECONDS_24_31"), 0x008, cm_TODReadSecondary_reg_8_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_SECONDS_32_39"), 0x009, cm_TODReadSecondary_reg_9_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_SECONDS_40_47"), 0x00A, cm_TODReadSecondary_reg_10_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_COUNTER"), 0x00B, cm_TODReadSecondary_reg_11_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_SEL_CFG_0"), 0x00C, cm_TODReadSecondary_reg_12_fields, 2 },
  { CM_NAME_STR("TOD_READ_SECONDARY_SEL_CFG_1"), 0x00D, cm_TODReadSecondary_reg_13_fields, 1 },
  { CM_NAME_STR("TOD_READ_SECONDARY_CMD"), 0x00E, cm_TODReadSecondary_reg_14_fields, 2 },
};
const cm_module_desc_t cm_TODReadSecondary_module = { CM_NAME_STR("TODReadSecondary"), cm_TODReadSecondary_bases, 4, cm_TODReadSecondary_regs, 15 };

static const uint16_t cm_Input_bases[16] = {0xC1B0, 0xC1C0, 0xC1D0, 0xC200, 0xC210, 0xC220, 0xC230, 0xC240, 0xC250, 0xC260, 0xC280, 0xC290, 0xC2A0, 0xC2B0, 0xC2C0, 0xC2D0};
static const cm_field_desc_t cm_Input_reg_0_fields[1] = {
  { CM_NAME_STR("M_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_1_fields[1] = {
  { CM_NAME_STR("M_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_2_fields[1] = {
  { CM_NAME_STR("M_16_23"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_3_fields[1] = {
  { CM_NAME_STR("M_24_31"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_4_fields[1] = {
  { CM_NAME_STR("M_32_39"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_5_fields[1] = {
  { CM_NAME_STR("M_40_47"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_6_fields[1] = {
  { CM_NAME_STR("N_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_7_fields[1] = {
  { CM_NAME_STR("N_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_8_fields[1] = {
  { CM_NAME_STR("IN_DIV_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_9_fields[1] = {
  { CM_NAME_STR("IN_DIV_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_10_fields[1] = {
  { CM_NAME_STR("IN_PHASE_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_11_fields[1] = {
  { CM_NAME_STR("IN_PHASE_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_Input_reg_12_fields[4] = {
  { CM_NAME_STR("FRAME_SYNC_PULSE_EN"), 7, 1 },
  { CM_NAME_STR("FRAME_SYNC_RESAMPLE_EDGE"), 6, 1 },
  { CM_NAME_STR("FRAME_SYNC_RESAMPLE_EN"), 5, 1 },
  { CM_NAME_STR("FRAME_SYNC_PULSE"), 0, 5 },
};
static const cm_field_desc_t cm_Input_reg_13_fields[6] = {
  { CM_NAME_STR("DPLL_PRED"), 7, 1 },
  { CM_NAME_STR("MUX_GPIO_IN"), 6, 1 },
  { CM_NAME_STR("IN_DIFF"), 5, 1 },
  { CM_NAME_STR("IN_PNMODE"), 4, 1 },
  { CM_NAME_STR("IN_INVERSE"), 3, 1 },
  { CM_NAME_STR("IN_EN"), 0, 1 },
};
static const cm_reg_desc_t cm_Input_regs[14] = {
  { CM_NAME_STR("INPUT_IN_FREQ_M_0_7"), 0x000, cm_Input_reg_0_fields, 1 },
  { CM_NAME_STR("INPUT_IN_FREQ_M_8_15"), 0x001, cm_Input_reg_1_fields, 1 },
  { CM_NAME_STR("INPUT_IN_FREQ_M_16_23"), 0x002, cm_Input_reg_2_fields, 1 },
  { CM_NAME_STR("INPUT_IN_FREQ_M_24_31"), 0x003, cm_Input_reg_3_fields, 1 },
  { CM_NAME_STR("INPUT_IN_FREQ_M_32_39"), 0x004, cm_Input_reg_4_fields, 1 },
  { CM_NAME_STR("INPUT_IN_FREQ_M_40_47"), 0x005, cm_Input_reg_5_fields, 1 },
  { CM_NAME_STR("INPUT_IN_FREQ_N_0_7"), 0x006, cm_Input_reg_6_fields, 1 },
  { CM_NAME_STR("INPUT_IN_FREQ_N_8_15"), 0x007, cm_Input_reg_7_fields, 1 },
  { CM_NAME_STR("INPUT_IN_DIV_0_7"), 0x008, cm_Input_reg_8_fields, 1 },
  { CM_NAME_STR("INPUT_IN_DIV_8_15"), 0x009, cm_Input_reg_9_fields, 1 },
  { CM_NAME_STR("INPUT_IN_PHASE_0_7"), 0x00A, cm_Input_reg_10_fields, 1 },
  { CM_NAME_STR("INPUT_IN_PHASE_8_15"), 0x00B, cm_Input_reg_11_fields, 1 },
  { CM_NAME_STR("INPUT_IN_SYNC"), 0x00C, cm_Input_reg_12_fields, 4 },
  { CM_NAME_STR("INPUT_IN_MODE"), 0x00D, cm_Input_reg_13_fields, 6 },
};
const cm_module_desc_t cm_Input_module = { CM_NAME_STR("Input"), cm_Input_bases, 16, cm_Input_regs, 14 };

static const uint16_t cm_Output_bases[12] = {0xCA14, 0xCA24, 0xCA34, 0xCA44, 0xCA54, 0xCA64, 0xCA80, 0xCA90, 0xCAA0, 0xCAB0, 0xCAC0, 0xCAD0};
static const cm_field_desc_t cm_Output_reg_0_fields[1] = {
  { CM_NAME_STR("Value"), 0, 8 },
};
static const cm_field_desc_t cm_Output_reg_1_fields[1] = {
  { CM_NAME_STR("Value"), 0, 8 },
};
static const cm_field_desc_t cm_Output_reg_2_fields[1] = {
  { CM_NAME_STR("Value"), 0, 8 },
};
static const cm_field_desc_t cm_Output_reg_3_fields[1] = {
  { CM_NAME_STR("Value"), 0, 8 },
};
static const cm_field_desc_t cm_Output_reg_4_fields[1] = {
  { CM_NAME_STR("Value"), 0, 8 },
};
static const cm_reg_desc_t cm_Output_regs[6] = {
  { CM_NAME_STR("OUT_DIV"), 0x000, cm_Output_reg_0_fields, 1 }, // dont care about fields
  { CM_NAME_STR("OUT_PHASE_ADJ_7_0"), 0x00C, cm_Output_reg_0_fields, 1 },
  { CM_NAME_STR("OUT_PHASE_ADJ_15_8"), 0x00D, cm_Output_reg_1_fields, 1 },
  { CM_NAME_STR("OUT_PHASE_ADJ_23_16"), 0x00E, cm_Output_reg_2_fields, 1 },
  { CM_NAME_STR("OUT_PHASE_ADJ_31_24"), 0x00F, cm_Output_reg_3_fields, 1 },
  { CM_NAME_STR("OUT_CTRL_1"), 0x009, cm_Output_reg_4_fields, 1 },
};
const cm_module_desc_t cm_Output_module = { CM_NAME_STR("Output"), cm_Output_bases, 12, cm_Output_regs, 5 };

static const uint16_t cm_REFMON_bases[16] = {0xC2E0, 0xC2EC, 0xC300, 0xC30C, 0xC318, 0xC324, 0xC330, 0xC33C, 0xC348, 0xC354, 0xC360, 0xC36C, 0xC380, 0xC38C, 0xC398, 0xC3A4};
static const cm_field_desc_t cm_REFMON_reg_0_fields[2] = {
  { CM_NAME_STR("VLD_INTERVAL"), 3, 4 },
  { CM_NAME_STR("FREQ_OFFS_LIM"), 0, 3 },
};
static const cm_field_desc_t cm_REFMON_reg_1_fields[1] = {
  { CM_NAME_STR("VLD_INTERVAL_SHORT"), 0, 8 },
};
static const cm_field_desc_t cm_REFMON_reg_2_fields[1] = {
  { CM_NAME_STR("IN_MON_TRANS_THRESHOLD_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_REFMON_reg_3_fields[1] = {
  { CM_NAME_STR("IN_MON_TRANS_THRESHOLD_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_REFMON_reg_4_fields[1] = {
  { CM_NAME_STR("IN_MON_TRANS_PERIOD_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_REFMON_reg_5_fields[1] = {
  { CM_NAME_STR("IN_MON_TRANS_PERIOD_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_REFMON_reg_6_fields[3] = {
  { CM_NAME_STR("QUAL_TIMER"), 5, 2 },
  { CM_NAME_STR("DSQUAL_TIMER"), 3, 2 },
  { CM_NAME_STR("ACT_LIM"), 0, 3 },
};
static const cm_field_desc_t cm_REFMON_reg_7_fields[1] = {
  { CM_NAME_STR("IN_MON_LOS_TOLERANCE_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_REFMON_reg_8_fields[1] = {
  { CM_NAME_STR("IN_MON_LOS_TOLERANCE_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_REFMON_reg_9_fields[2] = {
  { CM_NAME_STR("LOS_GAP"), 1, 2 },
  { CM_NAME_STR("LOS_MARGIN"), 0, 1 },
};
static const cm_field_desc_t cm_REFMON_reg_10_fields[6] = {
  { CM_NAME_STR("DIV_OR_NON_DIV_CLK_SELECT"), 5, 1 },
  { CM_NAME_STR("TRANS_DETECTOR_EN"), 4, 1 },
  { CM_NAME_STR("MASK_ACTIVITY"), 3, 1 },
  { CM_NAME_STR("MASK_FREQ"), 2, 1 },
  { CM_NAME_STR("MASK_LOS"), 1, 1 },
  { CM_NAME_STR("EN"), 0, 1 },
};
static const cm_reg_desc_t cm_REFMON_regs[11] = {
  { CM_NAME_STR("REF_MON_IN_MON_FREQ_CFG"), 0x000, cm_REFMON_reg_0_fields, 2 },
  { CM_NAME_STR("REF_MON_IN_MON_FREQ_VLD_INTV"), 0x001, cm_REFMON_reg_1_fields, 1 },
  { CM_NAME_STR("REF_MON_IN_MON_TRANS_THRESHOLD_0_7"), 0x002, cm_REFMON_reg_2_fields, 1 },
  { CM_NAME_STR("REF_MON_IN_MON_TRANS_THRESHOLD_8_15"), 0x003, cm_REFMON_reg_3_fields, 1 },
  { CM_NAME_STR("REF_MON_IN_MON_TRANS_PERIOD_0_7"), 0x004, cm_REFMON_reg_4_fields, 1 },
  { CM_NAME_STR("REF_MON_IN_MON_TRANS_PERIOD_8_15"), 0x005, cm_REFMON_reg_5_fields, 1 },
  { CM_NAME_STR("REF_MON_IN_MON_ACT_CFG"), 0x006, cm_REFMON_reg_6_fields, 3 },
  { CM_NAME_STR("REF_MON_IN_MON_LOS_TOLERANCE_0_7"), 0x008, cm_REFMON_reg_7_fields, 1 },
  { CM_NAME_STR("REF_MON_IN_MON_LOS_TOLERANCE_8_15"), 0x009, cm_REFMON_reg_8_fields, 1 },
  { CM_NAME_STR("REF_MON_IN_MON_LOS_CFG"), 0x00A, cm_REFMON_reg_9_fields, 2 },
  { CM_NAME_STR("REF_MON_IN_MON_CFG"), 0x00B, cm_REFMON_reg_10_fields, 6 },
};
const cm_module_desc_t cm_REFMON_module = { CM_NAME_STR("REFMON"), cm_REFMON_bases, 16, cm_REFMON_regs, 11 };

static const uint16_t cm_PWM_USER_DATA_bases[1] = {0xCBC8};
static const cm_field_desc_t cm_PWM_USER_DATA_reg_0_fields[1] = {
  { CM_NAME_STR("ENCODER_ID"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_USER_DATA_reg_1_fields[1] = {
  { CM_NAME_STR("DECODER_ID"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_USER_DATA_reg_2_fields[1] = {
  { CM_NAME_STR("BYTES"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_USER_DATA_reg_3_fields[1] = {
  { CM_NAME_STR("COMMAND_STATUS"), 0, 8 },
};
static const cm_reg_desc_t cm_PWM_USER_DATA_regs[4] = {
  { CM_NAME_STR("PWM_USER_DATA_PWM_SRC_ENCODER_ID"), 0x000, cm_PWM_USER_DATA_reg_0_fields, 1 },
  { CM_NAME_STR("PWM_USER_DATA_PWM_DST_DECODER_ID"), 0x001, cm_PWM_USER_DATA_reg_1_fields, 1 },
  { CM_NAME_STR("PWM_USER_DATA_PWM_USER_DATA_SIZE"), 0x002, cm_PWM_USER_DATA_reg_2_fields, 1 },
  { CM_NAME_STR("PWM_USER_DATA_PWM_USER_DATA_CMD_STS"), 0x003, cm_PWM_USER_DATA_reg_3_fields, 1 },
};
const cm_module_desc_t cm_PWM_USER_DATA_module = { CM_NAME_STR("PWM_USER_DATA"), cm_PWM_USER_DATA_bases, 1, cm_PWM_USER_DATA_regs, 4 };

static const uint16_t cm_EEPROM_bases[1] = {0xCF68};
static const cm_field_desc_t cm_EEPROM_reg_0_fields[2] = {
  { CM_NAME_STR("RESERVED"), 7, 1 },
  { CM_NAME_STR("I2C_ADDR"), 0, 7 },
};
static const cm_field_desc_t cm_EEPROM_reg_1_fields[1] = {
  { CM_NAME_STR("BYTES"), 0, 8 },
};
static const cm_field_desc_t cm_EEPROM_reg_2_fields[1] = {
  { CM_NAME_STR("EEPROM_OFFSET"), 0, 8 },
};
static const cm_field_desc_t cm_EEPROM_reg_3_fields[1] = {
  { CM_NAME_STR("EEPROM_OFFSET"), 0, 8 },
};
static const cm_field_desc_t cm_EEPROM_reg_4_fields[1] = {
  { CM_NAME_STR("EEPROM_CMD"), 0, 8 },
};
static const cm_field_desc_t cm_EEPROM_reg_5_fields[1] = {
  { CM_NAME_STR("EEPROM_CMD"), 0, 8 },
};
static const cm_reg_desc_t cm_EEPROM_regs[6] = {
  { CM_NAME_STR("EEPROM_I2C_ADDR"), 0x000, cm_EEPROM_reg_0_fields, 2 },
  { CM_NAME_STR("EEPROM_SIZE"), 0x001, cm_EEPROM_reg_1_fields, 1 },
  { CM_NAME_STR("EEPROM_OFFSET_LOW"), 0x002, cm_EEPROM_reg_2_fields, 1 },
  { CM_NAME_STR("EEPROM_OFFSET_HIGH"), 0x003, cm_EEPROM_reg_3_fields, 1 },
  { CM_NAME_STR("EEPROM_CMD_LOW"), 0x004, cm_EEPROM_reg_4_fields, 1 },
  { CM_NAME_STR("EEPROM_CMD_HIGH"), 0x005, cm_EEPROM_reg_5_fields, 1 },
};
const cm_module_desc_t cm_EEPROM_module = { CM_NAME_STR("EEPROM"), cm_EEPROM_bases, 1, cm_EEPROM_regs, 6 };

static const uint16_t cm_EEPROM_DATA_bases[1] = {0xCF80};
static const cm_field_desc_t cm_EEPROM_DATA_reg_0_fields[1] = {
  { CM_NAME_STR("DATA"), 0, 8 },
};
static const cm_reg_desc_t cm_EEPROM_DATA_regs[1] = {
  { CM_NAME_STR("BYTE_OTP_EEPROM_PWM_BUFF_{i}"), 0x000, cm_EEPROM_DATA_reg_0_fields, 1 },
};
const cm_module_desc_t cm_EEPROM_DATA_module = { CM_NAME_STR("EEPROM_DATA"), cm_EEPROM_DATA_bases, 1, cm_EEPROM_DATA_regs, 1 };

static const uint16_t cm_OUTPUT_TDC_CFG_bases[1] = {0xCCD0};
static const cm_field_desc_t cm_OUTPUT_TDC_CFG_reg_0_fields[1] = {
  { CM_NAME_STR("FAST_LOCK_ENABLE_DELAY_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_OUTPUT_TDC_CFG_reg_1_fields[1] = {
  { CM_NAME_STR("FAST_LOCK_ENABLE_DELAY_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_OUTPUT_TDC_CFG_reg_2_fields[1] = {
  { CM_NAME_STR("FAST_LOCK_DISABLE_DELAY_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_OUTPUT_TDC_CFG_reg_3_fields[1] = {
  { CM_NAME_STR("FAST_LOCK_DISABLE_DELAY_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_OUTPUT_TDC_CFG_reg_4_fields[3] = {
  { CM_NAME_STR("RESERVED"), 2, 6 },
  { CM_NAME_STR("REF_SEL"), 1, 1 },
  { CM_NAME_STR("ENABLE"), 0, 1 },
};
static const cm_reg_desc_t cm_OUTPUT_TDC_CFG_regs[5] = {
  { CM_NAME_STR("OUTPUT_TDC_CFG_GBL_0_0_7"), 0x000, cm_OUTPUT_TDC_CFG_reg_0_fields, 1 },
  { CM_NAME_STR("OUTPUT_TDC_CFG_GBL_0_8_15"), 0x001, cm_OUTPUT_TDC_CFG_reg_1_fields, 1 },
  { CM_NAME_STR("OUTPUT_TDC_CFG_GBL_1_0_7"), 0x002, cm_OUTPUT_TDC_CFG_reg_2_fields, 1 },
  { CM_NAME_STR("OUTPUT_TDC_CFG_GBL_1_8_15"), 0x003, cm_OUTPUT_TDC_CFG_reg_3_fields, 1 },
  { CM_NAME_STR("OUTPUT_TDC_CFG_GBL_2"), 0x004, cm_OUTPUT_TDC_CFG_reg_4_fields, 3 },
};
const cm_module_desc_t cm_OUTPUT_TDC_CFG_module = { CM_NAME_STR("OUTPUT_TDC_CFG"), cm_OUTPUT_TDC_CFG_bases, 1, cm_OUTPUT_TDC_CFG_regs, 5 };

static const uint16_t cm_OUTPUT_TDC_bases[4] = {0xCD00, 0xCD08, 0xCD10, 0xCD18};
static const cm_field_desc_t cm_OUTPUT_TDC_reg_0_fields[1] = {
  { CM_NAME_STR("SAMPLES_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_OUTPUT_TDC_reg_1_fields[1] = {
  { CM_NAME_STR("SAMPLES_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_OUTPUT_TDC_reg_2_fields[1] = {
  { CM_NAME_STR("TARGET_PHASE_OFFSET_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_OUTPUT_TDC_reg_3_fields[1] = {
  { CM_NAME_STR("TARGET_PHASE_OFFSET_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_OUTPUT_TDC_reg_4_fields[1] = {
  { CM_NAME_STR("ALIGN_TARGET_MASK"), 0, 8 },
};
static const cm_field_desc_t cm_OUTPUT_TDC_reg_5_fields[2] = {
  { CM_NAME_STR("TARGET_INDEX"), 4, 4 },
  { CM_NAME_STR("SOURCE_INDEX"), 0, 4 },
};
static const cm_field_desc_t cm_OUTPUT_TDC_reg_6_fields[6] = {
  { CM_NAME_STR("DISABLE_MEASUREMENT_FILTER"), 7, 1 },
  { CM_NAME_STR("ALIGN_THRESHOLD_COUNT"), 4, 3 },
  { CM_NAME_STR("ALIGN_RESET"), 3, 1 },
  { CM_NAME_STR("TYPE"), 2, 1 },
  { CM_NAME_STR("MODE"), 1, 1 },
  { CM_NAME_STR("GO"), 0, 1 },
};
static const cm_reg_desc_t cm_OUTPUT_TDC_regs[7] = {
  { CM_NAME_STR("OUTPUT_TDC_CTRL_0_0_7"), 0x000, cm_OUTPUT_TDC_reg_0_fields, 1 },
  { CM_NAME_STR("OUTPUT_TDC_CTRL_0_8_15"), 0x001, cm_OUTPUT_TDC_reg_1_fields, 1 },
  { CM_NAME_STR("OUTPUT_TDC_CTRL_1_0_7"), 0x002, cm_OUTPUT_TDC_reg_2_fields, 1 },
  { CM_NAME_STR("OUTPUT_TDC_CTRL_1_8_15"), 0x003, cm_OUTPUT_TDC_reg_3_fields, 1 },
  { CM_NAME_STR("OUTPUT_TDC_CTRL_2"), 0x004, cm_OUTPUT_TDC_reg_4_fields, 1 },
  { CM_NAME_STR("OUTPUT_TDC_CTRL_3"), 0x005, cm_OUTPUT_TDC_reg_5_fields, 2 },
  { CM_NAME_STR("OUTPUT_TDC_CTRL_4"), 0x006, cm_OUTPUT_TDC_reg_6_fields, 6 },
};
const cm_module_desc_t cm_OUTPUT_TDC_module = { CM_NAME_STR("OUTPUT_TDC"), cm_OUTPUT_TDC_bases, 4, cm_OUTPUT_TDC_regs, 7 };

static const uint16_t cm_INPUT_TDC_bases[1] = {0xCD20};
static const cm_field_desc_t cm_INPUT_TDC_reg_0_fields[1] = {
  { CM_NAME_STR("SDM_FRAC_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_INPUT_TDC_reg_1_fields[1] = {
  { CM_NAME_STR("SDM_FRAC_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_INPUT_TDC_reg_2_fields[1] = {
  { CM_NAME_STR("SDM_MOD_0_7"), 0, 8 },
};
static const cm_field_desc_t cm_INPUT_TDC_reg_3_fields[1] = {
  { CM_NAME_STR("SDM_MOD_8_15"), 0, 8 },
};
static const cm_field_desc_t cm_INPUT_TDC_reg_4_fields[2] = {
  { CM_NAME_STR("FBD_USER_CONFIG_EN"), 7, 1 },
  { CM_NAME_STR("FBD_INTEGER"), 0, 7 },
};
static const cm_field_desc_t cm_INPUT_TDC_reg_5_fields[2] = {
  { CM_NAME_STR("SDM_ORDER"), 1, 2 },
  { CM_NAME_STR("REF_SEL"), 0, 1 },
};
static const cm_reg_desc_t cm_INPUT_TDC_regs[6] = {
  { CM_NAME_STR("INPUT_TDC_SDM_FRAC_0_7"), 0x000, cm_INPUT_TDC_reg_0_fields, 1 },
  { CM_NAME_STR("INPUT_TDC_SDM_FRAC_8_15"), 0x001, cm_INPUT_TDC_reg_1_fields, 1 },
  { CM_NAME_STR("INPUT_TDC_SDM_MOD_0_7"), 0x002, cm_INPUT_TDC_reg_2_fields, 1 },
  { CM_NAME_STR("INPUT_TDC_SDM_MOD_8_15"), 0x003, cm_INPUT_TDC_reg_3_fields, 1 },
  { CM_NAME_STR("INPUT_TDC_FBD_CTRL"), 0x004, cm_INPUT_TDC_reg_4_fields, 2 },
  { CM_NAME_STR("INPUT_TDC_CTRL"), 0x005, cm_INPUT_TDC_reg_5_fields, 2 },
};
const cm_module_desc_t cm_INPUT_TDC_module = { CM_NAME_STR("INPUT_TDC"), cm_INPUT_TDC_bases, 1, cm_INPUT_TDC_regs, 6 };

static const uint16_t cm_PWM_SYNC_ENCODER_bases[8] = {0xCD80, 0xCD84, 0xCD88, 0xCD8C, 0xCD90, 0xCD94, 0xCD98, 0xCD9C};
static const cm_field_desc_t cm_PWM_SYNC_ENCODER_reg_0_fields[8] = {
  { CM_NAME_STR("PAYLOAD_CH_EN_7"), 7, 1 },
  { CM_NAME_STR("PAYLOAD_CH_EN_6"), 6, 1 },
  { CM_NAME_STR("PAYLOAD_CH_EN_5"), 5, 1 },
  { CM_NAME_STR("PAYLOAD_CH_EN_4"), 4, 1 },
  { CM_NAME_STR("PAYLOAD_CH_EN_3"), 3, 1 },
  { CM_NAME_STR("PAYLOAD_CH_EN_2"), 2, 1 },
  { CM_NAME_STR("PAYLOAD_CH_EN_1"), 1, 1 },
  { CM_NAME_STR("PAYLOAD_CH_EN_0"), 0, 1 },
};
static const cm_field_desc_t cm_PWM_SYNC_ENCODER_reg_1_fields[8] = {
  { CM_NAME_STR("PAYLOAD_SQUELCH_7"), 7, 1 },
  { CM_NAME_STR("PAYLOAD_SQUELCH_6"), 6, 1 },
  { CM_NAME_STR("PAYLOAD_SQUELCH_5"), 5, 1 },
  { CM_NAME_STR("PAYLOAD_SQUELCH_4"), 4, 1 },
  { CM_NAME_STR("PAYLOAD_SQUELCH_3"), 3, 1 },
  { CM_NAME_STR("PAYLOAD_SQUELCH_2"), 2, 1 },
  { CM_NAME_STR("PAYLOAD_SQUELCH_1"), 1, 1 },
  { CM_NAME_STR("PAYLOAD_SQUELCH_0"), 0, 1 },
};
static const cm_field_desc_t cm_PWM_SYNC_ENCODER_reg_2_fields[2] = {
  { CM_NAME_STR("PWM_SYNC_PHASE_CORR_DISABLE"), 1, 1 },
  { CM_NAME_STR("PWM_SYNC"), 0, 1 },
};
static const cm_reg_desc_t cm_PWM_SYNC_ENCODER_regs[3] = {
  { CM_NAME_STR("PWM_SYNC_ENCODER_PAYLOAD_CNFG"), 0x000, cm_PWM_SYNC_ENCODER_reg_0_fields, 8 },
  { CM_NAME_STR("PWM_SYNC_ENCODER_PAYLOAD_SQUELCH_CNFG"), 0x001, cm_PWM_SYNC_ENCODER_reg_1_fields, 8 },
  { CM_NAME_STR("PWM_SYNC_ENCODER_CMD"), 0x002, cm_PWM_SYNC_ENCODER_reg_2_fields, 2 },
};
const cm_module_desc_t cm_PWM_SYNC_ENCODER_module = { CM_NAME_STR("PWM_SYNC_ENCODER"), cm_PWM_SYNC_ENCODER_bases, 8, cm_PWM_SYNC_ENCODER_regs, 3 };

static const uint16_t cm_PWM_SYNC_DECODER_bases[16] = {0xCE00, 0xCE06, 0xCE0C, 0xCE12, 0xCE18, 0xCE1E, 0xCE24, 0xCE2A, 0xCE30, 0xCE36, 0xCE3C, 0xCE42, 0xCE48, 0xCE4E, 0xCE54, 0xCE5A};
static const cm_field_desc_t cm_PWM_SYNC_DECODER_reg_0_fields[4] = {
  { CM_NAME_STR("PAYLOAD_CH_EN_1"), 7, 1 },
  { CM_NAME_STR("SRC_CH_IDX_1"), 4, 3 },
  { CM_NAME_STR("PAYLOAD_CH_EN_0"), 3, 1 },
  { CM_NAME_STR("SRC_CH_IDX_0"), 0, 3 },
};
static const cm_field_desc_t cm_PWM_SYNC_DECODER_reg_1_fields[4] = {
  { CM_NAME_STR("PAYLOAD_CH_EN_3"), 7, 1 },
  { CM_NAME_STR("SRC_CH_IDX_3"), 4, 3 },
  { CM_NAME_STR("PAYLOAD_CH_EN_2"), 3, 1 },
  { CM_NAME_STR("SRC_CH_IDX_2"), 0, 3 },
};
static const cm_field_desc_t cm_PWM_SYNC_DECODER_reg_2_fields[4] = {
  { CM_NAME_STR("PAYLOAD_CH_EN_5"), 7, 1 },
  { CM_NAME_STR("SRC_CH_IDX_5"), 4, 3 },
  { CM_NAME_STR("PAYLOAD_CH_EN_4"), 3, 1 },
  { CM_NAME_STR("SRC_CH_IDX_4"), 0, 3 },
};
static const cm_field_desc_t cm_PWM_SYNC_DECODER_reg_3_fields[4] = {
  { CM_NAME_STR("PAYLOAD_CH_EN_7"), 7, 1 },
  { CM_NAME_STR("SRC_CH_IDX_7"), 4, 3 },
  { CM_NAME_STR("PAYLOAD_CH_EN_6"), 3, 1 },
  { CM_NAME_STR("SRC_CH_IDX_6"), 0, 3 },
};
static const cm_field_desc_t cm_PWM_SYNC_DECODER_reg_4_fields[4] = {
  { CM_NAME_STR("PWM_OUTPUT_SQUELCH"), 6, 1 },
  { CM_NAME_STR("PWM_CO_LOCATED_CR"), 5, 1 },
  { CM_NAME_STR("PWM_SYNC_CR_IDX"), 1, 4 },
  { CM_NAME_STR("PWM_SYNC"), 0, 1 },
};
static const cm_reg_desc_t cm_PWM_SYNC_DECODER_regs[5] = {
  { CM_NAME_STR("PWM_SYNC_DECODER_PAYLOAD_CNFG_0"), 0x000, cm_PWM_SYNC_DECODER_reg_0_fields, 4 },
  { CM_NAME_STR("PWM_SYNC_DECODER_PAYLOAD_CNFG_1"), 0x001, cm_PWM_SYNC_DECODER_reg_1_fields, 4 },
  { CM_NAME_STR("PWM_SYNC_DECODER_PAYLOAD_CNFG_2"), 0x002, cm_PWM_SYNC_DECODER_reg_2_fields, 4 },
  { CM_NAME_STR("PWM_SYNC_DECODER_PAYLOAD_CNFG_3"), 0x003, cm_PWM_SYNC_DECODER_reg_3_fields, 4 },
  { CM_NAME_STR("PWM_SYNC_DECODER_CMD"), 0x004, cm_PWM_SYNC_DECODER_reg_4_fields, 4 },
};
const cm_module_desc_t cm_PWM_SYNC_DECODER_module = { CM_NAME_STR("PWM_SYNC_DECODER"), cm_PWM_SYNC_DECODER_bases, 16, cm_PWM_SYNC_DECODER_regs, 5 };

static const uint16_t cm_PWM_Rx_Info_bases[1] = {0xCE80};
static const cm_field_desc_t cm_PWM_Rx_Info_reg_0_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_Rx_Info_reg_1_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_Rx_Info_reg_2_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_Rx_Info_reg_3_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_Rx_Info_reg_4_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_Rx_Info_reg_5_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_Rx_Info_reg_6_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_Rx_Info_reg_7_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_Rx_Info_reg_8_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_Rx_Info_reg_9_fields[2] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
  { CM_NAME_STR("PWM_RandID"), 0, 8 },
};
static const cm_field_desc_t cm_PWM_Rx_Info_reg_10_fields[4] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
  { CM_NAME_STR("DataFlag"), 7, 1 },
  { CM_NAME_STR("HandshakeData"), 5, 2 },
  { CM_NAME_STR("PWM_Transaction_ID"), 0, 5 },
};
static const cm_reg_desc_t cm_PWM_Rx_Info_regs[11] = {
  { CM_NAME_STR("PWM_TOD_SUBNS"), 0x000, cm_PWM_Rx_Info_reg_0_fields, 1 },
  { CM_NAME_STR("PWM_TOD_NS_7_0"), 0x001, cm_PWM_Rx_Info_reg_1_fields, 1 },
  { CM_NAME_STR("PWM_TOD_NS_15_8"), 0x002, cm_PWM_Rx_Info_reg_2_fields, 1 },
  { CM_NAME_STR("PWM_TOD_NS_23_16"), 0x003, cm_PWM_Rx_Info_reg_3_fields, 1 },
  { CM_NAME_STR("PWM_TOD_NS_31_24"), 0x004, cm_PWM_Rx_Info_reg_4_fields, 1 },
  { CM_NAME_STR("PWM_TOD_SEC_7_0"), 0x005, cm_PWM_Rx_Info_reg_5_fields, 1 },
  { CM_NAME_STR("PWM_TOD_SEC_15_8"), 0x006, cm_PWM_Rx_Info_reg_6_fields, 1 },
  { CM_NAME_STR("PWM_TOD_SEC_23_16"), 0x007, cm_PWM_Rx_Info_reg_7_fields, 1 },
  { CM_NAME_STR("PWM_TOD_SEC_31_24"), 0x008, cm_PWM_Rx_Info_reg_8_fields, 1 },
  { CM_NAME_STR("PWM_TOD_SEC_39_32"), 0x009, cm_PWM_Rx_Info_reg_9_fields, 2 },
  { CM_NAME_STR("PWM_TOD_SEC_47_40"), 0x00A, cm_PWM_Rx_Info_reg_10_fields, 4 },
};
const cm_module_desc_t cm_PWM_Rx_Info_module = { CM_NAME_STR("PWM_Rx_Info"), cm_PWM_Rx_Info_bases, 1, cm_PWM_Rx_Info_regs, 11 };

static const uint16_t cm_DPLL_Ctrl_bases[8] = {0xC600, 0xC63C, 0xC680, 0xC6BC, 0xC700, 0xC73C, 0xC780, 0xC7BC};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_0_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_1_fields[1] = {
  { CM_NAME_STR("BW_7_0"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_2_fields[2] = {
  { CM_NAME_STR("BW_13_8"), 0, 6 },
  { CM_NAME_STR("BW_UNIT"), 6, 2 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_3_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_4_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_5_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_6_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_7_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_8_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_9_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 4 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_10_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_11_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 5 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_12_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_13_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_14_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_15_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_16_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_17_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_18_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_19_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Ctrl_reg_20_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 1 },
};
static const cm_reg_desc_t cm_DPLL_Ctrl_regs[21] = {
  { CM_NAME_STR("DPLL_DECIMATOR_BW_MULT"), 0x003, cm_DPLL_Ctrl_reg_0_fields, 1 },
  { CM_NAME_STR("DPLL_BW_0"), 0x004, cm_DPLL_Ctrl_reg_1_fields, 1 },
  { CM_NAME_STR("DPLL_BW_1"), 0x005, cm_DPLL_Ctrl_reg_2_fields, 2 },
  { CM_NAME_STR("DPLL_PSL_7_0"), 0x006, cm_DPLL_Ctrl_reg_3_fields, 1 },
  { CM_NAME_STR("DPLL_PSL_15_8"), 0x007, cm_DPLL_Ctrl_reg_4_fields, 1 },
  { CM_NAME_STR("DPLL_PHASE_OFFSET_CFG_7_0"), 0x014, cm_DPLL_Ctrl_reg_5_fields, 1 },
  { CM_NAME_STR("DPLL_PHASE_OFFSET_CFG_15_8"), 0x015, cm_DPLL_Ctrl_reg_6_fields, 1 },
  { CM_NAME_STR("DPLL_PHASE_OFFSET_CFG_23_16"), 0x016, cm_DPLL_Ctrl_reg_7_fields, 1 },
  { CM_NAME_STR("DPLL_PHASE_OFFSET_CFG_31_24"), 0x017, cm_DPLL_Ctrl_reg_8_fields, 1 },
  { CM_NAME_STR("DPLL_PHASE_OFFSET_CFG_35_32"), 0x018, cm_DPLL_Ctrl_reg_9_fields, 1 },
  { CM_NAME_STR("DPLL_FINE_PHASE_ADV_CFG_7_0"), 0x01A, cm_DPLL_Ctrl_reg_10_fields, 1 },
  { CM_NAME_STR("DPLL_FINE_PHASE_ADV_CFG_12_8"), 0x01B, cm_DPLL_Ctrl_reg_11_fields, 1 },
  { CM_NAME_STR("FOD_FREQ_M_7_0"), 0x01C, cm_DPLL_Ctrl_reg_12_fields, 1 },
  { CM_NAME_STR("FOD_FREQ_M_15_8"), 0x01D, cm_DPLL_Ctrl_reg_13_fields, 1 },
  { CM_NAME_STR("FOD_FREQ_M_23_16"), 0x01E, cm_DPLL_Ctrl_reg_14_fields, 1 },
  { CM_NAME_STR("FOD_FREQ_M_31_24"), 0x01F, cm_DPLL_Ctrl_reg_15_fields, 1 },
  { CM_NAME_STR("FOD_FREQ_M_39_32"), 0x020, cm_DPLL_Ctrl_reg_16_fields, 1 },
  { CM_NAME_STR("FOD_FREQ_M_47_40"), 0x021, cm_DPLL_Ctrl_reg_17_fields, 1 },
  { CM_NAME_STR("FOD_FREQ_N_7_0"), 0x022, cm_DPLL_Ctrl_reg_18_fields, 1 },
  { CM_NAME_STR("FOD_FREQ_N_15_8"), 0x023, cm_DPLL_Ctrl_reg_19_fields, 1 },
  { CM_NAME_STR("DPLL_FRAME_PULSE_SYNC"), 0x03B, cm_DPLL_Ctrl_reg_20_fields, 1 },
};
const cm_module_desc_t cm_DPLL_Ctrl_module = { CM_NAME_STR("DPLL_Ctrl"), cm_DPLL_Ctrl_bases, 8, cm_DPLL_Ctrl_regs, 21 };

static const uint16_t cm_DPLL_Freq_Write_bases[8] = {0xC838, 0xC840, 0xC848, 0xC850, 0xC858, 0xC860, 0xC868, 0xC870};
static const cm_field_desc_t cm_DPLL_Freq_Write_reg_0_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Freq_Write_reg_1_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Freq_Write_reg_2_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Freq_Write_reg_3_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Freq_Write_reg_4_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Freq_Write_reg_5_fields[2] = {
  { CM_NAME_STR("VALUE"), 0, 2 },
  { CM_NAME_STR("Reserved"), 2, 6 },
};
static const cm_reg_desc_t cm_DPLL_Freq_Write_regs[6] = {
  { CM_NAME_STR("DPLL_WR_FREQ_7_0"), 0x000, cm_DPLL_Freq_Write_reg_0_fields, 1 },
  { CM_NAME_STR("DPLL_WR_FREQ_15_8"), 0x001, cm_DPLL_Freq_Write_reg_1_fields, 1 },
  { CM_NAME_STR("DPLL_WR_FREQ_23_16"), 0x002, cm_DPLL_Freq_Write_reg_2_fields, 1 },
  { CM_NAME_STR("DPLL_WR_FREQ_31_24"), 0x003, cm_DPLL_Freq_Write_reg_3_fields, 1 },
  { CM_NAME_STR("DPLL_WR_FREQ_39_32"), 0x004, cm_DPLL_Freq_Write_reg_4_fields, 1 },
  { CM_NAME_STR("DPLL_WR_FREQ_41_40"), 0x005, cm_DPLL_Freq_Write_reg_5_fields, 2 },
};
const cm_module_desc_t cm_DPLL_Freq_Write_module = { CM_NAME_STR("DPLL_Freq_Write"), cm_DPLL_Freq_Write_bases, 8, cm_DPLL_Freq_Write_regs, 6 };

static const uint16_t cm_DPLL_Config_bases[8] = {0xC3B0, 0xC400, 0xC438, 0xC480, 0xC4B8, 0xC500, 0xC538, 0xC580};
static const cm_field_desc_t cm_DPLL_Config_reg_0_fields[1] = {
  { CM_NAME_STR("DCO_INC_DEC_SIZE_7_0"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_1_fields[1] = {
  { CM_NAME_STR("DCO_INC_DEC_SIZE_15_8"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_2_fields[4] = {
  { CM_NAME_STR("FORCE_LOCK_INPUT"), 3, 5 },
  { CM_NAME_STR("GLOBAL_SYNC_EN"), 2, 1 },
  { CM_NAME_STR("REVERTIVE_EN"), 1, 1 },
  { CM_NAME_STR("HITLESS_EN"), 0, 1 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_3_fields[3] = {
  { CM_NAME_STR("HITLESS_TYPE"), 5, 1 },
  { CM_NAME_STR("FB_SELECT_REF"), 1, 4 },
  { CM_NAME_STR("FB_SELECT_REF_EN"), 0, 1 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_4_fields[4] = {
  { CM_NAME_STR("FRAME_SYNC_PULSE_RESYNC_EN"), 7, 1 },
  { CM_NAME_STR("FRAME_SYNC_MODE"), 5, 2 },
  { CM_NAME_STR("EXT_FB_REF_SELECT"), 1, 4 },
  { CM_NAME_STR("EXT_FB_EN"), 0, 1 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_5_fields[1] = {
  { CM_NAME_STR("UPDATE_RATE_CFG"), 0, 2 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_6_fields[2] = {
  { CM_NAME_STR("FILTER_STATUS_UPDATE_EN"), 2, 1 },
  { CM_NAME_STR("FILTER_STATUS_SELECT_CNFG"), 0, 2 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_7_fields[1] = {
  { CM_NAME_STR("HISTORY"), 0, 6 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_8_fields[1] = {
  { CM_NAME_STR("DPLL_HO_ADVCD_BW_7_0"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_9_fields[2] = {
  { CM_NAME_STR("BW_UNIT"), 6, 2 },
  { CM_NAME_STR("DPLL_HO_ADVCD_BW_15_8"), 0, 6 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_10_fields[1] = {
  { CM_NAME_STR("HOLDOVER_MODE"), 0, 3 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_11_fields[2] = {
  { CM_NAME_STR("PHASE_UNIT"), 6, 2 },
  { CM_NAME_STR("PHASE_LOCK_MAX_ERROR"), 0, 6 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_12_fields[1] = {
  { CM_NAME_STR("PHASE_MON_DUR"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_13_fields[2] = {
  { CM_NAME_STR("FFO_UNIT"), 6, 2 },
  { CM_NAME_STR("FFO_LOCK_MAX_ERROR"), 0, 6 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_14_fields[1] = {
  { CM_NAME_STR("FFO_MON_DUR"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_15_fields[3] = {
  { CM_NAME_STR("PRIORITY_GROUP_NUMBER"), 6, 2 },
  { CM_NAME_STR("PRIORITY_REF"), 1, 5 },
  { CM_NAME_STR("PRIORITY_EN"), 0, 1 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_16_fields[3] = {
  { CM_NAME_STR("RESERVED"), 2, 6 },
  { CM_NAME_STR("TRANS_SUPPRESS_EN"), 1, 1 },
  { CM_NAME_STR("TRANS_DETECT_EN"), 0, 1 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_17_fields[8] = {
  { CM_NAME_STR("LOCK_REC_PULL_IN_EN"), 7, 1 },
  { CM_NAME_STR("LOCK_REC_FAST_ACQ_EN"), 6, 1 },
  { CM_NAME_STR("LOCK_REC_PHASE_SNAP_EN"), 5, 1 },
  { CM_NAME_STR("LOCK_REC_FREQ_SNAP_EN"), 4, 1 },
  { CM_NAME_STR("LOCK_ACQ_PULL_IN_EN"), 3, 1 },
  { CM_NAME_STR("LOCK_ACQ_FAST_ACQ_EN"), 2, 1 },
  { CM_NAME_STR("LOCK_ACQ_PHASE_SNAP_EN"), 1, 1 },
  { CM_NAME_STR("LOCK_ACQ_FREQ_SNAP_EN"), 0, 1 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_18_fields[2] = {
  { CM_NAME_STR("PRE_FAST_ACQ_TIMER"), 4, 4 },
  { CM_NAME_STR("DAMP_FTR"), 0, 4 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_19_fields[1] = {
  { CM_NAME_STR("MAX_FFO"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_20_fields[1] = {
  { CM_NAME_STR("DPLL_FASTLOCK_PSL_7_0"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_21_fields[1] = {
  { CM_NAME_STR("DPLL_FASTLOCK_PSL_15_8"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_22_fields[1] = {
  { CM_NAME_STR("DPLL_FASTLOCK_FSL_7_0"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_23_fields[1] = {
  { CM_NAME_STR("DPLL_FASTLOCK_FSL_15_8"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_24_fields[1] = {
  { CM_NAME_STR("DPLL_FASTLOCK_BW_7_0"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_25_fields[2] = {
  { CM_NAME_STR("BW_UNIT"), 6, 2 },
  { CM_NAME_STR("DPLL_FASTLOCK_BW_15_8"), 0, 6 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_26_fields[1] = {
  { CM_NAME_STR("WRITE_FREQ_TIMEOUT_CNFG_7_0"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_27_fields[1] = {
  { CM_NAME_STR("WRITE_FREQ_TIMEOUT_CNFG_15_8"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_28_fields[1] = {
  { CM_NAME_STR("WRITE_PHASE_TIMEOUT_CNFG_7_0"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_29_fields[1] = {
  { CM_NAME_STR("WRITE_PHASE_TIMEOUT_CNFG_15_8"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_30_fields[3] = {
  { CM_NAME_STR("RESERVED"), 2, 6 },
  { CM_NAME_STR("WP_PRED"), 1, 1 },
  { CM_NAME_STR("PRED_EN"), 0, 1 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_31_fields[3] = {
  { CM_NAME_STR("RESERVED"), 3, 5 },
  { CM_NAME_STR("TOD_SYNC_SOURCE"), 1, 2 },
  { CM_NAME_STR("TOD_SYNC_EN"), 0, 1 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_32_fields[4] = {
  { CM_NAME_STR("RESERVED"), 5, 3 },
  { CM_NAME_STR("PRI_COMBO_SRC_EN"), 5, 1 },
  { CM_NAME_STR("PRI_COMBO_SRC_FILTERED_CNFG"), 4, 1 },
  { CM_NAME_STR("PRI_COMBO_SRC_ID"), 0, 4 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_33_fields[4] = {
  { CM_NAME_STR("RESERVED"), 5, 3 },
  { CM_NAME_STR("SEC_COMBO_SRC_EN"), 5, 1 },
  { CM_NAME_STR("SEC_COMBO_SRC_FILTERED_CNFG"), 4, 1 },
  { CM_NAME_STR("SEC_COMBO_SRC_ID"), 0, 4 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_34_fields[2] = {
  { CM_NAME_STR("RESERVED"), 4, 4 },
  { CM_NAME_STR("SLAVE_REFERENCE"), 0, 4 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_35_fields[2] = {
  { CM_NAME_STR("RESERVED"), 3, 5 },
  { CM_NAME_STR("MODE"), 0, 3 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_36_fields[2] = {
  { CM_NAME_STR("PFD_FB_CLK_SEL"), 4, 4 },
  { CM_NAME_STR("PFD_REF_CLK_SEL"), 0, 4 },
};
static const cm_field_desc_t cm_DPLL_Config_reg_37_fields[3] = {
  { CM_NAME_STR("WRITE_TIMER_MODE"), 6, 1 },
  { CM_NAME_STR("PLL_MODE"), 3, 3 },
  { CM_NAME_STR("STATE_MODE"), 0, 3 },
};
static const cm_reg_desc_t cm_DPLL_Config_regs[41] = {
  { CM_NAME_STR("DPLL_DCO_INC_DEC_SIZE_7_0"), 0x000, cm_DPLL_Config_reg_0_fields, 1 },
  { CM_NAME_STR("DPLL_DCO_INC_DEC_SIZE_15_8"), 0x001, cm_DPLL_Config_reg_1_fields, 1 },
  { CM_NAME_STR("DPLL_CTRL_0"), 0x002, cm_DPLL_Config_reg_2_fields, 4 },
  { CM_NAME_STR("DPLL_CTRL_1"), 0x003, cm_DPLL_Config_reg_3_fields, 3 },
  { CM_NAME_STR("DPLL_CTRL_2"), 0x004, cm_DPLL_Config_reg_4_fields, 4 },
  { CM_NAME_STR("DPLL_UPDATE_RATE_CFG"), 0x005, cm_DPLL_Config_reg_5_fields, 1 },
  { CM_NAME_STR("DPLL_FILTER_STATUS_UPDATE_CFG"), 0x006, cm_DPLL_Config_reg_6_fields, 2 },
  { CM_NAME_STR("DPLL_HO_ADVCD_HISTORY"), 0x007, cm_DPLL_Config_reg_7_fields, 1 },
  { CM_NAME_STR("DPLL_HO_ADVCD_BW_7_0"), 0x008, cm_DPLL_Config_reg_8_fields, 1 },
  { CM_NAME_STR("DPLL_HO_ADVCD_BW_15_8"), 0x009, cm_DPLL_Config_reg_9_fields, 2 },
  { CM_NAME_STR("DPLL_HO_CFG"), 0x00A, cm_DPLL_Config_reg_10_fields, 1 },
  { CM_NAME_STR("DPLL_LOCK_0"), 0x00B, cm_DPLL_Config_reg_11_fields, 2 },
  { CM_NAME_STR("DPLL_LOCK_1"), 0x00C, cm_DPLL_Config_reg_12_fields, 1 },
  { CM_NAME_STR("DPLL_LOCK_2"), 0x00D, cm_DPLL_Config_reg_13_fields, 2 },
  { CM_NAME_STR("DPLL_LOCK_3"), 0x00E, cm_DPLL_Config_reg_14_fields, 1 },
  { CM_NAME_STR("DPLL_REF_PRIORITY_0"), 0x00F, cm_DPLL_Config_reg_15_fields, 3 },
  { CM_NAME_STR("DPLL_REF_PRIORITY_1"), 0x010, cm_DPLL_Config_reg_15_fields, 3 },
  { CM_NAME_STR("DPLL_REF_PRIORITY_2"), 0x011, cm_DPLL_Config_reg_15_fields, 3 },
  { CM_NAME_STR("DPLL_REF_PRIORITY_3"), 0x012, cm_DPLL_Config_reg_15_fields, 3 },
  { CM_NAME_STR("DPLL_TRANS_CTRL"), 0x022, cm_DPLL_Config_reg_16_fields, 3 },
  { CM_NAME_STR("DPLL_FASTLOCK_CFG_0"), 0x023, cm_DPLL_Config_reg_17_fields, 8 },
  { CM_NAME_STR("DPLL_FASTLOCK_CFG_1"), 0x024, cm_DPLL_Config_reg_18_fields, 2 },
  { CM_NAME_STR("DPLL_MAX_FREQ_OFFSET"), 0x025, cm_DPLL_Config_reg_19_fields, 1 },
  { CM_NAME_STR("DPLL_FASTLOCK_PSL"), 0x026, cm_DPLL_Config_reg_20_fields, 1 },
  { CM_NAME_STR("DPLL_FASTLOCK_PSL_15_8"), 0x027, cm_DPLL_Config_reg_21_fields, 1 },
  { CM_NAME_STR("DPLL_FASTLOCK_FSL"), 0x028, cm_DPLL_Config_reg_22_fields, 1 },
  { CM_NAME_STR("DPLL_FASTLOCK_FSL_15_8"), 0x029, cm_DPLL_Config_reg_23_fields, 1 },
  { CM_NAME_STR("DPLL_FASTLOCK_BW"), 0x02A, cm_DPLL_Config_reg_24_fields, 1 },
  { CM_NAME_STR("DPLL_FASTLOCK_BW_15_8"), 0x02B, cm_DPLL_Config_reg_25_fields, 2 },
  { CM_NAME_STR("DPLL_WRITE_FREQ_TIMER"), 0x02C, cm_DPLL_Config_reg_26_fields, 1 },
  { CM_NAME_STR("DPLL_WRITE_FREQ_TIMER_15_8"), 0x02D, cm_DPLL_Config_reg_27_fields, 1 },
  { CM_NAME_STR("DPLL_WRITE_PHASE_TIMER"), 0x02E, cm_DPLL_Config_reg_28_fields, 1 },
  { CM_NAME_STR("DPLL_WRITE_PHASE_TIMER_15_8"), 0x02F, cm_DPLL_Config_reg_29_fields, 1 },
  { CM_NAME_STR("DPLL_PRED_CFG"), 0x030, cm_DPLL_Config_reg_30_fields, 3 },
  { CM_NAME_STR("DPLL_TOD_SYNC_CFG"), 0x031, cm_DPLL_Config_reg_31_fields, 3 },
  { CM_NAME_STR("DPLL_COMBO_SLAVE_CFG_0"), 0x032, cm_DPLL_Config_reg_32_fields, 4 },
  { CM_NAME_STR("DPLL_COMBO_SLAVE_CFG_1"), 0x033, cm_DPLL_Config_reg_33_fields, 4 },
  { CM_NAME_STR("DPLL_SLAVE_REF_CFG"), 0x034, cm_DPLL_Config_reg_34_fields, 2 },
  { CM_NAME_STR("DPLL_REF_MODE"), 0x035, cm_DPLL_Config_reg_35_fields, 2 },
  { CM_NAME_STR("DPLL_PHASE_MEASUREMENT_CFG"), 0x036, cm_DPLL_Config_reg_36_fields, 2 },
  { CM_NAME_STR("DPLL_MODE"), 0x037, cm_DPLL_Config_reg_37_fields, 3 },
};
const cm_module_desc_t cm_DPLL_Config_module = { CM_NAME_STR("DPLL_Config"), cm_DPLL_Config_bases, 8, cm_DPLL_Config_regs, 41 };

static const uint16_t cm_DPLL_GeneralStatus_bases[1] = {0xC014};
static const cm_field_desc_t cm_DPLL_GeneralStatus_reg_0_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_GeneralStatus_reg_1_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_GeneralStatus_reg_2_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_GeneralStatus_reg_3_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_GeneralStatus_reg_4_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_GeneralStatus_reg_5_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_field_desc_t cm_DPLL_GeneralStatus_reg_6_fields[1] = {
  { CM_NAME_STR("VALUE"), 0, 8 },
};
static const cm_reg_desc_t cm_DPLL_GeneralStatus_regs[7] = {
  { CM_NAME_STR("EEPROM_STATUS_7_0"), 0x008, cm_DPLL_GeneralStatus_reg_0_fields, 1 },
  { CM_NAME_STR("EEPROM_STATUS_8_15"), 0x009, cm_DPLL_GeneralStatus_reg_1_fields, 1 },
  { CM_NAME_STR("MAJOR RELEASE"), 0x010, cm_DPLL_GeneralStatus_reg_2_fields, 1 },
  { CM_NAME_STR("MINOR RELEASE"), 0x011, cm_DPLL_GeneralStatus_reg_3_fields, 1 },
  { CM_NAME_STR("HOTFIX RELEASE"), 0x012, cm_DPLL_GeneralStatus_reg_4_fields, 1 },
  { CM_NAME_STR("JTAG DEVICE ID"), 0x01C, cm_DPLL_GeneralStatus_reg_5_fields, 1 },
  { CM_NAME_STR("PRODUCT ID"), 0x01E, cm_DPLL_GeneralStatus_reg_6_fields, 1 },
};
const cm_module_desc_t cm_DPLL_GeneralStatus_module = { CM_NAME_STR("DPLL_GeneralStatus"), cm_DPLL_GeneralStatus_bases, 1, cm_DPLL_GeneralStatus_regs, 7 };

const cm_module_desc_t * const cm_all_modules[] = {
  &cm_Status_module,
  &cm_PWMEncoder_module,
  &cm_PWMDecoder_module,
  &cm_TOD_module,
  &cm_TODWrite_module,
  &cm_TODReadPrimary_module,
  &cm_TODReadSecondary_module,
  &cm_Input_module,
  &cm_Output_module,
  &cm_REFMON_module,
  &cm_PWM_USER_DATA_module,
  &cm_EEPROM_module,
  &cm_EEPROM_DATA_module,
  &cm_OUTPUT_TDC_CFG_module,
  &cm_OUTPUT_TDC_module,
  &cm_INPUT_TDC_module,
  &cm_PWM_SYNC_ENCODER_module,
  &cm_PWM_SYNC_DECODER_module,
  &cm_PWM_Rx_Info_module,
  &cm_DPLL_Ctrl_module,
  &cm_DPLL_Freq_Write_module,
  &cm_DPLL_Config_module,
  &cm_DPLL_GeneralStatus_module
};
const size_t cm_all_modules_count = 23;


int cm_dump_module(const cm_bus_t *bus, const cm_module_desc_t *mod, unsigned inst,
                   int (*printfn)(const char *fmt, ...))
{
    if (!mod || !bus || !printfn) return -1;
    if (inst >= mod->count) return -2;
    uint16_t base = mod->bases[inst];
    printfn("== %s[%u] @ 0x%04X ==\n", mod->name?mod->name:"(module)", inst, base);
    for (uint16_t i = 0; i < mod->nregs; ++i) {
        const cm_reg_desc_t *r = &mod->regs[i];
        uint8_t v=0; int rc = cm_read8(bus, (uint16_t)(base + r->offset), &v);
        if (rc) return rc;
        printfn("  %-40s @+0x%03X = 0x%02X\n", r->name?r->name:"(reg)", r->offset, v);
        for (uint16_t f = 0; f < r->nfields; ++f) {
            const cm_field_desc_t *fd = &r->fields[f];
            uint8_t fv = (uint8_t)((v >> fd->shift) & cm_mask8(fd->width));
            printfn("      %-32s [%2u:%u] = 0x%02X\n", fd->name?fd->name:"(field)", fd->shift+fd->width-1, fd->shift, fv);
        }
    }
    return 0;
}


/* ---- String-based lookup helpers implementation ------------------------- */

int cm_find_module(const char *name, const cm_module_desc_t **mod_out)
{
    if (!name || !mod_out) return -1;
    *mod_out = NULL;

    for (size_t i = 0; i < cm_all_modules_count; ++i) {
        const cm_module_desc_t *m = cm_all_modules[i];
        if (!m || !m->name) {
            /* CM_STRIP_NAMES may set names NULL; skip in that case. */
            continue;
        }
        if (strcmp(m->name, name) == 0) {
            *mod_out = m;
            return 0;
        }
    }
    return -2; /* not found */
}

int cm_find_reg(const cm_module_desc_t *mod,
                const char *reg_name,
                const cm_reg_desc_t **reg_out)
{
    if (!mod || !reg_name || !reg_out) return -1;
    *reg_out = NULL;

    for (uint16_t i = 0; i < mod->nregs; ++i) {
        const cm_reg_desc_t *r = &mod->regs[i];
        if (!r->name) continue;
	//fprintf(stderr, "Debug cm_find_reg, i=%d, name = %s\r\n", i, r->name);
        if (strcmp(r->name, reg_name) == 0) {
            *reg_out = r;
            return 0;
        }
    }
    return -2; /* not found */
}

int cm_find_field(const cm_reg_desc_t *reg,
                  const char *field_name,
                  const cm_field_desc_t **field_out)
{
    if (!reg || !field_name || !field_out) return -1;
    *field_out = NULL;

    for (uint16_t i = 0; i < reg->nfields; ++i) {
        const cm_field_desc_t *f = &reg->fields[i];
        if (!f->name) continue;
        if (strcmp(f->name, field_name) == 0) {
            *field_out = f;
            return 0;
        }
    }
    return -2; /* not found */
}

/* Helper to resolve module/instance/reg name into absolute address. */
static int cm_resolve_reg_addr(const cm_module_desc_t **mod_out,
                               const cm_reg_desc_t **reg_out,
                               uint16_t *addr_out,
                               const char *mod_name,
                               unsigned inst,
                               const char *reg_name)
{
    if (!mod_out || !reg_out || !addr_out) return -1;

    const cm_module_desc_t *mod = NULL;
    int rc = cm_find_module(mod_name, &mod);
    if (rc) {
	fprintf(stderr, "cm_resolve_reg_addr, find_module failed\n");
	return rc;
    }

    if (inst >= mod->count) {
	fprintf(stderr, "cm_resolve_reg_addr, inst >= mod->count failed\n");
        return -3; /* bad instance index */
    }
    uint16_t base = mod->bases[inst];

    const cm_reg_desc_t *reg = NULL;
    rc = cm_find_reg(mod, reg_name, &reg);
    if (rc) {
	fprintf(stderr, "cm_resolve_reg_addr, find_reg failed\n");
	return rc;
    }

    *mod_out  = mod;
    *reg_out  = reg;
    *addr_out = (uint16_t)(base + reg->offset);
    return 0;
}

int cm_string_read8(const cm_bus_t *bus,
                    const char *mod_name,
                    unsigned inst,
                    const char *reg_name,
                    uint8_t *value_out)
{
    if (!bus || !value_out) return -1;

    const cm_module_desc_t *mod = NULL;
    const cm_reg_desc_t *reg = NULL;
    uint16_t addr = 0;
    int rc = cm_resolve_reg_addr(&mod, &reg, &addr, mod_name, inst, reg_name);
    if (rc) return rc;

    return cm_read8(bus, addr, value_out);
}

int cm_string_write8(const cm_bus_t *bus,
                     const char *mod_name,
                     unsigned inst,
                     const char *reg_name,
                     uint8_t value)
{
    if (!bus) return -1;

    const cm_module_desc_t *mod = NULL;
    const cm_reg_desc_t *reg = NULL;
    uint16_t addr = 0;
    int rc = cm_resolve_reg_addr(&mod, &reg, &addr, mod_name, inst, reg_name);
    if (rc) return rc;

    return cm_write8(bus, addr, value);
}



int cm_string_trigger_rw(const cm_bus_t *bus,
                    const char *mod_name,
                    unsigned inst,
                    const char *reg_name)
{
    if (!bus ) return -1;

    const cm_module_desc_t *mod = NULL;
    const cm_reg_desc_t *reg = NULL;
    uint16_t addr = 0;
    uint8_t reg_value = 0;
    int rc = cm_resolve_reg_addr(&mod, &reg, &addr, mod_name, inst, reg_name);
    if (rc) return rc;

    cm_read8(bus, addr, &reg_value);
    cm_write8(bus, addr, reg_value);
    return 0;
}








int cm_string_field_read8(const cm_bus_t *bus,
                          const char *mod_name,
                          unsigned inst,
                          const char *reg_name,
                          const char *field_name,
                          uint8_t *value_out)
{
    if (!bus || !value_out) return -1;

    const cm_module_desc_t *mod = NULL;
    const cm_reg_desc_t *reg = NULL;
    uint16_t addr = 0;
    int rc = cm_resolve_reg_addr(&mod, &reg, &addr, mod_name, inst, reg_name);
    if (rc) return rc;

    const cm_field_desc_t *field = NULL;
    rc = cm_find_field(reg, field_name, &field);
    if (rc) return rc;
/*
    fprintf(stderr, "Debug cm_string_field_read8, addr=0x%x, shift=%d, width=%d\n",
	addr, field->shift, field->width);
*/
    return cm_field_read8(bus, addr, field->shift, field->width, value_out);
}

int cm_string_field_write8(const cm_bus_t *bus,
                           const char *mod_name,
                           unsigned inst,
                           const char *reg_name,
                           const char *field_name,
                           uint8_t value)
{
    if (!bus) return -1;

    const cm_module_desc_t *mod = NULL;
    const cm_reg_desc_t *reg = NULL;
    uint16_t addr = 0;
    int rc = cm_resolve_reg_addr(&mod, &reg, &addr, mod_name, inst, reg_name);
    if (rc) {
	fprintf(stderr, "cm_string_field_write8, resolve_reg_addr fail\n");
	return rc;
    }

    //fprintf(stderr,"cm_string_field_write8 debug, addr = 0x%x\n", addr);
    const cm_field_desc_t *field = NULL;
    rc = cm_find_field(reg, field_name, &field);
    if (rc) {
	fprintf(stderr, "cm_string_field_write8, cm_find_field fail\n");
	 return rc;
    }

    return cm_field_write8(bus, addr, field->shift, field->width, value);
}



int cm_string_write_bytes(const cm_bus_t *bus,
                          const char *mod_name,
                          unsigned inst,
                          const char *reg_name,
                          const uint8_t *data,
                          size_t len)
{
    if (!bus || !data || len == 0) {
        return -1;
    }

    const cm_module_desc_t *mod = NULL;
    const cm_reg_desc_t *reg = NULL;
    uint16_t addr = 0;
    int rc = cm_resolve_reg_addr(&mod, &reg, &addr, mod_name, inst, reg_name);
    if (rc) {
        return rc;  // module/reg/instance not found
    }

    /* IMPORTANT:
     * We use the multi-byte bus write here, which your cm_bus_t shim
     * maps to dpll_write_seq() under the hood. That means:
     *   - multi-byte registers are written in one burst
     *   - page register logic is handled by dpll_write_seq()
     */
    return bus->write(bus->user, addr, data, len);
}

int cm_string_read_bytes(const cm_bus_t *bus,
                         const char *mod_name,
                         unsigned inst,
                         const char *reg_name,
                         uint8_t *data,
                         size_t len)
{
    if (!bus || !data || len == 0) {
        return -1;
    }

    const cm_module_desc_t *mod = NULL;
    const cm_reg_desc_t *reg = NULL;
    uint16_t addr = 0;
    int rc = cm_resolve_reg_addr(&mod, &reg, &addr, mod_name, inst, reg_name);
    if (rc) {
        return rc;
    }

    return bus->read(bus->user, addr, data, len);
}




#define DPLL_MAX_M  ((1ULL << 48) - 1)
#define DPLL_MAX_N  (65535U)

/* Compute best 48-bit M and 16-bit N (1..65535) such that M/N ≈ freq_hz.
 *
 * On success:
 *   *M_out        = 48-bit numerator
 *   *N_reg_out    = encoded denominator for the chip (0 means 1)
 *   *actual_hz    = realized frequency M/N
 *   *error_hz     = actual_hz - freq_hz
 *
 * Return 0 on success, non-zero on error.
 */
int dpll_compute_input_ratio(double freq_hz,
                             uint64_t *M_out,
                             uint16_t *N_reg_out,
                             double *actual_hz,
                             double *error_hz)
{
    if (!M_out || !N_reg_out || !actual_hz || !error_hz) {
        return -1;
    }
    if (!(freq_hz > 0.0)) {
        return -2;  // non-positive frequency
    }

    double best_err = DBL_MAX;
    uint64_t best_M = 0;
    uint32_t best_N = 1;

    /* Brute-force search over N in [1..DPLL_MAX_N] */
    for (uint32_t N = 1; N <= DPLL_MAX_N; ++N) {
        double m_real = freq_hz * (double)N;

        if (m_real > (double)DPLL_MAX_M) {
            /* For your typical ranges (1..250 MHz), this never triggers,
             * but the check keeps it safe if someone passes something wild.
             */
            continue;
        }

        uint64_t M = (uint64_t)llround(m_real);
        double realized = (double)M / (double)N;
        double err = fabs(realized - freq_hz);

        if (err < best_err) {
            best_err = err;
            best_M = M;
            best_N = N;

            /* Early-exit if we hit exact representation */
            if (err == 0.0) {
                break;
            }
        }
    }

    if (best_M == 0 && best_N == 1) {
        /* Could not find anything reasonable (should not happen for sane freq) */
        return -3;
    }

    double realized = (double)best_M / (double)best_N;
    double err = realized - freq_hz;

    *M_out     = best_M;
    *N_reg_out = (best_N == 1) ? 0 : (uint16_t)best_N;  /* 0 encodes N=1 */
    *actual_hz = realized;
    *error_hz  = err;

    return 0;
}


/* Decode the M,N representation back to Hz.
 * N_reg is the raw register value (0 means N=1).
 */
double dpll_input_freq_from_ratio(uint64_t M, uint16_t N_reg)
{
    uint32_t N = (N_reg == 0) ? 1u : (uint32_t)N_reg;
    if (N == 0) {
        /* Should never happen (only via malformed N_reg == 0x0000 and special-cased as 1). */
        return 0.0;
    }
    return (double)M / (double)N;
}


#define DCO_MIN_HZ 500e6
#define DCO_MAX_HZ 750e6
#define ONE_PPS_HZ 1.0
#define ONE_PPS_TOL 1e-9

/* Cost function: sum of relative errors (could be tweaked if you want). */
static double dpll_output_cost(double f3, double a3,
                               double f4, double a4)
{
    double e3 = (f3 > 0.0) ? fabs(a3 - f3) / f3 : 0.0;
    double e4 = (f4 > 0.0) ? fabs(a4 - f4) / f4 : 0.0;
    return e3 + e4;
}

static int dpll_search_dco_dual(double f3, double f4,
                                double *fdco_out,
                                uint32_t *d3_out,
                                uint32_t *d4_out,
                                double *a3_out,
                                double *a4_out)
{
    /* Choose the higher frequency as the "anchor". */
    double f_hi = f3;
    double f_lo = f4;
    int hi_is_3 = 1;  /* 1 if f_hi corresponds to OUT3, 0 if OUT4 */

    if (f4 > f3) {
        f_hi = f4;
        f_lo = f3;
        hi_is_3 = 0;
    }

    if (!(f_hi > 0.0) || !(f_lo > 0.0)) {
        return -1;
    }

    /* D_hi range so that F_dco = D_hi * f_hi remains in [DCO_MIN_HZ, DCO_MAX_HZ]. */
    uint32_t D_hi_min = (uint32_t)ceil(DCO_MIN_HZ / f_hi);
    uint32_t D_hi_max = (uint32_t)floor(DCO_MAX_HZ / f_hi);

    if (D_hi_min == 0 || D_hi_min > D_hi_max) {
        return -2;
    }

    double best_cost = DBL_MAX;
    double best_fdco = 0.0;
    uint32_t best_D_hi = 0;
    uint32_t best_D_lo = 0;
    double best_a_hi = 0.0, best_a_lo = 0.0;

    for (uint32_t D_hi = D_hi_min; D_hi <= D_hi_max; ++D_hi) {

	double F_candidate = f_hi * (double)D_hi;
	double F_int = floor(F_candidate + 0.5);   // round to nearest integer Hz
	if (F_int < DCO_MIN_HZ || F_int > DCO_MAX_HZ)
	    continue;

	double F_dco = F_int;


        /* Best integer divider for the other output. */
        uint32_t D_lo = (uint32_t)llround(F_dco / f_lo);
        if (D_lo == 0) continue;

        double a_hi = F_dco / (double)D_hi;
        double a_lo = F_dco / (double)D_lo;

        double cost = dpll_output_cost(
            hi_is_3 ? f3 : f4, hi_is_3 ? a_hi : a_lo,
            hi_is_3 ? f4 : f3, hi_is_3 ? a_lo : a_hi);

        if (cost < best_cost) {
            best_cost = cost;
            best_fdco = F_dco;
            best_D_hi = D_hi;
            best_D_lo = D_lo;
            best_a_hi = a_hi;
            best_a_lo = a_lo;

            if (cost == 0.0) break; /* perfect match */
        }
    }

    if (best_cost == DBL_MAX) {
        return -3; /* no solution found in range (should not happen for sane freqs) */
    }

    double a3 = hi_is_3 ? best_a_hi : best_a_lo;
    double a4 = hi_is_3 ? best_a_lo : best_a_hi;
    uint32_t D3 = hi_is_3 ? best_D_hi : best_D_lo;
    uint32_t D4 = hi_is_3 ? best_D_lo : best_D_hi;

    if (fdco_out) *fdco_out = best_fdco;
    if (d3_out)  *d3_out    = D3;
    if (d4_out)  *d4_out    = D4;
    if (a3_out)  *a3_out    = a3;
    if (a4_out)  *a4_out    = a4;

    return 0;
}

static int dpll_search_dco_with_1pps(double f3, double f4,
                                     double *fdco_out,
                                     uint32_t *d3_out,
                                     uint32_t *d4_out,
                                     double *a3_out,
                                     double *a4_out)
{
    int out3_is_1 = fabs(f3 - ONE_PPS_HZ) < ONE_PPS_TOL;
    int out4_is_1 = fabs(f4 - ONE_PPS_HZ) < ONE_PPS_TOL;

    if (out3_is_1 && out4_is_1) {
        /* Both 1 PPS: simplest is F_dco = 500 MHz, D3=D4=F_dco. */
        double F_dco = DCO_MIN_HZ;
        uint32_t D = (uint32_t)F_dco;

        if (fdco_out) *fdco_out = F_dco;
        if (d3_out)   *d3_out   = D;
        if (d4_out)   *d4_out   = D;
        if (a3_out)   *a3_out   = 1.0;
        if (a4_out)   *a4_out   = 1.0;
        return 0;
    }

    /* Exactly one is 1 PPS. */
    double f_other = out3_is_1 ? f4 : f3;

    if (!(f_other > 0.0)) return -1;

    /* D_other range such that F_dco = D_other * f_other lies in [DCO_MIN, DCO_MAX]. */
    uint32_t D_min = (uint32_t)ceil(DCO_MIN_HZ / f_other);
    uint32_t D_max = (uint32_t)floor(DCO_MAX_HZ / f_other);

    if (D_min == 0 || D_min > D_max) {
        return -2;
    }

    /* If range is enormous (e.g., very low f_other), you might want to clamp. */
    const uint32_t MAX_STEPS = 1000000u;
    if ((uint64_t)(D_max - D_min + 1) > MAX_STEPS) {
        /* Simple fallback: treat as general case, no 1PPS guarantee. */
        return dpll_search_dco_dual(f3, f4, fdco_out, d3_out, d4_out, a3_out, a4_out);
    }

    double best_cost = DBL_MAX;
    double best_fdco = 0.0;
    uint32_t best_D_other = 0;
    double best_a_other = 0.0;

    for (uint32_t D = D_min; D <= D_max; ++D) {
        double F_candidate = f_other * (double)D;
        /* Force F_dco to be integer so 1 PPS can be exact: */
        double F_int = floor(F_candidate + 0.5);  /* round to nearest integer */
        if (F_int < DCO_MIN_HZ || F_int > DCO_MAX_HZ)
            continue;

        double a_other = F_int / (double)D;
        double err_rel = fabs(a_other - f_other) / f_other;

        if (err_rel < best_cost) {
            best_cost = err_rel;
            best_fdco = F_int;
            best_D_other = D;
            best_a_other = a_other;

            if (err_rel == 0.0) break; /* perfect match */
        }
    }

    if (best_cost == DBL_MAX) {
        /* Fall back to dual search if this somehow fails. */
        return dpll_search_dco_dual(f3, f4, fdco_out, d3_out, d4_out, a3_out, a4_out);
    }

    double F_dco = best_fdco;
    uint32_t D_other = best_D_other;

    double a3 = 0.0, a4 = 0.0;
    uint32_t D3 = 0, D4 = 0;

    if (out3_is_1) {
        /* OUT3 = 1 Hz -> D3 = F_dco, OUT4 approximates f4 */
        D3 = (uint32_t)F_dco;
        D4 = D_other;
        a3 = 1.0;
        a4 = best_a_other;
    } else {
        /* OUT4 = 1 Hz -> D4 = F_dco, OUT3 approximates f3 */
        D4 = (uint32_t)F_dco;
        D3 = D_other;
        a4 = 1.0;
        a3 = best_a_other;
    }

    if (fdco_out) *fdco_out = F_dco;
    if (d3_out)   *d3_out   = D3;
    if (d4_out)   *d4_out   = D4;
    if (a3_out)   *a3_out   = a3;
    if (a4_out)   *a4_out   = a4;

    return 0;
}
/* Compute a common DCO and dividers for OUT3/OUT4.
 *
 * Inputs:
 *   f3_hz, f4_hz : desired frequencies (Hz), > 0
 *
 * Outputs:
 *   *fdco_hz     : chosen DCO frequency (Hz)
 *   *d3, *d4     : integer dividers
 *   *a3, *a4     : realized output frequencies
 *   *e3, *e4     : errors (aX - fX)
 *
 * Returns 0 on success, non-zero on failure.
 */
int dpll_compute_output_dco_and_divs(double f3_hz, double f4_hz,
                                            double *fdco_hz,
                                            uint32_t *d3,
                                            uint32_t *d4,
                                            double *a3,
                                            double *a4,
                                            double *e3,
                                            double *e4)
{
    if (!(f3_hz > 0.0) || !(f4_hz > 0.0)) {
        return -1;
    }

    int out3_is_1 = fabs(f3_hz - ONE_PPS_HZ) < ONE_PPS_TOL;
    int out4_is_1 = fabs(f4_hz - ONE_PPS_HZ) < ONE_PPS_TOL;

    double F_dco = 0.0;
    uint32_t D3 = 0, D4 = 0;
    double A3 = 0.0, A4 = 0.0;

    int rc;
    if (out3_is_1 || out4_is_1) {
        rc = dpll_search_dco_with_1pps(f3_hz, f4_hz,
                                       &F_dco, &D3, &D4, &A3, &A4);
    } else {
        rc = dpll_search_dco_dual(f3_hz, f4_hz,
                                  &F_dco, &D3, &D4, &A3, &A4);
    }

    if (rc != 0) {
        return rc;
    }

    if (fdco_hz) *fdco_hz = F_dco;
    if (d3)      *d3      = D3;
    if (d4)      *d4      = D4;
    if (a3)      *a3      = A3;
    if (a4)      *a4      = A4;
    if (e3)      *e3      = (A3 - f3_hz);
    if (e4)      *e4      = (A4 - f4_hz);

    return 0;
}


/* Compute M/N for the DCO and integer dividers for OUT3/OUT4.
 *
 * Inputs:
 *   f3_req, f4_req : requested OUT3/OUT4 frequencies (Hz, >0).
 *
 * Outputs:
 *   *M_out         : 48-bit numerator for DCO (uint64_t; must fit < 2^48).
 *   *N_reg_out     : 16-bit denominator register value (0 encodes N=1).
 *   *div3_out      : 32-bit divider for OUT3.
 *   *div4_out      : 32-bit divider for OUT4.
 *   *fdco_out      : actual DCO frequency (Hz) with chosen M/N.
 *   *out3_actual   : realized OUT3 frequency (Hz).
 *   *out4_actual   : realized OUT4 frequency (Hz).
 *   *out3_err      : out3_actual - f3_req (Hz).
 *   *out4_err      : out4_actual - f4_req (Hz).
 *
 * Returns 0 on success, non-zero on failure.
 */
int dpll_compute_output_mndiv(double f3_req,
                              double f4_req,
                              uint64_t *M_out,
                              uint16_t *N_reg_out,
                              uint32_t *div3_out,
                              uint32_t *div4_out,
                              double   *fdco_out,
                              double   *out3_actual,
                              double   *out4_actual,
                              double   *out3_err,
                              double   *out4_err)
{
    if (!M_out || !N_reg_out || !div3_out || !div4_out ||
        !fdco_out || !out3_actual || !out4_actual ||
        !out3_err || !out4_err) {
        return -1;
    }
    if (!(f3_req > 0.0) || !(f4_req > 0.0)) {
        return -2;
    }

    /* Step 1: find integer DCO and dividers using your existing solver. */
    double fdco_tmp = 0.0;
    uint32_t D3 = 0, D4 = 0;
    double a3_tmp = 0.0, a4_tmp = 0.0;
    double e3_tmp = 0.0, e4_tmp = 0.0;

    int rc = dpll_compute_output_dco_and_divs(f3_req, f4_req,
                                              &fdco_tmp,
                                              &D3, &D4,
                                              &a3_tmp, &a4_tmp,
                                              &e3_tmp, &e4_tmp);
    if (rc != 0) {
        return rc;
    }

    /* Step 2: choose M/N.  Since fdco_tmp is integer Hz in [500e6,750e6],
     * we can simply use N=1 and M=fdco_tmp (exact).
     */
    uint64_t M = (uint64_t)llround(fdco_tmp);
    uint16_t N_reg = 0;  /* N=1 => N_reg=0 per chip encoding */

    /* Sanity: M must fit in 48 bits. */
    if (M >= (1ULL << 48)) {
        return -3;
    }

    double fdco_real = (double)M;  /* N=1 */

    /* Step 3: recompute the actual outputs and error with this exact M/N. */
    double out3_real = fdco_real / (double)D3;
    double out4_real = fdco_real / (double)D4;

    double err3 = out3_real - f3_req;
    double err4 = out4_real - f4_req;

    /* Fill outputs. */
    *M_out       = M;
    *N_reg_out   = N_reg;
    *div3_out    = D3;
    *div4_out    = D4;
    *fdco_out    = fdco_real;
    *out3_actual = out3_real;
    *out4_actual = out4_real;
    *out3_err    = err3;
    *out4_err    = err4;

    return 0;
}


