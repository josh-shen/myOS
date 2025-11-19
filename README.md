# myOS
A hobby 32-bit x86 operating system.
## Build Instructions
### Prerequisites
#### Linux/WSL:
```
sudo apt install build-essential libgmp-dev libmpc-dev libmpfr-dev qemu-system-x86 xorriso
```
##### WSL will also need to install GRUB:
```
sudo apt install grub-common grub-pc-bin
```
#### macOS:
```
xcode-select --install
brew install gmp libmpc mpfr qemu xorriso
```
### GCC Cross-Compiler  
Building this OS requires a GCC cross-compiler. Building the cross-compiler requires binutils and GCC. The latest source
code for these can be downloaded from:
-  https://ftp.gnu.org/gnu/binutils/
- https://ftp.gnu.org/gnu/gcc/
```
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
```
#### binutils
```
cd $HOME/src

tar xf binutils-x.y.z.tar.gz

mkdir build-binutils
cd build-binutils
../binutils.x.y.z/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make
make install
```
#### GCC
```
cd $HOME/src

mkdir build-gcc
cd build-gcc
../gcc-x.y.z/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers --disable-hosted-libstdcxx
make all-gcc
make all-target-libgcc
make all-target-libstdc++-v3
make install-gcc
make install-target-libgcc
make install-target-libstdc++-v3
```
### Build and Run
Save the cross-compiler to PATH. This new compiler can now be used to build the OS. Several scripts are used to build 
the OS. `iso.sh` creates a bootable CD-ROM image of the OS. `qemu.sh` starts the OS in a QEMU emulator.
## Memory Management
OS utilizes a virtual memory system. 
### Physical Memory Management
A PMM implemented with a buddy allocator manages blocks of physical memory. 
### Virtual Memory Management
Virtual memory and paging are handled by the VMM. VMM manages virtual address space as one continuous block. When a 
block of virtual address space is needed, the block is split and marked as used. Freeing virtual address space does the 
reverse, block is marked free and merged (if possible). 
### Kernel Heap
Built on top of the PMM and VMM is the kernel heap. The kernel heap uses a slab allocator. A slab allocator has caches 
for block sizes of 2^5 to 2^11. Each cache of N size contains slabs, which each contain blocks of memory of the same 
size N. A cache has three slabs, full, partial, and empty. When kmalloc requests memory, caches of size N provides the 
requested memory, unless the size is greater than 2^11, which then the request is fulfilled by the VMM instead.
### kswapd
The kswapd thread is a special kernel thread that manages memory usage. The thread uses a LRU cache to determine which
pages to free when memory usage exceeds a certain threshold. The thread uses three thresholds:
- min_watermark: calculated as 20 <= p <= 255, p = total free pages / 128. When this point is reached, kswapd will free 
pages *synchronously*
- low_watermark: twice the min_watermark. At this point, kswapd is woken up and starts freeing pages *asnychronously*.
- high_watermark: three times the min_watermark. When kswapd is freeing pages, it free pages up to this point.

The LRU cache maintains two lists: active and inactive. When a page is first allocated, it is added to the inactive 
list. When kswapd is woken up, it scans both lists. Pages in the active list are demoted to the inactive list if they 
have not been recently accessed. Pages in the inactive list are promoted to the active list if they have been recently 
accessed. The thread checks if a page has been accessed by checking its accessed bit in the PTE.
### Page Faults
