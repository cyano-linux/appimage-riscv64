/**************************************************************************
 *
 * Copyright (c) 2004-23 Simon Peter
 * Copyright (c) 2023 Cyano Hao
 *
 * All Rights Reserved.
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
 *
 **************************************************************************/

/* Minimal implemetation of appimagetool that can only create AppImages.
 *
 * For native (non-AppImage) build to work with QEMU user mode emulation.
 * For old distro releases such as CentOS 7.
 *
 * The baseline is manylinux1 (CentOS 5 GCC 4.8).
 *
 * squashfs-tools 4.4 or later required for zstd and offset support.
 *
 * Based on appimagetool.c from github.com/AppImage/appimagetool.
 */

#include <elf.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef LIBDIR
#define LIBDIR "/usr/local/lib"
#endif

using std::string;

class md5 {
	static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
	              "little endian only");

  public:
	using word = uint32_t;
	constexpr static size_t size = 16;
	union md5_t {
		word w[4];
		struct { // named word
			word A, B, C, D;
		};
		uint8_t b[size];
	};
	static_assert(sizeof(md5_t) == size, "md5 size must be 16 bytes");
	using type = md5_t;

  private:
	constexpr static size_t words_per_block = 16;
	constexpr static size_t block_size = words_per_block * sizeof(word);
	union block {
		word w[words_per_block];
		uint8_t b[block_size];
	};
	static_assert(sizeof(block) == 64, "block size must be 64 bytes");

  private:
	constexpr static int rounds_per_block = 4;
	constexpr static int steps_per_round = words_per_block;
	constexpr static int steps_per_block = steps_per_round * rounds_per_block;
	using constant_array_t = word[steps_per_block];

	// GCC 4.8 workaround: use 2 pairs of braces
	constexpr static constant_array_t S{
	    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
	    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
	};
	constexpr static constant_array_t K{
	    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
	    0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
	    0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
	    0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
	    0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
	    0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
	};

  private:
	constexpr static word F(word x, word y, word z) {
		return (x & y) | (~x & z);
	}
	constexpr static word G(word x, word y, word z) {
		return (x & z) | (y & ~z);
	}
	constexpr static word H(word x, word y, word z) { return x ^ y ^ z; }
	constexpr static word I(word x, word y, word z) { return y ^ (x | ~z); }
	constexpr static word rotate_left(word x, word n) {
		return (x << n) | (x >> (32 - n));
	}

  public:
	static md5_t calculate(string &&msg) {
		/* prepare: padding */
		uint64_t original_bits = msg.size() * 8;
		msg.push_back(0x80);
		while (msg.size() % 64 != 56)
			msg.push_back(0);
		for (int n = 0; n < 64; n += 8)
			msg.push_back((original_bits >> n) & 0xff);

		/* do calculate */
		md5_t result = {.w = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476}};
		for (size_t blk_pos = 0; blk_pos < msg.size(); blk_pos += 64) {
			const block &blk =
			    *reinterpret_cast<const block *>(msg.data() + blk_pos);
			// GCC 4.8 workaround: do not use uniform initialization
			md5_t buffer = result;
			word E, j;
			for (int i = 0; i < steps_per_block; i++) {
				const size_t round = i / steps_per_round;
				switch (round) {
				case 0:
					E = F(buffer.B, buffer.C, buffer.D);
					j = i;
					break;
				case 1:
					E = G(buffer.B, buffer.C, buffer.D);
					j = (5 * i + 1) % 16;
					break;
				case 2:
					E = H(buffer.B, buffer.C, buffer.D);
					j = (3 * i + 5) % 16;
					break;
				case 3:
					E = I(buffer.B, buffer.C, buffer.D);
					j = (7 * i) % 16;
					break;
				}
				E += buffer.A + K[i] + blk.w[j];
				buffer.A = buffer.D;
				buffer.D = buffer.C;
				buffer.C = buffer.B;
				buffer.B += rotate_left(E, S[i]);
			}
			result.A += buffer.A;
			result.B += buffer.B;
			result.C += buffer.C;
			result.D += buffer.D;
		}
		return result;
	}
};

#if __cplusplus < 201703L
// pre C++17: odr-used constexpr static member still requires a definition
constexpr md5::constant_array_t md5::S;
constexpr md5::constant_array_t md5::K;
#endif

const string runtime_file{LIBDIR "/appimagetool/runtime"};

void print_usage() {
	fputs("Usage:\n"
	      "  appimagetool SOURCE DESTINATION\n",
	      stderr);
}

string read_file(const string &filename) {
	std::ifstream ifs{filename};
	if (!ifs.is_open())
		throw std::runtime_error{"read_file: failed to open " + filename};
	return string{std::istreambuf_iterator<char>{ifs},
	              std::istreambuf_iterator<char>{}};
}

void run_external(const std::vector<string> &args) {
	const string &file = args[0];
	int pid = fork();
	if (pid == 0) {
		// child
		std::vector<char *> argv;
		std::transform(
		    args.begin(), args.end(), std::back_inserter(argv),
		    [](const string &arg) { return const_cast<char *>(arg.c_str()); });
		argv.push_back(nullptr);
		execvp(file.c_str(), argv.data());
		throw std::runtime_error{"run_external: execvp failed on " + file};
	} else if (pid > 0) {
		// parent
		int wstatus;
		if (waitpid(pid, &wstatus, 0) == -1)
			throw std::runtime_error{"run_external: waitpid failed on " + file};
		if (WIFEXITED(wstatus) && (WEXITSTATUS(wstatus) == 0))
			return;
		else
			throw std::runtime_error{"run_external: " + file +
			                         " exited with status " +
			                         std::to_string(WEXITSTATUS(wstatus))};
	} else
		throw std::runtime_error{"run_external: fork failed on " + file};
}

struct section_stat {
	size_t offset;
	size_t length;
};

section_stat
appimage_get_elf_section_offset_and_length(const string &elf,
                                           const string &section_name) {
	const char *data = elf.data();
	uint8_t klass = data[EI_CLASS];

	if (klass == ELFCLASS32) {
		auto elf = reinterpret_cast<const Elf32_Ehdr *>(data);
		auto shdr = reinterpret_cast<const Elf32_Shdr *>(data + elf->e_shoff);
		auto str_tab = reinterpret_cast<const char *>(
		    data + shdr[elf->e_shstrndx].sh_offset);
		for (int i = 0; i < elf->e_shnum; i++) {
			if (section_name == (str_tab + shdr[i].sh_name))
				return {shdr[i].sh_offset, shdr[i].sh_size};
		}
	} else if (klass == ELFCLASS64) {
		auto elf = reinterpret_cast<const Elf64_Ehdr *>(data);
		auto shdr = reinterpret_cast<const Elf64_Shdr *>(data + elf->e_shoff);
		auto str_tab = reinterpret_cast<const char *>(
		    data + shdr[elf->e_shstrndx].sh_offset);
		for (int i = 0; i < elf->e_shnum; i++) {
			if (section_name == (str_tab + shdr[i].sh_name))
				return {shdr[i].sh_offset, shdr[i].sh_size};
		}
	} else
		throw std::runtime_error{
		    "appimage_get_elf_section_offset_and_length: platforms other than "
		    "32-bit/64-bit are currently not supported!"};

	throw std::runtime_error{
	    "appimage_get_elf_section_offset_and_length: section " + section_name +
	    " not found"};
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		print_usage();
		return 1;
	}
	const string source{argv[1]};
	const string destination{argv[2]};
	const string runtime = read_file(runtime_file);

	{ /* part 1: mksquashfs */
		std::vector<string> mksquashfs_args{
		    "mksquashfs",
		    source,
		    destination,
		    "-offset",
		    std::to_string(runtime.size()),
		    "-comp",
		    "zstd",
		    "-root-owned",
		    "-noappend",
		    "-b",
		    "1M",
		    "-mkfs-time",
		    "0",
		};
		run_external(mksquashfs_args);
	}

	{ /* part 2: embed runtime */
		FILE *fpdst = fopen(destination.c_str(), "rb+");
		if (fpdst == nullptr)
			throw std::runtime_error{"Failed to open the AppImage for writing"};
		fseek(fpdst, 0, SEEK_SET);
		auto blocks = fwrite(runtime.data(), runtime.size(), 1, fpdst);
		fclose(fpdst);

		if (chmod(destination.c_str(), 0755) != 0)
			throw std::runtime_error{
			    "Failed to set the AppImage as executable"};
	}

	{ /* part 3: embed digest */
		auto elf = read_file(destination);

		auto section_digest =
		    appimage_get_elf_section_offset_and_length(elf, ".digest_md5");
		auto section_sign =
		    appimage_get_elf_section_offset_and_length(elf, ".sha256_sig");
		auto section_key =
		    appimage_get_elf_section_offset_and_length(elf, ".sig_key");

		if (section_digest.length < md5::size)
			throw std::runtime_error{
			    ".digest_md5 section in runtime's ELF header is too "
			    "small (found " +
			    std::to_string(section_digest.length) +
			    " bytes, minimum required: " + std::to_string(md5::size) +
			    " bytes)\n"};

		elf.replace(section_digest.offset, section_digest.length,
		            section_digest.length, 0);
		elf.replace(section_sign.offset, section_sign.length,
		            section_sign.length, 0);
		elf.replace(section_key.offset, section_key.length, section_key.length,
		            0);

		auto digest = md5::calculate(std::move(elf));

		std::unique_ptr<FILE, decltype(&fclose)> fpdst{
		    fopen(destination.c_str(), "rb+"), &fclose};
		if (!fpdst)
			throw std::runtime_error{
			    "Failed to open the AppImage for updating"};
		if (fseek(fpdst.get(), section_digest.offset, SEEK_SET) != 0)
			throw std::runtime_error{
			    "Failed to embed MD5 digest: could not seek to section offset"};
		if (fwrite(digest.b, md5::size, 1, fpdst.get()) != 1)
			throw std::runtime_error{
			    "Failed to embed MD5 digest: write failed"};
	}

	return 0;
}
