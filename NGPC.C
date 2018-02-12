//////////////////////////////////////////////////////////////////////////////
// ngpc.c                                                                   //
//////////////////////////////////////////////////////////////////////////////

#define POKELINK "PokeLink v1.01j1"
//
// v1.01j1
//      Jeff modified to work on win95/98.
//      Compiled with DJGPP.
//      Now displays cart Manufacturer & Device ID's.
//
// v1.01
//		Added size detection.
//		Little programming fix.
//		Added check for valid header in file.
//		Added Windows icon.
//
// v1.00
//		More command line options (/y /b /v)
//		Implemented some algorithm to make it faster.
//
// v0.02
//		Able to flash a file specified on command line (/w)
//		Added GBXPORT environment setting.
//
// v0.01
//		Shows license text of cartridge. (/i)
//
//
// This is my tool to flash NGPC carts with the
// Pocket Linker device. That's why I call it "PokeLink".
// Only EPP mode is supported right now because SPP sucks.
// Tested with: 32MBit Pocket Flash Card.
//
// Please credit me if you use anything from this source. Tnx.
//
// For bug reports or suggestions, just e-mail me... at um..
//	ngpcdev@egroups.com ;-)
//
// Dark Fader / BlackThunder
//


//  Game Name             Cart Name   Manufacturer ID  Device ID
// 32M Pocket Flash Card   ------          0x98          0x2f
// Pacman                 PACMAN           0x98          0xab
// Samurai Showdown 2     SAMURAI2v000     0x98          0x2f


//////////////////////////////////////////////////////////////////////////////
// includes                                                                 //
//////////////////////////////////////////////////////////////////////////////


//#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <ctype.h>
//#include <conio.h>

//#include <crt0.h>
//#include <go32.h>
//#include <pc.h>
//#include <string.h>
//#include <signal.h>
#include <dos.h>                        // needed for inportb,outportb
//#include <dpmi.h>



void DeInitGBX();

//////////////////////////////////////////////////////////////////////////////
// defines                                                                  //
//////////////////////////////////////////////////////////////////////////////
// EPP I/O registers
#define EPP_STAT	(port+1)
#define EPP_CTRL	(port+2)
#define EPP_AI		(port+3)
#define EPP_DATA	(port+4)

// EPP_CTRL register
#define CTRL_READ	0
#define CTRL_WRITE	1
#define CTRL_RESET	4

// EPP_AI register
#define AI_ADDR0	0	// A0-A7
#define AI_ADDR1	1	// A8-A15
#define AI_GBXCTRL	2	// A16-A20, auto-increment
#define AI_DATA		3	// D0-D7, A21-A23 (via latch)

// flash status bits
#define DATA_POLLING		7
#define TOGGLE_BIT			6	// toggles when busy
#define TIMEOUT				5	// 1 indicated timout error
#define SECTOR_ERASE_TIMER	3	// 0 when accepting sector erase command

// flash unlock addresses with chip select
#define fx0002AA    ((addr) & 0x200000 | 0x0002AA)
#define fx000555    ((addr) & 0x200000 | 0x000555)
#define fx000AAA    ((addr) & 0x200000 | 0x000AAA)
#define fx002AAA    ((addr) & 0x200000 | 0x002AAA)
#define fx005555	((addr) & 0x200000 | 0x005555)

// options
#define OPTION_INFO		1
#define OPTION_BACKUP	2
#define OPTION_WRITE	4
#define OPTION_VERIFY	8
#define OPTION_YES		16

//////////////////////////////////////////////////////////////////////////////
// variables                                                                //
//////////////////////////////////////////////////////////////////////////////
unsigned int port = 0x378;
char *filename = 0;
int options = 0;
int err = 0;

//////////////////////////////////////////////////////////////////////////////
// PauseOnError                                                             //
//////////////////////////////////////////////////////////////////////////////
void PauseOnError()
{
	if (err)
	{
		printf("Press any key to continue...\n");
		getch();
	}
}

//////////////////////////////////////////////////////////////////////////////
// InitGiveIO                                                               //
//////////////////////////////////////////////////////////////////////////////
//int InitGiveIO()
//{
//    // open & close giveio device for direct port access
//    if ((_winmajor < 4) || (_winmajor > 9)) return 0;
//    HANDLE h = CreateFile("\\\\.\\giveio", 0, 0, NULL, OPEN_ALWAYS, 0, NULL);
//    if (h == INVALID_HANDLE_VALUE) return err=1;
//    CloseHandle(h);
//    return 0;
//}

//////////////////////////////////////////////////////////////////////////////
// SetAddress                                                               //
//////////////////////////////////////////////////////////////////////////////
void SetAddress(unsigned long addr, int inc)
{
    //inc=0;

    outportb(EPP_CTRL, CTRL_WRITE);   //_outp
    outportb(EPP_AI, AI_ADDR0);
    outportb(EPP_DATA, (addr>>0) & 0xFF);  // A0-A7
    outportb(EPP_AI, AI_ADDR1);
    outportb(EPP_DATA, (addr>>8) & 0xFF);  // A8-A15
    outportb(EPP_AI, AI_GBXCTRL);
    outportb(EPP_DATA, 0x02);              // latch chip select
    outportb(EPP_AI, AI_DATA);
    outportb(EPP_DATA, ~(1<<(addr>>21)));  // A21-A23
    outportb(EPP_AI, AI_GBXCTRL);
    outportb(EPP_DATA,                     // A16-A20, auto-increment
		(((addr>>16) & 0x0F) << 2) |
		(((addr>>20) & 0x01) << 7) |
		(inc ? 0x01 : 0x00)
	);
    outportb(EPP_AI, AI_DATA);
}

//////////////////////////////////////////////////////////////////////////////
// InitGBX                                                                  //
//////////////////////////////////////////////////////////////////////////////
int InitGBX()
   {
   // Init/test GBX chip
   outportb(EPP_STAT, 1);     // EPP mode
   SetAddress(0x123456, 0);
   outportb(EPP_CTRL, CTRL_READ);
   outportb(EPP_AI, AI_ADDR0); if (inportb(EPP_DATA) != 0x56) return err=1;
   outportb(EPP_AI, AI_ADDR1); if (inportb(EPP_DATA) != 0x34) return err=1;
   outportb(EPP_AI, AI_GBXCTRL); if (inportb(EPP_DATA) != 0x7E) return err=1;
   return 0;
   }

//////////////////////////////////////////////////////////////////////////////
// DeInitGBX                                                                //
//////////////////////////////////////////////////////////////////////////////
void DeInitGBX()
   {
   outportb(EPP_CTRL, CTRL_WRITE);
   outportb(EPP_AI, AI_GBXCTRL);
   outportb(EPP_DATA, 0);
   // reset EPP port
   outportb(EPP_CTRL, CTRL_RESET);
   }

//////////////////////////////////////////////////////////////////////////////
// WriteByte                                                                //
//////////////////////////////////////////////////////////////////////////////
void WriteByte(unsigned char data)
   {
   outportb(EPP_CTRL, CTRL_WRITE);
   outportb(EPP_DATA, data);
   }

//////////////////////////////////////////////////////////////////////////////
// ReadByte                                                                 //
//////////////////////////////////////////////////////////////////////////////
unsigned char ReadByte()
   {
   outportb(EPP_CTRL, CTRL_READ);
   return inportb(EPP_DATA);
   }

//////////////////////////////////////////////////////////////////////////////
// SectorErase                                                              //
//////////////////////////////////////////////////////////////////////////////
int SectorErase(unsigned long addr)
{
	// 0x10000 per block
	// last 0x10000 is divided into 4 blocks
	// with sizes of: 0x8000, 0x2000, 0x2000, 0x4000
	while (ReadByte() & SECTOR_ERASE_TIMER);
	SetAddress(fx005555,0); WriteByte(0xAA);
	SetAddress(fx002AAA,0); WriteByte(0x55);
	SetAddress(fx005555,0); WriteByte(0x80);
	SetAddress(fx005555,0); WriteByte(0xAA);
	SetAddress(fx002AAA,0); WriteByte(0x55);
	SetAddress(addr,0);
	//while (ReadByte() & SECTOR_ERASE_TIMER);
	WriteByte(0x30);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Program                                                                  //
//////////////////////////////////////////////////////////////////////////////
int Program(unsigned long addr, unsigned char data, int reties)
{
    unsigned long to=10000;

    // program byte
	SetAddress(fx005555,0); WriteByte(0xAA);
	SetAddress(fx002AAA,0); WriteByte(0x55);
	SetAddress(fx005555,0); WriteByte(0xA0);
	SetAddress(addr,0);	WriteByte(data);

	//while ((ReadByte()&128) != (data&128));

    while (to--)
	{
		unsigned char s = ReadByte();
		if ((s & 128) == 0) return 0;	// ok
		if (s & 32)
		{
			int s = ReadByte();
			if ((s & 128) == 0) return 0;	// ok
			//SetAddress(addr,0);
			if (data == ReadByte()) return 0;	// ok
			//return err=1;	// fail
		}
		//if (data == ReadByte()) return 0;	// ok
	}

	SetAddress(addr,0);
	if (data == ReadByte()) return 0;	// ok
	if (reties == 0) return err=1;
	return Program(addr, data, reties-1);
}

//////////////////////////////////////////////////////////////////////////////
// ResetRead                                                                //
//////////////////////////////////////////////////////////////////////////////
void ResetRead()
{
    unsigned long addr=0;

    for (addr=0; addr<0x400000; addr+=0x200000)
	{
		SetAddress(fx005555,0); WriteByte(0xAA);
		SetAddress(fx002AAA,0); WriteByte(0x55);
		SetAddress(fx005555,0); WriteByte(0xF0);
	}
}

//////////////////////////////////////////////////////////////////////////////
// ChipErase                                                                //
//////////////////////////////////////////////////////////////////////////////
int ChipErase()
{
    unsigned long addr=0;

    for (addr=0; addr<0x400000; addr+=0x200000)
    {
        SetAddress(fx005555,0); WriteByte(0xAA);
        SetAddress(fx002AAA,0); WriteByte(0x55);
        SetAddress(fx005555,0); WriteByte(0x80);
        SetAddress(fx005555,0); WriteByte(0xAA);
        SetAddress(fx002AAA,0); WriteByte(0x55);
        SetAddress(fx005555,0); WriteByte(0x10);

		// wait for completion
		while (~ReadByte() & 0x80);

	    // data polling to tell when done
		/*unsigned char data = ReadByte();
		while (!(data & 0x80) && !(data & 0x20))
        {
			data = ReadByte();
		}
		if ((data & 0xA0) == 0xA0)
		{
			printf("Error erasing chip %c !\n", 'A'+(addr>>21));
			return err=1;
		}*/
    }
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Device ID                                                                //
//////////////////////////////////////////////////////////////////////////////
unsigned char ManufID()
{
	unsigned long addr=0;
	SetAddress(fx005555,0); WriteByte(0xAA);
	SetAddress(fx002AAA,0); WriteByte(0x55);
	SetAddress(fx005555,0); WriteByte(0x90);
    SetAddress(0x000000,0);   // manufacturer ID (0x98)
    //SetAddress(0x000001,0); // ID (0x2F)
	//SetAddress(0x000002,0);	// (0x01)
	//SetAddress(0x000003,0);	// (0x81)
	return ReadByte();
}


//////////////////////////////////////////////////////////////////////////////
// Device ID                                                                //
//////////////////////////////////////////////////////////////////////////////
unsigned char DeviceID()
{
	unsigned long addr=0;
	SetAddress(fx005555,0); WriteByte(0xAA);
	SetAddress(fx002AAA,0); WriteByte(0x55);
	SetAddress(fx005555,0); WriteByte(0x90);
	//SetAddress(0x000000,0);	// manufacturer ID (0x98)
    SetAddress(0x000001,0); // ID (0x2F)
    //SetAddress(0x000002,0);   // (0x01)
	//SetAddress(0x000003,0);	// (0x81)
	return ReadByte();
}

//////////////////////////////////////////////////////////////////////////////
// FileSize                                                                 //
//////////////////////////////////////////////////////////////////////////////
unsigned long FileSize(FILE *f)
{
    unsigned long size = 0;
	fseek(f, 0, SEEK_END);
    size = ftell(f);
	fseek(f, 0, SEEK_SET);
	return size;
}

//////////////////////////////////////////////////////////////////////////////
// DetectSize2                                                              //
//////////////////////////////////////////////////////////////////////////////
int DetectSize2(unsigned char *header, unsigned long addr)
   {
   int i=0;

   SetAddress(addr,1);
   for (i=0; i<28; i++)
      {
      if (header[i] != ReadByte()) return 1;
      }
   return 0;
   }

//////////////////////////////////////////////////////////////////////////////
// DetectSize                                                               //
//////////////////////////////////////////////////////////////////////////////
unsigned long DetectSize()
   {
   unsigned long size;
   unsigned char header[28];
   int i=0;

   // read header
   SetAddress(0x000000,1);
   for (i=0; i<28; i++) header[i] = ReadByte();
   size = 0x100000; if (!DetectSize2(header, size)) return size;
   size = 0x200000; if (!DetectSize2(header, size)) return size;
   size = 0x400000; return size;
   }

//////////////////////////////////////////////////////////////////////////////
// ActionInfo                                                               //
//////////////////////////////////////////////////////////////////////////////
int ActionInfo()
{
	unsigned long i;

	ResetRead();
	SetAddress(0x000000,1);

	// show recognition code
	printf("License string  : \"");
	for (i=0; i<28; i++) printf("%c", ReadByte());
	printf("\"\n");

	// show startup address
	i = ReadByte();
	i |= ReadByte()<<8;
	i |= ReadByte()<<16;
	i |= ReadByte()<<24;
    printf("Startup address : 0x%06lX\n", i);

	// show ID code's & color compatability
	i = ReadByte();
	i |= ReadByte()<<8;
    printf("ID code         : 0x%04lX\n", i);
    printf("ID sub-code     : 0x%02X\n", ReadByte());
	printf("Color compatable: %s\n", ReadByte() & 0x10 ? "yes" : "no");

	// show title
	printf("Title           : \"");
	for (i=0; i<16; i++) printf("%c", ReadByte());
	printf("\"\n");

	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// ActionVerify                                                             //
//////////////////////////////////////////////////////////////////////////////
int ActionVerify()
{
	FILE *f = fopen(filename, "rb");
    unsigned long percent = (FileSize(f)-0x100)/100;
    unsigned long addr=0;

    if (!f) { printf("Can't open file for verify!\n"); return err=1; }

	ResetRead();

    for (addr=0; ; addr++)
	{
        int data = 0;
		if ((addr & 0xFF) == 0x00)
		{
            printf("\rVerifying... 0x%06lX (%ld%%)", addr, addr/percent);
			SetAddress(addr,1);
		}
        data = fgetc(f);
		if (data == EOF) break;
		if (ReadByte() != data)
		{
			SetAddress(addr,0);
            printf("\rVerify error at 0x%06lX (w:0x%02X, r:0x%02X)!\n", addr, data, ReadByte());
			return err=1;
		}
	}
	printf("\n");

	fclose(f);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// ActionBackup                                                             //
//////////////////////////////////////////////////////////////////////////////
int ActionBackup()
{
	FILE *f = fopen(filename, "rb");
    unsigned long size = DetectSize();
	unsigned long percent = (size-0x100)/100;
    unsigned long addr=0;

    if (f)
	{
		fclose(f);
		printf("Overwrite file with backup?\n");
		if (tolower(getch()) != 'y') return 0;
	}
	f = fopen(filename, "wb");
	if (!f) { printf("Can't open file!\n"); return err=1; }

	ResetRead();

	// detect size

	// backup
    for (addr=0; addr<size; addr++)
	{
		if ((addr & 255) == 0)
		{
			SetAddress(addr,1);
            printf("\rBacking up... 0x%06lX (%ld%%)", addr, addr/percent);
		}
		fputc(ReadByte(), f);
	}
	printf("\n");

	fclose(f);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// ActionWrite                                                              //
//////////////////////////////////////////////////////////////////////////////
int ActionWrite()
{
	FILE *f = fopen(filename, "rb");
    unsigned long percent = (FileSize(f)-0x100)/100;
    int erase = 1;
    int fast=0;
    unsigned long addr=0;

    if (!f) { printf("Can't open file to flash!\n"); return err=1; }

//    printf("cp1\n");
	ResetRead();
//    printf("cp2\n");

	// verify header
	if ((options & OPTION_YES) == 0)
	{
        int i=0;
        unsigned long check=0;

		fseek(f, 0x0, SEEK_SET);
        for (i=0; i<28; i++)
		{
			check = (check<<1) ^ fgetc(f);
		}
		//printf("%08X\n", check); return 0;
		if (check != 0xDC4446C4)	//" LICENSED BY SNK CORPORATION"
		if (check != 0xE8D446C4)	//"COPYRIGHT BY SNK CORPORATION"
		{
			printf("Invalid header found, continue?\n");
			if (tolower(getch()) != 'y') return err=1;
		}
	}
//    printf("cp3\n");

	// check if it's the same cartridge

	// verify if same ID detected
	if ((options & OPTION_YES) == 0)
	{
		fseek(f, 0x20, SEEK_SET);
		SetAddress(0x20, 1);
		if (fgetc(f) == ReadByte())
		if (fgetc(f) == ReadByte())
		if (fgetc(f) == ReadByte())
		{
			printf("Cartridge ID matches with file, erase first?\n");
            if (tolower(getch()) != 'y') erase = 0;
		}
	}
//    printf("cp4\n");

	// erase chip first
	if (erase)
	{
        printf("Erasing...\n");
//        printf("Erasing...");
		if (ChipErase()) return err=1;
	}
//    printf("cp5\n");

//    printf("\n");

	ResetRead();

    fseek(f, 0, SEEK_SET);
    for (addr=0; ; addr++)
	{
        int data = 0;

        if ((addr & 255) == 0)
		{
			SetAddress(addr,1); fast=1;
            printf("\rProgramming... 0x%06lX (%ld%%)", addr, addr/percent);
		}
        data = fgetc(f);
		if (fast && (data == ReadByte())) continue;	// check same data in increment mode
		if (data == 0xFF) continue;			// nothing to write
		if (data == EOF) break;				// end of file
		fast=0;								// disable fast checking
		if (Program(addr, data, 3))
		{
			SetAddress(addr,0);
            printf("\rProgram error at 0x%06lX (w:0x%02X, r:0x%02X)!\n", addr, data, ReadByte());
			return err=1;
		}
	}
	printf("\n");

	fclose(f);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// main                                                                     //
//////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)  //*argv[])
{
    int a=0;

	// title
    printf(POKELINK" by Dark Fader / BlackThunder\n");
    printf("  Modified for win95/98 by Jeff Frohwein.\n");
    printf("  This version might not work on Win2000 or NT.\n\n");

	// parse command line
    for (a=1; a<argc; a++)
	{
		if ((argv[a][0] == '/') || (argv[a][0] == '-'))
		{
			// option
			switch (tolower(argv[a][1]))
			{
				case '?':
                case 'h':
				{
					printf("Syntax: PokeLink [<option(s)>] [<filename>]\n");
					printf("\n");
					printf("  /?           Shows this help screen.\n");
                    printf("  /h           Shows this help screen.\n");
                    printf("  /p<port>     Specified the port to use. Default = 0x378.\n");
					printf("                 (overridden by GBXPORT environment setting)\n");
					printf("  /w           Write & verify the specified file.\n");
					printf("  /v           Verify the specified file.\n");
					printf("  /i           Displays cartridge info.\n");
					printf("  /b           Backs up cartridge.\n");
					printf("  /y           Use YES on all questions.\n");
//                    printf("  /e           Pause on errors.\n");
					printf("  <filename>   File to write, backup or/and verify.\n");
					printf("\n");
					printf("For example: \"PokeLink /b /i /v /w mygame.ngp\" would\n");
					printf("  flash mygame.ngp, display info, back-up and then verify.\n");
					printf("  Quite useless example, but it shows the sequence.\n");
					printf("\n");
					return 0;
				}

				case 'p': { port = strtoul(&argv[a][2], NULL, 10); break; }
				case 'i': { options |= OPTION_INFO; break; }
				case 'b': { options |= OPTION_BACKUP; break; }
				case 'w': { options |= OPTION_WRITE; break; }
				case 'v': { options |= OPTION_VERIFY; break; }
				case 'y': { options |= OPTION_YES; break; }
//                case 'e': { atexit(PauseOnError); break; }

				default:
				{
					printf("Warning: Invalid option '%c'!\n", argv[a][1]);
					break;
				}
			}
		}
		else
		{
			// filename
			filename = argv[a];
			printf("Filename: \"%s\".\n", filename);
		}
	}

	// environment settings
	if (getenv("GBXPORT"))
	{
		port = strtoul(getenv("GBXPORT"), NULL, 10);
	}

	// Init GiveIO
//    if (InitGiveIO()) { printf("GiveIO driver could not be opened!\n"); return err=1; }

	// Check for Flash Linker at {GBXPORT} or 0x378
	if (InitGBX()) { printf("Flash Linker could not be found at port 0x%3X!\nPlease check the connection and that it's turned on.\n", port); return err=1; }
//    atexit(DeInitGBX);

    printf("Manufacturer ID = 0x%x, Device ID = 0x%x\n\n", ManufID(), DeviceID());

	// Check for cartridge
	if (DeviceID() == 0x00) { printf("No cartridge detected!\n"); return err=1; }

	// Options to perform
    if (options & OPTION_WRITE)
       if (ActionWrite()) return err=1;
    if (options & OPTION_INFO)
       if (ActionInfo()) return err=1;
    if (options & OPTION_BACKUP)
       if (ActionBackup()) return err=1;
    if (options & OPTION_VERIFY)
       if (ActionVerify()) return err=1;

    DeInitGBX();

	return 0;
}