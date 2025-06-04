#include "lsenv.h"
#include "reg-alloc.h"
#include "flag-lbt-bd.h"
#include <string.h>
#include "latx-options.h"
#include "fpu/softfloat.h"
#include "profile.h"
#include "translate-bd.h"
#include "runtime-trace.h"
#include "exec/translate-all.h"
#include "ir2-relocate.h"
#include "tu.h"
#include "imm-cache.h"


#if defined CONFIG_LATX_FLAG_REDUCTION && \
    defined(CONFIG_LATX_FLAG_REDUCTION_EXTEND)
int8 get_etb_type_bd(IR1_INST *pir1)
{
    if (ir1_is_branch_bd(pir1))
        return (int8)TB_TYPE_BRANCH;
    else if (ir1_is_call_bd(pir1) && !ir1_is_indirect_call_bd(pir1))
        return (int8)TB_TYPE_CALL;
    else if (ir1_is_jump_bd(pir1) && !ir1_is_indirect_jmp_bd(pir1))
        return (int8)TB_TYPE_JUMP;
    else if (ir1_is_return_bd(pir1))
        return (int8)TB_TYPE_RETURN;
    else if (ir1_opcode_bd(pir1) == ND_INS_CALL &&
        ir1_is_indirect_call_bd(pir1)) {
        return (int8)TB_TYPE_CALLIN;
    } else if (ir1_opcode_bd(pir1) == ND_INS_JMP &&
        ir1_is_indirect_jmp_bd(pir1)) {
        return (int8)TB_TYPE_JUMPIN;
    } else {
        return (int8)TB_TYPE_NONE;
    }

}
#endif


bool ir1_need_calculate_of_bd(IR1_INST *ir1)
{
    return ir1_is_of_def_bd(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << OF_USEDEF_BIT_INDEX);
}

bool ir1_need_calculate_cf_bd(IR1_INST *ir1)
{
    return (ir1_is_cf_def_bd(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << CF_USEDEF_BIT_INDEX) &&
                          ir1_opcode_bd(ir1) != ND_INS_DAA &&
                          ir1_opcode_bd(ir1) != ND_INS_DAS) ||
                          ir1_opcode_bd(ir1) == ND_INS_LZCNT;
}

bool ir1_need_calculate_pf_bd(IR1_INST *ir1)
{
    return ir1_is_pf_def_bd(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << PF_USEDEF_BIT_INDEX);
}

bool ir1_need_calculate_af_bd(IR1_INST *ir1)
{
    return ir1_is_af_def_bd(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << AF_USEDEF_BIT_INDEX) &&
                          ir1_opcode_bd(ir1) != ND_INS_DAA &&
                          ir1_opcode_bd(ir1) != ND_INS_DAS;
}

bool ir1_need_calculate_zf_bd(IR1_INST *ir1)
{
    return ir1_is_zf_def_bd(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << ZF_USEDEF_BIT_INDEX);
}

bool ir1_need_calculate_sf_bd(IR1_INST *ir1)
{
    return ir1_is_sf_def_bd(ir1) &&
           BITS_ARE_CLEAR(lsenv->tr_data->curr_ir1_skipped_eflags,
                          1 << SF_USEDEF_BIT_INDEX);
}

bool ir1_need_calculate_any_flag_bd(IR1_INST *ir1)
{
    return (ir1_get_eflag_def_bd(ir1) &
            ~(lsenv->tr_data->curr_ir1_skipped_eflags)) != 0;
}

bool ir1_need_reserve_h128_bd(IR1_INST *ir1)
{
    if (ir1_get_opnd_num_bd(ir1) == 0)
        return false;
    IR1_OPND_BD *dest = ir1_get_opnd_bd(ir1, 0);
    if (ir1_opnd_is_xmm_bd(dest) && ((INSTRUX *)(ir1->info))->OpCodeBytes[0] != 0xc4
        && ((INSTRUX *)(ir1->info))->OpCodeBytes[0] != 0xc5) {
        return true;
    }
    return false;
}

IR2_OPND save_h128_of_ymm_bd(IR1_INST *ir1)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(ir1, 0)));

    IR1_OPND_BD *dest = ir1_get_opnd_bd(ir1, 0);
    IR2_OPND temp = ra_alloc_ftemp();

    la_xvori_b(temp, ra_alloc_xmm(ir1_opnd_base_reg_num_bd(dest)), 0);
    return temp;
}

void restore_h128_of_ymm_bd(IR1_INST *ir1, IR2_OPND temp)
{
    lsassert(ir1_opnd_is_xmm_bd(ir1_get_opnd_bd(ir1, 0)));
    lsassert(temp._type != IR2_OPND_NONE);

    IR2_OPND opnd2 = ra_alloc_xmm(ir1_opnd_base_reg_num_bd(ir1_get_opnd_bd(ir1, 0)));

    la_xvpermi_q(opnd2, temp, 0x12);
}

static bool (*translate_functions[])(IR1_INST *) = {
    TRANS_FUNC_GEN_REAL_BD(INVALID, NULL),

    TRANS_FUNC_GEN_BD(AAA, aaa),
    TRANS_FUNC_GEN_BD(AAD, aad),
    TRANS_FUNC_GEN_BD(AAM, aam),
    TRANS_FUNC_GEN_BD(AAS, aas),
    TRANS_FUNC_GEN_BD(ADC, adc),
    TRANS_FUNC_GEN_BD(ADD, add),
//     TRANS_FUNC_GEN_BD(ADDPD, addpd),
//     TRANS_FUNC_GEN_BD(ADDPS, addps),
//     TRANS_FUNC_GEN_BD(ADDSD, addsd),
//     TRANS_FUNC_GEN_BD(ADDSS, addss),
//     TRANS_FUNC_GEN_BD(ADDSUBPD, addsubpd),
//     TRANS_FUNC_GEN_BD(ADDSUBPS, addsubps),
    // TRANS_FUNC_GEN_BD(AND, and),
//     TRANS_FUNC_GEN_BD(ANDNPD, andnpd),
//     TRANS_FUNC_GEN_BD(ANDNPS, andnps),
//     TRANS_FUNC_GEN_BD(ANDPD, andpd),
//     TRANS_FUNC_GEN_BD(ANDPS, andps),
//     TRANS_FUNC_GEN_BD(BSF, bsf),
//     TRANS_FUNC_GEN_BD(BSR, bsr),
//     TRANS_FUNC_GEN_BD(BSWAP, bswap),
    TRANS_FUNC_GEN_BD(BT, btx),
    TRANS_FUNC_GEN_BD(BTC, btx),
    TRANS_FUNC_GEN_BD(BTR, btx),
    TRANS_FUNC_GEN_BD(BTS, btx),
    TRANS_FUNC_GEN_BD(BLSR, blsr),
    TRANS_FUNC_GEN_BD(CALLNR, call),
    TRANS_FUNC_GEN_BD(CALLNI, callin),
    TRANS_FUNC_GEN_BD(CBW, cbw),
    TRANS_FUNC_GEN_BD(CDQ, cdq),
    TRANS_FUNC_GEN_BD(CDQE, cdqe),
    TRANS_FUNC_GEN_BD(CLC, clc),
    TRANS_FUNC_GEN_BD(CLD, cld),
    TRANS_FUNC_GEN_BD(CMC, cmc),
//     TRANS_FUNC_GEN_BD(CMOVA, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVAE, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVB, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVBE, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVE, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVG, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVGE, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVL, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVLE, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVNE, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVNO, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVNP, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVNS, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVO, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVP, cmovcc),
//     TRANS_FUNC_GEN_BD(CMOVS, cmovcc),
    TRANS_FUNC_GEN_BD(CMP, cmp),
//     TRANS_FUNC_GEN_BD(CMPSB, cmps),
//     TRANS_FUNC_GEN_BD(CMPSW, cmps),
//     TRANS_FUNC_GEN_BD(CMPSQ, cmps),
//     TRANS_FUNC_GEN_BD(CMPXCHG, cmpxchg),
//     TRANS_FUNC_GEN_BD(CMPXCHG8B, cmpxchg8b),
//     TRANS_FUNC_GEN_BD(CMPXCHG16B, cmpxchg16b),
// #ifdef CONFIG_LATX_XCOMISX_OPT
//     TRANS_FUNC_GEN_BD(COMISD, xcomisx),
//     TRANS_FUNC_GEN_BD(COMISS, xcomisx),
// #else
//     TRANS_FUNC_GEN_BD(COMISD, comisd),
//     TRANS_FUNC_GEN_BD(COMISS, comiss),
// #endif
    TRANS_FUNC_GEN_BD(CPUID, cpuid),
    TRANS_FUNC_GEN_BD(CQO, cqo),
//     TRANS_FUNC_GEN_BD(CVTDQ2PD, cvtdq2pd),
//     TRANS_FUNC_GEN_BD(CVTDQ2PS, cvtdq2ps),
//     TRANS_FUNC_GEN_BD(CVTPD2DQ, cvtpd2dq),
//     TRANS_FUNC_GEN_BD(CVTPD2PS, cvtpd2ps),
//     TRANS_FUNC_GEN_BD(CVTPS2DQ, cvtps2dq),
//     TRANS_FUNC_GEN_BD(CVTPS2PD, cvtps2pd),
//     TRANS_FUNC_GEN_BD(CVTSD2SI, cvtsx2si),
//     TRANS_FUNC_GEN_BD(CVTSD2SS, cvtsd2ss),
//     TRANS_FUNC_GEN_BD(CVTSI2SD, cvtsi2sd),
//     TRANS_FUNC_GEN_BD(CVTSI2SS, cvtsi2ss),
//     TRANS_FUNC_GEN_BD(CVTSS2SD, cvtss2sd),
//     TRANS_FUNC_GEN_BD(CVTSS2SI, cvtsx2si),
//     TRANS_FUNC_GEN_BD(CVTTPD2DQ, cvttpx2dq),
//     TRANS_FUNC_GEN_BD(CVTTPS2DQ, cvttpx2dq),
//     TRANS_FUNC_GEN_BD(CVTTSD2SI, cvttsx2si),
//     TRANS_FUNC_GEN_BD(CVTTSS2SI, cvttsx2si),
//     TRANS_FUNC_GEN_BD(CVTTPD2PI, cvttpd2pi),
//     TRANS_FUNC_GEN_BD(CVTTPS2PI, cvttps2pi),
    TRANS_FUNC_GEN_BD(CWD, cwd),
    TRANS_FUNC_GEN_BD(CWDE, cwde),
    TRANS_FUNC_GEN_BD(DAA, daa),
    TRANS_FUNC_GEN_BD(DAS, das),
    TRANS_FUNC_GEN_BD(DEC, dec),
    TRANS_FUNC_GEN_BD(DIV, div),
//     TRANS_FUNC_GEN_BD(DIVPD, divpd),
//     TRANS_FUNC_GEN_BD(DIVPS, divps),
//     TRANS_FUNC_GEN_BD(DIVSD, divsd),
//     TRANS_FUNC_GEN_BD(DIVSS, divss),
    TRANS_FUNC_GEN_BD(RETN, ret),
//     TRANS_FUNC_GEN_BD(MOVAPD, movapd),
//     TRANS_FUNC_GEN_BD(MOVAPS, movaps),
//     TRANS_FUNC_GEN_BD(ORPD, orpd),
//     TRANS_FUNC_GEN_BD(ORPS, orps),
//     TRANS_FUNC_GEN_BD(XORPD, xorpd),
//     TRANS_FUNC_GEN_BD(XORPS, xorps),
    TRANS_FUNC_GEN_BD(HLT, hlt),
    TRANS_FUNC_GEN_BD(IDIV, idiv),
    TRANS_FUNC_GEN_BD(IMUL, imul),
    TRANS_FUNC_GEN_BD(IN, in),
    TRANS_FUNC_GEN_BD(INC, inc),
    TRANS_FUNC_GEN_BD(INS, ins),
    TRANS_FUNC_GEN_BD(INT, int),
    TRANS_FUNC_GEN_BD(INT3, int_3),
    TRANS_FUNC_GEN_BD(IRET, iret),
//     TRANS_FUNC_GEN_BD(IRETD, iret),
//     TRANS_FUNC_GEN_BD(IRETQ, iretq),
// #ifdef CONFIG_LATX_XCOMISX_OPT
//     TRANS_FUNC_GEN_BD(UCOMISD, xcomisx),
//     TRANS_FUNC_GEN_BD(UCOMISS, xcomisx),
// #else
//     TRANS_FUNC_GEN_BD(UCOMISD, ucomisd),
//     TRANS_FUNC_GEN_BD(UCOMISS, ucomiss),
// #endif
//     TRANS_FUNC_GEN_BD(JCXZ, jcxz),
//     TRANS_FUNC_GEN_BD(JECXZ, jecxz),
//     TRANS_FUNC_GEN_BD(JRCXZ, jrcxz),
//     TRANS_FUNC_GEN_BD(JMP, jmp),
    TRANS_FUNC_GEN_BD(JMPNI,    jmpin),
//     TRANS_FUNC_GEN_BD(JAE, jcc),
//     TRANS_FUNC_GEN_BD(JA, jcc),
//     TRANS_FUNC_GEN_BD(JBE, jcc),
//     TRANS_FUNC_GEN_BD(JB, jcc),
//     TRANS_FUNC_GEN_BD(JE, jcc),
//     TRANS_FUNC_GEN_BD(JGE, jcc),
//     TRANS_FUNC_GEN_BD(JG, jcc),
//     TRANS_FUNC_GEN_BD(JLE, jcc),
//     TRANS_FUNC_GEN_BD(JL, jcc),
//     TRANS_FUNC_GEN_BD(JNE, jcc),
//     TRANS_FUNC_GEN_BD(JNO, jcc),
//     TRANS_FUNC_GEN_BD(JNP, jcc),
//     TRANS_FUNC_GEN_BD(JNS, jcc),
//     TRANS_FUNC_GEN_BD(JO, jcc),
//     TRANS_FUNC_GEN_BD(JP, jcc),
//     TRANS_FUNC_GEN_BD(JS, jcc),
    TRANS_FUNC_GEN_BD(LAHF, lahf),
//     TRANS_FUNC_GEN_BD(LDDQU, lddqu),
//     TRANS_FUNC_GEN_BD(LDMXCSR, ldmxcsr),
//     TRANS_FUNC_GEN_BD(LEA, lea),
    TRANS_FUNC_GEN_BD(LEAVE, leave),
    TRANS_FUNC_GEN_BD(LFENCE, lfence),
//     TRANS_FUNC_GEN_BD(OR, or),
    TRANS_FUNC_GEN_BD(SUB, sub),
//     TRANS_FUNC_GEN_BD(XOR, xor),
//     TRANS_FUNC_GEN_BD(LODSB, lods),
//     TRANS_FUNC_GEN_BD(LODSD, lods),
//     TRANS_FUNC_GEN_BD(LODSW, lods),
//     TRANS_FUNC_GEN_BD(LODSQ, lods),
    TRANS_FUNC_GEN_BD(LOOP, loop),
    TRANS_FUNC_GEN_BD(LOOPZ, loopz),
    TRANS_FUNC_GEN_BD(LOOPNZ, loopnz),
    TRANS_FUNC_GEN_BD(XADD, xadd),
//     TRANS_FUNC_GEN_BD(MASKMOVDQU, maskmovdqu),
//     TRANS_FUNC_GEN_BD(MAXPD, maxpd),
//     TRANS_FUNC_GEN_BD(MAXPS, maxps),
//     TRANS_FUNC_GEN_BD(MAXSD, maxsd),
//     TRANS_FUNC_GEN_BD(MAXSS, maxss),
    TRANS_FUNC_GEN_BD(MFENCE, mfence),
//     TRANS_FUNC_GEN_BD(MINPD, minpd),
//     TRANS_FUNC_GEN_BD(MINPS, minps),
//     TRANS_FUNC_GEN_BD(MINSD, minsd),
//     TRANS_FUNC_GEN_BD(MINSS, minss),
//     TRANS_FUNC_GEN_BD(CVTPD2PI, cvtpd2pi),
//     TRANS_FUNC_GEN_BD(CVTPI2PD, cvtpi2pd),
//     TRANS_FUNC_GEN_BD(CVTPI2PS, cvtpi2ps),
//     TRANS_FUNC_GEN_BD(CVTPS2PI, cvtps2pi),
    TRANS_FUNC_GEN_BD(EMMS, emms),
//     TRANS_FUNC_GEN_BD(MASKMOVQ, maskmovq),
//     TRANS_FUNC_GEN_BD(MOVD, movd),
//     TRANS_FUNC_GEN_BD(MOVDQ2Q, movdq2q),
//     TRANS_FUNC_GEN_BD(MOVNTQ, movntq),
//     TRANS_FUNC_GEN_BD(MOVQ2DQ, movq2dq),
//     TRANS_FUNC_GEN_BD(MOVQ, movq),
//     TRANS_FUNC_GEN_BD(PACKSSDW, packssdw),
//     TRANS_FUNC_GEN_BD(PACKSSWB, packsswb),
//     TRANS_FUNC_GEN_BD(PACKUSWB, packuswb),
//     TRANS_FUNC_GEN_BD(PADDB, paddb),
//     TRANS_FUNC_GEN_BD(PADDD, paddd),
//     TRANS_FUNC_GEN_BD(PADDQ, paddq),
//     TRANS_FUNC_GEN_BD(PADDSB, paddsb),
//     TRANS_FUNC_GEN_BD(PADDSW, paddsw),
//     TRANS_FUNC_GEN_BD(PADDUSB, paddusb),
//     TRANS_FUNC_GEN_BD(PADDUSW, paddusw),
//     TRANS_FUNC_GEN_BD(PADDW, paddw),
//     TRANS_FUNC_GEN_BD(PANDN, pandn),
//     TRANS_FUNC_GEN_BD(PAND, pand),
//     TRANS_FUNC_GEN_BD(PAVGB, pavgb),
//     TRANS_FUNC_GEN_BD(PAVGW, pavgw),
//     TRANS_FUNC_GEN_BD(PCMPEQB, pcmpeqb),
//     TRANS_FUNC_GEN_BD(PCMPEQD, pcmpeqd),
//     TRANS_FUNC_GEN_BD(PCMPEQW, pcmpeqw),
//     TRANS_FUNC_GEN_BD(PCMPGTB, pcmpgtb),
//     TRANS_FUNC_GEN_BD(PCMPGTW, pcmpgtw),
//     TRANS_FUNC_GEN_BD(PCMPGTD, pcmpgtd),
//     TRANS_FUNC_GEN_BD(PCMPGTQ, pcmpgtq),
//     TRANS_FUNC_GEN_BD(PEXTRW, pextrw),
//     TRANS_FUNC_GEN_BD(PINSRW, pinsrw),
//     TRANS_FUNC_GEN_BD(PMADDWD, pmaddwd),
//     TRANS_FUNC_GEN_BD(PMAXSW, pmaxsw),
//     TRANS_FUNC_GEN_BD(PMAXUB, pmaxub),
//     TRANS_FUNC_GEN_BD(PMINSW, pminsw),
//     TRANS_FUNC_GEN_BD(PMINUB, pminub),
//     TRANS_FUNC_GEN_BD(PMOVMSKB, pmovmskb),
//     TRANS_FUNC_GEN_BD(PMULHUW, pmulhuw),
//     TRANS_FUNC_GEN_BD(PMULHW, pmulhw),
//     TRANS_FUNC_GEN_BD(PMULLW, pmullw),
//     TRANS_FUNC_GEN_BD(PMULUDQ, pmuludq),
//     TRANS_FUNC_GEN_BD(POR, por),
//     TRANS_FUNC_GEN_BD(PSADBW, psadbw),
//     TRANS_FUNC_GEN_BD(PSHUFW, pshufw),
//     TRANS_FUNC_GEN_BD(PSLLD, pslld),
//     TRANS_FUNC_GEN_BD(PSLLQ, psllq),
//     TRANS_FUNC_GEN_BD(PSLLW, psllw),
//     TRANS_FUNC_GEN_BD(PSRAD, psrad),
//     TRANS_FUNC_GEN_BD(PSRAW, psraw),
//     TRANS_FUNC_GEN_BD(PSRLD, psrld),
//     TRANS_FUNC_GEN_BD(PSRLQ, psrlq),
//     TRANS_FUNC_GEN_BD(PSRLW, psrlw),
//     TRANS_FUNC_GEN_BD(PSUBB, psubb),
//     TRANS_FUNC_GEN_BD(PSUBD, psubd),
//     TRANS_FUNC_GEN_BD(PSUBQ, psubq),
//     TRANS_FUNC_GEN_BD(PSUBSB, psubsb),
//     TRANS_FUNC_GEN_BD(PSUBSW, psubsw),
//     TRANS_FUNC_GEN_BD(PSUBUSB, psubusb),
//     TRANS_FUNC_GEN_BD(PSUBUSW, psubusw),
//     TRANS_FUNC_GEN_BD(PSUBW, psubw),
//     TRANS_FUNC_GEN_BD(PUNPCKHBW, punpckhbw),
//     TRANS_FUNC_GEN_BD(PUNPCKHDQ, punpckhdq),
//     TRANS_FUNC_GEN_BD(PUNPCKHWD, punpckhwd),
//     TRANS_FUNC_GEN_BD(PUNPCKLBW, punpcklbw),
//     TRANS_FUNC_GEN_BD(PUNPCKLDQ, punpckldq),
//     TRANS_FUNC_GEN_BD(PUNPCKLWD, punpcklwd),
//     TRANS_FUNC_GEN_BD(PXOR, pxor),
//     TRANS_FUNC_GEN_BD(HADDPD, haddpd),
//     TRANS_FUNC_GEN_BD(HADDPS, haddps),
//     TRANS_FUNC_GEN_BD(HSUBPD, hsubpd),
//     TRANS_FUNC_GEN_BD(HSUBPS, hsubps),
//     TRANS_FUNC_GEN_BD(MOV, mov),
//     TRANS_FUNC_GEN_BD(MOVABS, mov),
//     TRANS_FUNC_GEN_BD(MOVDDUP, movddup),
//     TRANS_FUNC_GEN_BD(MOVDQA, movdqa),
//     TRANS_FUNC_GEN_BD(MOVDQU, movdqu),
//     TRANS_FUNC_GEN_BD(MOVHLPS, movhlps),
//     TRANS_FUNC_GEN_BD(MOVHPD, movhpd),
//     TRANS_FUNC_GEN_BD(MOVHPS, movhps),
//     TRANS_FUNC_GEN_BD(MOVLHPS, movlhps),
//     TRANS_FUNC_GEN_BD(MOVLPD, movlpd),
//     TRANS_FUNC_GEN_BD(MOVLPS, movlps),
//     TRANS_FUNC_GEN_BD(MOVMSKPD, movmskpd),
//     TRANS_FUNC_GEN_BD(MOVMSKPS, movmskps),
//     TRANS_FUNC_GEN_BD(MOVNTDQ, movntdq),
//     TRANS_FUNC_GEN_BD(MOVNTI, movnti),
//     TRANS_FUNC_GEN_BD(MOVNTPD, movntpd),
//     TRANS_FUNC_GEN_BD(MOVNTPS, movntps),
//     TRANS_FUNC_GEN_BD(MOVSB, movs),
//     TRANS_FUNC_GEN_BD(MOVSHDUP, movshdup),
//     TRANS_FUNC_GEN_BD(MOVSLDUP, movsldup),
//     TRANS_FUNC_GEN_BD(MOVSQ, movs),
//     TRANS_FUNC_GEN_BD(MOVSS, movss),
//     TRANS_FUNC_GEN_BD(MOVSW, movs),
//     TRANS_FUNC_GEN_BD(MOVSX, movsx),
//     TRANS_FUNC_GEN_BD(MOVSXD, movsxd),
//     TRANS_FUNC_GEN_BD(MOVUPD, movupd),
//     TRANS_FUNC_GEN_BD(MOVUPS, movups),
//     TRANS_FUNC_GEN_BD(MOVZX, movzx),
    TRANS_FUNC_GEN_BD(MUL, mul),
    TRANS_FUNC_GEN_BD(MULX, mulx),
//     TRANS_FUNC_GEN_BD(MULPD, mulpd),
//     TRANS_FUNC_GEN_BD(MULPS, mulps),
//     TRANS_FUNC_GEN_BD(MULSD, mulsd),
//     TRANS_FUNC_GEN_BD(MULSS, mulss),
    TRANS_FUNC_GEN_BD(NEG, neg),
    TRANS_FUNC_GEN_BD(NOP, nop),
//     TRANS_FUNC_GEN_BD(NOT, not),
    TRANS_FUNC_GEN_BD(OUT, out),
//     TRANS_FUNC_GEN_BD(PAUSE, pause),
//     TRANS_FUNC_GEN_BD(POPCNT, popcnt),
//     TRANS_FUNC_GEN_BD(POP, pop),
//     TRANS_FUNC_GEN_BD(POPAW, popaw),
//     TRANS_FUNC_GEN_BD(POPAL, popal),
    TRANS_FUNC_GEN_BD(POPF, popf),
//     TRANS_FUNC_GEN_BD(POPFD, popf),
//     TRANS_FUNC_GEN_BD(POPFQ, popf),
    TRANS_FUNC_GEN_BD(PREFETCH, prefetch),
    TRANS_FUNC_GEN_BD(PREFETCHNTA, prefetchnta),
    TRANS_FUNC_GEN_BD(PREFETCHT0, prefetcht0),
    TRANS_FUNC_GEN_BD(PREFETCHT1, prefetcht1),
    TRANS_FUNC_GEN_BD(PREFETCHT2, prefetcht2),
    TRANS_FUNC_GEN_BD(PREFETCHW, prefetchw),
//     TRANS_FUNC_GEN_BD(PSHUFD, pshufd),
//     TRANS_FUNC_GEN_BD(PSHUFHW, pshufhw),
//     TRANS_FUNC_GEN_BD(PSHUFLW, pshuflw),
//     TRANS_FUNC_GEN_BD(PSLLDQ, pslldq),
//     TRANS_FUNC_GEN_BD(PSRLDQ, psrldq),
//     TRANS_FUNC_GEN_BD(PUNPCKHQDQ, punpckhqdq),
//     TRANS_FUNC_GEN_BD(PUNPCKLQDQ, punpcklqdq),
//     TRANS_FUNC_GEN_BD(PUSH, push),
//     TRANS_FUNC_GEN_BD(PUSHAL, pushal),
//     TRANS_FUNC_GEN_BD(PUSHAW, pushaw),
    TRANS_FUNC_GEN_BD(PUSHF, pushf),
//     TRANS_FUNC_GEN_BD(PUSHFD, pushf),
//     TRANS_FUNC_GEN_BD(PUSHFQ, pushf),
//     TRANS_FUNC_GEN_BD(RCL, rcl),
//     TRANS_FUNC_GEN_BD(RCPPS, rcpps),
//     TRANS_FUNC_GEN_BD(RCPSS, rcpss),
//     TRANS_FUNC_GEN_BD(RCR, rcr),
    TRANS_FUNC_GEN_BD(RDTSC, rdtsc),
    TRANS_FUNC_GEN_BD(RDTSCP, rdtscp),
//     TRANS_FUNC_GEN_BD(ROL, rol),
//     TRANS_FUNC_GEN_BD(ROR, ror),
//     TRANS_FUNC_GEN_BD(RSQRTPS, rsqrtps),
//     TRANS_FUNC_GEN_BD(RSQRTSS, rsqrtss),
    TRANS_FUNC_GEN_BD(SAHF, sahf),
//     TRANS_FUNC_GEN_BD(SAL, sal),
//     TRANS_FUNC_GEN_BD(SAR, sar),
    TRANS_FUNC_GEN_BD(SBB, sbb),
//     TRANS_FUNC_GEN_BD(SCASB, scas),
//     TRANS_FUNC_GEN_BD(SCASD, scas),
//     TRANS_FUNC_GEN_BD(SCASQ, scas),
//     TRANS_FUNC_GEN_BD(SCASW, scas),
//     TRANS_FUNC_GEN_BD(SETAE, setcc),
//     TRANS_FUNC_GEN_BD(SETA, setcc),
//     TRANS_FUNC_GEN_BD(SETBE, setcc),
//     TRANS_FUNC_GEN_BD(SETB, setcc),
//     TRANS_FUNC_GEN_BD(SETE, setcc),
//     TRANS_FUNC_GEN_BD(SETGE, setcc),
//     TRANS_FUNC_GEN_BD(SETG, setcc),
//     TRANS_FUNC_GEN_BD(SETLE, setcc),
//     TRANS_FUNC_GEN_BD(SETL, setcc),
//     TRANS_FUNC_GEN_BD(SETNE, setcc),
//     TRANS_FUNC_GEN_BD(SETNO, setcc),
//     TRANS_FUNC_GEN_BD(SETNP, setcc),
//     TRANS_FUNC_GEN_BD(SETNS, setcc),
//     TRANS_FUNC_GEN_BD(SETO, setcc),
//     TRANS_FUNC_GEN_BD(SETP, setcc),
//     TRANS_FUNC_GEN_BD(SETS, setcc),
    TRANS_FUNC_GEN_BD(SFENCE, sfence),
    TRANS_FUNC_GEN_BD(CLFLUSH, clflush),
    TRANS_FUNC_GEN_BD(CLFLUSHOPT, clflushopt),
//     TRANS_FUNC_GEN_BD(SHL, shl),
//     TRANS_FUNC_GEN_BD(SHLD, shld),
//     TRANS_FUNC_GEN_BD(SHR, shr),
//     TRANS_FUNC_GEN_BD(SHRD, shrd),
//     TRANS_FUNC_GEN_BD(SARX, sarx),
//     TRANS_FUNC_GEN_BD(SHLX, shlx),
//     TRANS_FUNC_GEN_BD(SHRX, shrx),
//     TRANS_FUNC_GEN_BD(SHUFPD, shufpd),
//     TRANS_FUNC_GEN_BD(SHUFPS, shufps),
//     TRANS_FUNC_GEN_BD(SQRTPD, sqrtpd),
//     TRANS_FUNC_GEN_BD(SQRTPS, sqrtps),
//     TRANS_FUNC_GEN_BD(SQRTSD, sqrtsd),
//     TRANS_FUNC_GEN_BD(SQRTSS, sqrtss),
    TRANS_FUNC_GEN_BD(STC, stc),
    TRANS_FUNC_GEN_BD(STD, std),
//     TRANS_FUNC_GEN_BD(STMXCSR, stmxcsr),
//     TRANS_FUNC_GEN_BD(STOSB, stos),
//     TRANS_FUNC_GEN_BD(STOSD, stos),
//     TRANS_FUNC_GEN_BD(STOSQ, stos),
//     TRANS_FUNC_GEN_BD(STOSW, stos),
//     TRANS_FUNC_GEN_BD(SUBPD, subpd),
//     TRANS_FUNC_GEN_BD(SUBPS, subps),
//     TRANS_FUNC_GEN_BD(SUBSD, subsd),
//     TRANS_FUNC_GEN_BD(SUBSS, subss),
    TRANS_FUNC_GEN_BD(SYSCALL, syscall),
//     TRANS_FUNC_GEN_BD(TEST, test),
//     TRANS_FUNC_GEN_BD(UD2, ud2),
//     TRANS_FUNC_GEN_BD(UD1, ud2),
//     TRANS_FUNC_GEN_BD(UD0, ud2),
//     TRANS_FUNC_GEN_BD(TZCNT, tzcnt),
//     TRANS_FUNC_GEN_BD(UNPCKHPD, unpckhpd),
//     TRANS_FUNC_GEN_BD(UNPCKHPS, unpckhps),
//     TRANS_FUNC_GEN_BD(UNPCKLPD, unpcklpd),
//     TRANS_FUNC_GEN_BD(UNPCKLPS, unpcklps),
    TRANS_FUNC_GEN_BD(WAIT, wait_wrap),
//     TRANS_FUNC_GEN_BD(XCHG, xchg),
    TRANS_FUNC_GEN_BD(XLATB, xlat),
//     TRANS_FUNC_GEN_BD(CMPPD, cmppd),
//     TRANS_FUNC_GEN_BD(CMPPS, cmpps),
//     TRANS_FUNC_GEN_BD(CMPSS, cmpss),
//     TRANS_FUNC_GEN_BD(CMPEQSS, cmpeqss),
//     TRANS_FUNC_GEN_BD(CMPLTSS, cmpltss),
//     TRANS_FUNC_GEN_BD(CMPLESS, cmpless),
//     TRANS_FUNC_GEN_BD(CMPUNORDSS, cmpunordss),
//     TRANS_FUNC_GEN_BD(CMPNEQSS, cmpneqss),
//     TRANS_FUNC_GEN_BD(CMPNLTSS, cmpnltss),
//     TRANS_FUNC_GEN_BD(CMPNLESS, cmpnless),
//     TRANS_FUNC_GEN_BD(CMPORDSS, cmpordss),
//     TRANS_FUNC_GEN_BD(CMPEQSD, cmpeqsd),
//     TRANS_FUNC_GEN_BD(CMPLTSD, cmpltsd),
//     TRANS_FUNC_GEN_BD(CMPLESD, cmplesd),
//     TRANS_FUNC_GEN_BD(CMPUNORDSD, cmpunordsd),
//     TRANS_FUNC_GEN_BD(CMPNEQSD, cmpneqsd),
//     TRANS_FUNC_GEN_BD(CMPNLTSD, cmpnltsd),
//     TRANS_FUNC_GEN_BD(CMPNLESD, cmpnlesd),
//     TRANS_FUNC_GEN_BD(CMPORDSD, cmpordsd),
//     TRANS_FUNC_GEN_BD(CMPEQPS, cmpeqps),
//     TRANS_FUNC_GEN_BD(CMPLTPS, cmpltps),
//     TRANS_FUNC_GEN_BD(CMPLEPS, cmpleps),
//     TRANS_FUNC_GEN_BD(CMPUNORDPS, cmpunordps),
//     TRANS_FUNC_GEN_BD(CMPNEQPS, cmpneqps),
//     TRANS_FUNC_GEN_BD(CMPNLTPS, cmpnltps),
//     TRANS_FUNC_GEN_BD(CMPNLEPS, cmpnleps),
//     TRANS_FUNC_GEN_BD(CMPORDPS, cmpordps),
//     TRANS_FUNC_GEN_BD(CMPEQPD, cmpeqpd),
//     TRANS_FUNC_GEN_BD(CMPLTPD, cmpltpd),
//     TRANS_FUNC_GEN_BD(CMPLEPD, cmplepd),
//     TRANS_FUNC_GEN_BD(CMPUNORDPD, cmpunordpd),
//     TRANS_FUNC_GEN_BD(CMPNEQPD, cmpneqpd),
//     TRANS_FUNC_GEN_BD(CMPNLTPD, cmpnltpd),
//     TRANS_FUNC_GEN_BD(CMPNLEPD, cmpnlepd),
//     TRANS_FUNC_GEN_BD(CMPORDPD, cmpordpd),
    TRANS_FUNC_GEN_BD(ENDBR, endbr),
//     TRANS_FUNC_GEN_BD(LJMP, nop),
//     TRANS_FUNC_GEN_BD(LCALL, nop),
//     TRANS_FUNC_GEN_BD(LDS, nop),
    TRANS_FUNC_GEN_BD(ENTER, enter),
//     TRANS_FUNC_GEN_BD(LES, nop),
//     TRANS_FUNC_GEN_BD(OUTSB, nop),
//     TRANS_FUNC_GEN_BD(CLI, nop),
//     TRANS_FUNC_GEN_BD(STI, nop),
    TRANS_FUNC_GEN_BD(SALC, salc),
//     TRANS_FUNC_GEN_BD(BOUND, nop),
//     TRANS_FUNC_GEN_BD(INTO, nop),
//     TRANS_FUNC_GEN_BD(INSB, nop),
    TRANS_FUNC_GEN_BD(RETF, retf),
//     TRANS_FUNC_GEN_BD(INT1, nop),
//     TRANS_FUNC_GEN_BD(OUTSD, nop),
//     TRANS_FUNC_GEN_BD(SLDT, nop),
//     TRANS_FUNC_GEN_BD(ARPL, nop),
//     TRANS_FUNC_GEN_BD(SIDT, nop),

//     /* ssse3 */
//     TRANS_FUNC_GEN_BD(PSIGNB, psignb),
//     TRANS_FUNC_GEN_BD(PSIGNW, psignw),
//     TRANS_FUNC_GEN_BD(PSIGND, psignd),
//     TRANS_FUNC_GEN_BD(PABSB, pabsb),
//     TRANS_FUNC_GEN_BD(PABSW, pabsw),
//     TRANS_FUNC_GEN_BD(PABSD, pabsd),
//     TRANS_FUNC_GEN_BD(PALIGNR, palignr),
//     TRANS_FUNC_GEN_BD(PSHUFB, pshufb),
//     TRANS_FUNC_GEN_BD(PMULHRSW, pmulhrsw),
//     TRANS_FUNC_GEN_BD(PMADDUBSW, pmaddubsw),
//     TRANS_FUNC_GEN_BD(PHSUBW, phsubw),
//     TRANS_FUNC_GEN_BD(PHSUBD, phsubd),
//     TRANS_FUNC_GEN_BD(PHSUBSW, phsubsw),
//     TRANS_FUNC_GEN_BD(PHADDW, phaddw),
//     TRANS_FUNC_GEN_BD(PHADDD, phaddd),
//     TRANS_FUNC_GEN_BD(PHADDSW, phaddsw),

//     /* sse 4.1 fp */
//     TRANS_FUNC_GEN_BD(DPPS, dpps),
//     TRANS_FUNC_GEN_BD(DPPD, dppd),
//     TRANS_FUNC_GEN_BD(BLENDPS, blendps),
//     TRANS_FUNC_GEN_BD(BLENDPD, blendpd),
//     TRANS_FUNC_GEN_BD(BLENDVPS, blendvps),
//     TRANS_FUNC_GEN_BD(BLENDVPD, blendvpd),
//     TRANS_FUNC_GEN_BD(ROUNDPS, roundps),
//     TRANS_FUNC_GEN_BD(ROUNDSS, roundss),
//     TRANS_FUNC_GEN_BD(ROUNDPD, roundpd),
//     TRANS_FUNC_GEN_BD(ROUNDSD, roundsd),
//     TRANS_FUNC_GEN_BD(INSERTPS, insertps),
//     TRANS_FUNC_GEN_BD(EXTRACTPS, extractps),

//     /* sse 4.1 int */
//     TRANS_FUNC_GEN_BD(MPSADBW, mpsadbw),
//     TRANS_FUNC_GEN_BD(PHMINPOSUW, phminposuw),
//     TRANS_FUNC_GEN_BD(PMULLD, pmulld),
//     TRANS_FUNC_GEN_BD(PMULDQ, pmuldq),
//     TRANS_FUNC_GEN_BD(PBLENDVB, pblendvb),
//     TRANS_FUNC_GEN_BD(PBLENDW, pblendw),
//     TRANS_FUNC_GEN_BD(PMINSB, pminsb),
//     TRANS_FUNC_GEN_BD(PMINUW, pminuw),
//     TRANS_FUNC_GEN_BD(PMINSD, pminsd),
//     TRANS_FUNC_GEN_BD(PMINUD, pminud),
//     TRANS_FUNC_GEN_BD(PMAXSB, pmaxsb),
//     TRANS_FUNC_GEN_BD(PMAXUW, pmaxuw),
//     TRANS_FUNC_GEN_BD(PMAXSD, pmaxsd),
//     TRANS_FUNC_GEN_BD(PMAXUD, pmaxud),
//     TRANS_FUNC_GEN_BD(PINSRB, pinsrb),
//     TRANS_FUNC_GEN_BD(PINSRD, pinsrd),
//     TRANS_FUNC_GEN_BD(PINSRQ, pinsrq),
//     TRANS_FUNC_GEN_BD(PEXTRB, pextrb),
//     TRANS_FUNC_GEN_BD(PEXTRD, pextrd),
//     TRANS_FUNC_GEN_BD(PEXTRQ, pextrq),
//     TRANS_FUNC_GEN_BD(PMOVSXBW, pmovsxbw),
//     TRANS_FUNC_GEN_BD(PMOVZXBW, pmovzxbw),
//     TRANS_FUNC_GEN_BD(PMOVSXBD, pmovsxbd),
//     TRANS_FUNC_GEN_BD(PMOVZXBD, pmovzxbd),
//     TRANS_FUNC_GEN_BD(PMOVSXBQ, pmovsxbq),
//     TRANS_FUNC_GEN_BD(PMOVZXBQ, pmovzxbq),
//     TRANS_FUNC_GEN_BD(PMOVSXWD, pmovsxwd),
//     TRANS_FUNC_GEN_BD(PMOVZXWD, pmovzxwd),
//     TRANS_FUNC_GEN_BD(PMOVSXWQ, pmovsxwq),
//     TRANS_FUNC_GEN_BD(PMOVZXWQ, pmovzxwq),
//     TRANS_FUNC_GEN_BD(PMOVSXDQ, pmovsxdq),
//     TRANS_FUNC_GEN_BD(PMOVZXDQ, pmovzxdq),
//     TRANS_FUNC_GEN_BD(PTEST, ptest),
//     TRANS_FUNC_GEN_BD(PCMPEQQ, pcmpeqq),
//     TRANS_FUNC_GEN_BD(PACKUSDW, packusdw),
//     TRANS_FUNC_GEN_BD(MOVNTDQA, movntdqa),


//     /* fpu */
    TRANS_FUNC_GEN_BD(F2XM1, f2xm1_wrap),
    TRANS_FUNC_GEN_BD(FABS, fabs_wrap),
    TRANS_FUNC_GEN_BD(FADD, fadd_wrap),
    TRANS_FUNC_GEN_BD(FADDP, faddp_wrap),
    TRANS_FUNC_GEN_BD(FBLD, fbld_wrap),
    TRANS_FUNC_GEN_BD(FBSTP, fbstp_wrap),
    TRANS_FUNC_GEN_BD(FCHS, fchs_wrap),
    TRANS_FUNC_GEN_BD(FCMOVB, fcmovb_wrap),
    TRANS_FUNC_GEN_BD(FCMOVBE, fcmovbe_wrap),
    TRANS_FUNC_GEN_BD(FCMOVE, fcmove_wrap),
    TRANS_FUNC_GEN_BD(FCMOVNB, fcmovnb_wrap),
    TRANS_FUNC_GEN_BD(FCMOVNBE, fcmovnbe_wrap),
    TRANS_FUNC_GEN_BD(FCMOVNE, fcmovne_wrap),
    TRANS_FUNC_GEN_BD(FCMOVNU, fcmovnu_wrap),
    TRANS_FUNC_GEN_BD(FCMOVU, fcmovu_wrap),
    TRANS_FUNC_GEN_BD(FCOM, fcom_wrap),
    TRANS_FUNC_GEN_BD(FCOMI, fcomi_wrap),
    TRANS_FUNC_GEN_BD(FCOMIP, fcomip_wrap),
    TRANS_FUNC_GEN_BD(FCOMP, fcomp_wrap),
    TRANS_FUNC_GEN_BD(FCOMPP, fcompp_wrap),
    TRANS_FUNC_GEN_BD(FCOS, fcos_wrap),
    TRANS_FUNC_GEN_BD(FDECSTP, fdecstp_wrap),
    TRANS_FUNC_GEN_BD(FDIV, fdiv_wrap),
    TRANS_FUNC_GEN_BD(FDIVP, fdivp_wrap),
    TRANS_FUNC_GEN_BD(FDIVR, fdivr_wrap),
    TRANS_FUNC_GEN_BD(FDIVRP, fdivrp_wrap),
    TRANS_FUNC_GEN_BD(FFREE, ffree_wrap),
    TRANS_FUNC_GEN_BD(FFREEP, ffreep_wrap),
    TRANS_FUNC_GEN_BD(FIADD, fiadd_wrap),
    TRANS_FUNC_GEN_BD(FICOM, ficom_wrap),
    TRANS_FUNC_GEN_BD(FICOMP, ficomp_wrap),
    TRANS_FUNC_GEN_BD(FIDIV, fidiv_wrap),
    TRANS_FUNC_GEN_BD(FIDIVR, fidivr_wrap),
    TRANS_FUNC_GEN_BD(FILD, fild_wrap),
    TRANS_FUNC_GEN_BD(FIMUL, fimul_wrap),
    TRANS_FUNC_GEN_BD(FINCSTP, fincstp_wrap),
    TRANS_FUNC_GEN_BD(FIST, fist_wrap),
    TRANS_FUNC_GEN_BD(FISTP, fistp_wrap),
    TRANS_FUNC_GEN_BD(FISTTP, fisttp_wrap),
    TRANS_FUNC_GEN_BD(FISUB, fisub_wrap),
    TRANS_FUNC_GEN_BD(FISUBR, fisubr_wrap),
    TRANS_FUNC_GEN_BD(FLD1, fld1_wrap),
    TRANS_FUNC_GEN_BD(FLD, fld_wrap),
    TRANS_FUNC_GEN_BD(FLDCW, fldcw_wrap),
    TRANS_FUNC_GEN_BD(FLDENV, fldenv_wrap),
    TRANS_FUNC_GEN_BD(FLDL2E, fldl2e_wrap),
    TRANS_FUNC_GEN_BD(FLDL2T, fldl2t_wrap),
    TRANS_FUNC_GEN_BD(FLDLG2, fldlg2_wrap),
    TRANS_FUNC_GEN_BD(FLDLN2, fldln2_wrap),
    TRANS_FUNC_GEN_BD(FLDPI, fldpi_wrap),
    TRANS_FUNC_GEN_BD(FLDZ, fldz_wrap),
    TRANS_FUNC_GEN_BD(FMUL, fmul_wrap),
    TRANS_FUNC_GEN_BD(FMULP, fmulp_wrap),
    TRANS_FUNC_GEN_BD(FNCLEX, fnclex_wrap),
    TRANS_FUNC_GEN_BD(FNINIT, fninit_wrap),
    TRANS_FUNC_GEN_BD(FNOP, fnop_wrap),
    TRANS_FUNC_GEN_BD(FNSAVE, fnsave_wrap),
    TRANS_FUNC_GEN_BD(FNSTCW, fnstcw_wrap),
    TRANS_FUNC_GEN_BD(FNSTENV, fnstenv_wrap),
    TRANS_FUNC_GEN_BD(FNSTSW, fnstsw_wrap),
    TRANS_FUNC_GEN_BD(FPATAN, fpatan_wrap),
    TRANS_FUNC_GEN_BD(FPREM1, fprem1_wrap),
    TRANS_FUNC_GEN_BD(FPREM, fprem_wrap),
    TRANS_FUNC_GEN_BD(FPTAN, fptan_wrap),
    TRANS_FUNC_GEN_BD(FRNDINT, frndint_wrap),
    TRANS_FUNC_GEN_BD(FRSTOR, frstor_wrap),
    TRANS_FUNC_GEN_BD(FSCALE, fscale_wrap),
//     TRANS_FUNC_GEN_BD(FSETPM, fsetpm_wrap),
    TRANS_FUNC_GEN_BD(FSIN, fsin_wrap),
    TRANS_FUNC_GEN_BD(FSINCOS, fsincos_wrap),
    TRANS_FUNC_GEN_BD(FSQRT, fsqrt_wrap),
    TRANS_FUNC_GEN_BD(FST, fst_wrap),
    TRANS_FUNC_GEN_BD(FSTP, fstp_wrap),
    TRANS_FUNC_GEN_BD(FSUB, fsub_wrap),
    TRANS_FUNC_GEN_BD(FSUBP, fsubp_wrap),
    TRANS_FUNC_GEN_BD(FSUBR, fsubr_wrap),
    TRANS_FUNC_GEN_BD(FSUBRP, fsubrp_wrap),
    TRANS_FUNC_GEN_BD(FTST, ftst_wrap),
    TRANS_FUNC_GEN_BD(FUCOM, fucom_wrap),
    TRANS_FUNC_GEN_BD(FUCOMI, fucomi_wrap),
    TRANS_FUNC_GEN_BD(FUCOMIP, fucomip_wrap),
    TRANS_FUNC_GEN_BD(FUCOMP, fucomp_wrap),
    TRANS_FUNC_GEN_BD(FUCOMPP, fucompp_wrap),
    TRANS_FUNC_GEN_BD(FXAM, fxam_wrap),
    TRANS_FUNC_GEN_BD(FXCH, fxch_wrap),
    TRANS_FUNC_GEN_BD(FXRSTOR, fxrstor_wrap),
    TRANS_FUNC_GEN_BD(FXRSTOR64, fxrstor_wrap),
    TRANS_FUNC_GEN_BD(FXSAVE, fxsave_wrap),
    TRANS_FUNC_GEN_BD(FXSAVE64, fxsave_wrap),
    TRANS_FUNC_GEN_BD(FXTRACT, fxtract_wrap),
    TRANS_FUNC_GEN_BD(FYL2X, fyl2x_wrap),
    TRANS_FUNC_GEN_BD(FYL2XP1, fyl2xp1_wrap),

//     /* sha */
//     TRANS_FUNC_GEN_BD(SHA1MSG1, sha1msg1),
//     TRANS_FUNC_GEN_BD(SHA1MSG2, sha1msg2),
//     TRANS_FUNC_GEN_BD(SHA1NEXTE, sha1nexte),
//     TRANS_FUNC_GEN_BD(SHA1RNDS4, sha1rnds4),
//     TRANS_FUNC_GEN_BD(SHA256MSG1, sha256msg1),
//     TRANS_FUNC_GEN_BD(SHA256MSG2, sha256msg2),
//     TRANS_FUNC_GEN_BD(SHA256RNDS2, sha256rnds2),

//     TRANS_FUNC_GEN_BD(ANDN, andn),
//     TRANS_FUNC_GEN_BD(MOVBE, movbe),
//     TRANS_FUNC_GEN_BD(RORX, rorx),
    TRANS_FUNC_GEN_BD(BLSI, blsi),

//     TRANS_FUNC_GEN_BD(PCMPESTRI, pcmpestri),
//     TRANS_FUNC_GEN_BD(PCMPESTRM, pcmpestrm),
//     TRANS_FUNC_GEN_BD(PCMPISTRI, pcmpistri),
//     TRANS_FUNC_GEN_BD(PCMPISTRM, pcmpistrm),

//     TRANS_FUNC_GEN_BD(AESDEC, aesdec),
//     TRANS_FUNC_GEN_BD(AESDECLAST, aesdeclast),
//     TRANS_FUNC_GEN_BD(AESENC, aesenc),
//     TRANS_FUNC_GEN_BD(AESENCLAST, aesenclast),
//     TRANS_FUNC_GEN_BD(AESIMC, aesimc),
//     TRANS_FUNC_GEN_BD(AESKEYGENASSIST, aeskeygenassist),

    TRANS_FUNC_GEN_BD(PEXT, pext),
    TRANS_FUNC_GEN_BD(PDEP, pdep),
    TRANS_FUNC_GEN_BD(BEXTR, bextr),
    TRANS_FUNC_GEN_BD(BLSMSK, blsmsk),
    TRANS_FUNC_GEN_BD(BZHI, bzhi),
//     TRANS_FUNC_GEN_BD(LZCNT, lzcnt),
    TRANS_FUNC_GEN_BD(ADCX, adcx),
    TRANS_FUNC_GEN_BD(ADOX, adox),
    TRANS_FUNC_GEN_BD(CRC32, crc32),
//     TRANS_FUNC_GEN_BD(PCLMULQDQ, pclmulqdq),

//     TRANS_FUNC_GEN_REAL(ENDING, NULL),
};

bool ir1_translate_bd(IR1_INST *ir1)
{
#ifdef CONFIG_LATX_INSTS_PATTERN
    if (try_translate_instptn_bd(ir1)) {
        ra_free_all();
        return true;
    }
#endif

    /* 2. call translate_xx function */
    int tr_func_idx = ir1_opcode_bd(ir1) - ND_INS_INVALID;

    bool translation_success = false;

#ifdef CONFIG_LATX_DEBUG
    if (unlikely(latx_trace_mem)) {
        lsenv->current_ir1 = ir1_addr_bd(ir1);
    }
#endif

    if (ir1_opcode_bd(ir1) == ND_INS_CALLNI || ir1_opcode_bd(ir1) == ND_INS_CALLNR) {
        if (!ir1_is_indirect_call_bd(ir1)) {
            if (ir1_addr_next_bd(ir1) == ir1_target_addr_bd(ir1)) {
                return translate_callnext_bd(ir1);
            } else if (ht_pc_thunk_lookup(ir1_target_addr_bd(ir1)) >= 0) {
                return translate_callthunk_bd(ir1);
            }
        }
    }

    if (option_softfpu) {
        int opnd_num = ir1_get_opnd_num_bd(ir1);
        bool mmx_ins = false;
        for (int i = 0; i < opnd_num; i++) {
            if (ir1_opnd_is_mmx_bd(ir1_get_opnd_bd(ir1, i))) {
                mmx_ins = true;
            }
        }
        if (mmx_ins) {
            la_st_w(zero_ir2_opnd, env_ir2_opnd,
                      lsenv_offset_of_top(lsenv));
            la_st_d(zero_ir2_opnd, env_ir2_opnd,
                      lsenv_offset_of_tag_word(lsenv));
        }
    }

    /*sse instructions need to reserve h128 bit of ymm\
        if support avx instructions*/
    IR2_OPND temp = ir2_opnd_new_none();
    if (option_lative && option_enable_lasx && ir1_need_reserve_h128_bd(ir1)) {
        temp = save_h128_of_ymm_bd(ir1);
    }

    if (translate_functions[tr_func_idx] == NULL) {
        ir1_opcode_dump_bd(ir1);
#ifndef CONFIG_LATX_TU
        lsassertm(0, "%s %s %d error : this ins %d not implemented: %s\n",
            __FILE__, __func__, __LINE__, tr_func_idx, ((INSTRUX *)(ir1->info))->Mnemonic);
#elif defined(CONFIG_LATX_DEBUG)
        fprintf(stderr, "\033[31m%s %s %d error : this ins %d not implemented: %s\n\033[m",
            __FILE__, __func__, __LINE__, tr_func_idx, ((INSTRUX *)(ir1->info))->Mnemonic);
#endif
#if (defined CONFIG_LATX_TU || defined CONFIG_LATX_TS)
        return 0;
#endif
    } else {
        translation_success = translate_functions[tr_func_idx](ir1); /* TODO */
    }

    /*restore h128 bit of ymm after translating sse instructions*/
    if (option_lative && option_enable_lasx && temp._type != IR2_OPND_NONE) {
        restore_h128_of_ymm_bd(ir1, temp);
    }

#ifdef CONFIG_LATX_DEBUG

    if (unlikely(latx_trace_mem)) {
        if (ir1_opnd_num_bd(ir1) && ir1_opnd_is_mem_bd(ir1_get_opnd_bd(ir1, 0))) {
            IR2_OPND label_ne = ra_alloc_label();
            IR2_OPND mem_opnd = ra_alloc_trace(TRACE);
            IR2_OPND trace_addr = ra_alloc_itemp();
            li_d(trace_addr, latx_trace_mem);
            la_bne(mem_opnd, trace_addr, label_ne);
            li_d(trace_addr, ir1_addr_bd(ir1));
            la_st_d(trace_addr, env_ir2_opnd,
                    lsenv_offset_last_store_insn(lsenv));
            la_label(label_ne);
            ra_free_temp(trace_addr);
        }
    }

    if (unlikely(latx_break_insn) && ir1_addr_bd(ir1) == latx_break_insn) {
        if (option_debug_lative) {
            if (ir1_opcode_bd(ir1) != ND_INS_RETN && ir1_opcode_bd(ir1) != ND_INS_RETF) {
                la_ld_h(zero_ir2_opnd, zero_ir2_opnd, 0xde);
                latx_break_insn = ir1_addr_next_bd(ir1);
            }
        } else {
            la_st_h(zero_ir2_opnd, zero_ir2_opnd, 0x1);
        }
    }
#endif

#ifdef DEBUG_LATX_PER_INSN_DBAR
    la_dbar(0);
#endif
    /* 3. postprocess */
    ra_free_all();
    /* For now the line number of first member of struct translate_functions is 476, remember to
     * change it if the line number is changed.
     */

#ifdef CONFIG_LATX_DEBUG
    if (!translation_success) {
        ir1_opcode_dump_bd(ir1);

#ifdef CONFIG_LATX_TU
        fprintf(stderr, "%s %s %d error : this ins %d not implemented\n",
                    __FILE__, __func__, __LINE__, tr_func_idx);
#else
        lsassertm(0, "%s %s %d error : this ins %d not implemented: %s\n",
                    __FILE__, __func__, __LINE__, tr_func_idx, ((INSTRUX *)(ir1->info))->Mnemonic);
#endif

    }
#endif
    return translation_success;
}

static void generate_indirect_goto(void *code_buf, bool parallel)
{
    /*
     * WARNING!!!
     * 如果修改该函数，需要注意 unlink_indirect_jmp 中写 nop 的位置！！！
     */
    IR2_OPND next_x86_addr = ra_alloc_dbt_arg2();
    IR2_OPND base = ra_alloc_data();
    la_data_li(base, (ADDR)code_buf);

    /* indirect jmp */
    IR2_OPND jmp_entry = ra_alloc_itemp();
    IR2_OPND jmp_cache_addr = ra_alloc_static0();

    IR2_OPND next_tb = V0_RENAME_OPND;
    IR2_OPND target = ra_alloc_data();
    IR2_OPND label_miss = ra_alloc_label();
    /*
     * lookup HASH_JMP_CACHE
     * Step 1: calculate HASH = (x86_addr >> 12) ^ (x86_addr & 0xfff)
     * Step 2: load &HASH_JMP_CACHE[0]
     * Step 3: load tb = HASH_JMP_CACHE[HASH]
     * Step 4: if (tb == 0) {goto labal_miss}
     * Step 5: if (tb.pc == x86_addr) {goto fpu_rotate}
     *         else {goto labal_miss}
     */
    la_srli_d(next_tb, next_x86_addr, TB_JMP_CACHE_BITS);
    la_xor(next_tb, next_x86_addr, next_tb);
    la_bstrpick_d(next_tb, next_tb, TB_JMP_CACHE_BITS - 1, 0);

    if (!close_latx_parallel && !parallel) {
        la_alsl_d(next_tb, next_tb, jmp_cache_addr, 3);
        la_ld_d(jmp_entry, next_tb, 0);
        la_bne(jmp_entry, next_x86_addr, label_miss);
        la_ld_d(jmp_entry, next_tb, 8);
    } else {
        la_slli_d(next_tb, next_tb, 3);
        la_ldx_d(next_tb, next_tb, jmp_cache_addr);
        la_beq(next_tb, zero_ir2_opnd, label_miss);

        la_ld_d(jmp_entry, next_tb, 0);
        la_bne(jmp_entry, next_x86_addr, label_miss);

        /*
         * if jmp_lock set, goto miss, else see if CF_INVALID is set.
         * This method is faster than spin-lock/unlock because the
         * locked case is rare.
         */
        la_ld_d(jmp_entry, next_tb, offsetof(TranslationBlock, cflags));
        /* CF_INVALID */
        la_bstrpick_w(jmp_entry, jmp_entry, 18, 18);
        la_bne(jmp_entry, zero_ir2_opnd, label_miss);
        la_ld_d(jmp_entry, next_tb,
                offsetof(TranslationBlock, tc) +
                offsetof(struct tb_tc, ptr));
    }

/* hit: */
    la_jirl(zero_ir2_opnd, jmp_entry, 0);

    ra_free_temp(jmp_entry);
/* miss: */
    la_label(label_miss);
    /*
     * jump to epilogue with 0 return
     * two args are pass-through:
     * ra_alloc_dbt_arg1: current tb (last tb)
     * ra_alloc_dbt_arg2: next x86 ip
     */

    la_data_li(target, context_switch_native_to_bt_ret_0);
    aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE_RET_0, 0);

    return;
}

#ifdef CONFIG_LATX_XCOMISX_OPT
inline
void tr_generate_exit_tb_bd(IR1_INST *branch, int succ_id)
{
    tr_generate_exit_stub_tb_bd(branch, succ_id, NULL, NULL);
}

void tr_generate_exit_stub_tb_bd(IR1_INST *branch, int succ_id, void *func, IR1_INST *stub)
#else
void tr_generate_exit_tb_bd(IR1_INST *branch, int succ_id)
#endif
{
    TRANSLATION_DATA *t_data = lsenv->tr_data;
    TranslationBlock *tb = lsenv->tr_data->curr_tb;
    IR1_OPCODE_BD opcode = ir1_opcode_bd(branch);
    ADDR tb_addr = (ADDR)tb;
    ADDR succ_x86_addr = 0;
    /* The epilogue function needs to return the address of the tb */
    IR2_OPND tb_ptr_opnd = a0_ir2_opnd;
    IR2_OPND succ_x86_addr_opnd = ra_alloc_dbt_arg2();
    IR2_OPND goto_label_opnd = ra_alloc_label();
    IR2_OPND label_first_jmp_align = ra_alloc_label();

    IR2_OPND base = ra_alloc_data();
    IR2_OPND target = ra_alloc_data();
    /* set base_address data */
    la_data_li(base, (ADDR)tb->tc.ptr);

    switch (opcode) {
    case ND_INS_CALLNI:
    case ND_INS_CALLNR:
    case ND_INS_CALLFD:
    case ND_INS_CALLFI:
    case ND_INS_JMPNR:
    case ND_INS_JMPNI:
    case ND_INS_JMPFI:
    case ND_INS_JMPFD:
        /*
         * jmp_reset_offset[0] or [1] should be set when the instruction is
         * call or jmp. Because direct jmp or call only have one jmp target
         * address, jmp_reset_offset[n] may do not reset for right address.
         *
         * succ_x86_addr is set 1 for judging whether the instruction is
         * direct jmp or condition jmp.
         */
        if (ir1_is_indirect_jmp_bd(branch)) {
            goto indirect_jmp;
        } else {
            succ_x86_addr = 1;
            goto direct_jmp;
        }
        break;
    case ND_INS_JZ: //JE
    case ND_INS_JNZ://JNE
    case ND_INS_JS:
    case ND_INS_JNS:
    case ND_INS_JC: //JB
    case ND_INS_JNC://JAE
    case ND_INS_JO:
    case ND_INS_JNO:
    case ND_INS_JBE:
    case ND_INS_JNBE://JA
    case ND_INS_JP:
    case ND_INS_JNP:
    case ND_INS_JL:
    case ND_INS_JNL://JGE
    case ND_INS_JLE:
    case ND_INS_JNLE: //JG
    case ND_INS_JrCXZ:
    case ND_INS_LOOP:
    case ND_INS_LOOPZ:
    case ND_INS_LOOPNZ:
direct_jmp:
        /*
         * If option_lsfpu is open, condition jmp will jmp to next tb diretly.
         * Therefore LATX do not always need to update last_tb, after tb link.
         *
         * When tb is not link, LATX should save next_x86_addr, succ_id and
         * tb_ptr. next_x86_addr will be used to find next TB, succ_id will
         * be used to TB_link and tb_ptr will be used by jmp_glue.
         * After saving those information, LATX switch to translate next_tb.
         *
         * Also, if we need to patch the offset, the patch place need to be
         * aligned. This nop is used to do that.
         */
        if (!succ_id) {
            la_label(label_first_jmp_align);
            tb->first_jmp_align = ir2_opnd_label_id(&label_first_jmp_align);
        }
        la_code_align(2, 0x03400000);

#ifndef CONFIG_LATX_XCOMISX_OPT
        la_label(goto_label_opnd);
        tb->jmp_reset_offset[succ_id] = ir2_opnd_label_id(&goto_label_opnd);
#endif
        IR2_OPND ir2_opnd_addr;
        ir2_opnd_build(&ir2_opnd_addr, IR2_OPND_IMM, 0);

#ifdef CONFIG_LATX_XCOMISX_OPT
        /* Add stub for exit recover */
        if (func) {
            IR2_OPND stub_label = ra_alloc_label();
            la_label(stub_label);
            tb->jmp_stub_reset_offset[succ_id] = ir2_opnd_label_id(&stub_label);
            /* exit tb_link */
            la_nop();
#ifdef CONFIG_LATX_LARGE_CC
            la_nop();
#endif
            /* generate stub */
            ((bool (*)(IR1_INST *))func)(stub);

        }

        la_code_align(2, 0x03400000);

        la_label(goto_label_opnd);
        tb->jmp_reset_offset[succ_id] = ir2_opnd_label_id(&goto_label_opnd);
#endif
        la_b(ir2_opnd_addr);
#ifdef CONFIG_LATX_LARGE_CC
        la_nop();
#endif

#ifdef CONFIG_LATX_PROFILER
        la_profile_begin();
#endif
        aot_load_host_addr(tb_ptr_opnd, tb_addr, LOAD_TB_ADDR, 0);
        if (succ_x86_addr) {
            succ_x86_addr = ir1_target_addr_bd(branch);
        } else {
            succ_x86_addr = succ_id ? ir1_target_addr_bd(branch) : ir1_addr_next_bd(branch);
        }

        target_ulong call_offset __attribute__((unused)) =
                aot_get_call_offset(succ_x86_addr);
        aot_load_guest_addr(succ_x86_addr_opnd, succ_x86_addr,
                LOAD_CALL_TARGET, call_offset);

        bool self_jmp = ((opcode == ND_INS_JMPNR || opcode == ND_INS_JMPNI) &&
                         (succ_x86_addr == ir1_addr_bd(branch)) &&
                         (t_data->curr_ir1_count + 1 != MAX_IR1_NUM_PER_TB));

        if (!qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN) && !self_jmp) {
            la_ori(tb_ptr_opnd, tb_ptr_opnd, succ_id);
        } else {
            la_ori(tb_ptr_opnd, zero_ir2_opnd, succ_id);
        }
        la_data_li(target, context_switch_native_to_bt);
        aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE, 0);
        break;
    case ND_INS_RETN:
    case ND_INS_RETF:
    case ND_INS_IRET:
indirect_jmp:
        /*
         * If option_lsfpu is open, LATX do not need to fpu_rotate, therefore
         * do not save the TB pointer.
         *
         * If using tb_link, LATX will jmp to jmp_glue finding the next TB.
         */
        if (!qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN)) {
            CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
            CPUState *cpu = env_cpu(env);
            uint32_t parallel = cpu->tcg_cflags & CF_PARALLEL;
            if (!close_latx_parallel && !parallel) {
                IR2_OPND old_jmp_label = ra_alloc_label();
                la_label(old_jmp_label);
                tb->jmp_indirect = ir2_opnd_label_id(&old_jmp_label);
                generate_indirect_goto((void *)tb->tc.ptr, false);
                la_data_li(target, context_switch_native_to_bt_ret_0);
                aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE_RET_0, 0);
            } else {
                la_data_li(target, parallel_indirect_jmp_glue);
                IR2_OPND old_jmp_label = ra_alloc_label();
                la_label(old_jmp_label);
                tb->jmp_indirect = ir2_opnd_label_id(&old_jmp_label);
                aot_la_append_ir2_jmp_far(target, base, B_NATIVE_JMP_GLUE2, 0);
                la_data_li(target, context_switch_native_to_bt_ret_0);
                aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE_RET_0, 0);
            }
        } else {
            la_data_li(target, context_switch_native_to_bt_ret_0);
            aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE_RET_0, 0);
        }
#ifdef CONFIG_LATX_PROFILER
        la_profile_begin();
#endif
        break;
    default:
        lsassertm(0, "not implement %d.\n", opcode);
    }

#ifdef CONFIG_LATX_PROFILER
    la_profile_end();
#endif
}

IR2_OPND tr_lat_spin_lock_bd(IR2_OPND mem_addr, int imm)
{
    IR2_OPND label_lat_lock = ra_alloc_label();
    IR2_OPND label_locked= ra_alloc_label();
    IR2_OPND lat_lock_addr = ra_alloc_itemp();
    IR2_OPND lat_lock_val= ra_alloc_itemp();
    IR2_OPND cpu_index = ra_alloc_itemp();
    /*compute lat_lock offset by add (mem_addr+imm)[9:6]*/
    la_addi_w(lat_lock_addr, mem_addr, imm);
    la_bstrpick_d(lat_lock_val, lat_lock_addr, 9, 6);
    la_slli_w(lat_lock_val, lat_lock_val, 6);

    TranslationBlock *tb __attribute__((unused)) = NULL;
    if (option_aot) {
        tb = (TranslationBlock *)lsenv->tr_data->curr_tb;
    }
    aot_load_host_addr(lat_lock_addr, (ADDR)lat_lock,
        LOAD_HOST_LATLOCK, 0);
    la_add_d(lat_lock_addr, lat_lock_addr, lat_lock_val);

    la_ld_w(cpu_index, env_ir2_opnd,
                      lsenv_offset_of_cpu_index(lsenv));
    la_addi_w(cpu_index, cpu_index, 1);
    //spin lock
    la_label(label_lat_lock);
    la_ll_w(lat_lock_val, lat_lock_addr, 0);
    la_bne(lat_lock_val, zero_ir2_opnd, label_locked);
    la_or(lat_lock_val, lat_lock_val, cpu_index);
    la_label(label_locked);
    la_bne(lat_lock_val, cpu_index, label_lat_lock);
    la_sc_w(lat_lock_val, lat_lock_addr, 0);
    la_beq(lat_lock_val, zero_ir2_opnd, label_lat_lock);

    ra_free_temp(cpu_index);
    ra_free_temp(lat_lock_val);

	return lat_lock_addr;
}