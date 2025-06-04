/**
 * @file hbr.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief HBR optimization header file
 */
#ifndef _HBR_BD_H_
#define _HBR_BD_H_
#include "common.h"
#include "ir1-bd.h"

#ifdef CONFIG_LATX_HBR
#define SHBR_NTYPE    0x1
#define SHBR_STYPE    0x2
#define SHBR_PTYPE    0x4
#define SHBR_SSE      0x8

void hbr_opt_bd(TranslationBlock **tb_list, int tb_num_in_tu);
/* void tb_xmm_analyse(TranslationBlock *tb); */
uint8_t get_inst_type_bd(IR1_INST *ir1);
bool can_shbr_opt64_bd(IR1_INST *ir1);
bool can_shbr_opt32_bd(IR1_INST *ir1);
#define SHBR_ON_64_BD(_ir1) can_shbr_opt64_bd(_ir1)
#define SHBR_ON_32_BD(_ir1) can_shbr_opt32_bd(_ir1)

#ifdef TARGET_X86_64
bool can_ghbr_opt_bd(IR1_INST *ir1);
#define GHBR_ON_BD(_ir1) can_ghbr_opt_bd(_ir1)
#else
#define GHBR_ON_BD(_ir1) (0)
#endif

#else /* !CONFIG_LATX_HBR */
#define SHBR_ON_64_BD(_ir1) (0)
#define SHBR_ON_32_BD(_ir1) (0)
#define GHBR_ON_BD(_ir1) (0)
#endif

#endif
