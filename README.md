# microRTOS
This repository is to be used as a reference for the RTOS developed by April Douglas and Matija Jankovic for ECE 445M in Spring 2024.

The repository includes libraries used in the creation and operation of the OS such as the FAT filesystem driver ff.c, SD card driver eDisk.c, and ELF file loader loader.c

### What is self-written
- OS.c (contains all thread creation, scheduling, FIFO, and other OS code)
- osasm.s (context switching assembly code and SVC handlers)
- eFile.c (file layer for user programs to interact with the filesystem)
- heap.c (basic first-fit heap allocation implementation for OS allocated structures)
- Jitter.c (used to measure time jitter on a thread)

### What is modified by us to work with the OS
- UART0Int.c (allows UART communication, for example to a connected PC serial terminal, to be safe during context switching)
- WideTimer1.c (modified from the original for bugfixing)

### What is unmodified and used as a library
- loader.c (ELF file loader)
- ff.c (FAT filsystem driver)
- eDisk.c (SD card driver)
