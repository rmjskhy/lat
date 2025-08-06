#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "flag-lbt-bd.h"
#include "translate-bd.h"
#include "insts-pattern.h"
#include "tu.h"
#include "hbr-bd.h"

#ifdef CONFIG_LATX_INSTS_PATTERN

#define WRAP(ins) (ND_INS_##ins)
#define EFLAGS_CACULATE(opnd0, opnd1, inst, i)                       \
    do {                                                             \
        bool need_calc_flag = ir1_need_calculate_any_flag_bd(inst);     \
        if (!need_calc_flag)                                         \
            break;                                                   \
        TranslationBlock *tb = lsenv->tr_data->curr_tb;              \
        IR2_OPND eflags = ra_alloc_label();                          \
        la_label(eflags);                                            \
        tb->eflags_target_arg[i] = ir2_opnd_label_id(&eflags);       \
        generate_eflag_calculation_bd(opnd0, opnd0, opnd1, inst, true); \
    } while (0)

static bool translate_cmp_jcc(IR1_INST *ir1)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1->instptn.next;

    ((INSTRUX *)(curr->info))->Instruction = WRAP(CMP);

    int em = ZERO_EXTENSION;
    switch (ir1_opcode_bd(next)) {
    case WRAP(JL):
    case WRAP(JNL): //JGE
    case WRAP(JLE):
    case WRAP(JNLE)://JG
        em = SIGN_EXTENSION;
        break;
    default:
        break;
    }

    IR2_OPND src_opnd_0 = load_ireg_from_ir1_bd(ir1_get_opnd_bd(curr, 0), em, false);
    IR2_OPND src_opnd_1 = load_ireg_from_ir1_bd(ir1_get_opnd_bd(curr, 1), em, false);

    IR2_OPND target_label_opnd = ra_alloc_label();
#ifdef CONFIG_LATX_TU
    IR2_OPND tu_reset_label_opnd = ra_alloc_label();
    la_label(tu_reset_label_opnd);
#endif

    switch (ir1_opcode_bd(next)) {
    case WRAP(JC):  //JB
        la_bltu(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JNC): //JAE
        la_bgeu(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JZ):  //JE
        la_beq(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JNZ): //JNE
        la_bne(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JBE):
        la_bgeu(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JNBE): //JA
        la_bltu(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JL):
        la_blt(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JNL): //JGE
        la_bge(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JLE):
        la_bge(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JNLE):  //JG
        la_blt(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    default:
        lsassert(0);
        break;
    }

#ifdef CONFIG_LATX_TU
    if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
        TranslationBlock *tb = lsenv->tr_data->curr_tb;
        tu_jcc_nop_gen(tb);
        la_label(target_label_opnd);
        /*tb->jmp_target_arg[0] = target_label_opnd._label_id;*/
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND translated_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(ir2_opnd_new(IR2_OPND_IMM, 0));
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }

        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);

        IR2_OPND target_label_opnd2 = ra_alloc_label();
        switch (ir1_opcode_bd(next)) {
        case WRAP(JC):  //JB
            la_bltu(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JNC): //JAE
            la_bgeu(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JZ):  //JE
            la_beq(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JNZ): //JNE
            la_bne(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JBE):
            la_bgeu(src_opnd_1, src_opnd_0, target_label_opnd2);
            break;
        case WRAP(JNBE): //JA
            la_bltu(src_opnd_1, src_opnd_0, target_label_opnd2);
            break;
        case WRAP(JL):
            la_blt(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JNL): //JGE
            la_bge(src_opnd_0, src_opnd_1, target_label_opnd2);
            break;
        case WRAP(JLE):
            la_bge(src_opnd_1, src_opnd_0, target_label_opnd2);
            break;
        case WRAP(JNLE): //JG
            la_blt(src_opnd_1, src_opnd_0, target_label_opnd2);
            break;
        default:
            lsassert(0);
            break;
        }
        /* not taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 0); */
        tr_generate_exit_tb_bd(next, 0);

        la_label(target_label_opnd2);
        /* taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 1); */
        tr_generate_exit_tb_bd(next, 1);

        /*
         * the backup of the eflags instruction, which is used
         * to recover the eflags instruction when unlink a tb.
         */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, EFLAG_BACKUP); */
        return true;
    }
#endif

    /* not taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 0);
    tr_generate_exit_tb_bd(next, 0);

    la_label(target_label_opnd);
    /* taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 1);
    tr_generate_exit_tb_bd(next, 1);

    /*
     * the backup of the eflags instruction, which is used
     * to recover the eflags instruction when unlink a tb.
     */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, EFLAG_BACKUP);
    return true;
}

static inline void gen_sub_bd(IR2_OPND dest, IR2_OPND src_opnd_0, IR2_OPND src_opnd_1,
        IR2_OPND mem_opnd, int imm, IR1_INST *ir1, IR1_OPND_BD *opnd0, int opnd0_size)
{
    la_sub_d(dest, src_opnd_0, src_opnd_1);
#ifdef TARGET_X86_64
    if (!GHBR_ON_BD(ir1) && ir1_opnd_is_gpr_bd(opnd0) && opnd0_size == 32) {
        la_mov32_zx(dest, dest);
    }
#endif
    /* write back */
    if (ir1_opnd_is_gpr_bd(opnd0)) {
        /* r16/r8 */
        if (opnd0_size < 32) {
            store_ireg_to_ir1_bd(dest, opnd0, false);
        }
    } else {
        la_st_by_op_size(dest, mem_opnd, imm, opnd0_size);
    }
}

static IR2_OPND load_opnd_from_opnd(IR2_OPND src_opnd, EXTENSION_MODE em, int opnd_size)
{
    lsassert(em == SIGN_EXTENSION || em == ZERO_EXTENSION ||
             em == UNKNOWN_EXTENSION);
    IR2_OPND ret_opnd = ra_alloc_itemp_internal();
    if (opnd_size == 64 || em == UNKNOWN_EXTENSION) {
#ifdef CONFIG_LATX_TU
        if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
            la_mov64(ret_opnd, src_opnd);
            return ret_opnd;
        }
#endif
        return src_opnd;
    }
    if (opnd_size == 32) {
        if (em == SIGN_EXTENSION ) {
            la_mov32_sx(ret_opnd, src_opnd);
        } else if (em == ZERO_EXTENSION ) {
            la_mov32_zx(ret_opnd, src_opnd);
        }
    } else if (opnd_size == 16) {
        if (em == SIGN_EXTENSION) {
            la_ext_w_h(ret_opnd, src_opnd);
        }
        else if (em == ZERO_EXTENSION ) {
            la_bstrpick_d(ret_opnd, src_opnd, 15, 0);
        }
    } else {
        if (em == SIGN_EXTENSION) {
            la_ext_w_b(ret_opnd, src_opnd);
        } else if (em == ZERO_EXTENSION ) {
            la_andi(ret_opnd, src_opnd, 0xff);
        }
    }
    return ret_opnd;
}

static inline void jcc_gen_bcc_bd(IR2_OPND src_opnd_0, IR2_OPND src_opnd_1,
        IR2_OPND target_label_opnd, IR1_INST *jcc_inst) {
    switch (ir1_opcode_bd(jcc_inst)) {
    case WRAP(JC):
        la_bltu(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JNC):
        la_bgeu(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JZ):
        la_beq(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JNZ):
        la_bne(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JBE):
        la_bgeu(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JNBE):
        la_bltu(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JL):
        la_blt(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JNL):
        la_bge(src_opnd_0, src_opnd_1, target_label_opnd);
        break;
    case WRAP(JLE):
        la_bge(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JNLE):
        la_blt(src_opnd_1, src_opnd_0, target_label_opnd);
        break;
    default:
        lsassert(0);
        break;
    }
}

static bool translate_sub_jcc(IR1_INST *ir1)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1->instptn.next;

    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(ir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(ir1, 1);
    IR2_OPND src_opnd_0, src_opnd_1;
    IR2_OPND bcc_src0, bcc_src1;
    IR2_OPND dest, mem_opnd;
    int imm, opnd0_size;

    ((INSTRUX *)(curr->info))->Instruction = WRAP(SUB);
    opnd0_size = ir1_opnd_size_bd(opnd0);

    bool is_lock = ir1_is_prefix_lock_bd(ir1) && ir1_opnd_is_mem_bd(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }
    if (is_lock) {
        translate_sub_bd(curr);
        translate_jcc_bd(next);
        return true;
    }

    /* int em = SIGN_EXTENSION; */
    int em = ZERO_EXTENSION;
    switch (ir1_opcode_bd(next)) {
    case WRAP(JL):
    case WRAP(JNL):
    case WRAP(JLE):
    case WRAP(JNLE):
        em = SIGN_EXTENSION;
        break;
    default:
        break;
    }

    src_opnd_1 = load_ireg_from_ir1_bd(opnd1, UNKNOWN_EXTENSION, false);
    if (ir1_opnd_is_gpr_bd(opnd0)) {
        src_opnd_0 = convert_gpr_opnd_bd(opnd0, UNKNOWN_EXTENSION);
        if (opnd0_size >= 32) {
            dest = src_opnd_0;
        } else {
            dest = ra_alloc_itemp();
        }
    } else {
        src_opnd_0 = ra_alloc_itemp();
        dest = src_opnd_0;
        mem_opnd = convert_mem_bd(opnd0, &imm);
        la_ld_by_op_size(src_opnd_0, mem_opnd, imm, opnd0_size);
    }

    bcc_src0 = load_opnd_from_opnd(src_opnd_0, em, opnd0_size);
    bcc_src1 = load_opnd_from_opnd(src_opnd_1, em, ir1_opnd_size_bd(opnd1));

    /* bcc_src0 = load_ireg_from_ir1_bd(opnd0, em, false); */
    /* bcc_src1 = load_ireg_from_ir1_bd(opnd1, em, false); */

    IR2_OPND target_label_opnd = ra_alloc_label();
#ifdef CONFIG_LATX_TU
    IR2_OPND tu_reset_label_opnd = ra_alloc_label();
    if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
        gen_sub_bd(dest, src_opnd_0, src_opnd_1, mem_opnd, imm,
                ir1, opnd0, opnd0_size);
        la_label(tu_reset_label_opnd);
        jcc_gen_bcc_bd(bcc_src0, bcc_src1, target_label_opnd, next);
        TranslationBlock *tb = lsenv->tr_data->curr_tb;
        tu_jcc_nop_gen(tb);
        la_label(target_label_opnd);
        /*tb->jmp_target_arg[0] = target_label_opnd._label_id;*/
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND translated_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(ir2_opnd_new(IR2_OPND_IMM, 0));
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }
        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);
        IR2_OPND target_label_opnd2 = ra_alloc_label();
        jcc_gen_bcc_bd(bcc_src0, bcc_src1, target_label_opnd2, next);
        tr_generate_exit_tb_bd(next, 0);
        la_label(target_label_opnd2);
        tr_generate_exit_tb_bd(next, 1);
        /* ra_free_temp(bcc_src0); */
        /* ra_free_temp(bcc_src1); */
        return true;
    } else
#endif
    {
        jcc_gen_bcc_bd(bcc_src0, bcc_src1, target_label_opnd, next);
    }
    /* not taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 0);
    gen_sub_bd(dest, src_opnd_0, src_opnd_1, mem_opnd, imm,
            ir1, opnd0, opnd0_size);
    tr_generate_exit_tb_bd(next, 0);

    la_label(target_label_opnd);
    /* taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 1);
    gen_sub_bd(dest, src_opnd_0, src_opnd_1, mem_opnd, imm,
            ir1, opnd0, opnd0_size);
    tr_generate_exit_tb_bd(next, 1);
    /*
     * the backup of the eflags instruction, which is used
     * to recover the eflags instruction when unlink a tb.
     */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, EFLAG_BACKUP);
    /* ra_free_temp(bcc_src0); */
    /* ra_free_temp(bcc_src1); */

    return true;
}

#ifdef CONFIG_LATX_XCOMISX_OPT
static inline bool xcomisx_jcc(IR1_INST *ir1, bool is_double, bool qnan_exp)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1->instptn.next;
    bool (*trans)(IR1_INST *) = translate_xcomisx_bd;
    IR2_INST* (*la_fcmp)(IR2_OPND, IR2_OPND, IR2_OPND, int);

    if (is_double) {
        la_fcmp = la_fcmp_cond_d;
        if (qnan_exp) {
            ((INSTRUX *)(curr->info))->Instruction = WRAP(COMISD);
        } else {
            ((INSTRUX *)(curr->info))->Instruction = WRAP(UCOMISD);
        }
    } else {
        la_fcmp = la_fcmp_cond_s;
        if (qnan_exp) {
            ((INSTRUX *)(curr->info))->Instruction = WRAP(COMISS);
        } else {
            ((INSTRUX *)(curr->info))->Instruction = WRAP(UCOMISS);
        }
    }

    IR2_OPND dest = load_freg128_from_ir1_bd(ir1_get_opnd_bd(ir1, 0));
    IR2_OPND src = load_freg128_from_ir1_bd(ir1_get_opnd_bd(ir1, 1));
    IR2_OPND target_label_opnd = ra_alloc_label();

    switch (ir1_opcode_bd(next)) {
    case WRAP(JNBE):
        la_fcmp(fcc7_ir2_opnd, src, dest, FCMP_COND_CLT + qnan_exp);
        break;
    case WRAP(JNC):
        la_fcmp(fcc7_ir2_opnd, src, dest, FCMP_COND_CLE + qnan_exp);
        break;
    case WRAP(JC):
	/* below or NAN, x86 special define */
        la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CULT + qnan_exp);
        break;
    case WRAP(JBE):
	/* below or equal or NAN, x86 special define */
        la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CULE + qnan_exp);
        break;
    case WRAP(JZ):
    case WRAP(JLE):
	/* equal or NAN, x86 special define */
        la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CUEQ + qnan_exp);
        break;
    case WRAP(JNZ):
    case WRAP(JNLE):
        la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CNE + qnan_exp);
        break;
    case WRAP(JL):
        break;
    case WRAP(JNL):
        la_b(target_label_opnd);
        break;
    default:
        lsassert(0);
        break;
    }

#ifdef CONFIG_LATX_TU
    IR2_OPND tu_reset_label_opnd = ra_alloc_label();
    la_label(tu_reset_label_opnd);
#endif
    if (ir1_opcode_bd(next) != WRAP(JL))
        la_bcnez(fcc7_ir2_opnd, target_label_opnd);

#ifdef CONFIG_LATX_TU
    if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
        TranslationBlock *tb = lsenv->tr_data->curr_tb;
        tu_jcc_nop_gen(tb);
        la_label(target_label_opnd);
        /*tb->jmp_target_arg[0] = target_label_opnd._label_id;*/
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND translated_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(ir2_opnd_new(IR2_OPND_IMM, 0));
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }

        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);

        IR2_OPND target_label_opnd2 = ra_alloc_label();
        switch (ir1_opcode_bd(next)) {
        case WRAP(JNBE):
            la_fcmp(fcc7_ir2_opnd, src, dest, FCMP_COND_CLT + qnan_exp);
            break;
        case WRAP(JNC):
            la_fcmp(fcc7_ir2_opnd, src, dest, FCMP_COND_CLE + qnan_exp);
            break;
        case WRAP(JC):
    	/* below or NAN, x86 special define */
            la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CULT + qnan_exp);
            break;
        case WRAP(JBE):
    	/* below or equal or NAN, x86 special define */
            la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CULE + qnan_exp);
            break;
        case WRAP(JZ):
        case WRAP(JLE):
    	/* equal or NAN, x86 special define */
            la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CUEQ + qnan_exp);
            break;
        case WRAP(JNZ):
        case WRAP(JNLE):
            la_fcmp(fcc7_ir2_opnd, dest, src, FCMP_COND_CNE + qnan_exp);
            break;
        case WRAP(JL):
            break;
        case WRAP(JNL):
            la_b(target_label_opnd);
        break;
        default:
            lsassert(0);
            break;
        }
        if (ir1_opcode_bd(next) != WRAP(JL))
            la_bcnez(fcc7_ir2_opnd, target_label_opnd2);
        /* not taken */
        tr_generate_exit_stub_tb_bd(next, 0, trans, curr);

        la_label(target_label_opnd2);
        /* taken */
        tr_generate_exit_stub_tb_bd(next, 1, trans, curr);

        return true;
    }
#endif


    /* not taken */
    tr_generate_exit_stub_tb_bd(next, 0, trans, curr);

    la_label(target_label_opnd);
    /* taken */
    tr_generate_exit_stub_tb_bd(next, 1, trans, curr);

    return true;
}

static bool translate_comisd_jcc(IR1_INST *ir1)
{
    return xcomisx_jcc(ir1, true, true);
}

static bool translate_comiss_jcc(IR1_INST *ir1)
{
    return xcomisx_jcc(ir1, false, true);
}

static bool translate_ucomisd_jcc(IR1_INST *ir1)
{
    return xcomisx_jcc(ir1, true, false);
}

static bool translate_ucomiss_jcc(IR1_INST *ir1)
{
    return xcomisx_jcc(ir1, false, false);
}
#endif

static bool translate_bt_jcc(IR1_INST *ir1)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1->instptn.next;

    ((INSTRUX *)(curr->info))->Instruction = WRAP(BT);
    IR1_OPND_BD *bt_opnd0 = ir1_get_opnd_bd(curr, 0);
    IR1_OPND_BD *bt_opnd1 = ir1_get_opnd_bd(curr, 1);
    IR2_OPND src_opnd_0, src_opnd_1, bit_offset;
    int imm;

    src_opnd_1 = load_ireg_from_ir1_bd(bt_opnd1, ZERO_EXTENSION, false);

    bit_offset = ra_alloc_itemp();
    la_bstrpick_d(bit_offset, src_opnd_1,
        __builtin_ctz(ir1_opnd_size_bd(bt_opnd0)) - 1, 0);
    if (ir1_opnd_is_gpr_bd(bt_opnd0)) {
        /* r16/r32/r64 */
        src_opnd_0 = convert_gpr_opnd_bd(bt_opnd0, UNKNOWN_EXTENSION);
    } else {
        src_opnd_0 = ra_alloc_itemp();
        IR2_OPND tmp_mem_op = convert_mem_bd(bt_opnd0, &imm);
        IR2_OPND mem_opnd = ra_alloc_itemp();
        la_or(mem_opnd, tmp_mem_op, zero_ir2_opnd);
#ifdef CONFIG_LATX_IMM_REG
        imm_cache_free_temp_helper(tmp_mem_op);
#else
        ra_free_temp_auto(tmp_mem_op);
#endif

        if (ir1_opnd_is_gpr_bd(bt_opnd1)) {
            IR2_OPND tmp = ra_alloc_itemp();
            IR2_OPND src1 = convert_gpr_opnd_bd(bt_opnd1, UNKNOWN_EXTENSION);
            int opnd_size = ir1_opnd_size_bd(bt_opnd0);
            int ctz_opnd_size = __builtin_ctz(opnd_size);
            int ctz_align_size = __builtin_ctz(opnd_size / 8);
            lsassertm((opnd_size == 16) || (opnd_size == 32) ||
                (opnd_size == 64), "%s opnd_size error!", __func__);
            la_srai_d(tmp, src1, ctz_opnd_size);
            la_alsl_d(mem_opnd, tmp, mem_opnd, ctz_align_size - 1);
            ra_free_temp(tmp);
        }

        if (ir1_opnd_size_bd(bt_opnd0) == 64) {
            /* m64 */
            la_ld_d(src_opnd_0, mem_opnd, imm);
        } else {
            /* m16/m32 */
            la_ld_w(src_opnd_0, mem_opnd, imm);
        }
        ra_free_temp(mem_opnd);
    }

    IR2_OPND temp_opnd = ra_alloc_itemp();
    la_srl_d(temp_opnd, src_opnd_0, bit_offset);
    la_andi(temp_opnd, temp_opnd, 1);

    IR2_OPND target_label_opnd = ra_alloc_label();
#ifdef CONFIG_LATX_TU
    IR2_OPND tu_reset_label_opnd = ra_alloc_label();
    la_label(tu_reset_label_opnd);
#endif

    switch (ir1_opcode_bd(next)) {
    case WRAP(JC):   /*CF=1*/
        la_bne(temp_opnd, zero_ir2_opnd, target_label_opnd);
        break;
    case WRAP(JNC):  /*CF=0*/
        la_beq(temp_opnd, zero_ir2_opnd, target_label_opnd);
        break;
    default:
        lsassert(0);
        break;
    }

#ifdef CONFIG_LATX_TU
    if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
        TranslationBlock *tb = lsenv->tr_data->curr_tb;
        tu_jcc_nop_gen(tb);
        la_label(target_label_opnd);
        /*tb->jmp_target_arg[0] = target_label_opnd._label_id;*/
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND translated_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(ir2_opnd_new(IR2_OPND_IMM, 0));
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }

        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);

        IR2_OPND target_label_opnd2 = ra_alloc_label();
        switch (ir1_opcode_bd(next)) {
            case WRAP(JC):   /*CF=1*/
                la_bne(temp_opnd, zero_ir2_opnd, target_label_opnd2);
                break;
            case WRAP(JNC):  /*CF=0*/
                la_beq(temp_opnd, zero_ir2_opnd, target_label_opnd2);
                break;
            default:
                lsassert(0);
                break;
        }
        /* not taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 0); */
        tr_generate_exit_tb_bd(next, 0);

        la_label(target_label_opnd2);
        /* taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 1); */
        tr_generate_exit_tb_bd(next, 1);

        /*
         * the backup of the eflags instruction, which is used
         * to recover the eflags instruction when unlink a tb.
         */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, EFLAG_BACKUP); */
        return true;
    }
#endif


    /* not taken */
    EFLAGS_CACULATE(src_opnd_0, bit_offset, curr, 0);
    tr_generate_exit_tb_bd(next, 0);

    la_label(target_label_opnd);
    /* taken */
    EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 1);
    tr_generate_exit_tb_bd(next, 1);

    /*
     * the backup of the eflags instruction, which is used
     * to recover the eflags instruction when unlink a tb.
     */
    EFLAGS_CACULATE(src_opnd_0, bit_offset, curr, EFLAG_BACKUP);

    ra_free_temp_auto(src_opnd_0);
    ra_free_temp(bit_offset);
    ra_free_temp_auto(src_opnd_1);
    return true;
}

static bool translate_cqo_idiv(IR1_INST *ir1)
{
    IR1_INST *next = ir1->instptn.next;

    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1_bd(ir1_get_opnd_bd(next, 0), SIGN_EXTENSION, false);

    IR2_OPND label_z = ra_alloc_label();
    la_bne(src_opnd_0, zero_ir2_opnd, label_z);
    la_break(0x7);
    la_label(label_z);

    if (ir1_opnd_size_bd(ir1_get_opnd_bd(next, 0)) != 64) {
        lsassert(0);
    } else {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1_bd(&rax_ir1_opnd_bd, SIGN_EXTENSION, false);
        IR2_OPND temp_src = ra_alloc_itemp();
        IR2_OPND temp1_opnd = ra_alloc_itemp();

        la_or(temp_src, zero_ir2_opnd, src_opnd_1);

        la_mod_d(temp1_opnd, temp_src, src_opnd_0);
        la_div_d(temp_src, temp_src, src_opnd_0);

        store_ireg_to_ir1_bd(temp_src, &rax_ir1_opnd_bd, false);
        store_ireg_to_ir1_bd(temp1_opnd, &rdx_ir1_opnd_bd, false);

        ra_free_temp(temp_src);
        ra_free_temp(temp1_opnd);
    }

    return true;
}

static bool translate_cmp_sbb(IR1_INST *ir1)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1->instptn.next;

    /* cmp */
    IR1_OPND_BD *cmp_opnd0 = ir1_get_opnd_bd(curr, 0);
    IR1_OPND_BD *cmp_opnd1 = ir1_get_opnd_bd(curr, 1);

    bool cmp_opnd1_is_imm = ir1_opnd_is_simm12_bd(cmp_opnd1);

    IR2_OPND cmp_opnd_0 = load_ireg_from_ir1_bd(cmp_opnd0, SIGN_EXTENSION, false);
    IR2_OPND cmp_opnd_1;

    /* sbb */
    IR1_OPND_BD *sbb_opnd0 = ir1_get_opnd_bd(next, 0);
    lsassert(ir1_opnd_is_same_reg_bd(sbb_opnd0, ir1_get_opnd_bd(next, 1)));
    bool opnd_clobber = ir1_opnd_size_bd(sbb_opnd0) != 64;

    IR2_OPND cond = opnd_clobber
                        ? ra_alloc_itemp()
                        : ra_alloc_gpr(ir1_opnd_base_reg_num_bd(sbb_opnd0));
    /* caculate cmp */
    if (cmp_opnd1_is_imm) {
        la_sltui(cond, cmp_opnd_0, ir1_opnd_simm_bd(cmp_opnd1));
    } else {
        cmp_opnd_1 = load_ireg_from_ir1_bd(cmp_opnd1, SIGN_EXTENSION, false);
        la_sltu(cond, cmp_opnd_0, cmp_opnd_1);
    }

    /* we need change to sub because sbb uses CF (not calculate) */
    ((INSTRUX *)(next->info))->Instruction = WRAP(SUB);
    generate_eflag_calculation_bd(zero_ir2_opnd, zero_ir2_opnd, cond, next, true);

    la_sub_d(cond, zero_ir2_opnd, cond);
    if (opnd_clobber) {
        store_ireg_to_ir1_bd(cond, sbb_opnd0, false);
        ra_free_temp(cond);
    }

    return true;
}

static bool translate_test_jcc(IR1_INST *ir1)
{
    IR1_INST *curr = ir1;
    IR1_INST *next = ir1->instptn.next;
#ifdef CONFIG_LATX_TU
    bool is_branch = true;
#endif

    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(ir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(ir1, 1);

    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1_bd(opnd0, SIGN_EXTENSION, false);
    
    IR2_OPND src_opnd_1;
    IR2_OPND temp;
    int is_same_reg = ir1_opnd_is_same_reg_bd(opnd0, opnd1);
    if (!is_same_reg) {
        src_opnd_1 = load_ireg_from_ir1_bd(opnd1, SIGN_EXTENSION, false);
        temp = ra_alloc_itemp();
        la_and(temp, src_opnd_0, src_opnd_1);
    }

    IR2_OPND target_label_opnd = ra_alloc_label();

#ifdef CONFIG_LATX_TU
    IR2_OPND tu_reset_label_opnd = ra_alloc_label();
    la_label(tu_reset_label_opnd);
#endif


    switch (ir1_opcode_bd(next)) {
    case WRAP(JZ):
        la_beq(is_same_reg ? src_opnd_0 : temp, zero_ir2_opnd, target_label_opnd);
        // la_beqz(src_opnd_0, target_label_opnd);
        break;
    case WRAP(JNZ):
        // la_bnez(src_opnd_0, target_label_opnd);
        la_bne(is_same_reg ? src_opnd_0 : temp, zero_ir2_opnd, target_label_opnd);
        break;
    case WRAP(JS):
        lsassert(ir1_opnd_is_same_reg_bd(opnd0, opnd1));
        la_blt(src_opnd_0, zero_ir2_opnd, target_label_opnd);
        break;
    case WRAP(JNS):
        lsassert(ir1_opnd_is_same_reg_bd(opnd0, opnd1));
        la_bge(src_opnd_0, zero_ir2_opnd, target_label_opnd);
        break;
    case WRAP(JLE):
        lsassert(ir1_opnd_is_same_reg_bd(opnd0, opnd1));
        la_bge(zero_ir2_opnd, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JNLE):
        lsassert(ir1_opnd_is_same_reg_bd(opnd0, opnd1));
        la_blt(zero_ir2_opnd, src_opnd_0, target_label_opnd);
        break;
    case WRAP(JNO):
        /*
         * OF = 0
         * For compatibility with bcc+b.
         */
        la_beq(zero_ir2_opnd, zero_ir2_opnd, target_label_opnd);
        // la_beqz(zero_ir2_opnd, target_label_opnd);
        break;
    case WRAP(JO):
        /* OF = 1 */
#ifdef CONFIG_LATX_TU
        is_branch = false;
#endif
        break;
    case WRAP(JC):
        /* CF = 1 */
#ifdef CONFIG_LATX_TU
        is_branch = false;
#endif
        break;
    case WRAP(JBE):
        /* CF = 1 or ZF = 1 */
        la_beq(is_same_reg ? src_opnd_0 : temp, zero_ir2_opnd, target_label_opnd);
        // la_beqz(src_opnd_0, target_label_opnd);
        break;
    case WRAP(JNBE):
        la_bne(is_same_reg ? src_opnd_0 : temp, zero_ir2_opnd, target_label_opnd);
        /* CF = 0 and ZF = 0 */
        // la_bnez(src_opnd_0, target_label_opnd);
        break;
    case WRAP(JNC):
        /*
         * CF = 0
         * For compatibility with bcc+b.
         */
        la_beq(zero_ir2_opnd, zero_ir2_opnd, target_label_opnd);
        // la_beqz(zero_ir2_opnd, target_label_opnd);
        break;
    default:
        lsassert(0);
        break;
    }
#ifdef CONFIG_LATX_TU
    if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
        TranslationBlock *tb = lsenv->tr_data->curr_tb;
        tu_jcc_nop_gen(tb);
        la_label(target_label_opnd);
        if (is_branch) {
            /*tb->jmp_target_arg[0] = target_label_opnd._label_id;*/
            tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
        } else {
            /*tb->jmp_target_arg[0] = TB_JMP_RESET_OFFSET_INVALID;*/
            tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
        }
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND translated_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(ir2_opnd_new(IR2_OPND_IMM, 0));
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }

        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);

        IR2_OPND target_label_opnd2 = ra_alloc_label();
        switch (ir1_opcode_bd(next)) {
        case WRAP(JZ):
            la_beq(is_same_reg ? src_opnd_0 : temp, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JNZ):
            la_bne(is_same_reg ? src_opnd_0 : temp, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JS):
            lsassert(ir1_opnd_is_same_reg_bd(opnd0, opnd1));
            la_blt(src_opnd_0, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JNS):
            lsassert(ir1_opnd_is_same_reg_bd(opnd0, opnd1));
            la_bge(src_opnd_0, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JLE):
            lsassert(ir1_opnd_is_same_reg_bd(opnd0, opnd1));
            la_bge(zero_ir2_opnd, src_opnd_0, target_label_opnd2);
            break;
        case WRAP(JNLE):
            lsassert(ir1_opnd_is_same_reg_bd(opnd0, opnd1));
            la_blt(zero_ir2_opnd, src_opnd_0, target_label_opnd2);
            break;
        case WRAP(JNO):
            /*
             * OF = 0
             * For compatibility with bcc+b.
             */
            la_beq(zero_ir2_opnd, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JO):
            /* OF = 1 */
#ifdef CONFIG_LATX_TU
            is_branch = false;
#endif
            break;
        case WRAP(JC):
            /* CF = 1 */
#ifdef CONFIG_LATX_TU
            is_branch = false;
#endif
            break;
        case WRAP(JBE):
            /* CF = 1 or ZF = 1 */
            la_beq(is_same_reg ? src_opnd_0 : temp, zero_ir2_opnd, target_label_opnd2);
            break;
        case WRAP(JNBE):
            la_bne(is_same_reg ? src_opnd_0 : temp, zero_ir2_opnd, target_label_opnd2);
            /* CF = 0 and ZF = 0 */
            break;
        case WRAP(JNC):
            /*
             * CF = 0
             * For compatibility with bcc+b.
             */
            la_beq(zero_ir2_opnd, zero_ir2_opnd, target_label_opnd2);
            break;
        default:
            lsassert(0);
            break;
        }
        /* not taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, 0); */
        tr_generate_exit_tb_bd(next, 0);

        la_label(target_label_opnd2);
        /* taken */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, 1); */
        tr_generate_exit_tb_bd(next, 1);

        /*
         * the backup of the eflags instruction, which is used
         * to recover the eflags instruction when unlink a tb.
         */
        /* EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, EFLAG_BACKUP); */

        return true;
    }
#endif

    /* not taken */
    EFLAGS_CACULATE(src_opnd_0, is_same_reg ? src_opnd_0 : src_opnd_1, curr, 0);
    tr_generate_exit_tb_bd(next, 0);

    la_label(target_label_opnd);
    /* taken */
    EFLAGS_CACULATE(src_opnd_0, is_same_reg ? src_opnd_0 : src_opnd_1, curr, 1);
    tr_generate_exit_tb_bd(next, 1);

    /*
     * the backup of the eflags instruction, which is used
     * to recover the eflags instruction when unlink a tb.
     */
    EFLAGS_CACULATE(src_opnd_0, is_same_reg ? src_opnd_0 : src_opnd_1, curr, EFLAG_BACKUP);
    return true;
}

static bool translate_xor_div(IR1_INST *ir1)
{
    IR1_INST *next = ir1->instptn.next;

    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1_bd(ir1_get_opnd_bd(next, 0), ZERO_EXTENSION, false);

    IR2_OPND label_z = ra_alloc_label();
    la_bne(src_opnd_0, zero_ir2_opnd, label_z);
    la_break(0x7);
    la_label(label_z);

    IR2_OPND temp_src = ra_alloc_itemp();
    IR2_OPND temp1_opnd = ra_alloc_itemp();
    if (ir1_opnd_size_bd(ir1_get_opnd_bd(next, 0)) == 32) {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1_bd(&eax_ir1_opnd_bd, ZERO_EXTENSION, false);
        la_or(temp_src, zero_ir2_opnd, zero_ir2_opnd);
        la_or(temp_src, zero_ir2_opnd, src_opnd_1);

        la_mod_du(temp1_opnd, temp_src, src_opnd_0);
        la_div_du(temp_src, temp_src, src_opnd_0);

        store_ireg_to_ir1_bd(temp_src, &eax_ir1_opnd_bd, false);
        store_ireg_to_ir1_bd(temp1_opnd, &edx_ir1_opnd_bd, false);
    } else if (ir1_opnd_size_bd(ir1_get_opnd_bd(next, 0)) == 64) {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1_bd(&rax_ir1_opnd_bd, ZERO_EXTENSION, false);

        la_or(temp_src, zero_ir2_opnd, src_opnd_1);

        la_mod_du(temp1_opnd, temp_src, src_opnd_0);
        la_div_du(temp_src, temp_src, src_opnd_0);

        store_ireg_to_ir1_bd(temp_src, &rax_ir1_opnd_bd, false);
        store_ireg_to_ir1_bd(temp1_opnd, &rdx_ir1_opnd_bd, false);
    } else {
        lsassert(0);
    }
    ra_free_temp(temp_src);
    ra_free_temp(temp1_opnd);

    return true;
}

static bool translate_cdq_idiv(IR1_INST *ir1)
{
    IR1_INST *next = ir1->instptn.next;

    IR2_OPND src_opnd_0 =
        load_ireg_from_ir1_bd(ir1_get_opnd_bd(next, 0), SIGN_EXTENSION, false);

    IR2_OPND label_z = ra_alloc_label();
    la_bne(src_opnd_0, zero_ir2_opnd, label_z);
    la_break(0x7);
    la_label(label_z);

    if (ir1_opnd_size_bd(ir1_get_opnd_bd(next, 0)) != 32) {
        lsassert(0);
    } else {
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1_bd(&eax_ir1_opnd_bd, SIGN_EXTENSION, false);
        IR2_OPND temp_src = ra_alloc_itemp();
        IR2_OPND temp1_opnd = ra_alloc_itemp();

        la_or(temp_src, zero_ir2_opnd, src_opnd_1);

        la_mod_d(temp1_opnd, temp_src, src_opnd_0);
        la_div_d(temp_src, temp_src, src_opnd_0);

        store_ireg_to_ir1_bd(temp_src, &eax_ir1_opnd_bd, false);
        store_ireg_to_ir1_bd(temp1_opnd, &edx_ir1_opnd_bd, false);

        ra_free_temp(temp_src);
        ra_free_temp(temp1_opnd);
    }
    return true;
}

static bool ir1_is_same_opnd(IR1_OPND_BD *opnd0, IR1_OPND_BD *opnd1)
{
    if (ir1_opnd_is_same_reg_bd(opnd0, opnd1)) {
        if (ir1_opnd_is_pc_relative_bd(opnd0))
            return true;
    } else if (ir1_opnd_is_mem_bd(opnd0) && ir1_opnd_is_mem_bd(opnd1)) {
        if (opnd0->Info.Memory.Base == opnd1->Info.Memory.Base &&
            opnd0->Info.Memory.Index == opnd1->Info.Memory.Index &&
            opnd0->Info.Memory.Seg == opnd1->Info.Memory.Seg &&
            opnd0->Info.Memory.Scale == opnd1->Info.Memory.Scale &&
            opnd0->Info.Memory.Disp == opnd1->Info.Memory.Disp) {
            if (!ir1_opnd_is_pc_relative_bd(opnd0))
                return true;
        }
    }
    return false;
}

static
bool translate_cmp_xxcc_con(IR1_INST *ir1)
{
    IR1_INST *curr = ir1;
    IR1_OPND_BD *cmp_opnd0 = ir1_get_opnd_bd(curr, 0);
    IR1_OPND_BD *cmp_opnd1 = ir1_get_opnd_bd(curr, 1);
    IR2_OPND src0 = load_ireg_from_ir1_bd(ir1_get_opnd_bd(curr, 0), SIGN_EXTENSION, false);
    IR2_OPND src1 = load_ireg_from_ir1_bd(ir1_get_opnd_bd(curr, 1), SIGN_EXTENSION, false);
    generate_eflag_calculation_bd(src0, src0, src1, curr, true);

    int src0_change = 0;
    int src1_change = 0;

    for (; curr->instptn.next != NULL;) {
        curr = curr->instptn.next;
        lsenv->tr_data->curr_ir1_inst = curr;
        lsenv->tr_data->curr_ir1_count++;

        IR1_OPND_BD *next_opnd0 = ir1_get_opnd_bd(curr, 0);
        if (!src0_change && ir1_opnd_is_gpr_bd(cmp_opnd0) && ir1_opnd_is_gpr_bd(next_opnd0) &&
            ir1_opnd_is_same_reg_without_width_bd(cmp_opnd0, next_opnd0)) {
            src0_change = 1;
            IR2_OPND src0_t = ra_alloc_itemp();
            la_or(src0_t, zero_ir2_opnd, src0);
            ra_free_temp_auto(src0);
            src0 = src0_t;
        }
        if (!src1_change && ir1_opnd_is_gpr_bd(cmp_opnd1) && ir1_opnd_is_gpr_bd(next_opnd0) &&
            ir1_opnd_is_same_reg_without_width_bd(cmp_opnd1, next_opnd0)) {
            src1_change = 1;
            IR2_OPND src1_t = ra_alloc_itemp();
            la_or(src1_t, zero_ir2_opnd, src1);
            ra_free_temp_auto(src1);
            src1 = src1_t;
        }

        IR2_OPND target_label = ra_alloc_label();
        IR2_OPND exit_label = ra_alloc_label();

        int is_cmovcc = 0;
        switch (ir1_opcode_bd(curr)) {
        case WRAP(CMOVC):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETC):
            la_bltu(src0, src1, target_label);
            break;
        case WRAP(CMOVNC):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETNC):
            la_bgeu(src0, src1, target_label);
            break;
        case WRAP(CMOVZ):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETZ):
            la_beq(src0, src1, target_label);
            break;
        case WRAP(CMOVNZ):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETNZ):
            la_bne(src0, src1, target_label);
            break;
        case WRAP(CMOVBE):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETBE):
            la_bgeu(src1, src0, target_label);
            break;
        case WRAP(CMOVNBE):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETNBE):
            la_bltu(src1, src0, target_label);
            break;
        case WRAP(CMOVL):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETL):
            la_blt(src0, src1, target_label);
            break;
        case WRAP(CMOVNL):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETNL):
            la_bge(src0, src1, target_label);
            break;
        case WRAP(CMOVLE):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETLE):
            la_bge(src1, src0, target_label);
            break;
        case WRAP(CMOVNLE):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETNLE):
            la_blt(src1, src0, target_label);
            break;
        default:        lsassert(0);        break;
        }

        if (is_cmovcc) {
            IR1_OPND_BD *next_opnd1 = ir1_get_opnd_bd(curr, 1);

            /* no mov */
            if (ir1_opnd_size_bd(next_opnd0) == 32) {
                IR2_OPND dest_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num_bd(next_opnd0));
                la_mov32_zx(dest_opnd, dest_opnd);
            }
            la_b(exit_label);
            la_label(target_label);

            IR2_OPND *src_opnd_t = ir1_is_same_opnd(next_opnd1, cmp_opnd0) ? (src0_change ? NULL : &src0) : NULL;
            src_opnd_t = ir1_is_same_opnd(next_opnd1, cmp_opnd1) ? (src1_change ? NULL : &src1) : NULL;
            if (src_opnd_t) {
                store_ireg_to_ir1_bd(*src_opnd_t, next_opnd0, false);
            } else {
                IR2_OPND src_opnd = load_ireg_from_ir1_bd(next_opnd1, SIGN_EXTENSION, false);
                store_ireg_to_ir1_bd(src_opnd, next_opnd0, false);
                ra_free_temp_auto(src_opnd);
            }
        } else {
            /* set 0 */
            store_ireg_to_ir1_bd(zero_ir2_opnd, next_opnd0, false);
            la_b(exit_label);
            la_label(target_label);

            /* set 1 */
            IR2_OPND temp1 = ra_alloc_itemp();
            la_ori(temp1, zero_ir2_opnd, 1);
            store_ireg_to_ir1_bd(temp1, next_opnd0, false);
            ra_free_temp(temp1);
        }
        la_label(exit_label);
    }
    return true;
}

static
bool translate_test_xxcc_con(IR1_INST *pir1)
{
    IR1_INST *curr = pir1;
    IR1_OPND_BD *test_opnd0 = ir1_get_opnd_bd(curr, 0);
    IR1_OPND_BD *test_opnd1 = ir1_get_opnd_bd(curr, 1);

    int is_same_reg = ir1_opnd_is_same_reg_bd(test_opnd0, test_opnd1);

    IR2_OPND src0 = load_ireg_from_ir1_bd(test_opnd0, SIGN_EXTENSION, false);
    IR2_OPND src1;
    IR2_OPND temp;
    if (is_same_reg) {
        generate_eflag_calculation_bd(src0, src0, src0, curr, true);
    } else {
        src1 = load_ireg_from_ir1_bd(test_opnd1, SIGN_EXTENSION, false);
        generate_eflag_calculation_bd(src0, src0, src1, curr, true);
        temp = ra_alloc_itemp();
        la_and(temp, src0, src1);
    }

    int src0_change = 0;
    for (; curr->instptn.next != NULL;) {
        curr = curr->instptn.next;
        lsenv->tr_data->curr_ir1_inst = curr;
        lsenv->tr_data->curr_ir1_count++;

        IR1_OPND_BD *next_opnd0 = ir1_get_opnd_bd(curr, 0);
        if (!src0_change && ir1_opnd_is_gpr_bd(test_opnd0) && ir1_opnd_is_gpr_bd(next_opnd0) &&
            ir1_opnd_is_same_reg_without_width_bd(test_opnd0, next_opnd0)) {
            src0_change = 1;
            IR2_OPND src0_t = ra_alloc_itemp();
            la_or(src0_t, zero_ir2_opnd, src0);
            ra_free_temp_auto(src0);
            src0 = src0_t;
        }

        IR2_OPND target_label = ra_alloc_label();
        IR2_OPND exit_label = ra_alloc_label();

        int is_cmovcc = 0;
        switch (ir1_opcode_bd(curr)) {
        case WRAP(CMOVZ):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETZ):
            la_beqz(is_same_reg ? src0 : temp, target_label);
            break;
        case WRAP(CMOVNZ):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETNZ):
            la_bnez(is_same_reg ? src0 : temp, target_label);
            break;
        case WRAP(CMOVS):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETS):
            lsassert(ir1_opnd_is_same_reg_bd(test_opnd0, test_opnd1));
            la_blt(src0, zero_ir2_opnd, target_label);
            break;
        case WRAP(CMOVNS):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETNS):
            lsassert(ir1_opnd_is_same_reg_bd(test_opnd0, test_opnd1));
            la_bge(src0, zero_ir2_opnd, target_label);
            break;
        case WRAP(CMOVLE):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETLE):
            lsassert(ir1_opnd_is_same_reg_bd(test_opnd0, test_opnd1));
            la_bge(zero_ir2_opnd, src0, target_label);
            break;
        case WRAP(CMOVNLE):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETNLE):
            lsassert(ir1_opnd_is_same_reg_bd(test_opnd0, test_opnd1));
            la_blt(zero_ir2_opnd, src0, target_label);
            break;
        case WRAP(CMOVNO):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETNO):
            /* OF = 0 For compatibility with bcc+b */
            //latxs_append_ir2_opnd2(LISA_BEQZ, zero_ir2_opnd, &target_label);
            la_b(target_label);
            break;
        case WRAP(CMOVO):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETO):
            /* TODO tb->opt_bcc = false;*/
            /* OF = 1 */
            break;
        case WRAP(CMOVC):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETC):
            /* TODO tb->opt_bcc = false;*/
            /* CF = 1 */
            break;
        case WRAP(CMOVBE):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETBE):
            /* CF = 1 or ZF = 1 */
            la_beqz(is_same_reg ? src0 : temp, target_label);
            break;
        case WRAP(CMOVNBE):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETNBE):
            /* CF = 0 and ZF = 0 */
            la_bnez(is_same_reg ? src0 : temp, target_label);
            break;
        case WRAP(CMOVNC):
            is_cmovcc = 1;  __attribute__((fallthrough));
        case WRAP(SETNC):
            /* CF = 0 For compatibility with bcc+b */
            //latxs_append_ir2_opnd2(LISA_BEQZ, zero_ir2_opnd, &target_label);
            la_b(target_label);
            break;
        default:
            lsassert(0);
            break;
        }

        if (is_cmovcc) {
            IR1_OPND_BD *next_opnd1 = ir1_get_opnd_bd(curr, 1);

            /* no mov */
            if (ir1_opnd_size_bd(next_opnd0) == 32) {
                IR2_OPND dest_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num_bd(next_opnd0));
                la_mov32_zx(dest_opnd, dest_opnd);
            }
            la_b(exit_label);
            la_label(target_label);

            IR2_OPND *src_opnd_t = NULL;
            src_opnd_t = ir1_is_same_opnd(next_opnd1, test_opnd0) ? (src0_change ? NULL : &src0) : src_opnd_t;
            if (!is_same_reg) {
                src_opnd_t = ir1_is_same_opnd(next_opnd1, test_opnd1) ? &src1 : src_opnd_t;
            }

            if (src_opnd_t) {
                store_ireg_to_ir1_bd(*src_opnd_t, next_opnd0, false);
            } else {
                IR2_OPND src_opnd = load_ireg_from_ir1_bd(next_opnd1, SIGN_EXTENSION, false);;
                store_ireg_to_ir1_bd(src_opnd, next_opnd0, false);
                ra_free_temp_auto(src_opnd);
            }
        } else {
            /* set 0 */
            store_ireg_to_ir1_bd(zero_ir2_opnd, ir1_get_opnd_bd(curr, 0), false);
            la_b(exit_label);
            la_label(target_label);

            /* set 1 */
            IR2_OPND temp1 = ra_alloc_itemp();
            la_ori(temp1, zero_ir2_opnd, 1);
            store_ireg_to_ir1_bd(temp1, next_opnd0, false);
            ra_free_temp(temp1);
        }
        la_label(exit_label);
    }

    return true;
}

static bool translate_ucomisd_seta(IR1_INST *pir1)
{
    IR1_INST *curr = pir1;
    IR1_INST *next = curr->instptn.next;

    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(curr, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(curr, 1);
    IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
    IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
    /* 0. set flag = 0 */
    IR2_OPND flag_zf = ra_alloc_itemp();
    IR2_OPND flag_pf = ra_alloc_itemp();
    IR2_OPND flag = ra_alloc_itemp();
    la_mov64(flag, zero_ir2_opnd);

    /* 1. check ZF, are they equal & unordered? */
    la_fcmp_cond_d(fcc0_ir2_opnd, dest, src, FCMP_COND_CUEQ);
    la_movcf2gr(flag_zf, fcc0_ir2_opnd);

    /* 2. check CF, are they less & unordered? */
    la_fcmp_cond_d(fcc2_ir2_opnd, dest, src, FCMP_COND_CULT);
    la_movcf2gr(flag, fcc2_ir2_opnd);

    /* 3. check PF, are they unordered? (= ZF & CF) */
    la_and(flag_pf, flag, flag_zf);

    la_bstrins_w(flag, flag_zf, ZF_BIT_INDEX, ZF_BIT_INDEX);
    la_bstrins_w(flag, flag_pf, PF_BIT_INDEX, PF_BIT_INDEX);

    ra_free_temp(flag_pf);
    ra_free_temp(flag_zf);
    /* 4. mov flag to EFLAGS */
    la_x86mtflag(flag, 0x3f);

    ra_free_temp(flag);

    IR2_OPND* cj = NULL;
    switch (ir1_opcode_bd(next)) {
    case WRAP(SETNBE):
        la_fcmp_cond_d(fcc0_ir2_opnd, src, dest, FCMP_COND_CLT);
        cj = &fcc0_ir2_opnd;
        break;
    default:
        lsassert(0);
        break;
    }

    IR2_OPND target_label = ra_alloc_label();
    IR2_OPND exit_label = ra_alloc_label();
    la_bcnez(*cj, target_label);

    /* set 0 */
    store_ireg_to_ir1_bd(zero_ir2_opnd, ir1_get_opnd_bd(next, 0), false);
    la_b(exit_label);
    la_label(target_label);

    /* set 1 */
    IR2_OPND temp1 = ra_alloc_itemp();
    la_ori(temp1, zero_ir2_opnd, 1);
    store_ireg_to_ir1_bd(temp1, ir1_get_opnd_bd(next, 0), false);

    la_label(exit_label);

    return true;
}

static
bool translate_cmp_xx_jcc(IR1_INST *pir1)
{
    TRANSLATION_DATA *td = lsenv->tr_data;

    if (ir1_opcode_bd(pir1) == WRAP(CMP)) {
        IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
        IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);

        td->ptn_itemp0 = ra_alloc_ptn_itemp();
        td->ptn_itemp1 = ra_alloc_ptn_itemp();
        load_ireg_from_ir1_2_bd(td->ptn_itemp0, opnd0, SIGN_EXTENSION, false);
        load_ireg_from_ir1_2_bd(td->ptn_itemp1, opnd1, SIGN_EXTENSION, false);
        int os = ir1_opnd_size_bd(opnd0); /* 8, 16, 32, 64 */
        switch (os)
        {
        case 8:
            la_x86sub_b(td->ptn_itemp0, td->ptn_itemp1);
            break;
        case 16:
            la_x86sub_h(td->ptn_itemp0, td->ptn_itemp1);
            break;
        case 32:
            la_x86sub_w(td->ptn_itemp0, td->ptn_itemp1);
            break;
        case 64:
            la_x86sub_d(td->ptn_itemp0, td->ptn_itemp1);
            break;
        default:
            break;
        }

    } else {
        if (!td->ptn_itemp_status) {
            switch (ir1_opcode_bd(pir1)) {
            case WRAP(JC):
            case WRAP(JNC):
            case WRAP(JZ):
            case WRAP(JNZ):
            case WRAP(JBE):
            case WRAP(JNBE):
            case WRAP(JL):
            case WRAP(JNL):
            case WRAP(JLE):
            case WRAP(JNLE):
                translate_jcc_bd(pir1);
                break;
            default:
                lsassert(0);
                break;
            }
            return true;
        }

        IR2_OPND target_label_opnd = ra_alloc_label();
#ifdef CONFIG_LATX_TU
        IR2_OPND tu_reset_label_opnd = ra_alloc_label();
        la_label(tu_reset_label_opnd);
#endif
        switch (ir1_opcode_bd(pir1)) {
        case WRAP(JC):
            la_bltu(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd);
            break;
        case WRAP(JNC):
            la_bgeu(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd);
            break;
        case WRAP(JZ):
            la_beq(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd);
            break;
        case WRAP(JNZ):
            la_bne(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd);
            break;
        case WRAP(JBE):
            la_bgeu(td->ptn_itemp1, td->ptn_itemp0, target_label_opnd);
            break;
        case WRAP(JNBE):
            la_bltu(td->ptn_itemp1, td->ptn_itemp0, target_label_opnd);
            break;
        case WRAP(JL):
            la_blt(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd);
            break;
        case WRAP(JNL):
            la_bge(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd);
            break;
        case WRAP(JLE):
            la_bge(td->ptn_itemp1, td->ptn_itemp0, target_label_opnd);
            break;
        case WRAP(JNLE):
            la_blt(td->ptn_itemp1, td->ptn_itemp0, target_label_opnd);
            break;
        default:
            lsassert(0);
            break;
        }
#ifdef CONFIG_LATX_TU
        if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
            TranslationBlock *tb = lsenv->tr_data->curr_tb;
            tu_jcc_nop_gen(tb);
            la_label(target_label_opnd);
            /*tb->jmp_target_arg[0] = target_label_opnd._label_id;*/
            tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
            if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
                IR2_OPND translated_label_opnd = ra_alloc_label();
                /* la_code_align(2, 0x03400000); */
                la_label(translated_label_opnd);
                la_b(ir2_opnd_new(IR2_OPND_IMM, 0));
                la_nop();
                tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
            }

            IR2_OPND unlink_label_opnd = ra_alloc_label();
            la_label(unlink_label_opnd);
            tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
            set_use_tu_jmp(tb);

            IR2_OPND target_label_opnd2 = ra_alloc_label();
            switch (ir1_opcode_bd(pir1)) {
            case WRAP(JC):
            la_bltu(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd2);
                break;
            case WRAP(JNC):
                la_bgeu(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd2);
                break;
            case WRAP(JZ):
                la_beq(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd2);
                break;
            case WRAP(JNZ):
                la_bne(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd2);
                break;
            case WRAP(JBE):
                la_bgeu(td->ptn_itemp1, td->ptn_itemp0, target_label_opnd2);
                break;
            case WRAP(JNBE):
                la_bltu(td->ptn_itemp1, td->ptn_itemp0, target_label_opnd2);
                break;
            case WRAP(JL):
                la_blt(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd2);
                break;
            case WRAP(JNL):
                la_bge(td->ptn_itemp0, td->ptn_itemp1, target_label_opnd2);
                break;
            case WRAP(JLE):
                la_bge(td->ptn_itemp1, td->ptn_itemp0, target_label_opnd2);
                break;
            case WRAP(JNLE):
                la_blt(td->ptn_itemp1, td->ptn_itemp0, target_label_opnd2);
                break;
            default:
                lsassert(0);
                break;
            }
            td->ptn_itemp_status = 0;
            /* not taken */
            /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 0); */
            tr_generate_exit_tb_bd(pir1, 0);

            la_label(target_label_opnd2);
            /* taken */
            /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, 1); */
            tr_generate_exit_tb_bd(pir1, 1);

            /*
            * the backup of the eflags instruction, which is used
            * to recover the eflags instruction when unlink a tb.
            */
            /* EFLAGS_CACULATE(src_opnd_0, src_opnd_1, curr, EFLAG_BACKUP); */
            return true;
        }
#endif
        td->ptn_itemp_status = 0;
        /* not taken */
        tr_generate_exit_tb_bd(pir1, 0);

        la_label(target_label_opnd);
        /* taken */
        tr_generate_exit_tb_bd(pir1, 1);
    }

    return true;
}

static
bool translate_test_xx_jcc(IR1_INST *pir1)
{
    TRANSLATION_DATA *td = lsenv->tr_data;

    if (ir1_opcode_bd(pir1) == WRAP(TEST)) {
        IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
        IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
        int os = ir1_opnd_size_bd(opnd0);

        td->ptn_itemp0 = ra_alloc_ptn_itemp();
        load_ireg_from_ir1_2_bd(td->ptn_itemp0, opnd0, SIGN_EXTENSION, false);

        bool is_same_reg = ir1_opnd_is_same_reg_bd(opnd0, opnd1);
        IR2_OPND src1;
        if (!is_same_reg) {
            src1 = ra_alloc_itemp();
            load_ireg_from_ir1_2_bd(src1, opnd1, SIGN_EXTENSION, false);
        }

        switch (os)
        {
        case 8:
            la_x86and_b(td->ptn_itemp0, is_same_reg ? td->ptn_itemp0 : src1);
            break;
        case 16:
            la_x86and_h(td->ptn_itemp0, is_same_reg ? td->ptn_itemp0 : src1);
            break;
        case 32:
            la_x86and_w(td->ptn_itemp0, is_same_reg ? td->ptn_itemp0 : src1);
            break;
        case 64:
            la_x86and_d(td->ptn_itemp0, is_same_reg ? td->ptn_itemp0 : src1);
            break;
        default:
            break;
        }
        if (!is_same_reg) {
            la_and(td->ptn_itemp0, td->ptn_itemp0, src1);
        }
    } else {
        if (!td->ptn_itemp_status) {
            switch (ir1_opcode_bd(pir1)) {
            case WRAP(JZ):
            case WRAP(JNZ):
            case WRAP(JS):
            case WRAP(JNS):
            case WRAP(JLE):
            case WRAP(JNLE):
            case WRAP(JNO):
            case WRAP(JO):
            case WRAP(JC):
            case WRAP(JBE):
            case WRAP(JNBE):
            case WRAP(JNC):
                translate_jcc_bd(pir1);
                break;
            default:        lsassert(0);        break;
            }
            return true;
        }

        IR2_OPND target_label_opnd = ra_alloc_label();
#ifdef CONFIG_LATX_TU
        bool is_branch = true;
        IR2_OPND tu_reset_label_opnd = ra_alloc_label();
        la_label(tu_reset_label_opnd);
#endif
        switch (ir1_opcode_bd(pir1)) {
        case WRAP(JZ):
            la_beqz(td->ptn_itemp0, target_label_opnd);
            break;
        case WRAP(JNZ):
            la_bnez(td->ptn_itemp0, target_label_opnd);
            break;
        case WRAP(JS):
            la_blt(td->ptn_itemp0, zero_ir2_opnd, target_label_opnd);
            break;
        case WRAP(JNS):
            la_bge(td->ptn_itemp0, zero_ir2_opnd, target_label_opnd);
            break;
        case WRAP(JLE):
            la_bge(zero_ir2_opnd, td->ptn_itemp0, target_label_opnd);
            break;
        case WRAP(JNLE):
            la_blt(zero_ir2_opnd, td->ptn_itemp0, target_label_opnd);
            break;
        case WRAP(JNO):
            /* OF = 0 For compatibility with bcc+b */
            la_b(target_label_opnd);
            break;
        case WRAP(JO):
            /* TODO tb->opt_bcc = false;*/
            /* OF = 1 */
#ifdef CONFIG_LATX_TU
        is_branch = false;
#endif
            break;
        case WRAP(JC):
            /* TODO tb->opt_bcc = false;*/
            /* CF = 1 */
#ifdef CONFIG_LATX_TU
        is_branch = false;
#endif
            break;
        case WRAP(JBE):
            /* CF = 1 or ZF = 1 */
            la_beqz(td->ptn_itemp0, target_label_opnd);
            break;
        case WRAP(JNBE):
            /* CF = 0 and ZF = 0 */
            la_bnez(td->ptn_itemp0, target_label_opnd);
            break;
        case WRAP(JNC):
            /* CF = 0 For compatibility with bcc+b */
            //latxs_append_ir2_opnd2(LISA_BEQZ, zero, &target_label);
            la_b(target_label_opnd);
            break;
        default:
            lsassert(0);
            break;
        }
#ifdef CONFIG_LATX_TU
        if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
            TranslationBlock *tb = lsenv->tr_data->curr_tb;
            tu_jcc_nop_gen(tb);
            la_label(target_label_opnd);
            if (is_branch) {
                /*tb->jmp_target_arg[0] = target_label_opnd._label_id;*/
                tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
            } else {
                /*tb->jmp_target_arg[0] = TB_JMP_RESET_OFFSET_INVALID;*/
                tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
            }
            if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
                IR2_OPND translated_label_opnd = ra_alloc_label();
                /* la_code_align(2, 0x03400000); */
                la_label(translated_label_opnd);
                la_b(ir2_opnd_new(IR2_OPND_IMM, 0));
                la_nop();
                tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
            }

            IR2_OPND unlink_label_opnd = ra_alloc_label();
            la_label(unlink_label_opnd);
            tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
            set_use_tu_jmp(tb);

            IR2_OPND target_label_opnd2 = ra_alloc_label();
            switch (ir1_opcode_bd(pir1)) {
            case WRAP(JZ):
                la_beqz(td->ptn_itemp0, target_label_opnd2);
                break;
            case WRAP(JNZ):
                la_bnez(td->ptn_itemp0, target_label_opnd2);
                break;
            case WRAP(JS):
                la_blt(td->ptn_itemp0, zero_ir2_opnd, target_label_opnd2);
                break;
            case WRAP(JNS):
                la_bge(td->ptn_itemp0, zero_ir2_opnd, target_label_opnd2);
                break;
            case WRAP(JLE):
                la_bge(zero_ir2_opnd, td->ptn_itemp0, target_label_opnd2);
                break;
            case WRAP(JNLE):
                la_blt(zero_ir2_opnd, td->ptn_itemp0, target_label_opnd2);
                break;
            case WRAP(JNO):
                /*
                * OF = 0
                * For compatibility with bcc+b.
                */
                la_b(target_label_opnd2);
                break;
            case WRAP(JO):
                /* OF = 1 */
    #ifdef CONFIG_LATX_TU
                is_branch = false;
    #endif
                break;
            case WRAP(JC):
                /* CF = 1 */
    #ifdef CONFIG_LATX_TU
                is_branch = false;
    #endif
                break;
            case WRAP(JBE):
                /* CF = 1 or ZF = 1 */
                la_beqz(td->ptn_itemp0, target_label_opnd2);
                break;
            case WRAP(JNBE):
                la_bnez(td->ptn_itemp0, target_label_opnd2);
                /* CF = 0 and ZF = 0 */
                break;
            case WRAP(JNC):
                /*
                * CF = 0
                * For compatibility with bcc+b.
                */
                la_b(target_label_opnd2);
                break;
            default:
                lsassert(0);
                break;
            }
            td->ptn_itemp_status = 0;
            /* not taken */
            /* EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, 0); */
            tr_generate_exit_tb_bd(pir1, 0);

            la_label(target_label_opnd2);
            /* taken */
            /* EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, 1); */
            tr_generate_exit_tb_bd(pir1, 1);

            /*
            * the backup of the eflags instruction, which is used
            * to recover the eflags instruction when unlink a tb.
            */
            /* EFLAGS_CACULATE(src_opnd_0, src_opnd_0, curr, EFLAG_BACKUP); */

            return true;
        }
#endif
        td->ptn_itemp_status = 0;

        /* not taken */
        tr_generate_exit_tb_bd(pir1, 0);

        la_label(target_label_opnd);

        /* taken */
        tr_generate_exit_tb_bd(pir1, 1);
    }

    return true;
}

static
bool translate_bt_xx_jcc(IR1_INST *pir1)
{
    IR1_INST *curr = pir1;
    TRANSLATION_DATA *td = lsenv->tr_data;

    if (ir1_opcode_bd(curr) == WRAP(BT)) {

        IR1_OPND_BD *bt_opnd0 = ir1_get_opnd_bd(curr, 0);
        IR1_OPND_BD *bt_opnd1 = ir1_get_opnd_bd(curr, 1);

        IR2_OPND src_opnd_0, src_opnd_1, bit_offset;
        int imm;

        src_opnd_1 = load_ireg_from_ir1_bd(bt_opnd1, ZERO_EXTENSION, false);

        bit_offset = ra_alloc_itemp();
        la_bstrpick_d(bit_offset, src_opnd_1,
            __builtin_ctz(ir1_opnd_size_bd(bt_opnd0)) - 1, 0);
        ra_free_temp_auto(src_opnd_1);
        if (ir1_opnd_is_gpr_bd(bt_opnd0)) {
            /* r16/r32/r64 */
            src_opnd_0 = convert_gpr_opnd_bd(bt_opnd0, UNKNOWN_EXTENSION);
        } else {
            src_opnd_0 = ra_alloc_itemp();
            IR2_OPND tmp_mem_op = convert_mem_bd(bt_opnd0, &imm);
            IR2_OPND mem_opnd = ra_alloc_itemp();
            la_or(mem_opnd, tmp_mem_op, zero_ir2_opnd);
    #ifdef CONFIG_LATX_IMM_REG
            imm_cache_free_temp_helper(tmp_mem_op);
    #else
            ra_free_temp_auto(tmp_mem_op);
    #endif

            if (ir1_opnd_is_gpr_bd(bt_opnd1)) {
                IR2_OPND tmp = ra_alloc_itemp();
                IR2_OPND src1 = convert_gpr_opnd_bd(bt_opnd1, UNKNOWN_EXTENSION);
                int opnd_size = ir1_opnd_size_bd(bt_opnd0);
                int ctz_opnd_size = __builtin_ctz(opnd_size);
                int ctz_align_size = __builtin_ctz(opnd_size / 8);
                lsassertm((opnd_size == 16) || (opnd_size == 32) ||
                    (opnd_size == 64), "%s opnd_size error!", __func__);
                la_srai_d(tmp, src1, ctz_opnd_size);
                la_alsl_d(mem_opnd, tmp, mem_opnd, ctz_align_size - 1);
                ra_free_temp(tmp);
            }

            if (ir1_opnd_size_bd(bt_opnd0) == 64) {
                /* m64 */
                la_ld_d(src_opnd_0, mem_opnd, imm);
            } else {
                /* m16/m32 */
                la_ld_w(src_opnd_0, mem_opnd, imm);
            }
            ra_free_temp(mem_opnd);
        }

        td->ptn_itemp0 = ra_alloc_ptn_itemp();
        la_srl_d(td->ptn_itemp0, src_opnd_0, bit_offset);
        ra_free_temp(bit_offset);
        ra_free_temp_auto(src_opnd_0);
        la_andi(td->ptn_itemp0, td->ptn_itemp0, 1);

        /* update EFALGS */
        la_x86mtflag(td->ptn_itemp0, 0x1);
    } else {
        if (!td->ptn_itemp_status) {
            switch (ir1_opcode_bd(curr)) {
            case WRAP(JC):
            case WRAP(JNC):
                translate_jcc_bd(curr);
                break;
            default:
                lsassert(0);
                break;
            }
            return true;
        }

        IR2_OPND target_label_opnd = ra_alloc_label();
#ifdef CONFIG_LATX_TU
        IR2_OPND tu_reset_label_opnd = ra_alloc_label();
        la_label(tu_reset_label_opnd);
#endif

        switch (ir1_opcode_bd(curr)) {
        case WRAP(JC):   /*CF=1*/
            la_bne(td->ptn_itemp0, zero_ir2_opnd, target_label_opnd);
            break;
        case WRAP(JNC):  /*CF=0*/
            la_beq(td->ptn_itemp0, zero_ir2_opnd, target_label_opnd);
            break;
        default:
            lsassert(0);
            break;
        }
#ifdef CONFIG_LATX_TU
        if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
            TranslationBlock *tb = lsenv->tr_data->curr_tb;
            tu_jcc_nop_gen(tb);
            la_label(target_label_opnd);
            /*tb->jmp_target_arg[0] = target_label_opnd._label_id;*/
            tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
            if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
                IR2_OPND translated_label_opnd = ra_alloc_label();
                /* la_code_align(2, 0x03400000); */
                la_label(translated_label_opnd);
                la_b(ir2_opnd_new(IR2_OPND_IMM, 0));
                la_nop();
                tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
            }

            IR2_OPND unlink_label_opnd = ra_alloc_label();
            la_label(unlink_label_opnd);
            tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
            set_use_tu_jmp(tb);

            IR2_OPND target_label_opnd2 = ra_alloc_label();
            switch (ir1_opcode_bd(curr)) {
                case WRAP(JC):   /*CF=1*/
                    la_bne(td->ptn_itemp0, zero_ir2_opnd, target_label_opnd2);
                    break;
                case WRAP(JNC):  /*CF=0*/
                    la_beq(td->ptn_itemp0, zero_ir2_opnd, target_label_opnd2);
                    break;
                default:
                    lsassert(0);
                    break;
            }
            td->ptn_itemp_status = 0;

            tr_generate_exit_tb_bd(curr, 0);

            la_label(target_label_opnd2);

            tr_generate_exit_tb_bd(curr, 1);

            return true;
        }
#endif

        td->ptn_itemp_status = 0;

        tr_generate_exit_tb_bd(curr, 0);

        la_label(target_label_opnd);
        /* taken */
        tr_generate_exit_tb_bd(curr, 1);
    }

    return true;
}

static inline bool xcomisx_xx_jcc(IR1_INST *pir1, bool is_jcc, bool is_double, bool qnan_exp)
{
    IR1_INST *curr = pir1;
    IR1_INST *next = curr->instptn.next;

    if (!is_jcc) {
        IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(curr, 0);
        IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(curr, 1);
        IR2_OPND dest = load_freg128_from_ir1_bd(opnd0);
        IR2_OPND src = load_freg128_from_ir1_bd(opnd1);
        
        translate_xcomisx_bd(curr);
        
        IR2_INST* (*la_fcmp)(IR2_OPND, IR2_OPND, IR2_OPND, int);
        if (is_double) {
            la_fcmp = la_fcmp_cond_d;
        } else {
            la_fcmp = la_fcmp_cond_s;
        }

        switch (ir1_opcode_bd(next)) {
        case WRAP(JNBE):
            la_fcmp(fcc0_ir2_opnd, src, dest, FCMP_COND_CLT + qnan_exp);
            break;
        case WRAP(JNC):
            la_fcmp(fcc0_ir2_opnd, src, dest, FCMP_COND_CLE + qnan_exp);
            break;
        case WRAP(JC):
            la_fcmp(fcc0_ir2_opnd, dest, src, FCMP_COND_CULT + qnan_exp);
        /* below or NAN, x86 special define */
            break;
        case WRAP(JBE):
        /* below or equal or NAN, x86 special define */
            la_fcmp(fcc0_ir2_opnd, dest, src, FCMP_COND_CULE + qnan_exp);
            break;
        case WRAP(JZ):
        case WRAP(JLE):
            la_fcmp(fcc0_ir2_opnd, dest, src, FCMP_COND_CUEQ + qnan_exp);
        /* equal or NAN, x86 special define */
            break;
        case WRAP(JNZ):
        case WRAP(JNLE):
            la_fcmp(fcc0_ir2_opnd, dest, src, FCMP_COND_CNE + qnan_exp);
            break;
        case WRAP(JL):
        case WRAP(JNL):
            break;
        default:
            lsassert(0);
            break;
        }
    } else {

        IR2_OPND target_label_opnd = ra_alloc_label();
#ifdef CONFIG_LATX_TU
        IR2_OPND tu_reset_label_opnd = ra_alloc_label();
        la_label(tu_reset_label_opnd);
#endif
        if (ir1_opcode_bd(curr) == WRAP(JNL)) {
            la_b(target_label_opnd);
        } else if (ir1_opcode_bd(curr) == WRAP(JL)) {
        } else {
            la_bcnez(fcc0_ir2_opnd, target_label_opnd);
        }
#ifdef CONFIG_LATX_TU
    if (judge_tu_eflag_gen(lsenv->tr_data->curr_tb)) {
        TranslationBlock *tb = lsenv->tr_data->curr_tb;
        tu_jcc_nop_gen(tb);
        la_label(target_label_opnd);
        /*tb->jmp_target_arg[0] = target_label_opnd._label_id;*/
        tb->tu_jmp[TU_TB_INDEX_TARGET] = tu_reset_label_opnd._label_id;
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            IR2_OPND translated_label_opnd = ra_alloc_label();
            /* la_code_align(2, 0x03400000); */
            la_label(translated_label_opnd);
            la_b(ir2_opnd_new(IR2_OPND_IMM, 0));
            la_nop();
            tb->tu_jmp[TU_TB_INDEX_NEXT] = translated_label_opnd._label_id;
        }

        IR2_OPND unlink_label_opnd = ra_alloc_label();
        la_label(unlink_label_opnd);
        tb->tu_unlink.stub_offset = unlink_label_opnd._label_id;
        set_use_tu_jmp(tb);

        IR2_OPND target_label_opnd2 = ra_alloc_label();
        if (ir1_opcode_bd(curr) == WRAP(JNL)) {
            la_b(target_label_opnd2);
        } else if (ir1_opcode_bd(curr) == WRAP(JL)) {
        } else {
            la_bcnez(fcc0_ir2_opnd, target_label_opnd2);
        }

        /* not taken */
        tr_generate_exit_tb_bd(curr, 0);

        la_label(target_label_opnd2);
        /* taken */
        tr_generate_exit_tb_bd(curr, 1);

        return true;
    }
#endif
        /* not taken */
        tr_generate_exit_tb_bd(curr, 0);

        la_label(target_label_opnd);

        /* taken */
        tr_generate_exit_tb_bd(curr, 1);

    }
    return true;
}

static bool translate_comisd_xx_jcc(IR1_INST *pir1)
{
    if (ir1_opcode_bd(pir1) == WRAP(COMISD)) {
        return xcomisx_xx_jcc(pir1, false, true, true);
    } else {
        lsassert(ir1_opcode_bd(pir1) == WRAP(JNBE) ||
            ir1_opcode_bd(pir1) == WRAP(JNC) ||
            ir1_opcode_bd(pir1) == WRAP(JC) ||
            ir1_opcode_bd(pir1) == WRAP(JBE) ||
            ir1_opcode_bd(pir1) == WRAP(JNZ) ||
            ir1_opcode_bd(pir1) == WRAP(JZ) ||
            ir1_opcode_bd(pir1) == WRAP(JL) ||
            ir1_opcode_bd(pir1) == WRAP(JNL) ||
            ir1_opcode_bd(pir1) == WRAP(JLE) ||
            ir1_opcode_bd(pir1) == WRAP(JNLE));
        return xcomisx_xx_jcc(pir1, true, true, true);
    }
}

static bool translate_comiss_xx_jcc(IR1_INST *pir1)
{
    if (ir1_opcode_bd(pir1) == WRAP(COMISS)) {
        return xcomisx_xx_jcc(pir1, false, false, true);
    } else {
        lsassert(ir1_opcode_bd(pir1) == WRAP(JNBE) ||
            ir1_opcode_bd(pir1) == WRAP(JNC) ||
            ir1_opcode_bd(pir1) == WRAP(JC) ||
            ir1_opcode_bd(pir1) == WRAP(JBE) ||
            ir1_opcode_bd(pir1) == WRAP(JNZ) ||
            ir1_opcode_bd(pir1) == WRAP(JZ) ||
            ir1_opcode_bd(pir1) == WRAP(JL) ||
            ir1_opcode_bd(pir1) == WRAP(JNL) ||
            ir1_opcode_bd(pir1) == WRAP(JLE) ||
            ir1_opcode_bd(pir1) == WRAP(JNLE));
        return xcomisx_xx_jcc(pir1, true, false, true);
    }
}

static bool translate_ucomisd_xx_jcc(IR1_INST *pir1)
{
    if (ir1_opcode_bd(pir1) == WRAP(UCOMISD)) {
        return xcomisx_xx_jcc(pir1, false, true, false);
    } else {
        lsassert(ir1_opcode_bd(pir1) == WRAP(JNBE) ||
            ir1_opcode_bd(pir1) == WRAP(JNC) ||
            ir1_opcode_bd(pir1) == WRAP(JC) ||
            ir1_opcode_bd(pir1) == WRAP(JBE) ||
            ir1_opcode_bd(pir1) == WRAP(JNZ) ||
            ir1_opcode_bd(pir1) == WRAP(JZ) ||
            ir1_opcode_bd(pir1) == WRAP(JL) ||
            ir1_opcode_bd(pir1) == WRAP(JNL) ||
            ir1_opcode_bd(pir1) == WRAP(JLE) ||
            ir1_opcode_bd(pir1) == WRAP(JNLE));
        return xcomisx_xx_jcc(pir1, true, true, false);
    }
}

static bool translate_ucomiss_xx_jcc(IR1_INST *pir1)
{
    if (ir1_opcode_bd(pir1) == WRAP(UCOMISS)) {
        return xcomisx_xx_jcc(pir1, false, false, false);
    } else {
        lsassert(ir1_opcode_bd(pir1) == WRAP(JNBE) ||
            ir1_opcode_bd(pir1) == WRAP(JNC) ||
            ir1_opcode_bd(pir1) == WRAP(JC) ||
            ir1_opcode_bd(pir1) == WRAP(JBE) ||
            ir1_opcode_bd(pir1) == WRAP(JNZ) ||
            ir1_opcode_bd(pir1) == WRAP(JZ) ||
            ir1_opcode_bd(pir1) == WRAP(JL) ||
            ir1_opcode_bd(pir1) == WRAP(JNL) ||
            ir1_opcode_bd(pir1) == WRAP(JLE) ||
            ir1_opcode_bd(pir1) == WRAP(JNLE));
        return xcomisx_xx_jcc(pir1, true, false, false);
    }
}
#undef WRAP

bool try_translate_instptn_bd(IR1_INST *pir1)
{
    instptn_check_false();

    switch (pir1->instptn.opc) {
    case INSTPTN_OPC_NONE:
        return false;
    case INSTPTN_OPC_NOP:
    case INSTPTN_OPC_NOP_FOR_EXP:
        return true;
    case INSTPTN_OPC_CMP_JCC:
        return translate_cmp_jcc(pir1);
    case INSTPTN_OPC_TEST_JCC:
        return translate_test_jcc(pir1);
    case INSTPTN_OPC_CQO_IDIV:
        return translate_cqo_idiv(pir1);
    case INSTPTN_OPC_BT_JCC:
        return translate_bt_jcc(pir1);
    case INSTPTN_OPC_COMISD_JCC:
        return translate_comisd_jcc(pir1);
    case INSTPTN_OPC_COMISS_JCC:
        return translate_comiss_jcc(pir1);
    case INSTPTN_OPC_UCOMISD_JCC:
        return translate_ucomisd_jcc(pir1);
    case INSTPTN_OPC_UCOMISS_JCC:
        return translate_ucomiss_jcc(pir1);
    case INSTPTN_OPC_XOR_DIV:
        return translate_xor_div(pir1);
    case INSTPTN_OPC_CDQ_IDIV:
        return translate_cdq_idiv(pir1);
    case INSTPTN_OPC_CMP_SBB:
        return translate_cmp_sbb(pir1);

    case INSTPTN_OPC_CMP_XXCC_CON:
        return translate_cmp_xxcc_con(pir1);
    case INSTPTN_OPC_TEST_XXCC_CON:
        return translate_test_xxcc_con(pir1);

    case INSTPTN_OPC_UCOMISD_SETA:
        return translate_ucomisd_seta(pir1);
    case INSTPTN_OPC_SUB_JCC:
        return translate_sub_jcc(pir1);

    case INSTPTN_OPC_CMP_XX_JCC:
        return translate_cmp_xx_jcc(pir1);
    case INSTPTN_OPC_TEST_XX_JCC:
        return translate_test_xx_jcc(pir1);
    case INSTPTN_OPC_BT_XX_JCC:
        return translate_bt_xx_jcc(pir1);
    case INSTPTN_OPC_COMISD_XX_JCC:
        return translate_comisd_xx_jcc(pir1);
    case INSTPTN_OPC_COMISS_XX_JCC:
        return translate_comiss_xx_jcc(pir1);
    case INSTPTN_OPC_UCOMISD_XX_JCC:
        return translate_ucomisd_xx_jcc(pir1);
    case INSTPTN_OPC_UCOMISS_XX_JCC:
        return translate_ucomiss_xx_jcc(pir1);
    default:
        lsassert(0);
        break;
    }

    return false;
}

#endif
