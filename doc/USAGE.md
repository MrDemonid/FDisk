# Free FDISK Options (Windows Fork)

## English

### Syntax
```
FDISK [<drive#>] [commands]...
```
- **no argument** – Runs in interactive mode
- `/INFO` – Displays partition information of `<drive#>`

---

### Commands to create and delete partitions
- `<size>` is a number in megabytes or `MAX` for maximum size  
  or `<number>,100` for `<number>` percent
- `<type#>` is a numeric partition type or FAT-12/16/32 if `/SPEC` not given

```
/PRI:<size> [/SPEC:<type#>]   Creates a primary partition
/EXT:<size>                   Creates an extended DOS partition
/LOG:<size> [/SPEC:<type#>]   Creates a logical drive
/PRIO,/EXTO,/LOGO             Same as above, but avoids FAT32
/AUTO                         Automatically partitions the disk
/DELETE {/PRI[:#] | /EXT | /LOG:<part#> | /NUM:<part#>}  Deletes a partition (logical drives start at /NUM=5)
/DELETEALL                    Deletes all partitions from <drive#>
```

---

### Setting active partitions
```
/ACTIVATE:<partition#>   Sets <partition#> active
/DEACTIVATE              Deactivates all partitions
```

---

### MBR (Master Boot Record) management
```
/CLEARMBR   Deletes all partitions and boot code
/LOADMBR    Loads partition table and code from "boot.mbr" into MBR
/SAVEMBR    Saves partition table and code into "boot.mbr"
```

---

### MBR code modifications (partitions intact)
```
/IPL        Installs the standard boot code into MBR <drive#> (same as /MBR and /CMBR for compatibility)
/SMARTIPL   Installs DriveSmart IPL into MBR <drive#>
/LOADIPL    Writes 440 code bytes from "boot.mbr" into MBR
```

---

### Advanced partition table modification
```
/MODIFY:<part#>,<type#>       Changes partition type to <type#> (logical drives start at "5")
/MOVE:<srcpart#>,<destpart#>  Moves primary partitions
/SWAP:<1stpart#>,<2ndpart#>   Swaps primary partitions
```

---

### Disk flags
```
/CLEARFLAG[{:<flag#>} | /ALL]  Resets <flag#> or all flags on <drive#>
/SETFLAG:<flag#>[,<value>]     Sets <flag#> to 1 or <value>
/TESTFLAG:<flag#>[,<value>]    Tests <flag#> for 1 or <value>
```

---

### Disk information
```
/STATUS    Displays the current partition layout
/DUMP      Dumps partition information from all hard disks (debugging)
```

---

### Interactive UI switches
```
/UI        Always starts UI if given as last argument
/FPRMT     Prompts for FAT32/FAT16 in interactive mode
/XO        Enables extended options
```

---

### For image files
```
/IMAGE:<file>,cccc:hhh:ss   Mounts a disk image with parameters:
   <file> – disk image file name
   cccc  – cylinders (up to 1024)
   hhh   – heads (up to 255)
   ss    – sectors (from 1 to 63)
```

---


## Русский

### Синтаксис
```
FDISK [<номер_диска>] [команды]...
```
- **без аргументов** – Запуск в интерактивном режиме
- `/INFO` – Отображает информацию о разделах на `<номер_диска>`

---

### Команды для создания и удаления разделов
- `<size>` – число в мегабайтах или `MAX` для максимального размера либо `<число>,100` для процента от всего диска
- `<type#>` – числовой код типа раздела или FAT-12/16/32, если `/SPEC` не задан

```
/PRI:<size> [/SPEC:<type#>]   Создает основной раздел
/EXT:<size>                   Создает расширенный раздел
/LOG:<size> [/SPEC:<type#>]   Создает логический диск
/PRIO,/EXTO,/LOGO             То же самое, но без FAT32
/AUTO                         Автоматическая разбивка диска
/DELETE {/PRI[:#] | /EXT | /LOG:<номер> | /NUM:<номер>} Удаляет раздел (логические диски начинаются с /NUM=5)
/DELETEALL                    Удаляет все разделы на диске <номер_диска>
```

---

### Установка активного раздела
```
/ACTIVATE:<номер>    Устанавливает раздел активным
/DEACTIVATE          Снимает активность со всех разделов
```

---

### Управление MBR (главная загрузочная запись)
```
/CLEARMBR   Удаляет все разделы и загрузочный код
/LOADMBR    Загружает таблицу разделов и код из файла "boot.mbr" в MBR
/SAVEMBR    Сохраняет таблицу разделов и код в файл "boot.mbr"
```

---

### Изменения загрузочного кода (разделы не затрагиваются)
```
/IPL        Устанавливает стандартный загрузочный код в MBR <диск> (синонимы: /MBR, /CMBR)
/SMARTIPL   Устанавливает загрузчик DriveSmart IPL в MBR <диск>
/LOADIPL    Записывает 440 байт кода из "boot.mbr" в MBR
```

---

### Расширенные изменения таблицы разделов
```
/MODIFY:<номер>,<тип>        Меняет тип раздела на <тип> (логические диски начинаются с "5")
/MOVE:<src>,<dst>            Перемещает основные разделы
/SWAP:<1>,<2>                Меняет местами основные разделы
```

---

### Флаги на диске
```
/CLEARFLAG[{:<флаг>} | /ALL]  Сбрасывает <флаг> или все флаги на диске
/SETFLAG:<флаг>[,<значение>]  Устанавливает <флаг> в 1 или <значение>
/TESTFLAG:<флаг>[,<значение>] Проверяет, установлен ли <флаг> в 1 или <значение>
```

---

### Информация о дисках
```
/STATUS   Показывает текущую разметку диска
/DUMP     Выгружает информацию о разделах всех дисков (для отладки)
```

---

### Параметры интерфейса
```
/UI        Всегда запускает интерактивный режим (если последний аргумент)
/FPRMT     Спрашивает FAT32/FAT16 при работе в интерактивном режиме
/XO        Включает расширенные параметры
```

---

### Работа с файлами-образами
```
/IMAGE:<файл>,cccc:hhh:ss   Подключает файл-образ диска:
   <файл> – имя файла образа
   cccc   – цилиндры (не более 1024)
   hhh    – головки (не более 255)
   ss     – секторы (от 1 до 63)
```

---

### Отличия Windows-версии
**Убраны опции:**
- `/REBOOT` – не нужна, изменения видны сразу.
- `/MONO` – под Windows не имеет смысла.
- `/X` – Windows не работает с INT 13.

**Добавлены опции командной строки:**
- `/IMAGE` – подключить файл как образ диска.

**Добавлены опции INI-файла:**
- `/LANG` – задает используемый язык в программе.

---

## License / Лицензия

This program is Copyright © 1998–2025 by Brian E. Reifsnyder and
The FreeDOS Project under the terms of the GNU General Public License,
version 2.

This is a modified version of Free FDisk adapted for Microsoft Windows.
Copyright © 2025 Andrey Hlus.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License,
version 2, as published by the Free Software Foundation.

This program comes as-is and without warranty of any kind. The author of
this software assumes no responsibility pertaining to the use or mis-use of
this software. By using this software, the operator is understood to be
agreeing to the terms of the above.

----
