#ifndef SACD_H
#define SACD_H

#include "../../deadbeef.h"

#define START_OF_MASTER_TOC 510
#define SACD_LSN_SIZE 2048
#define SACD_PSN_SIZE 2064
#define SACD_MASTER_TOC_ID "SACDMTOC"
#define SACD_MASTER_TEXT_ID "SACDText"
#define SACD_MASTER_MAN_ID "SACD_Man"
#define MAX_LANGUAGE_COUNT 8
#define SUPPORTED_VERSION_MAJOR 1
#define SUPPORTED_VERSION_MINOR 20
#define SACD_MAX_AERA_COUNT 3

typedef struct
{
    DB_fileinfo_t info;
    uint32_t hints;
    int current_start_lsn;
    int current_length_lsn;
    int current_seek;
    int current_duration;
    char* current_path;
} sacd_info_t;

typedef struct
{
    uint8_t category;  // category_t
    uint16_t reserved;
    uint8_t genre; // genre_t
}
genre_table_t;

typedef struct
{
  char language_code[2]; // ISO639-2 Language code
  uint8_t character_set; // char_set_t, 1 (ISO 646)
  uint8_t reserved;
} locale_table_t;

typedef struct
{
    char id[8]; // SACDMTOC

    struct
    {
        uint8_t major;
        uint8_t minor;
    } version; // 1.20 / 0x0114

    uint8_t reserved01[6];
    uint16_t album_set_size;
    uint16_t album_sequence_number;
    uint8_t reserved02[4];
    char album_catalog_number[16]; // 0x00 when empty, else padded with spaces for short strings
    genre_table_t album_genre[4];
    uint8_t reserved03[8];
    uint32_t area_1_toc_1_start;
    uint32_t area_1_toc_2_start;
    uint32_t area_2_toc_1_start;
    uint32_t area_2_toc_2_start;
    uint8_t disc_type_reserved   : 7;
    uint8_t disc_type_hybrid     : 1;
    uint8_t reserved04[3];
    uint16_t area_1_toc_size;
    uint16_t area_2_toc_size;
    char disc_catalog_number[16]; // 0x00 when empty, else padded with spaces for short strings
    genre_table_t disc_genre[4];
    uint16_t disc_date_year;
    uint8_t disc_date_month;
    uint8_t disc_date_day;
    uint8_t reserved05[4];
    uint8_t text_area_count;
    uint8_t reserved06[7];
    locale_table_t locales[MAX_LANGUAGE_COUNT];
} master_toc_t;

typedef struct
{
    char id[8]; // SACD_Man, manufacturer information
    uint8_t information[2040];
} master_man_t;

typedef struct
{
    char id[8]; // SACDText
    uint8_t reserved[8];
    uint16_t album_title_position;
    uint16_t album_artist_position;
    uint16_t album_publisher_position;
    uint16_t album_copyright_position;
    uint16_t album_title_phonetic_position;
    uint16_t album_artist_phonetic_position;
    uint16_t album_publisher_phonetic_position;
    uint16_t album_copyright_phonetic_position;
    uint16_t disc_title_position;
    uint16_t disc_artist_position;
    uint16_t disc_publisher_position;
    uint16_t disc_copyright_position;
    uint16_t disc_title_phonetic_position;
    uint16_t disc_artist_phonetic_position;
    uint16_t disc_publisher_phonetic_position;
    uint16_t disc_copyright_phonetic_position;
    uint8_t  data[2000];
} master_sacd_text_t;

typedef struct
{
    char id[8]; // TWOCHTOC or MULCHTOC

    struct
    {
        uint8_t major;
        uint8_t minor;
    } version; // 1.20 / 0x0114

    uint16_t size; // ex. 40 (total size of TOC)
    uint8_t reserved01[4];
    uint32_t max_byte_rate;
    uint8_t sample_frequency; // 0x04 = (64 * 44.1 kHz) (physically there can be no other values, or..? :)
    uint8_t frame_format : 4;
    uint8_t reserved02   : 4;
    uint8_t reserved03[10];
    uint8_t channel_count;
    uint8_t loudspeaker_config : 5;
    uint8_t extra_settings : 3;
    uint8_t max_available_channels;
    uint8_t area_mute_flags;
    uint8_t reserved04[12];
    uint8_t track_attribute : 4;
    uint8_t reserved05 : 4;
    uint8_t reserved06[15];

    struct
    {
        uint8_t minutes;
        uint8_t seconds;
        uint8_t frames;
    } total_playtime;

    uint8_t reserved07;
    uint8_t track_offset;
    uint8_t track_count;
    uint8_t reserved08[2];
    uint32_t track_start;
    uint32_t track_end;
    uint8_t text_area_count;
    uint8_t reserved09[7];
    locale_table_t languages[10];
    uint16_t track_text_offset;
    uint16_t index_list_offset;
    uint16_t access_list_offset;
    uint8_t reserved10[10];
    uint16_t area_description_offset;
    uint16_t copyright_offset;
    uint16_t area_description_phonetic_offset;
    uint16_t copyright_phonetic_offset;
    uint8_t data[1896];
} area_toc_t;

typedef struct
{
    char id[8]; // SACDTRL1
    uint32_t track_start_lsn[255];
    uint32_t track_length_lsn[255];
} area_tracklist_offset_t;

typedef struct
{
    uint8_t minutes;
    uint8_t seconds;
    uint8_t frames;
    uint8_t reserved : 5;
    uint8_t extra_use : 3;
} area_tracklist_time_start_t;

typedef struct
{
    uint8_t minutes;
    uint8_t seconds;
    uint8_t frames;
    uint8_t reserved : 3;
    uint8_t track_flags_tmf1 : 1;
    uint8_t track_flags_tmf2 : 1;
    uint8_t track_flags_tmf3 : 1;
    uint8_t track_flags_tmf4 : 1;
    uint8_t track_flags_ilp : 1;
} area_tracklist_time_duration_t;

typedef struct
{
    char id[8]; // SACDTRL2
    area_tracklist_time_start_t start[255];
    area_tracklist_time_duration_t duration[255];
} area_tracklist_time_t;



typedef struct
{
    int start_lsn;
    int length_lsn;
    int duration;
} track_info_t;


#define SWAP16(d) (((d) & 0xff) << 8 | (((d) >> 8) & 0xff))

#endif