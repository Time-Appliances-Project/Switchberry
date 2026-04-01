
/*
 * renesas_cm8a34001_tables.h
 * Auto-generated structured register tables for 8A34001 ClockMatrix
 *
 * This provides:
 *  - const tables for modules: bases[], registers[], fields[] with human-readable names
 *  - a tiny bus shim + generic helpers
 *  - utility dump/peek APIs for iteration
 *
 * If memory is tight, define CM_STRIP_NAMES to remove 'name' strings from the tables.
 */

#ifndef RENESAS_CM8A34001_TABLES_H
#define RENESAS_CM8A34001_TABLES_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct cm_bus {
    void *user;
    int (*read8)(void *user, uint16_t addr, uint8_t *val);
    int (*write8)(void *user, uint16_t addr, uint8_t val);
    int (*read)(void *user, uint16_t addr, uint8_t *buf, size_t len);
    int (*write)(void *user, uint16_t addr, const uint8_t *buf, size_t len);
} cm_bus_t;

static inline uint8_t cm_mask8(unsigned w){ return (w>=8)?0xFFu:((1u<<w)-1u); }
static inline int cm_read8 (const cm_bus_t *b, uint16_t a, uint8_t *v){ return b->read8(b->user,a,v); }
static inline int cm_write8(const cm_bus_t *b, uint16_t a, uint8_t v){ return b->write8(b->user,a,v); }


static inline int cm_field_read8 (const cm_bus_t *b, uint16_t a, unsigned sh, unsigned w, uint8_t *out)
{
   	uint8_t v=0; 
	int rc=cm_read8(b,a,&v); 
	if(rc) return rc; 
	*out=(uint8_t)((v>>sh)&cm_mask8(w)); 
	return 0; 
}
static inline int cm_field_write8(const cm_bus_t *b, uint16_t a, unsigned sh, unsigned w, uint8_t val)
{
	uint8_t v=0; 
	int rc=cm_read8(b,a,&v); 
	if(rc) return rc; 
	uint8_t m=(uint8_t)(cm_mask8(w)<<sh);
	//fprintf(stderr, "cm_field_write8, a=0x%x, read=0x%x\n", a, v);
	v=(uint8_t)((v & (uint8_t)~m) | ((val & cm_mask8(w))<<sh)); 
	//fprintf(stderr, "cm_field_write8, a=0x%x, writing=0x%x\n", a, v);
	return cm_write8(b,a,v); 
}







#ifndef CM_STRIP_NAMES
  #define CM_NAME_STR(x) x
#else
  #define CM_NAME_STR(x) NULL
#endif

typedef struct cm_field_desc {
    const char *name;   /* may be NULL if CM_STRIP_NAMES */
    uint8_t     shift;
    uint8_t     width;
} cm_field_desc_t;

typedef struct cm_reg_desc {
    const char *name;   /* may be NULL if CM_STRIP_NAMES */
    uint16_t    offset;
    const cm_field_desc_t *fields;
    uint16_t    nfields;
} cm_reg_desc_t;

typedef struct cm_module_desc {
    const char *name;   /* may be NULL if CM_STRIP_NAMES */
    const uint16_t *bases;
    uint16_t        count;
    const cm_reg_desc_t *regs;
    uint16_t        nregs;
} cm_module_desc_t;

/* ---- Declarations for all modules ---- */
extern const cm_module_desc_t cm_Status_module;
extern const cm_module_desc_t cm_PWMEncoder_module;
extern const cm_module_desc_t cm_PWMDecoder_module;
extern const cm_module_desc_t cm_TOD_module;
extern const cm_module_desc_t cm_TODWrite_module;
extern const cm_module_desc_t cm_TODReadPrimary_module;
extern const cm_module_desc_t cm_TODReadSecondary_module;
extern const cm_module_desc_t cm_Input_module;
extern const cm_module_desc_t cm_Output_module;
extern const cm_module_desc_t cm_REFMON_module;
extern const cm_module_desc_t cm_PWM_USER_DATA_module;
extern const cm_module_desc_t cm_EEPROM_module;
extern const cm_module_desc_t cm_EEPROM_DATA_module;
extern const cm_module_desc_t cm_OUTPUT_TDC_CFG_module;
extern const cm_module_desc_t cm_OUTPUT_TDC_module;
extern const cm_module_desc_t cm_INPUT_TDC_module;
extern const cm_module_desc_t cm_PWM_SYNC_ENCODER_module;
extern const cm_module_desc_t cm_PWM_SYNC_DECODER_module;
extern const cm_module_desc_t cm_PWM_Rx_Info_module;
extern const cm_module_desc_t cm_DPLL_Ctrl_module;
extern const cm_module_desc_t cm_DPLL_Freq_Write_module;
extern const cm_module_desc_t cm_DPLL_Config_module;
extern const cm_module_desc_t cm_DPLL_GeneralStatus_module;

/* Aggregate index of all modules for iteration */
extern const cm_module_desc_t * const cm_all_modules[];
extern const size_t cm_all_modules_count;

/* ---- Utility: dump a module instance ----
 * printfn must be a printf-like function (e.g., printf).
 * Returns 0 on success, non-zero on first read error.
 */
int cm_dump_module(const cm_bus_t *bus, const cm_module_desc_t *mod, unsigned inst,
                   int (*printfn)(const char *fmt, ...));



/* ---- String-based lookup helpers -----------------------------------------
 *
 * These helpers let you address modules, registers, and fields by name.
 *
 * NOTE: If CM_STRIP_NAMES is defined and names are stripped, these will
 *       return error codes because the name pointers are NULL.
 */

/* Look up a module by name (e.g., "Input", "DPLL_Ctrl"). */
int cm_find_module(const char *name, const cm_module_desc_t **mod_out);

/* Look up a register by name within a module. */
int cm_find_reg(const cm_module_desc_t *mod,
                const char *reg_name,
                const cm_reg_desc_t **reg_out);

/* Look up a field by name within a register. */
int cm_find_field(const cm_reg_desc_t *reg,
                  const char *field_name,
                  const cm_field_desc_t **field_out);

/* Convenience: read a full 8-bit register by module/instance/reg name. */
int cm_string_read8(const cm_bus_t *bus,
                    const char *mod_name,
                    unsigned inst,
                    const char *reg_name,
                    uint8_t *value_out);

/* Convenience: write a full 8-bit register by module/instance/reg name. */
int cm_string_write8(const cm_bus_t *bus,
                     const char *mod_name,
                     unsigned inst,
                     const char *reg_name,
                     uint8_t value);

/* Convenience: Given a trigger read, read the value and write back same value to trigger module update of clock matrix */
int cm_string_trigger_rw(const cm_bus_t *bus,
                    const char *mod_name,
                    unsigned inst,
                    const char *reg_name);



/* Convenience: read a bitfield by module/instance/reg/field name. */
int cm_string_field_read8(const cm_bus_t *bus,
                          const char *mod_name,
                          unsigned inst,
                          const char *reg_name,
                          const char *field_name,
                          uint8_t *value_out);

/* Convenience: write a bitfield by module/instance/reg/field name. */
int cm_string_field_write8(const cm_bus_t *bus,
                           const char *mod_name,
                           unsigned inst,
                           const char *reg_name,
                           const char *field_name,
                           uint8_t value);


/* Convenience: write a sequence of bytes starting at a named register.
 *
 * Example:
 *   // Write 6 bytes of M[0..47] starting at INPUT_IN_FREQ_M_0_7:
 *   uint8_t m_bytes[6] = { ... };
 *   cm_string_write_bytes(&g_cm_bus,
 *                         "Input",              // module
 *                         inst,                // instance
 *                         "INPUT_IN_FREQ_M_0_7", // first reg
 *                         m_bytes,
 *                         6);
 */
int cm_string_write_bytes(const cm_bus_t *bus,
                          const char *mod_name,
                          unsigned inst,
                          const char *reg_name,
                          const uint8_t *data,
                          size_t len);

/* Convenience: read a sequence of bytes starting at a named register. */
int cm_string_read_bytes(const cm_bus_t *bus,
                         const char *mod_name,
                         unsigned inst,
                         const char *reg_name,
                         uint8_t *data,
                         size_t len);



// general clockmatrix utility functions for input and output setting
int dpll_compute_input_ratio(double freq_hz,
                             uint64_t *M_out,
                             uint16_t *N_reg_out,
                             double *actual_hz,
                             double *error_hz);

double dpll_input_freq_from_ratio(uint64_t M, uint16_t N_reg);


int dpll_compute_output_dco_and_divs(double f3_hz, double f4_hz,
                                            double *fdco_hz,
                                            uint32_t *d3,
                                            uint32_t *d4,
                                            double *a3,
                                            double *a4,
                                            double *e3,
                                            double *e4);



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
                              double   *out4_err);

#ifdef __cplusplus
}
#endif

#endif /* RENESAS_CM8A34001_TABLES_H */
