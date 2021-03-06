/*
 * This file is part of libbluray
 * Copyright (C) 2010  hpi1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#if !defined(_GRAPHICS_CONTROLLER_H_)
#define _GRAPHICS_CONTROLLER_H_

#include <util/attributes.h>

#include <stdint.h>

/*
 *
 */

struct bd_registers_s;
struct bd_overlay_s;

/*
 * types
 */

typedef struct graphics_controller_s GRAPHICS_CONTROLLER;

typedef void (*gc_overlay_proc_f)(void *, const struct bd_overlay_s * const);

typedef enum {
    GC_CTRL_NOP,
    GC_CTRL_VK_KEY,          /* param: bd_vk_key_e */
    GC_CTRL_ENABLE_BUTTON,   /* param: button_id */
    GC_CTRL_DISABLE_BUTTON,  /* param: button_id */
    GC_CTRL_SET_BUTTON_PAGE,
    GC_CTRL_POPUP,           /* param: on/off */
    GC_CTRL_IG_END,          /* execution of IG object is complete */
    GC_CTRL_RESET,           /* reset graphics controller */
    GC_CTRL_MOUSE_MOVE,      /* move selected button to (x,y), param: (x<<16 | y) */
} gc_ctrl_e;

typedef struct {
    /* HDMV navigation command sequence */
    int   num_nav_cmds;
    void *nav_cmds;

    /* Sound idx */
    int   sound_id_ref;
} GC_NAV_CMDS;

/*
 * init / free
 */

BD_PRIVATE GRAPHICS_CONTROLLER *gc_init(struct bd_registers_s *regs,
                                        void *handle, gc_overlay_proc_f func);

BD_PRIVATE void                 gc_free(GRAPHICS_CONTROLLER **p);

/*
 * input stream (MPEG-TS IG stream)
 */

BD_PRIVATE void                 gc_decode_ts(GRAPHICS_CONTROLLER *p,
                                             uint16_t pid,
                                             uint8_t *block, unsigned num_blocks,
                                             int64_t stc);

/*
 * run graphics controller
 */

BD_PRIVATE int                  gc_run(GRAPHICS_CONTROLLER *p,
                                       /* in */  gc_ctrl_e msg, uint32_t param,
                                       /* out */ GC_NAV_CMDS *cmds);


#endif // _GRAPHICS_CONTROLLER_H_
