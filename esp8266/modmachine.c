/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2015 Damien P. George
 * Copyright (c) 2016 Paul Sokolovsky
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>

#include "py/obj.h"
#include "py/runtime.h"
#include "extmod/machine_mem.h"
#include "utils.h"
#include "modpyb.h"

#include "os_type.h"
#include "osapi.h"
#include "etshal.h"
#include "user_interface.h"

#if MICROPY_PY_MACHINE

STATIC mp_obj_t machine_freq(mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        // get
        return mp_obj_new_int(system_get_cpu_freq() * 1000000);
    } else {
        // set
        mp_int_t freq = mp_obj_get_int(args[0]) / 1000000;
        if (freq != 80 && freq != 160) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError,
                    "frequency can only be either 80Mhz or 160MHz"));
        }
        system_update_cpu_freq(freq);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_freq_obj, 0, 1, machine_freq);

typedef struct _esp_timer_obj_t {
    mp_obj_base_t base;
    os_timer_t timer;
    mp_obj_t callback;
} esp_timer_obj_t;

const mp_obj_type_t esp_timer_type;

STATIC void esp_timer_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    esp_timer_obj_t *self = self_in;
    mp_printf(print, "Timer(%p)", &self->timer);
}

STATIC mp_obj_t esp_timer_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);
    esp_timer_obj_t *tim = m_new_obj(esp_timer_obj_t);
    tim->base.type = &esp_timer_type;
    return tim;
}

STATIC void esp_timer_cb(void *arg) {
    esp_timer_obj_t *self = arg;
    call_function_1_protected(self->callback, self);
}

STATIC mp_obj_t esp_timer_init_helper(esp_timer_obj_t *self, mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
//        { MP_QSTR_freq,         MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_period,       MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0xffffffff} },
        { MP_QSTR_mode,         MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 1} },
        { MP_QSTR_callback,     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    self->callback = args[2].u_obj;
    // Be sure to disarm timer before making any changes
    os_timer_disarm(&self->timer);
    os_timer_setfn(&self->timer, esp_timer_cb, self);
    os_timer_arm(&self->timer, args[0].u_int, args[1].u_int);

    return mp_const_none;
}

STATIC mp_obj_t esp_timer_init(mp_uint_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    return esp_timer_init_helper(args[0], n_args - 1, args + 1, kw_args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(esp_timer_init_obj, 1, esp_timer_init);

STATIC mp_obj_t esp_timer_deinit(mp_obj_t self_in) {
    esp_timer_obj_t *self = self_in;
    os_timer_disarm(&self->timer);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp_timer_deinit_obj, esp_timer_deinit);

STATIC const mp_map_elem_t esp_timer_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit), (mp_obj_t)&esp_timer_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_init), (mp_obj_t)&esp_timer_init_obj },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_callback), (mp_obj_t)&esp_timer_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ONE_SHOT), MP_OBJ_NEW_SMALL_INT(false) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PERIODIC), MP_OBJ_NEW_SMALL_INT(true) },
};
STATIC MP_DEFINE_CONST_DICT(esp_timer_locals_dict, esp_timer_locals_dict_table);

const mp_obj_type_t esp_timer_type = {
    { &mp_type_type },
    .name = MP_QSTR_Timer,
    .print = esp_timer_print,
    .make_new = esp_timer_make_new,
    .locals_dict = (mp_obj_t)&esp_timer_locals_dict,
};

STATIC const mp_rom_map_elem_t machine_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_umachine) },
    { MP_ROM_QSTR(MP_QSTR_mem8), MP_ROM_PTR(&machine_mem8_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem16), MP_ROM_PTR(&machine_mem16_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem32), MP_ROM_PTR(&machine_mem32_obj) },

    { MP_ROM_QSTR(MP_QSTR_freq), MP_ROM_PTR(&machine_freq_obj) },

    { MP_ROM_QSTR(MP_QSTR_Timer), MP_ROM_PTR(&esp_timer_type) },
    { MP_ROM_QSTR(MP_QSTR_Pin), MP_ROM_PTR(&pyb_pin_type) },
};

STATIC MP_DEFINE_CONST_DICT(machine_module_globals, machine_module_globals_table);

const mp_obj_module_t mp_module_machine = {
    .base = { &mp_type_module },
    .name = MP_QSTR_umachine,
    .globals = (mp_obj_dict_t*)&machine_module_globals,
};

#endif // MICROPY_PY_MACHINE
