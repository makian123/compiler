#include "elf.hpp"
#include <string>
#include <cstdint>
#include <iostream>
#include <concepts>
#include <type_traits>
#include <cstring>
#include <bit>

using std::uint16_t, std::uint64_t, std::uint32_t;

struct CommonHeader{
	//0x7f + "ELF"
	char magic[4];
	//[1] 32bit, [2] 64bit
	char architecture{};
	//[1] little, [2] big
	char endianness{};
	//[0] SYS-V
	char OSABI{};
	char padding[8] = { 0 };
	//[1] reloc, [2] execute, [3] shared, [4] core
	uint16_t objType{};

	/**
	 * [0] none
	 * [2] sparc
	 * [3] x8
	 * [8] MIPS
	 * [0x14] PPC
	 * [0x28] ARM
	 * [0x2A] SuperH
	 * [0x32] IA-64
	 * [0x3E] x86_64
	 * [0xB7] AArch64
	 * [0xF3] RISC-V
	*/
	uint16_t instructSet{};
	uint16_t elfVersion{};
};
struct ELFHeader32{
	CommonHeader common;
	uint32_t pEntry{};
	uint32_t pHTable{};
	uint32_t sHTable{};

	uint32_t flags{};
	uint16_t headerSize{};
	uint16_t pHTEntrySize{};
	uint16_t pHTEntryCount{};
	uint16_t sHTEntrySize{};
	uint16_t sHTEntryCount{};
	uint16_t sHTIndex{};

	ELFHeader32() = default;
};
struct ELFHeader64{
	CommonHeader common;
	uint64_t pEntry{};
	uint64_t pHTable{};
	uint64_t sHTable{};

	uint32_t flags{};
	uint16_t headerSize{};
	uint16_t pHTEntrySize{};
	uint16_t pHTEntryCount{};
	uint16_t sHTEntrySize{};
	uint16_t sHTEntryCount{};
	uint16_t sHTIndex{};

	ELFHeader64() = default;
};

struct PHeader32{
	/**
	 * [0] null
	 * [1] load
	 * [2] dynamic
	 * [3] interp
	 * [4] note
	*/
	uint32_t segmentType;
	uint32_t pOffset;
	uint32_t pVaddr;
	uint32_t undefined;
	uint32_t pFilesz;
	uint32_t memsz;
	/**
	 * [1] executable
	 * [2] writable
	 * [4] readable
	*/
	uint32_t flags;
	//Power of 2
	uint32_t alignement;
};
struct PHeader64{
	/**
	 * [0] null
	 * [1] load
	 * [2] dynamic
	 * [3] interp
	 * [4] note
	*/
	uint32_t segmentType;
	uint32_t pOffset;
	uint64_t pVaddr;
	uint64_t undefined;
	uint64_t pFilesz;
	uint64_t memsz;
	/**
	 * [1] executable
	 * [2] writable
	 * [4] readable
	*/
	uint64_t flags;
	//Power of 2
	uint64_t alignement;
};

static constexpr char machineEndianness = (std::endian::native == std::endian::little ? 1 : 2);
static constexpr char machineArchitecture = (INTPTR_MAX == INT32_MAX ? 1 : 2);

template<typename T>
concept ELF = std::is_same_v<T, ELFHeader64> || std::is_same_v<T, ELFHeader32>;
template<ELF T>
T EmptyHeader(){
	CommonHeader h;

	std::strncpy(h.magic, "\177ELF", 4);
	if constexpr(std::is_same_v<T, ELFHeader64>) h.architecture = 2;
	else h.architecture = 1;



	T ret;
	ret.common = h;

	return ret;
}

void BuildFile(){
}