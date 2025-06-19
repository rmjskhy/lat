#include "common.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"
#include "translate-bd.h"
#include "lsenv.h"
#include "accel/tcg/internal.h"
#include "ir1-optimization.h"
#include "latx-config.h"
#include "tu.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "aot_page.h"





#if defined(CONFIG_LATX_TU) || defined(CONFIG_LATX_AOT)
void get_last_info_bd(TranslationBlock *tb, IR1_INST* pir1)
{
    if (tb->icount == 0) {
        return;
    }
    tb->s_data->next_pc = ir1_addr_next_bd(pir1);

    if (ir1_is_branch_bd(pir1)) {
        if (likely(ir1_opnd_is_offs_bd(ir1_get_opnd_bd(pir1, 0)))) {
            tb->s_data->last_ir1_type = (int8)IR1_TYPE_BRANCH;
            tb->s_data->target_pc = ir1_target_addr_bd(pir1);
        } else {
            assert(0);
        }
    }
    else if (ir1_is_call_bd(pir1) && !ir1_is_indirect_call_bd(pir1)) {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_CALL;
        tb->s_data->target_pc = ir1_target_addr_bd(pir1);
    }
    else if (ir1_is_jump_bd(pir1) && !ir1_is_indirect_jmp_bd(pir1)) {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_JUMP;
        tb->s_data->target_pc = ir1_target_addr_bd(pir1);
    }
    else if (ir1_is_return_bd(pir1)) {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_RET;
    }
    else if (ir1_is_call_bd(pir1) &&
        ir1_is_indirect_call_bd(pir1)) {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_CALLIN;
    } else if (ir1_is_jump_bd(pir1) &&
        ir1_is_indirect_jmp_bd(pir1)) {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_JUMPIN;
    } else {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_NORMAL;
    }
    return;
}
#endif