/*
// Program:  Free FDISK
// Module:   PDISKIO.C
// Based on code by Brian E. Reifsnyder
// Copyright: 1998-2008, under the terms of the GNU GPL, Version 2
//
// Modified and adapted for Windows by: Andrey Hlus, 2025
*/


#define PDISKIO
#define _CRT_SECURE_NO_WARNINGS        // чтобы не ругался на deprecation стандартных strcpy, scanf и тд.

#include <stdint.h>
#include <tchar.h>
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "ansicon.h"
#include "printf.h"
#include "pdiskio.h"


#define MAX_RETRIES 3

unsigned char sector_buffer[SECT_SIZE];
int brief_partition_table[MAX_DISKS][27];
TCHAR drive_lettering_buffer[MAX_DISKS][27];
Partition_Table part_table[MAX_DISKS];

static unsigned char disk_address_packet[16];
static unsigned char result_buffer[26];
extern char bootnormal_code[];

static int Get_Hard_Drive_Parameters(int physical_drive);
static int Read_Physical_Sectors_LBA_only(int drive, long long LBA, int number_of_sectors);
static void Clear_Partition_Table_Area_Of_Sector_Buffer(void);

/*void Error_Handler( int error );*/
static void Get_Partition_Information(void);

static void StorePartitionInSectorBuffer(unsigned char* sect_buffer,
                                         struct Partition* pPart);

static void Load_Brief_Partition_Table(void);

static int Read_Primary_Table(int drive, Partition_Table* pDrive,
                              int* num_ext);
static int Read_Extended_Table(int drive, Partition_Table* pDrive);
static void Clear_Partition_Tables(Partition_Table* pDrive);
static int Clear_Boot_Sector(int drive, unsigned long cylinder,
                             unsigned long head, unsigned long sector);
static unsigned long combine_cs(unsigned long cylinder,
                                unsigned long sector);
static void extract_chs(unsigned char* p, unsigned long* cyl,
                        unsigned long* head, unsigned long* sect);

extern void Clear_Screen(int type);
/*  extern void Convert_Long_To_Integer(long number);*/
extern void Pause(void);
extern void Print_Centered(int y, char* text, int style);
extern void Position_Cursor(int row, int column);


static void CopySddString(const STORAGE_DEVICE_DESCRIPTOR* sdd, DWORD offset, TCHAR* dest, size_t destSize);


int os_version = 0;
int os_oem = 0;


////////////////////////////////////////////////////////////

char bootnormal_code[512];


void pdiskio_init(void)
{
    for (int i = 0; i < MAX_DISKS; i++)
    {
        user_defined_chs_settings[i].defined = FALSE;
    }
    flags.drive_number = 128;
    flags.version = COMP_W98;
    flags.dla = DLA_MSDOS;
    flags.total_number_hard_disks = 255;
    flags.verbose = TRUE;
    flags.lba_marker = TRUE;
}

//////////////////////////////////////////////////////////


void Determine_DOS_Version(void)
{
    os_version = OS_WIN_ME;
    os_oem = 0xff; // MS-DOS
}

int Is_Ext_Part(int num_type)
{
    return num_type == 5 || num_type == 0x0f;
}

int Is_Dos_Part(int num_type)
{
    return num_type == 0x01 || num_type == 0x04 || num_type == 0x06 ||
        num_type == 0x0b || num_type == 0x0c || num_type == 0x0e;
}

int Is_Supp_Ext_Part(int num_type)
{
    return (num_type == 5) || (num_type == 0x0f && (flags.version >= COMP_W95));
}

int Is_Pri_Tbl_Empty(void)
{
    int index;
    Partition_Table* pDrive = &part_table[flags.drive_number - 0x80];

    for (index = 0; index < 4; index++)
    {
        if (pDrive->pri_part[index].num_type != 0)
        {
            return 0;
        }
    }

    return 1;
}

int IsRecognizedFatPartition(unsigned partitiontype)
{
    switch (partitiontype)
    {
    case 1:
    case 4:
    case 6:
        return TRUE;
    case 0x0e:
        if (flags.version >= COMP_W95)
        {
            return TRUE;
        }
        break;
    case 0x0b:
    case 0x0c:
        if (flags.version >= COMP_W95B)
        {
            return TRUE;
        }
        break;
    default: ;
    }
    return FALSE;
}

int Lock_Unlock_Drive(int drive_num, int lock)
{
    return 1;
}

/* Clear the Boot Sector of a partition */
int Clear_Boot_Sector(int drive, unsigned long cylinder, unsigned long head, unsigned long sector)
{
    unsigned char stored_sector_buffer[SECT_SIZE];

    /* Save sector_buffer[512] into stored_sector_buffer[512] */
    memcpy(stored_sector_buffer, sector_buffer, SECT_SIZE);

    /* Write all 0xf6 values to sector_buffer[index] */
    memset(sector_buffer, 0xf6, SECT_SIZE);

    for (long index = 0; index < 16; index++)
    {
        int error_code = Write_Physical_Sectors(drive, cylinder, head, sector + index, 1);

        if (error_code != 0)
        {
            return error_code;
        }
    }

    /* Restore sector_buffer[512] to its original contents */
    memcpy(sector_buffer, stored_sector_buffer, SECT_SIZE);

    return 0;
}

/* Clear The Partition Table Area Of sector_buffer only. */
static void Clear_Partition_Table_Area_Of_Sector_Buffer(void)
{
    memset(sector_buffer + 0x1be, 0, 4 * 16);
}

/* Clear Sector Buffer */
void Clear_Sector_Buffer(void)
{
    memset(sector_buffer, 0, SECT_SIZE);
}

/* Combine Cylinder and Sector Values */
static unsigned long combine_cs(unsigned long cylinder, unsigned long sector)
{
    unsigned short c = cylinder;
    unsigned short s = sector;

    return (s & 0x3f) | ((c & 0x0300) >> 2) | ((c & 0xff) << 8);
}

int Num_Ext_Part(Partition_Table* pDrive)
{
    int index;
    int num_ext = 0;
    for (index = 0; index < 4; index++)
    {
        if (Is_Supp_Ext_Part(pDrive->pri_part[index].num_type))
        {
            num_ext++;
        }
    }
    return num_ext;
}

void dla_msdos(int* cl);
void dla_drdos(int* cl);
void dla_nondos(void);

/* Determine drive letters */
int Determine_Drive_Letters(void)
/* Returns last used drive letter as ASCII number. */
{
    int current_letter = _T('C');

    Load_Brief_Partition_Table();

    /* Clear drive_lettering_buffer[8] [27] */
    memset(drive_lettering_buffer, 0, sizeof(drive_lettering_buffer));

    if (flags.dla == DLA_MSDOS)
    {
        dla_msdos(&current_letter);
    }
    else
    {
        /* DR-DOS drive letter assignment puts all primaries of all drives
           first, in order of partition table, then all logicals, drive by
           drive. */
        dla_drdos(&current_letter);
    }

    /* assign numbers to non-DOS partitions */
    dla_nondos();

    return current_letter - 1;
}


void dla_msdos(int* cl)
{
    int current_letter = *cl;
    int index = 0;
    int sub_index = 0;
    int active_part_found[MAX_DISKS] = {0};

    /* assign up to one drive letter to an active or non-active partition
       per disk */
    for (index = 0; index < MAX_DISKS; index++)
    {
        const Partition_Table* pDrive = &part_table[index];
        if (!pDrive->usable)
        {
            continue;
        }

        /* find active partition for drive */
        for (sub_index = 0; (sub_index < 4) && (current_letter <= _T('Z')); sub_index++)
        {
            if ((IsRecognizedFatPartition(brief_partition_table[index][sub_index]))
                && (pDrive->pri_part[sub_index].active_status == 0x80))
            {
                drive_lettering_buffer[index][sub_index] = (TCHAR)current_letter;
                active_part_found[index] = 1;
                current_letter++;
                break;
            }
        }

        /* no active partition, try to find one non-active primary */
        if (!active_part_found[index])
        {
            for (sub_index = 0; (sub_index < 4) && (current_letter <= 'Z'); sub_index++)
            {
                if (drive_lettering_buffer[index][sub_index] == 0 && IsRecognizedFatPartition(
                    brief_partition_table[index][sub_index]))
                {
                    drive_lettering_buffer[index][sub_index] = (TCHAR)current_letter;
                    current_letter++;
                    break;
                }
            }
        }
    }

    /* Next assign drive letters to applicable extended partitions... */
    for (index = 0; index < MAX_DISKS; index++)
    {
        Partition_Table* pDrive = &part_table[index];
        if (!pDrive->usable)
        {
            continue;
        }

        for (sub_index = 4; (sub_index < 27) && (current_letter <= _T('Z')); sub_index++)
        {
            if (IsRecognizedFatPartition(brief_partition_table[index][sub_index]))
            {
                drive_lettering_buffer[index][sub_index] = (TCHAR)current_letter;
                current_letter++;
            }
        }
    }

    /* Return to the primary partitions to assign drive letters to the
       remaining primary partitions */
    for (index = 0; index < MAX_DISKS; index++)
    {
        Partition_Table* pDrive = &part_table[index];
        if (!pDrive->usable)
        {
            continue;
        }

        for (sub_index = 0; (sub_index < 4) && (current_letter <= _T('Z')); sub_index++)
        {
            if (drive_lettering_buffer[index][sub_index] == 0)
            {
                if (IsRecognizedFatPartition(
                    brief_partition_table[index][sub_index]))
                {
                    drive_lettering_buffer[index][sub_index] = (TCHAR)current_letter;
                    current_letter++;
                }
            }
        }
    }
    *cl = current_letter;
}


void dla_drdos(int* cl)
{
    int current_letter = *cl;
    int index = 0;
    int sub_index = 0;

    /* assign up to one drive letter to an active or non-active partition
       per disk */
    for (index = 0; index < MAX_DISKS; index++)
    {
        Partition_Table* pDrive = &part_table[index];
        if (!pDrive->usable)
        {
            continue;
        }

        /* find active partition for drive */
        for (sub_index = 0; (sub_index < 4) && (current_letter <= _T('Z')); sub_index++)
        {
            if ((IsRecognizedFatPartition(
                brief_partition_table[index][sub_index])))
            {
                drive_lettering_buffer[index][sub_index] = (TCHAR)current_letter;
                current_letter++;
            }
        }
    }

    /* Next assign drive letters to applicable extended partitions... */
    for (index = 0; index < MAX_DISKS; index++)
    {
        Partition_Table* pDrive = &part_table[index];
        if (!pDrive->usable)
        {
            continue;
        }

        for (sub_index = 4; (sub_index < 27) && (current_letter <= _T('Z')); sub_index++)
        {
            if (IsRecognizedFatPartition(
                brief_partition_table[index][sub_index]))
            {
                drive_lettering_buffer[index][sub_index] = (TCHAR)current_letter;
                current_letter++;
            }
        }
    }

    *cl = current_letter;
}


void dla_nondos(void)
{
    int index = 0;
    int sub_index = 0;

    /* Find the Non-DOS Logical Drives in the Extended Partition Table */
    for (index = 0; index < MAX_DISKS; index++)
    {
        Partition_Table* pDrive = &part_table[index];
        if (!pDrive->usable)
        {
            continue;
        }

        pDrive->num_of_non_dos_log_drives = 0;
        int non_dos_partition_counter = '1';

        for (sub_index = 4; sub_index < 27; sub_index++)
        {
            if (brief_partition_table[index][sub_index] > 0)
            {
                int non_dos_partition = TRUE;

                if (IsRecognizedFatPartition(brief_partition_table[index][sub_index]))
                {
                    non_dos_partition = FALSE;
                }

                if ((non_dos_partition == TRUE) && (non_dos_partition_counter <= _T('9')))
                {
                    drive_lettering_buffer[index][sub_index] = (TCHAR)non_dos_partition_counter;
                    pDrive->num_of_non_dos_log_drives++;
                    non_dos_partition_counter++;
                }
            }
        }
    }
}


void Clear_Partition(Partition* p)
{
    memset(p, 0, sizeof(Partition));
    strcpy(p->vol_label, "           ");
}

void Copy_Partition(Partition* dst, const Partition* src)
{
    dst->active_status = src->active_status;
    dst->num_type = src->num_type;
    dst->start_cyl = src->start_cyl;
    dst->start_head = src->start_head;
    dst->start_sect = src->start_sect;
    dst->end_cyl = src->end_cyl;
    dst->end_head = src->end_head;
    dst->end_sect = src->end_sect;
    dst->rel_sect = src->rel_sect;
    dst->num_sect = src->num_sect;
    dst->size_in_MB = src->size_in_MB;
    strcpy(dst->vol_label, src->vol_label);
}

void lba_to_chs(const unsigned long lba_value, const Partition_Table* pDrive,
                unsigned long* cyl, unsigned long* head,
                unsigned long* sect)
{
    *sect = lba_value % pDrive->total_sect + 1;
    *head = lba_value / pDrive->total_sect % (pDrive->total_head + 1);
    *cyl = lba_value / ((pDrive->total_head + 1) * pDrive->total_sect);
}

void extract_chs(unsigned char* p, unsigned long* cyl, unsigned long* head, unsigned long* sect)
{
    const unsigned short cs = *(unsigned short*)(p + 1);

    if (cyl)
    {
        *cyl = (cs >> 8) | ((cs << 2) & 0x0300u);
    }
    if (head)
    {
        *head = *p;
    }
    if (sect)
    {
        *sect = cs & 0x3f;
    }
}

/**
 * Подсчет доступных физических дисков в системе.
 * @return Кол-во дисков, или 0.
 */
static int Get_Hard_Drives()
{
    int total_number_hard_disks = 0;
    const int MAX_ENUM_DRIVES = 32;

    for (int i = 0; i < MAX_ENUM_DRIVES; ++i)
    {
        TCHAR devPath[64];
        if (_stprintf(devPath, _T("\\\\.\\PhysicalDrive%d"), i) < 0)
            continue;

        HANDLE hDisk = CreateFile(devPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0,
                                  NULL);
        if (hDisk != INVALID_HANDLE_VALUE)
        {
            total_number_hard_disks++;
            CloseHandle(hDisk);
        }
        else
        {
            DWORD lastError = GetLastError();
            // Если доступ запрещён или занят — всё равно считаем устройство "существующим", т.к. оно реально присутствует.
            if (lastError == ERROR_ACCESS_DENIED || lastError == ERROR_SHARING_VIOLATION || lastError ==
                ERROR_LOCK_VIOLATION)
            {
                total_number_hard_disks++;
            }
        }
    }
    return total_number_hard_disks;
}


/* Get the parameters of the hard disk */
static int Get_Hard_Drive_Parameters(int physical_drive)
{
    DWORD bytes = 0;

    if (flags.total_number_hard_disks == 255)
    {
        // узнаем кол-во доступных дисков
        int total_number_hard_disks = Get_Hard_Drives();
        if (total_number_hard_disks == 0)
            return 255;
        flags.total_number_hard_disks = total_number_hard_disks;
    }

    int drive = physical_drive - 0x80;
    if (drive < 0 || drive >= flags.total_number_hard_disks)
    {
        return 255;
    }

    Partition_Table* pDrive = &part_table[drive];

    // Инициализация полей (как в DOS-функции)
    pDrive->total_head = 0;
    pDrive->total_sect = 0;
    pDrive->total_cyl = 0;
    pDrive->size_truncated = FALSE;
    pDrive->num_of_log_drives = 0;
    memset(pDrive->product, _T(' '), sizeof(pDrive->product));
    if (_stprintf(pDrive->file_name, _T("\\\\.\\PhysicalDrive%d"), drive) < 0)
    {
        return 1;
    }

    if (user_defined_chs_settings[drive].defined == TRUE)
    {
        pDrive->total_cyl = user_defined_chs_settings[drive].total_cylinders;
        pDrive->total_head = user_defined_chs_settings[drive].total_heads;
        pDrive->total_sect = user_defined_chs_settings[drive].total_sectors;
        return 0;
    }

    // Открываем диск. Для чтения/записи требуется админ (или соответствующие привилегии).
    HANDLE hDevice = CreateFile(pDrive->file_name, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE)
    {
        DWORD le = GetLastError();
        if (le == ERROR_FILE_NOT_FOUND || le == ERROR_INVALID_DRIVE) return 255;
        return (int)le;
    }

    // Сначала пробуем современный (расширенный) ioctl
    DISK_GEOMETRY_EX geomEx;
    BOOL ok = DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geomEx, sizeof(geomEx), &bytes,
                              NULL);

    DISK_GEOMETRY baseGeom;
    LARGE_INTEGER diskBytes = {0};

    if (ok)
    {
        // Успех: используем geomEx.Geometry + geomEx.DiskSize
        baseGeom = geomEx.Geometry;
        diskBytes = geomEx.DiskSize;
    }
    else
    {
        // fallback: придется использовать старый IOCTL_DISK_GET_DRIVE_GEOMETRY
        bytes = 0;
        ok = DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &baseGeom, sizeof(baseGeom), &bytes,
                             NULL);
        if (!ok)
        {
            int error_code = 0;
            error_code = (int)GetLastError();
            CloseHandle(hDevice);
            return error_code;
        }
        // попробуем получить точный размер через IOCTL_DISK_GET_LENGTH_INFO
        GET_LENGTH_INFORMATION lenInfo;
        bytes = 0;
        ok = DeviceIoControl(hDevice, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &lenInfo, sizeof(lenInfo), &bytes, NULL);
        if (ok)
        {
            diskBytes = lenInfo.Length;
        }
        else
        {
            // Если и это не удалось, восстанавливаем размер по CHS (теряем точность размера и возможно обрежется)
            diskBytes.QuadPart = baseGeom.Cylinders.QuadPart * (LONGLONG)baseGeom.TracksPerCylinder * (LONGLONG)baseGeom
                .SectorsPerTrack * (LONGLONG)baseGeom.BytesPerSector;
        }
    }

    // Проверки «нормальности» геометрии
    if (baseGeom.TracksPerCylinder == 0 || baseGeom.SectorsPerTrack == 0)
    {
        // USB-эмулятор диска?
        CloseHandle(hDevice);
        return 255;
    }

    // Проверяем размер сектора с SECT_SIZE -> поддерживаем только диски с секторами в SECT_SIZE
    if (baseGeom.BytesPerSector != SECT_SIZE)
    {
        // Это наверное современные диск-терабайтник с сектором в 4Кб.
        CloseHandle(hDevice);
        return 1;
    }

    // disk size in sectors (64-bit)
    if (diskBytes.QuadPart <= 0)
    {
        CloseHandle(hDevice);
        return 255;
    }
    uint64_t total_sectors_64 = (uint64_t)(diskBytes.QuadPart / baseGeom.BytesPerSector);

    // Windows DISK_GEOMETRY: Cylinders == количество цилиндров, TracksPerCylinder == heads count, SectorsPerTrack == sectors per track
    // В нашей структуре total_head/total_cyl — это "максимальные индексы (zero-based)".
    pDrive->total_sect = baseGeom.SectorsPerTrack;
    pDrive->total_head = (baseGeom.TracksPerCylinder ? (baseGeom.TracksPerCylinder - 1) : 0);
    pDrive->total_cyl = (baseGeom.Cylinders.QuadPart ? (baseGeom.Cylinders.QuadPart - 1) : 0);

    // Если размер диска в секторах не умещается в 32 бит (т.е. больше 2Tb), то обрезаем:
    if (total_sectors_64 > 0xffffffffUL)
    {
        pDrive->size_truncated = TRUE;
        pDrive->disk_size_sect = 0xffffffffUL;
    } else {
        pDrive->disk_size_sect = total_sectors_64;
    }
    pDrive->total_cyl = pDrive->disk_size_sect / (pDrive->total_sect * (pDrive->total_head + 1)) - 1;


    // Получение имени устройства (модели диска)
    STORAGE_PROPERTY_QUERY query = {0};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    STORAGE_DESCRIPTOR_HEADER deschdr;
    BOOL ok2 = DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &deschdr, sizeof(deschdr),
                               &bytes, NULL);

    if (ok2 && deschdr.Size > 0)
    {
        STORAGE_DEVICE_DESCRIPTOR* sdd = malloc(deschdr.Size);
        if (sdd)
        {
            ok2 = DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), sdd, deschdr.Size, &bytes, NULL);
            if (ok2)
            {
                if (sdd->ProductIdOffset)
                {
                    CopySddString(sdd, sdd->ProductIdOffset, pDrive->product, sizeof(pDrive->product) / sizeof(TCHAR));
                } else {
                    _tcsncpy(pDrive->product, pDrive->file_name, sizeof(pDrive->product) / sizeof(TCHAR));
                }
            }
            free(sdd);
        }
    } else {
        _tcsncpy(pDrive->product, pDrive->file_name, sizeof(pDrive->product) / sizeof(TCHAR));
    }

    CloseHandle(hDevice);

    // const TCHAR* mediaTypeStr;
    // switch (baseGeom.MediaType) {
    //     case FixedMedia: mediaTypeStr = _T("Fixed hard disk"); break;
    //     case RemovableMedia: mediaTypeStr = _T("Removable media"); break;
    //     case Unknown: mediaTypeStr = _T("Unknown"); break;
    //     default: mediaTypeStr = _T("Other"); break;
    // }
    //
    // con_printf(_T("Media type: %s\n"), mediaTypeStr);
    // con_printf(_T("CHS geometry: C=%lld H=%lu S=%lu\n"),
    //            baseGeom.Cylinders.QuadPart,
    //            baseGeom.TracksPerCylinder,
    //            baseGeom.SectorsPerTrack);
    // con_printf(_T("Total sectors (LBA): %llu\n"), total_sectors_64);
    // con_printf(_T("Total sectors (CHS): %llu\n"), (baseGeom.Cylinders.QuadPart * (uint64_t)baseGeom.TracksPerCylinder * (uint64_t)baseGeom.SectorsPerTrack * (uint64_t)baseGeom.BytesPerSector) / baseGeom.BytesPerSector);
    // con_printf(_T("Bytes per sector: %lu\n"), baseGeom.BytesPerSector);
    //
    // con_printf(_T("Disk size (bytes): %llu\n"), diskBytes.QuadPart);
    // con_printf(_T("Disk size (MB): %lu\n"), pDrive->disk_size_mb);
    // con_printf(_T("Disk size (GB): %llu\n"), diskBytes.QuadPart / (1024ULL*1024ULL*1024ULL));
    // con_readkey();

    return 0;
}


static void extract_filename(const TCHAR *fullName, TCHAR *fName)
{
    TCHAR _drive[_MAX_DRIVE];
    TCHAR _dir[_MAX_DIR];
    TCHAR _fname[_MAX_FNAME];
    TCHAR _ext[_MAX_EXT];

    _tsplitpath(fullName, _drive, _dir, _fname, _ext);
    _tcscpy(fName, _fname);
    _tcscat(fName, _ext);
}

/* Get the parameters of the hard disk */
static int Get_Image_Drive_Parameters(Partition_Table* pDrive, TCHAR *fullPath, int cylinders, int heads, int sectors)
{
    WIN32_FIND_DATA fd;
    TCHAR _fname[MAX_PATH];

    // Проверяем корректность параметров и наличие файла
    if (cylinders > 1024 || heads > 255 || sectors > 63)
    {
        return 1; // неправильная геометрия
    }

    extract_filename(fullPath, _fname);

    if (_tcslen(fullPath) >= sizeof(pDrive->file_name) / sizeof(TCHAR))
    {
        return 254;
    }

    HANDLE hDevice = FindFirstFile(fullPath, &fd);
    if (hDevice == INVALID_HANDLE_VALUE)
    {
        DWORD le = GetLastError();
        if (le == ERROR_FILE_NOT_FOUND || le == ERROR_PATH_NOT_FOUND || le == ERROR_DIRECTORY)
            return 255;
        return (int)le;
    }
    uint64_t size = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
    FindClose(hDevice);

    // проверяем соответствие размеров
    uint64_t total_sectors_64 = cylinders * heads * sectors;
    if (total_sectors_64 > (size / SECT_SIZE))
    {
        return 2;   // файл меньше заданного размера по C/H/S
    }

    // Инициализация полей (как в DOS-функции)
    pDrive->total_head = 0;
    pDrive->total_sect = 0;
    pDrive->total_cyl = 0;
    pDrive->size_truncated = FALSE;
    pDrive->num_of_log_drives = 0;
    _tcsncpy(pDrive->product, _fname, sizeof(pDrive->product) / sizeof(TCHAR));
    pDrive->product[sizeof(pDrive->product) / sizeof(TCHAR) -1] = _T('\0');
    _tcsncpy(pDrive->file_name, fullPath, sizeof(pDrive->file_name) / sizeof(TCHAR));
    pDrive->file_name[sizeof(pDrive->file_name) / sizeof(TCHAR)-1] = _T('\0');

    // Заполняем поля Partition_Table, поддерживая семантику «zero-based max indices»
    pDrive->total_sect = sectors;
    pDrive->total_head = heads - 1;
    pDrive->total_cyl = cylinders - 1;

    // Если размер диска в секторах не умещается в 32 бит (т.е. больше 2Tb), то обрезаем:
    if (total_sectors_64 > 0xffffffffUL)
    {
        pDrive->size_truncated = TRUE;
        pDrive->disk_size_sect = 0xffffffffUL;
    } else {
        pDrive->disk_size_sect = total_sectors_64;
    }
    pDrive->total_cyl = pDrive->disk_size_sect / (pDrive->total_sect * (pDrive->total_head + 1)) - 1;

    return 0;
}


// return physical start of a logical partition
static unsigned long get_log_drive_start(Partition_Table* pDrive, int partnum)
{
    if (partnum == 0)
    {
        return pDrive->log_drive[partnum].rel_sect + pDrive->ptr_ext_part->rel_sect;
    }
    return pDrive->log_drive[partnum].rel_sect + pDrive->ptr_ext_part->rel_sect + pDrive->next_ext[partnum - 1].rel_sect;
}

/* Get the volume labels and file system types from the boot sectors */
static void Get_Partition_Information(void)
{
    int drivenum;
    int partnum;
    int label_offset;
    unsigned long lba_sect;

    /* First get the information from the primary partitions. */

    for (drivenum = 0; drivenum < MAX_DISKS; drivenum++)
    {
        Partition_Table* pDrive = &part_table[drivenum];

        for (partnum = 0; partnum < 4; partnum++)
        {
            strcpy(pDrive->pri_part[partnum].vol_label, "           ");

            /* Check for and get the volume label on a FAT partition. */
            if (IsRecognizedFatPartition(
                pDrive->pri_part[partnum].num_type))
            {
                if (pDrive->pri_part[partnum].num_type == 11 ||
                    pDrive->pri_part[partnum].num_type == 12)
                {
                    label_offset = 71;
                }
                else
                {
                    label_offset = 43;
                }

                Read_Physical_Sectors_LBA_only((drivenum + 128), pDrive->pri_part[partnum].rel_sect, 1);

                if (flags.verbose)
                {
                    con_printf(_T("primary %d sect %lu label %11.11s\n"), partnum,
                               pDrive->pri_part[partnum].rel_sect,
                               A2T((sector_buffer + label_offset)));
                }

                if (sector_buffer[label_offset + 10] >= 32 &&
                    sector_buffer[label_offset + 10] <= 122)
                {
                    /* Get Volume Label */
                    memcpy(pDrive->pri_part[partnum].vol_label, sector_buffer + label_offset, 11);
                }
            }
        } /* primary partitions */

        /* Get the information from the extended partitions. */

        for (partnum = 0; partnum < MAX_LOGICAL_DRIVES; partnum++)
        {
            strcpy(pDrive->log_drive[partnum].vol_label, "           ");

            if (IsRecognizedFatPartition(
                pDrive->log_drive[partnum].num_type))
            {
                if (pDrive->log_drive[partnum].num_type == 11 ||
                    pDrive->log_drive[partnum].num_type == 12)
                {
                    label_offset = 71;
                }
                else
                {
                    label_offset = 43;
                }

                lba_sect = get_log_drive_start(pDrive, partnum);
                Read_Physical_Sectors_LBA_only((drivenum + 128), lba_sect, 1);

                if (sector_buffer[label_offset + 10] >= 32 &&
                    sector_buffer[label_offset + 10] <= 122)
                {
                    /* Get Volume Label */
                    memcpy(pDrive->log_drive[partnum].vol_label,
                           sector_buffer + label_offset, 11);
                }
            }
        } /* while(partnum<23); */
    } /* while(drivenum<8);*/
}

/* Initialize the LBA Structures */
void Initialize_LBA_Structures(void)
{
    /* Initialize the Disk Address Packet */
    /* ---------------------------------- */

    memset(disk_address_packet, 0, sizeof(disk_address_packet));

    /* Packet size = 16 */
    disk_address_packet[0] = 16;

    /* Reserved, must be 0 */
    disk_address_packet[1] = 0;

    /* Number of blocks to transfer = 1 */
    disk_address_packet[2] = 1;

    /* Reserved, must be 0 */
    disk_address_packet[3] = 0;

    /* Initialize the Result Buffer */
    /* ---------------------------- */

    /* Buffer Size = 26 */
    result_buffer[0] = 26;
}

int Delete_EMBR_Chain_Node(Partition_Table* pDrive, int logical_drive_number)
{
    Partition *p, *nep;

    if (logical_drive_number >= pDrive->num_of_log_drives)
    {
        return 99;
    }

    p = &pDrive->log_drive[logical_drive_number];

    /* If the logical drive to be deleted is not the first logical drive.   */
    /* Assume that there are extended partition tables after this one. If   */
    /* there are not any more extended partition tables, nothing will be    */
    /* harmed by the shift. */
    if (logical_drive_number > 0)
    {
        int index;
        nep = &pDrive->next_ext[logical_drive_number];

        /* Move the extended partition information from this table to the    */
        /* previous table.                                                   */
        Copy_Partition(nep - 1, nep);

        /* Shift all the following extended partition tables left by 1.      */
        for (index = logical_drive_number; index < MAX_LOGICAL_DRIVES - 1;
             index++)
        {
            p = &pDrive->log_drive[index];
            nep = &pDrive->next_ext[index];

            Copy_Partition(p, p + 1);
            Copy_Partition(nep, nep + 1);
            pDrive->log_drive_created[index] =
                pDrive->log_drive_created[index + 1];
            pDrive->next_ext_exists[index - 1] =
                (p->num_type > 0) ? TRUE : FALSE;
        }

        Clear_Partition(&pDrive->log_drive[MAX_LOGICAL_DRIVES - 1]);
        Clear_Partition(&pDrive->next_ext[MAX_LOGICAL_DRIVES - 1]);
        pDrive->log_drive_created[MAX_LOGICAL_DRIVES - 1] = FALSE;
        pDrive->next_ext_exists[MAX_LOGICAL_DRIVES - 1] = FALSE;
        pDrive->next_ext_exists[MAX_LOGICAL_DRIVES - 2] = FALSE;
        pDrive->num_of_log_drives--;
    }
    else
    {
        /* Delete the first logical partition */
        Clear_Partition(p);
        pDrive->log_drive_created[0] = FALSE;
        if (pDrive->num_of_log_drives == 1)
        {
            pDrive->num_of_log_drives--;
        }
    }

    pDrive->part_values_changed = TRUE;
    flags.partitions_have_changed = TRUE;

    return 0;
}

static void Normalize_Log_Table(Partition_Table* pDrive)
{
    int index = 1;

    while (index < pDrive->num_of_log_drives)
    {
        if (pDrive->log_drive[index].num_type == 0)
        {
            Delete_EMBR_Chain_Node(pDrive, index);
        }
        else
        {
            index++;
        }
    }
}

/* Load the Partition Tables and get information on all drives */
int Read_Partition_Tables(void)
{
    int num_ext;
    int error_code = 0;

    flags.maximum_drive_number = 0;
    flags.more_than_one_drive = FALSE;

    for (int drive = 0; drive < MAX_DISKS; drive++)
    {
        Partition_Table* pDrive = &part_table[drive];
        num_ext = 0;

        Clear_Partition_Tables(pDrive);

        /* Get the hard drive parameters and ensure that the drive exists. */
        error_code = Get_Hard_Drive_Parameters(drive + 0x80);

        if (error_code != 0)
        {
            pDrive->total_cyl = 0;
            pDrive->total_head = 0;
            pDrive->total_sect = 0;
            continue;
        }

        pDrive->disk_size_sect = (pDrive->total_cyl + 1) * (pDrive->total_head + 1) * pDrive->total_sect;
        pDrive->disk_size_mb = Convert_Cyl_To_MB((pDrive->total_cyl + 1), pDrive->total_head + 1, pDrive->total_sect);

        pDrive->part_values_changed = FALSE;
        flags.maximum_drive_number = drive + 0x80;

        error_code = Read_Primary_Table(drive, pDrive, &num_ext);
        if (error_code != 0)
        {
            continue;
        }
        pDrive->usable = TRUE;

        if (num_ext == 1)
        {
            error_code = Read_Extended_Table(drive, pDrive);

            if (error_code != 0)
            {
                /* if there is an error processing the extended partition table
                   chain editing of logical drives will be disabled */
                continue;
            }
            pDrive->ext_usable = TRUE;
        }

        /* remove link-only EMBRs with no logical partition in it */
        Normalize_Log_Table(pDrive);
    }

    flags.more_than_one_drive = flags.maximum_drive_number > 0x80;
    flags.partitions_have_changed = FALSE;

    Determine_Drive_Letters();
    Get_Partition_Information();

    return 0;
}

int Mount_Image_File(TCHAR* fName, unsigned int cylinders, unsigned int heads, unsigned int sectors)
{
    int num_ext;
    int error_code = 0;

    int drive = flags.maximum_drive_number + 1 - 0x80;
    if (drive >= MAX_DISKS || drive < 0)
    {
        return 1;
    }
    Partition_Table* pDrive = &part_table[drive];
    num_ext = 0;

    Clear_Partition_Tables(pDrive);

    /* Get the hard drive parameters and ensure that the drive exists. */
    error_code = Get_Image_Drive_Parameters(pDrive, fName, cylinders, heads, sectors);
    if (error_code != 0)
        return error_code;

    pDrive->disk_size_sect = (pDrive->total_cyl + 1) * (pDrive->total_head + 1) * pDrive->total_sect;
    pDrive->disk_size_mb = Convert_Cyl_To_MB((pDrive->total_cyl + 1), pDrive->total_head + 1, pDrive->total_sect);

    pDrive->part_values_changed = FALSE;
    flags.maximum_drive_number = drive + 0x80;

    error_code = Read_Primary_Table(drive, pDrive, &num_ext);
    if (error_code != 0)
        return error_code;

    pDrive->usable = TRUE;

    if (num_ext == 1)
    {
        error_code = Read_Extended_Table(drive, pDrive);

        if (error_code != 0)
        {
            /* if there is an error processing the extended partition table
               chain editing of logical drives will be disabled */
            return error_code;
        }
        pDrive->ext_usable = TRUE;
    }

    /* remove link-only EMBRs with no logical partition in it */
    Normalize_Log_Table(pDrive);

    Determine_Drive_Letters();
    Get_Partition_Information();

    return 0;
}

static void Read_Table_Entry(unsigned char* buf, Partition_Table* pDrive, Partition* p, unsigned long lba_offset)
{
    p->active_status = buf[0x00];

    p->num_type = buf[0x04];
    p->rel_sect = *(unsigned long*)(buf + 0x08);
    p->num_sect = *(unsigned long*)(buf + 0x0c);

    /* If int 0x13 extensions are used compute the virtual CHS values. */
    if (p->rel_sect == 0 && p->num_sect == 0)
    {
        p->start_cyl = p->start_head = p->start_sect = 0;
        p->end_cyl = p->end_head = p->end_sect = 0;
    }
    else
    {
        lba_to_chs(p->rel_sect + lba_offset, pDrive, &p->start_cyl,
                   &p->start_head, &p->start_sect);
        lba_to_chs(p->rel_sect + lba_offset + p->num_sect - 1, pDrive,
                   &p->end_cyl, &p->end_head, &p->end_sect);
    }

    if (p->num_sect == 0xfffffffful)
    {
        /* a protective GPT partition starts at sector
           1 and has a size of 0xffffffff */
        p->end_cyl = pDrive->total_cyl;
    }

    p->size_in_MB = Convert_Sect_To_MB(p->num_sect);
    /*Convert_Cyl_To_MB( p->end_cyl - p->start_cyl + 1,
                          pDrive->total_head + 1, pDrive->total_sect );*/
}


static int Read_Primary_Table(int drive, Partition_Table* pDrive,
                              int* num_ext)
{
    Partition* p;
    int error_code;

    /* Read the Primary Partition Table. */
    if ((error_code = Read_Physical_Sectors(drive + 0x80, 0, 0, 1, 1)) != 0)
    {
        return error_code;
    }

    if (*(unsigned short*)(sector_buffer + 510) != 0xAA55)
    {
        /* no partition table at all */
        return 0;
    }

    if (sector_buffer[0x1be + 4] == 0xEE)
    {
        // TODO: читаем характеристики GPT-раздела.
        int t = 0;
    }

    int entry_offset = 0x1be;
    p = pDrive->pri_part;

    for (int index = 0; index < 4; index++)
    {
        /* process all four slots in MBR */
        Read_Table_Entry(sector_buffer + entry_offset, pDrive, p, 0);
        if (Is_Supp_Ext_Part(p->num_type))
        {
            /* store pointer to first found ext part and count ext parts */
            (*num_ext)++;
            if (!pDrive->ptr_ext_part)
            {
                pDrive->ptr_ext_part = p;
                pDrive->ext_num_sect = p->num_sect;
                pDrive->ext_size_mb = p->size_in_MB;
            }
        }

        p++;
        entry_offset += 16;
    }

    return 0;
}


static int Read_Extended_Table(int drive, Partition_Table* pDrive)
{
    Partition* ep; /* EMBR entry of MBR, first in link chain */
    Partition* nep; /* current EMBR link chain poinrter */
    Partition* p; /* logical partition at entry 1 in current EMBR */

    unsigned long rel_sect;
    int error_code = 0;
    int num_drives = 0;

    /* consider ext part incompatible until opposite is proofed */
    pDrive->ext_usable = FALSE;
    pDrive->num_of_log_drives = 0;
    pDrive->num_of_non_dos_log_drives = 0;

    /* no EMBR eintry in MBR, abort */
    if (!pDrive->ptr_ext_part)
    {
        return 0;
    }

    nep = ep = pDrive->ptr_ext_part;
    p = pDrive->log_drive;

    do
    {
        if (num_drives == MAX_LOGICAL_DRIVES)
        {
            /* make sure we bail out if we cannot handle the number of
               logical partitions */
            return 1;
        }

        error_code = Read_Physical_Sectors(
            drive + 0x80, nep->start_cyl, nep->start_head, nep->start_sect, 1);
        if (error_code != 0)
        {
            return error_code;
        }

        if (*(unsigned short*)(sector_buffer + 510) != 0xAA55)
        {
            /* no valid EMBR signature, abort */
            return 1;
        }

        /* determine LBA offset to calculate CHS values from for
           logical partition, because EMBR entry stores relativ values */
        rel_sect = ep->rel_sect + ((nep != ep) ? nep->rel_sect : 0);
        Read_Table_Entry(sector_buffer + 0x1be, pDrive, p, rel_sect);

        nep = (nep == ep) ? pDrive->next_ext : nep + 1;

        Read_Table_Entry(sector_buffer + 0x1be + 16, pDrive, nep,
                         ep->rel_sect);
        if (Is_Supp_Ext_Part(nep->num_type))
        {
            pDrive->next_ext_exists[num_drives] = TRUE;
        }
        else if (nep->num_type != 0)
        {
            /* No valid EMBR link in second entry found and not end of chain.
               Treat as an error condition */
            return 1;
        }

        if (p->num_type != 0 || nep->num_type != 0)
        {
            num_drives += 1;
        }

        if (sector_buffer[0x1c2 + 32] != 0 ||
            sector_buffer[0x1c2 + 48] != 0)
        {
            /* third and forth entry in EMBR are not empty.
               Treat as an error condition. */
            return 1;
        }

        p += 1;
    }
    while (nep->num_type != 0);

    pDrive->num_of_log_drives = num_drives;

    return 0;
}

/* Load the brief_partition_table[8] [27] */
static void Load_Brief_Partition_Table(void)
{
    int drivenum;
    int partnum;

    for (drivenum = 0; drivenum < MAX_DISKS; drivenum++)
    {
        Partition_Table* pDrive = &part_table[drivenum];

        /* Load the primary partitions into brief_partition_table[8] [27] */
        for (partnum = 0; partnum < 4; partnum++)
        {
            brief_partition_table[drivenum][partnum] =
                pDrive->pri_part[partnum].num_type;
        }

        /* Load the extended partitions into brief_partition_table[8] [27] */
        for (partnum = 0; partnum < MAX_LOGICAL_DRIVES; partnum++)
        {
            brief_partition_table[drivenum][partnum + 4] =
                pDrive->log_drive[partnum].num_type;
        }
    }
}

static void Clear_Partition_Tables(Partition_Table* pDrive)
{
    int index;

    pDrive->usable = FALSE;

    /* Clear the partition_table_structure structure. */
    pDrive->product[0] = 0;
    pDrive->file_name[0] = 0;
    pDrive->pri_free_space = 0;
    pDrive->free_start_cyl = 0;
    pDrive->free_start_head = 0;
    pDrive->free_start_sect = 0;
    pDrive->free_end_cyl = 0;

    for (index = 0; index < 4; index++)
    {
        Clear_Partition(&pDrive->pri_part[index]);
        pDrive->pri_part_created[index] = FALSE;
    }
    Clear_Extended_Partition_Table(pDrive);
}

/* Clear the Extended Partition Table Buffers */
void Clear_Extended_Partition_Table(Partition_Table* pDrive)
{
    pDrive->ext_usable = FALSE;
    pDrive->ptr_ext_part = NULL;
    pDrive->ext_size_mb = 0;
    pDrive->ext_num_sect = 0;
    pDrive->ext_free_space = 0;

    pDrive->log_start_cyl = 0;
    pDrive->log_end_cyl = 0;

    pDrive->log_free_loc = 0;
    pDrive->num_of_log_drives = 0;
    pDrive->num_of_non_dos_log_drives = 0;

    for (int index = 0; index < MAX_LOGICAL_DRIVES; index++)
    {
        Clear_Partition(&pDrive->log_drive[index]);
        Clear_Partition(&pDrive->next_ext[index]);
        pDrive->next_ext_exists[index] = FALSE;
        pDrive->log_drive_created[index] = FALSE;
    }
}

/* Reset the drive, used if read or write errors occur */
static void Reset_Drive(int drive)
{
    printf("TODO: make Reset_Drive()!!!\n");
}

/* Read_Physical_Sector */
int Read_Physical_Sectors(int drive, unsigned long cylinder, unsigned long head, unsigned long sector,
                          int number_of_sectors)
{
    /* Translate CHS values to LBA values. */
    unsigned long LBA_address = chs_to_lba(&part_table[drive - 128], cylinder, head, sector);
    return Read_Physical_Sectors_LBA_only(drive, LBA_address, number_of_sectors);
}


/* Read a physical sector using LBA values */
static int Read_Physical_Sectors_LBA_only(int drive, long long LBA_address, int number_of_sectors)
{
    DWORD bytesRead;
    LARGE_INTEGER offset;

    if (number_of_sectors != 1)
    {
        con_print(_T("sector != 1\n"));
        halt(1);
    }
    if (drive < 0x80 || drive > flags.maximum_drive_number)
    {
        con_printf(_T("drive number (%d) out of range\n"), drive);
        halt(1);
    }

    HANDLE hDisk = CreateFile(part_table[drive - 0x80].file_name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE)
    {
        return (int)GetLastError();
    }

    // Смещение = LBA * размер сектора
    offset.QuadPart = LBA_address * 512; // TODO: заменить бы на получение реального размера сектора!

    for (int try = 0; try < MAX_RETRIES; try++)
    {
        if (SetFilePointerEx(hDisk, offset, NULL, FILE_BEGIN))
        {
            if (ReadFile(hDisk, &sector_buffer, 512, &bytesRead, NULL) || bytesRead != 512)
            {
                CloseHandle(hDisk);
                return 0;
            }
        }
    }
    int err = (int)GetLastError();
    CloseHandle(hDisk);
    return err;
}


/* Translate a CHS value to an LBA value. */
uint64_t chs_to_lba(Partition_Table* pDrive, uint64_t cylinder, uint64_t head, uint64_t sector)
{
    if (cylinder == 0 && head == 0 && sector == 0)
        return 0;
    return (((cylinder * (pDrive->total_head + 1) + head) * pDrive->total_sect + sector - 1));
}

/* Write partition tables */
int Write_Partition_Tables(void)
{
    int error_code = 0;
    int drive_index = 0;
    int drives_with_error = 0;
    int index;

    unsigned long extended_cylinder = 0;
    unsigned long extended_head = 0;
    unsigned long extended_sector = 0;

    for (drive_index = 0; drive_index < MAX_DISKS; drive_index++)
    {
        Partition_Table* pDrive = &part_table[drive_index];

        if ((pDrive->part_values_changed != TRUE && flags.partitions_have_changed != TRUE) || !pDrive->usable)
        {
            continue; /* nothing done, continue with next drive */
        }

        Clear_Sector_Buffer();

        error_code = Read_Physical_Sectors((drive_index + 0x80), 0, 0, 1, 1);

        if (error_code != 0)
        {
            goto drive_error;
        }

        if ((*(unsigned short*)(sector_buffer + 510) != 0xAA55) &&
            (flags.no_ipl != TRUE))
        {
            /* install MBR code if we install a new MBR */
            memcpy(sector_buffer, bootnormal_code, SIZE_OF_IPL);
        }

        Clear_Partition_Table_Area_Of_Sector_Buffer();

        for (index = 0; index < 4; index++)
        {
            /* If this partition was just created, clear its boot sector. */
            if (pDrive->pri_part_created[index] == TRUE)
            {
                error_code = Clear_Boot_Sector(
                    (drive_index + 128), pDrive->pri_part[index].start_cyl,
                    pDrive->pri_part[index].start_head,
                    pDrive->pri_part[index].start_sect);
                if (error_code != 0)
                {
                    goto drive_error;
                }
            }

            if ((pDrive->pri_part[index].num_type == 0x05) ||
                (pDrive->pri_part[index].num_type == 0x0f))
            {
                extended_cylinder = pDrive->pri_part[index].start_cyl;
                extended_head = pDrive->pri_part[index].start_head;
                extended_sector = pDrive->pri_part[index].start_sect;
            }

            StorePartitionInSectorBuffer(&sector_buffer[0x1be + index * 16],
                                         &pDrive->pri_part[index]);
        }

        /* Add the partition table marker values */
        sector_buffer[0x1fe] = 0x55;
        sector_buffer[0x1ff] = 0xaa;

        error_code = Write_Physical_Sectors((drive_index + 0x80), 0, 0, 1, 1);
        if (error_code != 0)
        {
            goto drive_error;
        }

        /* Write the Extended Partition Table, if applicable. */

        if (pDrive->ptr_ext_part && pDrive->ext_usable)
        {
            for (index = 0; index < MAX_LOGICAL_DRIVES; index++)
            {
                /* If this logical drive was just created, clear its boot sector. */
                if (pDrive->log_drive_created[index] == TRUE)
                {
                    if (pDrive->log_drive[index].start_cyl !=
                        extended_cylinder)
                    {
                        con_printf(
                            _T("pDrive->log_drive[index].start_cyl (%lu) != extended_cylinder (%lu)"),
                            pDrive->log_drive[index].start_cyl, extended_cylinder);
                        Pause();
                    }

                    error_code = Clear_Boot_Sector(
                        (drive_index + 0x80), pDrive->log_drive[index].start_cyl,
                        pDrive->log_drive[index].start_head,
                        pDrive->log_drive[index].start_sect);
                    if (error_code != 0)
                    {
                        goto drive_error;
                    }
                }

                Clear_Sector_Buffer();

                /* Add the partition table marker values */
                sector_buffer[0x1fe] = 0x55;
                sector_buffer[0x1ff] = 0xaa;

                StorePartitionInSectorBuffer(&sector_buffer[0x1be],
                                             &pDrive->log_drive[index]);

                if (pDrive->next_ext_exists[index] == TRUE)
                {
                    StorePartitionInSectorBuffer(&sector_buffer[0x1be + 16],
                                                 &pDrive->next_ext[index]);
                }

                error_code = Write_Physical_Sectors((drive_index + 0x80), extended_cylinder, extended_head,
                                                    extended_sector, 1);
                if (error_code != 0)
                {
                    goto drive_error;
                }

                if (pDrive->next_ext_exists[index] != TRUE)
                {
                    break;
                }

                extended_cylinder = pDrive->next_ext[index].start_cyl;
                extended_head = pDrive->next_ext[index].start_head;
                extended_sector = pDrive->next_ext[index].start_sect;
            }
        }

        Lock_Unlock_Drive(drive_index + 0x80, 0);
        continue;

    drive_error:
        Lock_Unlock_Drive(drive_index + 0x80, 0);
        drives_with_error |= 1 << drive_index;
    } /* for (drive_index) */

    return drives_with_error;
}

/* Write physical sectors */
int Write_Physical_Sectors(int drive, unsigned long cylinder, unsigned long head, unsigned long sector,
                           int number_of_sectors)
{
    DWORD bytesWrite;
    LARGE_INTEGER offset;

    if (number_of_sectors != 1)
    {
        con_print(_T("Write_Physical_Sectors(): sector != 1\n"));
        halt(1);
    }
    if (drive < 0x80 || drive > flags.maximum_drive_number)
    {
        con_printf(_T("Write_Physical_Sectors(): drive number (%d) out of range\n"), drive);
        halt(1);
    }

    unsigned long LBA_address = chs_to_lba(&part_table[drive - 128], cylinder, head, sector);

    HANDLE hDisk = CreateFile(part_table[drive - 0x80].file_name, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, 0,
                              NULL);
    if (hDisk == INVALID_HANDLE_VALUE)
    {
        return (int)GetLastError();
    }

    // Смещение = LBA * размер сектора
    offset.QuadPart = LBA_address * 512; // TODO: заменить бы на получение реального размера сектора!

    for (int try = 0; try < MAX_RETRIES; try++)
    {
        if (SetFilePointerEx(hDisk, offset, NULL, FILE_BEGIN))
        {
            if (WriteFile(hDisk, &sector_buffer, 512, &bytesWrite, NULL) || bytesWrite != 512)
            {
                CloseHandle(hDisk);
                return 0;
            }
        }
    }
    int err = (int)GetLastError();
    CloseHandle(hDisk);
    return err;
}


static void StorePartitionInSectorBuffer(unsigned char* sect_buffer, struct Partition* pPart)
{
    unsigned long start_cyl = pPart->start_cyl;
    unsigned long start_head = pPart->start_head;
    unsigned long start_sect = pPart->start_sect;
    unsigned long end_cyl = pPart->end_cyl;
    unsigned long end_head = pPart->end_head;
    unsigned long end_sect = pPart->end_sect;

    if (start_cyl > 1023 && flags.lba_marker)
    {
        start_cyl = 1023;
        start_head = 254;
        start_sect = 63;
    }

    if (end_cyl > 1023 && flags.lba_marker)
    {
        end_cyl = 1023;
        end_head = 254;
        end_sect = 63;
    }

    sect_buffer[0x00] = pPart->active_status;
    sect_buffer[0x01] = start_head;

    *(_u16*)(sect_buffer + 0x02) = combine_cs(start_cyl, start_sect);

    sect_buffer[0x04] = pPart->num_type;

    sect_buffer[0x05] = end_head;

    *(_u16*)(sect_buffer + 0x06) = combine_cs(end_cyl, end_sect);

    *(_u32*)(sect_buffer + 0x08) = pPart->rel_sect;
    *(_u32*)(sect_buffer + 0x0c) = pPart->num_sect;
}

/* convert cylinder count to MB and do overflow checking */
unsigned long Convert_Cyl_To_MB(unsigned long num_cyl, unsigned long total_heads, unsigned long total_sect)
{
    unsigned long mb1 = (num_cyl * total_heads * total_sect) / 2048ul;
    unsigned long mb2 = ((num_cyl - 1) * total_heads * total_sect) / 2048ul;

    return (mb1 > mb2 || num_cyl == 0) ? mb1 : mb2;
}

unsigned long Convert_Sect_To_MB(unsigned long num_sect)
{
    return num_sect / 2048ul;
}

unsigned long Convert_To_Percentage(unsigned long small_num,
                                    unsigned long large_num)
{
    unsigned long percentage;

    /* fix for Borland C not supporting unsigned long long:
       divide values until 100 * small_value fits in unsigned long */
    while (small_num > 42949672ul)
    {
        small_num >>= 1;
        large_num >>= 1;
    }

    if (!large_num)
    {
        return 0;
    }
    percentage = 100 * small_num / large_num;

    if ((100 * small_num % large_num) >= large_num / 2)
    {
        percentage++;
    }
    if (percentage > 100)
    {
        percentage = 100;
    }

    return percentage;
}


/**

 * destSize — это
 */


/**
* Копирует строку из sdd по указанному смещению в TCHAR-буфер.
 * @param sdd Исходный буфер.
 * @param offset Смещение в исходном буфере, содержащее строку asciiz.
 * @param dest Буфер назначения.
 * @param destSize Размер буфера в TCHAR.
 */
static void CopySddString(const STORAGE_DEVICE_DESCRIPTOR* sdd, DWORD offset, TCHAR* dest, size_t destSize)
{
    dest[0] = _T('\0');
    if (!dest || destSize == 0)
        return;

    if (offset == 0 || offset >= sdd->Size)
    {
        return; // строка отсутствует
    }

    const char* src = (const char*)sdd + offset;

#ifdef UNICODE
    // Конвертация из ANSI → Unicode
    MultiByteToWideChar(CP_ACP, 0, src, -1, dest, (int)destSize);
#else
    // Просто копируем как ANSI
    strncpy(dest, src, destSize);
    dest[destSize - 1] = _T('\0');
#endif

    // Убираем хвостовые пробелы (встречаются на CF и прочей фигне)
    size_t len = _tcslen(dest);
    while (len > 0 && _istspace(dest[len - 1]))
    {
        dest[--len] = _T('\0');
    }
}
