#include <efi.h>
#include <efilib.h>
#include <elf.h>

typedef unsigned long long size_t;

// Default frame buffer struct. Holds information for any graphics driver
typedef struct {
	void* BaseAddress;
	size_t BufferSize;
	unsigned int Width;
	unsigned int Height;
	unsigned int PixelsPerScanLine;
} Framebuffer;

// Magic bytes to identify header
#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

typedef struct {
	unsigned char magic[2];			// bytes that header stores to identify file as psf file
	unsigned char mode;					// mode psf font is in
	unsigned char charsize;			// how large the char size is in bytes
} PSF1_HEADER;

typedef struct {
	PSF1_HEADER* psf1_Header;
	void* glyphBuffer;
} PSF1_FONT;

Framebuffer framebuffer;

/*
**	Param:
*/
Framebuffer* InitializeGOP(){
	EFI_GUID gopGUID = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
	EFI_STATUS status;

	// Calls the calling convention of UEFI to make sure things are called correctly
	status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGUID, NULL, (void**)&gop);
	if(EFI_ERROR(status)){
		Print(L"Unable to locate GOP\n\r");
		return NULL;
	} else {
		Print(L"GOP Located\n\r");
	}

	framebuffer.BaseAddress = (void*)gop->Mode->FrameBufferBase;
	framebuffer.BufferSize = gop->Mode->FrameBufferSize;
	framebuffer.Width = gop->Mode->Info->HorizontalResolution;
	framebuffer.Height = gop->Mode->Info->VerticalResolution;
	framebuffer.PixelsPerScanLine = gop->Mode->Info->PixelsPerScanLine;

	return &framebuffer;
}

/*
**	Param:
** Directory - location of FileInfo
** Path - Array of chars that define system Path
**
** Required param:
** ImageHandle
** SystemTable
*/
EFI_FILE* LoadFile(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable){
	EFI_FILE* LoadedFile;

	// Get EFI_LOADED_IMAGE_PROTOCOL (https://dox.ipxe.org/structEFI__LOADED__IMAGE__PROTOCOL.html)
	EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
	SystemTable->BootServices->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage);

	// Get EFI_SIMPLE_FILE_SYSTEM_PROTOCOL (https://dox.ipxe.org/SimpleFileSystem_8h.html)
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
		SystemTable->BootServices->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&FileSystem);

	// Check if directory is NULL. If it is null, set directory to root of file system
	if(Directory == NULL){
		FileSystem->OpenVolume(FileSystem, &Directory);
	}

	// Check if file a file is loaded. If not then return NULL, if successful, load the file.
	EFI_STATUS s = Directory->Open(Directory, &LoadedFile, Path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
	if(s != EFI_SUCCESS){
		return NULL;
	}

	return LoadedFile;
}

PSF1_FONT* LoadPSF1Font(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable){
	EFI_FILE* font = LoadFile(Directory, Path, ImageHandle, SystemTable);
	if(font == NULL){
		return NULL;
	}

	PSF1_HEADER* fontHeader;
	SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(PSF1_HEADER), (void**)&fontHeader);
	UINTN size = sizeof(PSF1_HEADER);
	font->Read(font, &size, fontHeader);

	if(fontHeader->magic[0] != PSF1_MAGIC0 || fontHeader->magic[1] != PSF1_MAGIC1){
		return NULL;
	}

	UINTN glyphBufferSize = fontHeader->charsize * 256;
	if(fontHeader->mode == 1){	// 512 glyph Mode
		glyphBufferSize = fontHeader->charsize * 512;
	}

	void* glyphBuffer;
	{
		font->SetPosition(font, sizeof(PSF1_HEADER));
		SystemTable->BootServices->AllocatePool(EfiLoaderData, glyphBufferSize, (void**)&glyphBuffer);
		font->Read(font, &glyphBufferSize, glyphBuffer);
	}

	PSF1_FONT* finishedFont;
	SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(PSF1_FONT), (void**)&finishedFont);
	finishedFont->psf1_Header = fontHeader;
	finishedFont->glyphBuffer = glyphBuffer;

	return finishedFont;
}

// Memory compare
int memcmp(const void* aptr, const void* bptr, size_t n){
	const unsigned char* a = aptr;
	const unsigned char* b = bptr;

	for(size_t i = 0; i < n; i++){
		if(a[i] < b[i]){
			return -1;
		} else if(a[i] > b[i]){
			return 1;
		}
	}

	return 0;
}

EFI_STATUS efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
	// Setup UEFI environment to allow for special commands (ex. Print(L""))
	InitializeLib(ImageHandle, SystemTable);
	Print(L"Hello World from EFI_Main! \n\r");

	// Check if file loaded is loaded successfully
	EFI_FILE* Kernel = LoadFile(NULL, L"kernel.elf", ImageHandle, SystemTable);
	if(Kernel== NULL){
		Print(L"Could not load kernel \n\r");
	} else {
		Print(L"Kernel loaded successfully \n\r");
	}

	// Create ELF header
	Elf64_Ehdr header;
	{
		UINTN FileInfoSize;
		EFI_FILE_INFO* FileInfo;
		Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, NULL);
		SystemTable->BootServices->AllocatePool(EfiLoaderData, FileInfoSize, (void**)&FileInfo);		// Allocate memory
		Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, (void**)&FileInfo);

		// Fill with Kernel header information
		UINTN size = sizeof(header);
		Kernel->Read(Kernel, &size, &header);			// Read bytes
	}

	/*
	** ELF Identification  (https://docs.oracle.com/cd/E19683-01/816-1386/chapter6-35342/index.html)
	** EI_MAG0 = File identification
	** EI_CLASS = File class
	** EI_DATA = Data encoding
	** ET_EXEC = Executable file
	** EM_X86_64 = AMD x86-64 architecture
	** EV_CURRENT = Current version
	*/
	if(
		memcmp(&header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
		header.e_ident[EI_CLASS] != ELFCLASS64 ||
		header.e_ident[EI_DATA] != ELFDATA2LSB ||
		header.e_type != ET_EXEC ||
		header.e_machine != EM_X86_64 ||
		header.e_version != EV_CURRENT
	)
	{
		Print(L"Kernel format is bad! \r\n");
	} else {
		Print(L"Kernel header successfully verified! \r\n");
	}

	Elf64_Phdr* phdrs;
	{
		// Set offsite of file in bytes
		Kernel->SetPosition(Kernel, header.e_phoff);
		UINTN size = header.e_phnum * header.e_phentsize;

		// Allocate memory for program headers
		SystemTable->BootServices->AllocatePool(EfiLoaderData, size, (void**)&phdrs);
		Kernel->Read(Kernel, &size, phdrs);
	}

	// Load information for header
	for(
		Elf64_Phdr* phdr = phdrs;
		(char*)phdr < (char*) phdrs + header.e_phnum * header.e_phentsize;
		phdr = (Elf64_Phdr*)((char*)phdr + header.e_phentsize)
	)
	{
		// Load the program into the memory where it needs to be
		switch(phdr->p_type){
			case PT_LOAD:
			{
				// Get memory size and round up to prevent overflow
				int pages = (phdr->p_memsz + 0x1000 - 1) / 0x1000;

				// Allocate memory for pages
				Elf64_Addr segment = phdr->p_paddr;
				SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);

				// Set correct offset for Kernel
				Kernel->SetPosition(Kernel, phdr->p_offset);
				UINTN size = phdr->p_filesz;
				Kernel->Read(Kernel, &size, (void*)segment);
				break;
			}
		}
	}

	Print(L"Kernel Loaded\n\r");

	// Call entry of Kernel
	void(*KernelStart)(Framebuffer*, PSF1_FONT*) = ((__attribute__((sysv_abi)) void (*)(Framebuffer*, PSF1_FONT*) ) header.e_entry);

	PSF1_FONT* newFont = LoadPSF1Font(NULL, L"zap-light16.psf", ImageHandle, SystemTable);
	if(newFont == NULL){
		Print(L"Font is not valid or is not found!\n\r");
	} else {
		Print(L"Font found. char size = %d\n\r", newFont->psf1_Header->charsize);
	}

	Framebuffer* newBuffer = InitializeGOP();

	// Display Base, Size, Width, Height, and PixelsPerScanLine for frame buffer
	Print(L"Base: 0x%x\n\r	Size: 0x%x\n\r	Width: %d\n\r	Height: %d\n\r	PixelsPerScanLine: %d\n\r	\n\r",
	newBuffer->BaseAddress,
	newBuffer->BufferSize,
	newBuffer->Width,
	newBuffer->Height,
	newBuffer->PixelsPerScanLine);

	KernelStart(newBuffer, newFont);

	return EFI_SUCCESS; // Exit the UEFI application
}
