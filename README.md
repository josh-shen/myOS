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
