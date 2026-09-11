// SPDX-License-Identifier: GPL-2.0
//
// vim: set ts=8 sw=8 noet tw=80 cc=80 fo+=t :

/**
 * @file      debug.c
 * @author    midnight walker
 * @brief     Log-level management helpers.
 * @version   0.1
 * @date      2026-08-29
 *
 * @details   This file exposes functions to query, set, enable, and disable
 *            log levels using a bitmask.
 *
 * @copyright GNU General Public License v2.0
 */

#include "debug.h"

static unsigned int dbg_loglevel =
    (DBG_LOGLEVEL_FATAL | DBG_LOGLEVEL_WARN | DBG_LOGLEVEL_ERROR |
     DBG_LOGLEVEL_INFO | DBG_LOGLEVEL_DEBUG);

unsigned int get_debug_loglevel(void)
{
    return dbg_loglevel;
}

unsigned int set_debug_loglevel(const unsigned int newlevel)
{
    dbg_loglevel = newlevel;
    return dbg_loglevel;
}

unsigned int disable_debug_loglevel(const unsigned int level)
{
    dbg_loglevel &= ~level;
    return dbg_loglevel;
}

unsigned int enable_debug_loglevel(const unsigned int level)
{
    dbg_loglevel |= level;
    return dbg_loglevel;
}