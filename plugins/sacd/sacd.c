#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "sacd.h"


static DB_decoder_t plugin;
static DB_functions_t *deadbeef;



static int is_sacd(const char* path) {
    FILE* fp;
    char sacdmtoc[8];
    int retval = 0;
    
    while (1) {
        fp = fopen(path, "r");
        if (fp == NULL) {
            retval = 0;
            break;
        }
        fseek(fp, START_OF_MASTER_TOC * SACD_LSN_SIZE, SEEK_SET);
        if (fread(sacdmtoc, 1, 8, fp) == 8) {
            if (memcmp(sacdmtoc, SACD_MASTER_TOC_ID, 8) == 0) {
                retval = 1;
                break;
            }
        }
        fseek(fp, START_OF_MASTER_TOC * SACD_PSN_SIZE + 12, SEEK_SET);
        if (fread(sacdmtoc, 1, 8, fp) == 8) {
            if (memcmp(sacdmtoc, SACD_MASTER_TOC_ID, 8) == 0) {
                retval = 1;
                break;
            }
        }
        break;
    }
    fseek(fp, 0, SEEK_SET);
    return retval;
}

static DB_fileinfo_t *
sacd_open (uint32_t hints)
{
    sacd_info_t *info = calloc(1, sizeof (sacd_info_t));
    if (info) {
        info->hints = hints;
        info->info.plugin = &plugin;
        info->info.fmt.bps = 32;
        info->info.fmt.channels = 2;
        info->info.fmt.samplerate = 176400;
        info->current_path = malloc(PATH_MAX);
    }
    return (DB_fileinfo_t *)info;
}

static int
sacd_init(DB_fileinfo_t *_info, DB_playItem_t *it)
{
    char plt_buffer[64];
    sacd_info_t* info = (sacd_info_t*)_info;

    deadbeef->pl_lock();
    memset(plt_buffer, 0, 64);
    deadbeef->pl_get_meta(it, "start_lsn", plt_buffer, 64);
    info->current_start_lsn = atoi(plt_buffer);
    memset(plt_buffer, 0,  64);
    deadbeef->pl_get_meta(it, "length_lsn", plt_buffer, 64);
    info->current_length_lsn = atoi(plt_buffer);
    memset(plt_buffer, 0,  64);
    deadbeef->pl_get_meta(it, "duration", plt_buffer, 64);
    info->current_duration = atoi(plt_buffer);
    memset(info->current_path, 0, PATH_MAX);
    deadbeef->pl_get_meta(it, "path", info->current_path, PATH_MAX);
    deadbeef->pl_unlock();

    info->current_seek = 0;

    return 0;
}

static int
sacd_read(DB_fileinfo_t *_info, char *bytes, int size)
{
    sacd_info_t* info = (sacd_info_t*)_info;
    FILE* fp;
    int count;
    
    fp = fopen(info->current_path, "r");
    if (fp == NULL) {
        return 0;
    }

    fseek(fp, info->current_start_lsn * SACD_LSN_SIZE + info->current_seek, SEEK_SET);

    if (info->current_length_lsn * SACD_LSN_SIZE > size) {
        count = fread(bytes, size, 1, fp);
        info->current_seek = count;
    } else {
        count = fread(bytes, info->current_length_lsn * SACD_LSN_SIZE, 1, fp);
        info->current_seek = count;
    }
    return info->current_length_lsn * SACD_LSN_SIZE - (info->current_seek - info->current_start_lsn * SACD_LSN_SIZE);
}

static void
sacd_free(DB_fileinfo_t *_info)
{
    sacd_info_t* info = (sacd_info_t*)_info;
    if (info) {
        if (info->current_path) {
            free(info->current_path);
        }
        free(info);
    }
}

static int
sacd_seek(DB_fileinfo_t *_info, float sec) {
    sacd_info_t* info = (sacd_info_t*)_info;
    float percentage = 0.0;
    float total = (float)(info->current_length_lsn * SACD_LSN_SIZE);

    percentage = sec / (float)info->current_duration;

    info->current_seek = (int)(total * percentage);

    return 0;
}

#define MAX_TRACK_COUNT 32
static DB_playItem_t *
sacd_insert_area(ddb_playlist_t *plt, DB_playItem_t *after, const char *path, int area_offset, int area_toc_size)
{
    FILE* fp;
    uint8_t buffer[SACD_LSN_SIZE];
    uint8_t* area_data;
    area_toc_t* area_toc;
    area_tracklist_offset_t* tracklist;
    area_tracklist_time_t* tracklist_time;
    track_info_t track_info[MAX_TRACK_COUNT];
    
    area_data = (uint8_t*)malloc(area_toc_size * SACD_LSN_SIZE);
    if (area_data == NULL) {
        return NULL;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        return NULL;
    }

    fseek(fp, area_offset, SEEK_SET);
    if (fread((void*)&area_data, area_toc_size, 1, fp) != area_toc_size) {
        return NULL;
    }
    area_toc = (area_toc_t*)area_data;
    SWAP16(area_toc->size);
    for (int offset = area_offset + SACD_LSN_SIZE; offset < area_toc->size * SACD_LSN_SIZE; offset += SACD_LSN_SIZE) {
        fseek(fp, offset, SEEK_SET);
        memset(buffer, 0, SACD_LSN_SIZE);
        fread(buffer, SACD_LSN_SIZE, 1, fp);
        if (strncmp((const char*)buffer, "SACDTRL1", 8) == 0) {
            tracklist = (area_tracklist_offset_t*)buffer;
            for (int i = 0; i < area_toc->track_count; i++) {
                track_info[i].start_lsn = tracklist->track_start_lsn[i];
                track_info[i].length_lsn = tracklist->track_length_lsn[i];
            }
        }
        if (strncmp((const char*)buffer, "SACDTRL2", 8) == 0) {
            tracklist_time = (area_tracklist_time_t*)buffer;
            for (int i = 0; i < area_toc->track_count; i++) {
                track_info->duration = tracklist_time->duration->minutes * 60 + tracklist_time->duration->seconds;
            }
        }
    }
    free(area_data);
    area_data = NULL;
    area_toc = NULL;

    for (int i = 0; i < area_toc->track_count; i++) {
        // Add track info
        DB_playItem_t *it = deadbeef->pl_item_alloc_init(path, plugin.plugin.id);
        if (it) {
            deadbeef->pl_add_meta(it, ":FILETYPE", "sacd");
            deadbeef->plt_set_item_duration(plt, it, track_info->duration);

            char track[4];
            snprintf(track, sizeof (track), "%02d", deadbeef->plt_get_item_idx(plt, after, 0) + 1);
            deadbeef->pl_add_meta(it, "track", track);

            char temp[10];
            snprintf(temp, sizeof (temp), "%08x", track_info->start_lsn);
            deadbeef->pl_add_meta(it, "start_lsn", temp);
            snprintf(temp, sizeof (temp), "%08x", track_info->length_lsn);
            deadbeef->pl_add_meta(it, "length_lsn", temp);

            deadbeef->pl_add_meta(it, "path", path);

            it = deadbeef->plt_insert_item(plt, after, it);
            after = it;
        }
    }

    return after;
}

static DB_playItem_t *
sacd_insert(ddb_playlist_t *plt, DB_playItem_t *after, const char *path)
{
    FILE* fp;
    master_toc_t master_toc;

    // Verify iso is a sacd image
    if (!is_sacd(path)) {
        return NULL;
    }
    // Open File
    fp = fopen(path, "r");

    // Read master toc
    fseek(fp, START_OF_MASTER_TOC, SEEK_SET);
    if (fread((void*)&master_toc, sizeof(master_toc_t), 1, fp) != sizeof(master_toc_t)) {
        return NULL;
    }

    // Read Area 1
    if (master_toc.area_1_toc_1_start) {
        after = sacd_insert_area(plt, after, path, master_toc.area_1_toc_1_start, master_toc.area_1_toc_size);
        if (!after) {
            return NULL;
        }
    }
    // Read Area 2
    if (master_toc.area_2_toc_1_start) {
        after = sacd_insert_area(plt, after, path, master_toc.area_2_toc_1_start, master_toc.area_2_toc_size);
        if (!after) {
            return NULL;
        }
    }
    return after;
}


static DB_decoder_t plugin = {
    DDB_PLUGIN_SET_API_VERSION
    .plugin.version_major = 1,
    .plugin.version_minor = 0,
    .plugin.type = DB_PLUGIN_DECODER,
    .plugin.id = "sacd",
    .plugin.name = "Super Audio CD player",
    .plugin.descr = "Super Audio CD  ISO player",
    .plugin.copyright =
        "Copyright (C) 2023 Textar Pu <puwenxing@gmail.com>\n"
        "\n"
        "This program is free software; you can redistribute it and/or\n"
        "modify it under the terms of the GNU General Public License\n"
        "as published by the Free Software Foundation; either version 2\n"
        "of the License, or (at your option) any later version.\n"
        "\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program; if not, write to the Free Software\n"
        "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.\n"
    ,
    .plugin.website = "https://github.com/cqpwx/deadbeef",
    .open = sacd_open,
    .init = sacd_init,
    .free = sacd_free,
    .read = sacd_read,
    .seek = sacd_seek,
    .insert = sacd_insert,
    .exts = (char const *[]){"iso", NULL}
};

DB_plugin_t * sacd_load (DB_functions_t *api) {
    deadbeef = api;
    return DB_PLUGIN (&plugin);
}